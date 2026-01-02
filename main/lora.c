#include "lora.h"
#include <string.h>
#include "bsp/tanmatsu.h"
#include "chatdb_messages.h"
#include "crypto/aes.h"
#include "crypto/hmac_sha256.h"
#include "esp_err.h"
#include "esp_hosted_custom.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "lora_protocol.h"
#include "meshcore/packet.h"
#include "meshcore/payload/advert.h"
#include "meshcore/payload/grp_txt.h"
#include "portmacro.h"
#include "tanmatsu_coprocessor.h"

const char TAG[] = "lora";

QueueHandle_t     lora_packet_queue          = NULL;
SemaphoreHandle_t lora_mutex                 = NULL;
SemaphoreHandle_t lora_transaction_semaphore = NULL;
uint32_t          lora_sequence_number       = 0;
uint8_t           lora_packet_buffer[sizeof(uint32_t) + 512];
size_t            lora_packet_size = 0;

static esp_err_t lora_transaction(const uint8_t* request, size_t request_length, uint8_t* out_response,
                                  size_t* response_length, size_t max_response_length) {
    esp_err_t result = ESP_FAIL;
    xSemaphoreTake(lora_mutex, portMAX_DELAY);
    xSemaphoreTake(lora_transaction_semaphore, 0);  // Clear semaphore
    result = esp_hosted_send_custom(1, (uint8_t*)request, request_length);
    if (result == ESP_OK) {
        if (xSemaphoreTake(lora_transaction_semaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {  // Wait for response
            if (lora_packet_size <= max_response_length) {
                memcpy(out_response, lora_packet_buffer, lora_packet_size);
                *response_length = lora_packet_size;
                result           = ESP_OK;
            } else {
                result = ESP_ERR_INVALID_SIZE;
            }
        } else {
            result = ESP_ERR_TIMEOUT;
        }
    }
    lora_sequence_number++;
    xSemaphoreGive(lora_mutex);
    return result;
}

//////

const char* type_to_string(meshcore_payload_type_t type) {
    switch (type) {
        case MESHCORE_PAYLOAD_TYPE_REQ:
            return "Request";
        case MESHCORE_PAYLOAD_TYPE_RESPONSE:
            return "Response";
        case MESHCORE_PAYLOAD_TYPE_TXT_MSG:
            return "Plain text message";
        case MESHCORE_PAYLOAD_TYPE_ACK:
            return "Acknowledgement";
        case MESHCORE_PAYLOAD_TYPE_ADVERT:
            return "Node advertisement";
        case MESHCORE_PAYLOAD_TYPE_GRP_TXT:
            return "Group text message (unverified)";
        case MESHCORE_PAYLOAD_TYPE_GRP_DATA:
            return "Group data message (unverified)";
        case MESHCORE_PAYLOAD_TYPE_ANON_REQ:
            return "Anonymous request";
        case MESHCORE_PAYLOAD_TYPE_PATH:
            return "Returned path";
        case MESHCORE_PAYLOAD_TYPE_TRACE:
            return "Trace";
        case MESHCORE_PAYLOAD_TYPE_MULTIPART:
            return "Multipart";
        case MESHCORE_PAYLOAD_TYPE_RAW_CUSTOM:
            return "Custom raw";
        default:
            return "UNKNOWN";
    }
}

const char* route_to_string(meshcore_route_type_t route) {
    switch (route) {
        case MESHCORE_ROUTE_TYPE_TRANSPORT_FLOOD:
            return "Transport flood";
        case MESHCORE_ROUTE_TYPE_FLOOD:
            return "Flood";
        case MESHCORE_ROUTE_TYPE_DIRECT:
            return "Direct";
        case MESHCORE_ROUTE_TYPE_TRANSPORT_DIRECT:
            return "Transport direct";
        default:
            return "Unknown";
    }
}

const char* role_to_string(meshcore_device_role_t role) {
    switch (role) {
        case MESHCORE_DEVICE_ROLE_CHAT_NODE:
            return "Chat Node";
        case MESHCORE_DEVICE_ROLE_REPEATER:
            return "Repeater";
        case MESHCORE_DEVICE_ROLE_ROOM_SERVER:
            return "Room Server";
        case MESHCORE_DEVICE_ROLE_SENSOR:
            return "Sensor";
        default:
            return "Unknown";
    }
}

static uint8_t mc_keys[2][16] = {
    {0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a, 0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72},
    {0x16, 0xf1, 0xb7, 0xa6, 0xa9, 0x61, 0xce, 0x16, 0xd0, 0x8b, 0xfa, 0xd7, 0xbd, 0x6f, 0x47, 0x47},
};

void meshcore_parse(uint8_t* packet, size_t packet_length) {
    meshcore_message_t message;
    if (meshcore_deserialize(packet, packet_length, &message) < 0) {
        ESP_LOGE(TAG, "Failed to deserialize message");
        return;
    }

    printf("Type: %s [%d]\n", type_to_string(message.type), message.type);
    printf("Route: %s [%d]\n", route_to_string(message.route), message.route);
    printf("Version: %d\n", message.version);
    printf("Path Length: %d\n", message.path_length);
    if (message.path_length > 0) {
        printf("Path: ");
        for (unsigned int i = 0; i < message.path_length; i++) {
            printf("0x%02x, ", message.path[i]);
        }
        printf("\n");
    }
    printf("Payload Length: %d\n", message.payload_length);
    if (message.payload_length > 0) {
        printf("Payload [%d]: ", message.payload_length);
        for (unsigned int i = 0; i < message.payload_length; i++) {
            printf("%02X", message.payload[i]);
        }
        printf("\n");
    }

    if (message.type == MESHCORE_PAYLOAD_TYPE_ADVERT) {
        meshcore_advert_t advert;
        if (meshcore_advert_deserialize(message.payload, message.payload_length, &advert) >= 0) {
            printf("Decoded node advertisement:\n");
            printf("Public Key: ");
            for (unsigned int i = 0; i < MESHCORE_PUB_KEY_SIZE; i++) {
                printf("%02X", advert.pub_key[i]);
            }
            printf("\n");
            printf("Timestamp: %" PRIu32 "\n", advert.timestamp);
            printf("Signature: ");
            for (unsigned int i = 0; i < MESHCORE_SIGNATURE_SIZE; i++) {
                printf("%02X", advert.signature[i]);
            }
            printf("\n");
            printf("Role: %s\n", role_to_string(advert.role));
            if (advert.position_valid) {
                printf("Position: lat=%" PRIi32 ", lon=%" PRIi32 "\n", advert.position_lat, advert.position_lon);
            } else {
                printf("Position: (not available)\n");
            }
            if (advert.extra1_valid) {
                printf("Extra1: %u\n", advert.extra1);
            } else {
                printf("Extra1: (not available)\n");
            }
            if (advert.extra2_valid) {
                printf("Extra2: %u\n", advert.extra2);
            } else {
                printf("Extra2: (not available)\n");
            }
            if (advert.name_valid) {
                printf("Name: %s\n", advert.name);
            } else {
                printf("Name: (not available)\n");
            }
        } else {
            printf("Failed to decode node advertisement payload.\n");
        }
    } else if (message.type == MESHCORE_PAYLOAD_TYPE_GRP_TXT) {
        meshcore_grp_txt_t grp_txt;
        if (meshcore_grp_txt_deserialize(message.payload, message.payload_length, &grp_txt) >= 0) {
            printf("Decoded group text message:\n");
            printf("Channel Hash: %02X\n", grp_txt.channel_hash);
            printf("Data Length: %d\n", grp_txt.data_length);
            printf("Received MAC:");

            for (unsigned int i = 0; i < MESHCORE_CIPHER_MAC_SIZE; i++) {
                printf("%02X", grp_txt.mac[i]);
            }
            printf("\n");

            printf("Data [%d]: ", grp_txt.data_length);
            for (unsigned int i = 0; i < grp_txt.data_length; i++) {
                printf("%02X", grp_txt.data[i]);
            }
            printf("\n");

            // TO-DO: all of this MAC verification and decryption should be moved somewhere else

            for (uint8_t i = 0; i < 2; i++) {
                uint8_t* key = mc_keys[i];

                uint8_t out[128];
                size_t out_len = hmac_sha256(key, 16, grp_txt.data, grp_txt.data_length, out, MESHCORE_CIPHER_MAC_SIZE);

                printf("Calculated MAC [%d]: ", out_len);
                for (unsigned int i = 0; i < out_len; i++) {
                    printf("%02X", out[i]);
                }
                printf("\n");

                if (memcmp(out, grp_txt.mac, MESHCORE_CIPHER_MAC_SIZE) == 0) {
                    printf("MAC verification: SUCCESS\n");

                    // Copy encrypted data to buffer for decryption, AES works in-place
                    grp_txt.decrypted.data_length = grp_txt.data_length;
                    memcpy(grp_txt.decrypted.data, grp_txt.data, grp_txt.data_length);

                    struct AES_ctx ctx;
                    AES_init_ctx(&ctx, key);
                    for (uint8_t i = 0; i < (grp_txt.decrypted.data_length / 16); i++) {
                        AES_ECB_decrypt(&ctx, &grp_txt.decrypted.data[i * 16]);
                    }

                    printf("Data [%d]: ", grp_txt.decrypted.data_length);
                    for (unsigned int i = 0; i < grp_txt.decrypted.data_length; i++) {
                        printf("%02X", grp_txt.decrypted.data[i]);
                    }
                    printf("\n");

                    uint8_t position = 0;
                    memcpy(&grp_txt.decrypted.timestamp, grp_txt.decrypted.data, sizeof(uint32_t));
                    position                            += sizeof(uint32_t);
                    grp_txt.decrypted.text_type          = grp_txt.decrypted.data[position];
                    position                            += sizeof(uint8_t);
                    size_t text_length                   = grp_txt.decrypted.data_length - position;
                    grp_txt.decrypted.text               = (char*)&grp_txt.decrypted.data[position];
                    grp_txt.decrypted.text[text_length]  = '\0';

                    printf("Timestamp: %" PRIu32 "\n", grp_txt.decrypted.timestamp);
                    printf("Text Type: %u\n", grp_txt.decrypted.text_type);
                    printf("Message: '%s'\n", grp_txt.decrypted.text);

                    char*  ptr      = grp_txt.decrypted.text;
                    char*  text_ptr = grp_txt.decrypted.text;
                    size_t len      = strlen(ptr);

                    for (size_t i = 0; i < len; i++) {
                        ptr = &grp_txt.decrypted.text[i];
                        if (*ptr == ':') {
                            *ptr = '\0';
                            if (i + 2 < len) {
                                text_ptr = ptr + 2;
                            }
                            break;
                        }
                    }

                    chat_message_t chat_message = {0};
                    snprintf(chat_message.name, CHAT_MESSAGE_NAME_SIZE, "%s", grp_txt.decrypted.text);
                    snprintf(chat_message.text, CHAT_MESSAGE_TEXT_SIZE, "%s", text_ptr);
                    chat_message.timestamp = grp_txt.decrypted.timestamp;
                    chat_message_store("/sd/messages.dat", &chat_message);
                    break;
                } else {
                    printf("MAC verification: FAILURE (key %u)\n", i);
                }
            }
        } else {
            printf("Failed to decode group text message payload.\n");
        }
    }
}

//////

esp_err_t lora_transaction_receive(uint8_t* packet, size_t length) {
    if (length > sizeof(lora_packet_buffer) || length < sizeof(lora_protocol_header_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    lora_protocol_header_t* header = (lora_protocol_header_t*)packet;
    if (header->type == LORA_PROTOCOL_TYPE_PACKET_RX) {
        // to-do add to queue
        printf("Received LoRa packet\r\n");
        for (size_t i = sizeof(lora_protocol_header_t); i < length - sizeof(lora_protocol_header_t); i++) {
            printf("%02X ", packet[i]);
        }
        printf("\r\n");
        tanmatsu_coprocessor_handle_t handle;
        bsp_tanmatsu_coprocessor_get_handle(&handle);
        meshcore_parse(&packet[sizeof(lora_protocol_header_t)], length - sizeof(lora_protocol_header_t));
        tanmatsu_coprocessor_set_message(handle, true, false, false, true, false, false, false, false);
        vTaskDelay(pdMS_TO_TICKS(500));
        tanmatsu_coprocessor_set_message(handle, false, false, false, false, false, false, false, false);
        return ESP_OK;
    }
    // Else it's a response to a transaction
    memcpy(lora_packet_buffer, packet, length);
    lora_packet_size = length;
    xSemaphoreGive(lora_transaction_semaphore);
    return ESP_OK;
}

esp_err_t lora_init(QueueHandle_t packet_queue) {
    lora_packet_queue          = packet_queue;
    lora_mutex                 = xSemaphoreCreateMutex();
    lora_transaction_semaphore = xSemaphoreCreateBinary();
    if (lora_mutex == NULL || lora_transaction_semaphore == NULL) {
        if (lora_mutex != NULL) {
            vSemaphoreDelete(lora_mutex);
        }
        if (lora_transaction_semaphore != NULL) {
            vSemaphoreDelete(lora_transaction_semaphore);
        }
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t lora_get_mode(lora_protocol_mode_t* out_mode) {
    if (out_mode == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lora_protocol_header_t request = {
        .sequence_number = lora_sequence_number,
        .type            = LORA_PROTOCOL_TYPE_GET_MODE,
    };
    uint8_t   response[sizeof(lora_protocol_header_t) + sizeof(lora_protocol_mode_params_t)] = {0};
    size_t    response_length                                                                = 0;
    esp_err_t result =
        lora_transaction((uint8_t*)&request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < sizeof(lora_protocol_header_t) + sizeof(lora_protocol_mode_params_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    lora_protocol_header_t* header = (lora_protocol_header_t*)response;
    if (header->sequence_number != request.sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", header->sequence_number);
        return ESP_FAIL;
    }
    if (header->type != LORA_PROTOCOL_TYPE_GET_MODE) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", header->type);
        return ESP_FAIL;
    }
    lora_protocol_mode_params_t* params = (lora_protocol_mode_params_t*)(response + sizeof(lora_protocol_header_t));
    *out_mode                           = params->mode;
    return ESP_OK;
}

esp_err_t lora_set_mode(const lora_protocol_mode_t mode) {
    size_t                  request_length = sizeof(lora_protocol_header_t) + sizeof(lora_protocol_mode_params_t);
    uint8_t                 request[request_length];
    lora_protocol_header_t* header      = (lora_protocol_header_t*)request;
    header->sequence_number             = lora_sequence_number;
    header->type                        = LORA_PROTOCOL_TYPE_SET_MODE;
    lora_protocol_mode_params_t* params = (lora_protocol_mode_params_t*)(request + sizeof(lora_protocol_header_t));
    params->mode                        = mode;
    uint8_t   response[sizeof(lora_protocol_header_t)] = {0};
    size_t    response_length                          = 0;
    esp_err_t result = lora_transaction(request, request_length, response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < sizeof(lora_protocol_header_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    lora_protocol_header_t* response_header = (lora_protocol_header_t*)response;
    if (response_header->sequence_number != header->sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", response_header->sequence_number);
        return ESP_FAIL;
    }
    if (response_header->type != LORA_PROTOCOL_TYPE_ACK) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", response_header->type);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t lora_get_config(lora_protocol_config_params_t* out_config) {
    if (out_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lora_protocol_header_t request = {
        .sequence_number = lora_sequence_number,
        .type            = LORA_PROTOCOL_TYPE_GET_CONFIG,
    };
    uint8_t   response[sizeof(lora_protocol_header_t) + sizeof(lora_protocol_config_params_t)] = {0};
    size_t    response_length                                                                  = 0;
    esp_err_t result =
        lora_transaction((uint8_t*)&request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < sizeof(lora_protocol_header_t) + sizeof(lora_protocol_config_params_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    lora_protocol_header_t* header = (lora_protocol_header_t*)response;
    if (header->sequence_number != request.sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", header->sequence_number);
        return ESP_FAIL;
    }
    if (header->type != LORA_PROTOCOL_TYPE_GET_CONFIG) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", header->type);
        return ESP_FAIL;
    }
    lora_protocol_config_params_t* params = (lora_protocol_config_params_t*)(response + sizeof(lora_protocol_header_t));
    memcpy(out_config, params, sizeof(lora_protocol_config_params_t));
    return ESP_OK;
}

esp_err_t lora_set_config(const lora_protocol_config_params_t* config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t                  request_length = sizeof(lora_protocol_header_t) + sizeof(lora_protocol_config_params_t);
    uint8_t                 request[request_length];
    lora_protocol_header_t* header        = (lora_protocol_header_t*)request;
    header->sequence_number               = lora_sequence_number;
    header->type                          = LORA_PROTOCOL_TYPE_SET_CONFIG;
    lora_protocol_config_params_t* params = (lora_protocol_config_params_t*)(request + sizeof(lora_protocol_header_t));
    memcpy(params, config, sizeof(lora_protocol_config_params_t));
    uint8_t   response[sizeof(lora_protocol_header_t)] = {0};
    size_t    response_length                          = 0;
    esp_err_t result = lora_transaction(request, request_length, response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < sizeof(lora_protocol_header_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    lora_protocol_header_t* response_header = (lora_protocol_header_t*)response;
    if (response_header->sequence_number != header->sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", response_header->sequence_number);
        return ESP_FAIL;
    }
    if (response_header->type != LORA_PROTOCOL_TYPE_ACK) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", response_header->type);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t lora_get_status(lora_protocol_status_params_t* out_status) {
    if (out_status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lora_protocol_header_t request = {
        .sequence_number = lora_sequence_number,
        .type            = LORA_PROTOCOL_TYPE_GET_STATUS,
    };
    uint8_t   response[sizeof(lora_protocol_header_t) + sizeof(lora_protocol_status_params_t)] = {0};
    size_t    response_length                                                                  = 0;
    esp_err_t result =
        lora_transaction((uint8_t*)&request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < sizeof(lora_protocol_header_t) + sizeof(lora_protocol_status_params_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    lora_protocol_header_t* header = (lora_protocol_header_t*)response;
    if (header->sequence_number != request.sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", header->sequence_number);
        return ESP_FAIL;
    }
    if (header->type != LORA_PROTOCOL_TYPE_GET_STATUS) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", header->type);
        return ESP_FAIL;
    }
    lora_protocol_status_params_t* params = (lora_protocol_status_params_t*)(response + sizeof(lora_protocol_header_t));
    memcpy(out_status, params, sizeof(lora_protocol_status_params_t));
    return ESP_OK;
}

esp_err_t lora_send_packet(const lora_protocol_lora_packet_t* packet) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t lora_receive_packet(uint8_t* out_buffer, uint8_t* out_length, uint16_t max_length) {
    return ESP_ERR_NOT_SUPPORTED;
}
