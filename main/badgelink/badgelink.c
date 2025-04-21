
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include "../usb_device.h"
#include "assert.h"
#include "badgelink_appfs.h"
#include "badgelink_fs.h"
#include "badgelink_internal.h"
#include "badgelink_nvs.h"
#include "badgelink_startapp.h"
#include "cobs.h"
#include "esp_crc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "string.h"

// Set to 1 to see raw bytes before/after COBS encoding/decoding.
#ifndef DUMP_RAW_BYTES
#define DUMP_RAW_BYTES 0
#endif

static char const TAG[] = "badgelink";

typedef struct {
    uint8_t data[31];
    uint8_t len;
} fragment_t;

// Badgelink packet singleton used for both the request and its response.
badgelink_Packet badgelink_packet;
// What the current file transfer is for.
badgelink_xfer_t badgelink_xfer_type = BADGELINK_XFER_NONE;
// Whether the current file transfer is host->badge.
bool             badgelink_xfer_is_upload;
// Current transfer position.
uint32_t         badgelink_xfer_pos;
// Transfer file size.
uint32_t         badgelink_xfer_size;

// Next serial number received must be larger mod 32.
static uint32_t next_serial = 0;
// Buffer for transmitted / received frames.
// Frame refers here to the networking term, not the computer graphics term.
static uint8_t  frame_buffer[BADGELINK_BUF_CAP];

// Queue that sends received data over to the BadgeLink thread.
static QueueHandle_t rxqueue;
// Handle to the BadgeLink thread.
static TaskHandle_t  badgelink_thread_handle;
// Main function for the BadgeLink thread.
static void          badgelink_thread_main(void*);

// Prepare the data for the BadgeLink service to start.
void badgelink_init() {
    rxqueue = xQueueCreate(16, sizeof(fragment_t));
}

// Start the badgelink service.
void badgelink_start() {
    xTaskCreate(badgelink_thread_main, "BadgeLink", 8192, NULL, 0, &badgelink_thread_handle);
}

// Encode and send a packet.
void badgelink_send_packet() {
    // Allocate memory to encode the packet.
    size_t packed_len;
    bool   encodable = pb_get_encoded_size(&packed_len, &badgelink_Packet_msg, &badgelink_packet);
    if (!encodable) {
        ESP_LOGE(TAG, "[BUG] Packet is not encodable");
        return;
    }
    size_t encoded_len = COBS_ENCODED_MAX_LENGTH(packed_len + 4);
    if (encoded_len >= BADGELINK_BUF_CAP) {
        ESP_LOGE(TAG, "[BUG] Too much data to send");
        return;
    }

    // Offset the data to be packed so it's placed at the end of the buffer.
    // This is needed so that the COBS encode doesn't overwrite any of the data before it's processed.
    size_t offset = BADGELINK_BUF_CAP - packed_len - 4;

    // Encode packet and add CRC32 checksum.
    pb_ostream_t encode_stream = pb_ostream_from_buffer(frame_buffer + offset, packed_len);
    pb_encode(&encode_stream, &badgelink_Packet_msg, &badgelink_packet);
    uint32_t crc                          = esp_crc32_le(0, frame_buffer + offset, packed_len);
    frame_buffer[packed_len + offset]     = crc;
    frame_buffer[packed_len + offset + 1] = crc >> 8;
    frame_buffer[packed_len + offset + 2] = crc >> 16;
    frame_buffer[packed_len + offset + 3] = crc >> 24;

#if DUMP_RAW_BYTES
    printf("Response:");
    for (size_t i = 0; i < packed_len + 4; i++) {
        printf(" %02x", frame_buffer[i + offset]);
    }
    printf("\n");
#endif

    // COBS-encode the buffer for sending.
    encoded_len = cobs_encode(frame_buffer, frame_buffer + offset, packed_len + 4);

#if DUMP_RAW_BYTES
    printf("Encoded:");
    for (size_t i = 0; i < encoded_len; i++) {
        printf(" %02x", frame_buffer[i]);
    }
    printf("\n");
#endif

    // Send the raw frame.
    usb_send_data(frame_buffer, encoded_len);
}

// Send a status response packet.
void badgelink_send_status(badgelink_StatusCode code) {
    badgelink_packet.which_packet                = badgelink_Packet_response_tag;
    badgelink_packet.packet.response.which_resp  = 0;
    badgelink_packet.packet.response.status_code = code;
    badgelink_send_packet();
}

// Abort / finish a transfer.
static void xfer_stop(bool abnormal) {
    switch (badgelink_xfer_type) {
        case BADGELINK_XFER_APPFS:
            badgelink_appfs_xfer_stop(abnormal);
            break;
        case BADGELINK_XFER_FS:
            badgelink_fs_xfer_stop(abnormal);
            break;
        default:
            break;
    }
    badgelink_xfer_type = BADGELINK_XFER_NONE;
}

// Handle an upload chunk.
static void xfer_upload_chunk() {
    badgelink_Chunk* chunk = &badgelink_packet.packet.request.req.upload_chunk;
    if (chunk->position != badgelink_xfer_pos) {
        ESP_LOGE(TAG, "Incorrect chunk position; expected %" PRIu32 " but got %" PRIu32, badgelink_xfer_pos,
                 chunk->position);
        xfer_stop(true);
        badgelink_status_ill_state();
        return;
    }
    if (badgelink_xfer_pos + chunk->data.size > badgelink_xfer_size) {
        ESP_LOGE(TAG, "Incorrect chunk size");
        xfer_stop(true);
        badgelink_status_ill_state();
        return;
    }

    // `chunk->data.size` will be overwritten by respone packet, save it.
    uint32_t chunk_len = chunk->data.size;

    switch (badgelink_xfer_type) {
        case BADGELINK_XFER_APPFS:
            badgelink_appfs_xfer_upload();
            break;
        case BADGELINK_XFER_FS:
            badgelink_fs_xfer_upload();
            break;
        default:
            ESP_LOGE(TAG, "Invalid internal transfer state");
            badgelink_status_int_err();
            break;
    }

    badgelink_xfer_pos += chunk_len;
}

// Handle a download chunk.
static void xfer_download_chunk() {
    switch (badgelink_xfer_type) {
        case BADGELINK_XFER_APPFS:
            badgelink_appfs_xfer_download();
            break;
        case BADGELINK_XFER_FS:
            badgelink_fs_xfer_download();
            break;
        default:
            ESP_LOGE(TAG, "Invalid internal transfer state");
            badgelink_status_int_err();
            break;
    }

    badgelink_xfer_pos += badgelink_packet.packet.response.resp.download_chunk.data.size;
}

// Handle transfer control packet.
static void xfer_ctrl() {
    badgelink_XferReq ctrl = badgelink_packet.packet.request.req.xfer_ctrl;

    switch (ctrl) {
        case badgelink_XferReq_XferContinue:
            if (badgelink_xfer_is_upload) {
                ESP_LOGE(TAG, "Download continue sent while in upload transfer");
                xfer_stop(true);
                badgelink_status_ill_state();
            } else {
                xfer_download_chunk();
            }
            break;
        case badgelink_XferReq_XferAbort:
            xfer_stop(true);
            break;
        case badgelink_XferReq_XferFinish:
            if (badgelink_xfer_pos != badgelink_xfer_size) {
                ESP_LOGE(TAG, "Transfer finished too early");
                badgelink_status_ill_state();
                xfer_stop(true);
            } else {
                xfer_stop(false);
            }
            break;
        default:
            badgelink_status_unsupported();
            break;
    }
}

// Handle a received packet.
static void handle_packet() {
    if (badgelink_packet.which_packet == badgelink_Packet_sync_tag) {
        if (!badgelink_packet.packet.sync) {
            badgelink_status_malformed();
            return;
        }
        // Sync packet received; set next expected serial number and respond with the same sync packet.
        next_serial = badgelink_packet.serial + 1;
        badgelink_send_packet();
        return;
    } else if (badgelink_packet.which_packet != badgelink_Packet_request_tag) {
        badgelink_status_malformed();
        return;
    } else if (badgelink_packet.serial - next_serial >= UINT32_MAX >> 1) {
        // Sequence number is negative, ignore the packet.
        // This serves primarily to ignore retransmissions.
        return;
    }

    next_serial = badgelink_packet.serial + 1;

    if (badgelink_xfer_type != BADGELINK_XFER_NONE) {
        if (badgelink_packet.packet.request.which_req == badgelink_Request_upload_chunk_tag) {
            if (badgelink_xfer_is_upload) {
                xfer_upload_chunk();
            } else {
                ESP_LOGE(TAG, "Upload chunk sent while in download transfer");
                xfer_stop(true);
                badgelink_status_ill_state();
            }
            return;
        } else if (badgelink_packet.packet.request.which_req == badgelink_Request_xfer_ctrl_tag) {
            xfer_ctrl();
            return;
        } else {
            ESP_LOGE(TAG, "Transfer cancelled abruptly");
            xfer_stop(true);
        }
    }

    switch (badgelink_packet.packet.request.which_req) {
        case badgelink_Request_upload_chunk_tag:
            ESP_LOGE(TAG, "Transfer chunk without transfer in progress");
            badgelink_status_ill_state();
            break;
        case badgelink_Request_xfer_ctrl_tag:
            ESP_LOGE(TAG, "Transfer control without transfer in progress");
            badgelink_status_ill_state();
            break;
        case badgelink_Request_start_app_tag:
            badgelink_startapp_handle();
            break;
        case badgelink_Request_nvs_action_tag:
            badgelink_nvs_handle();
            break;
        case badgelink_Request_appfs_action_tag:
            badgelink_appfs_handle();
            break;
        case badgelink_Request_fs_action_tag:
            badgelink_fs_handle();
            break;
        default:
            badgelink_status_unsupported();
            break;
    }
}

// Handle a received frame.
static void handle_frame(size_t len) {
    if (len < 6) {
        // Any frame cannot be smaller than the COBS-encoded length of 5 bytes.
        // That is because this assumes no protobuf packet is smaller than a byte,
        // and there is always a 4-byte CRC32 checksum.
        return;
    }

#if DUMP_RAW_BYTES
    ESP_LOGI(TAG, "Received %zu-byte frame", len);
    printf("Data:");
    for (size_t i = 0; i < len; i++) {
        printf(" %02x", frame_buffer[i]);
    }
    printf("\n");
#endif

    // Decode the COBS frame.
    assert(frame_buffer[len - 1] == 0);
    size_t decoded_len = cobs_decode(frame_buffer, frame_buffer, len);

#if DUMP_RAW_BYTES
    printf("Decoded:");
    for (size_t i = 0; i < decoded_len; i++) {
        printf(" %02x", frame_buffer[i]);
    }
    printf("\n");

    ESP_LOGI(TAG, "COBS-decoded successfully");
#endif

    // Check the CRC32.
    uint32_t actual_crc = esp_crc32_le(0, frame_buffer, decoded_len - 4);
    uint32_t packet_crc = frame_buffer[decoded_len - 4] | (frame_buffer[decoded_len - 3] << 8) |
                          (frame_buffer[decoded_len - 2] << 16) | (frame_buffer[decoded_len - 1] << 24);

    if (actual_crc != packet_crc) {
        // CRC32 error; send CRC error response and ignore.
        ESP_LOGE(TAG, "CRC32 error; packet: 0x%08" PRIx32 ", actual 0x%08" PRIx32, packet_crc, actual_crc);
        return;
    }

    // Try to decode the packet.
    pb_istream_t decode_istream = pb_istream_from_buffer(frame_buffer, decoded_len - 4);
    if (!pb_decode(&decode_istream, &badgelink_Packet_msg, &badgelink_packet)) {
        ESP_LOGE(TAG, "Failed to decode %zu-byte packet", decoded_len);
    } else {
        handle_packet();
    }
}

// Main function for the BadgeLink thread.
static void badgelink_thread_main(void* ignored) {
    (void)ignored;

    // Amount of data in the receive buffer.
    size_t     rxbuf_len = 0;
    // Fragment received from USB or UART.
    fragment_t frag;
    // Frame being dropped because it's too long.
    bool       toolong = false;

    while (1) {
        xQueueReceive(rxqueue, &frag, portMAX_DELAY);
        for (size_t i = 0; i < frag.len; i++) {
            if (rxbuf_len >= BADGELINK_BUF_CAP) {
                toolong = true;
            }
            if (toolong) {
                if (frag.data[i] == 0) {
                    toolong   = false;
                    rxbuf_len = 0;
                }
            } else {
                frame_buffer[rxbuf_len++] = frag.data[i];
                if (frag.data[i] == 0) {
                    handle_frame(rxbuf_len);
                    rxbuf_len = 0;
                }
            }
        }
    }
}

// Handle received data.
void badgelink_rxdata_cb(uint8_t const* buf, size_t len) {
    fragment_t frag;
    while (len) {
        if (len > sizeof(frag.data)) {
            frag.len = sizeof(frag.data);
        } else {
            frag.len = len;
        }
        memcpy(frag.data, buf, frag.len);
        if (xQueueSend(rxqueue, &frag, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Fragment dropped");
        }
        buf += frag.len;
        len -= frag.len;
    }
}
