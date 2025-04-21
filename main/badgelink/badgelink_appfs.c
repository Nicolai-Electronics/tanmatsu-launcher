
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include "badgelink_appfs.h"
#include "appfs.h"
#include "esp_crc.h"
#include "esp_log.h"

static char const TAG[] = "badgelink_appfs";

// AppFS FD used for file transfer.
static appfs_handle_t xfer_fd;
// CRC32 of file transferred.
static uint32_t       xfer_crc32;

// Calculate the CRC32 of an AppFS file.
static uint32_t calc_app_crc32(appfs_handle_t fd) {
    uint8_t  buffer[128];
    int      size;
    uint32_t crc = 0;
    appfsEntryInfo(fd, NULL, &size);
    for (int i = 0; i < size; i += sizeof(buffer)) {
        int max = sizeof(buffer);
        if (max > size - i) {
            max = size - i;
        }
        appfsRead(fd, i, buffer, max);
        crc = esp_crc32_le(crc, buffer, max);
    }
    return crc;
}

// Handle an AppFS request packet.
void badgelink_appfs_handle() {
    switch (badgelink_packet.packet.request.req.appfs_action.type) {
        case badgelink_FsActionType_FsActionList:
            badgelink_appfs_list();
            break;
        case badgelink_FsActionType_FsActionDelete:
            badgelink_appfs_delete();
            break;
        case badgelink_FsActionType_FsActionUpload:
            badgelink_appfs_upload();
            break;
        case badgelink_FsActionType_FsActionDownload:
            badgelink_appfs_download();
            break;
        case badgelink_FsActionType_FsActionStat:
            badgelink_appfs_stat();
            break;
        case badgelink_FsActionType_FsActionCrc23:
            badgelink_appfs_crc32();
            break;
        case badgelink_FsActionType_FsActionGetUsage:
            badgelink_appfs_usage();
            break;
        default:
            badgelink_status_unsupported();
            break;
    }
}

// Handle an AppFS upload (host->badge) transfer.
void badgelink_appfs_xfer_upload() {
    badgelink_Chunk* chunk = &badgelink_packet.packet.request.req.upload_chunk;
    esp_err_t        ec    = appfsWrite(xfer_fd, badgelink_xfer_pos, chunk->data.bytes, chunk->data.size);
    if (ec) {
        ESP_LOGE(TAG, "%s: error %s", __FUNCTION__, esp_err_to_name(ec));
        badgelink_status_int_err();
    } else {
        badgelink_status_ok();
    }
}

// Handle an AppFS download (badge->host) transfer.
void badgelink_appfs_xfer_download() {
    badgelink_packet.which_packet                = badgelink_Packet_response_tag;
    badgelink_packet.packet.response.status_code = badgelink_StatusCode_StatusOk;
    badgelink_packet.packet.response.which_resp  = badgelink_Response_download_chunk_tag;
    badgelink_Chunk* chunk                       = &badgelink_packet.packet.response.resp.download_chunk;

    chunk->position  = badgelink_xfer_pos;
    chunk->data.size = sizeof(chunk->data.bytes) < badgelink_xfer_size - badgelink_xfer_pos
                           ? sizeof(chunk->data.bytes)
                           : badgelink_xfer_size - badgelink_xfer_pos;
    esp_err_t ec     = appfsRead(xfer_fd, badgelink_xfer_pos, chunk->data.bytes, chunk->data.size);

    if (ec) {
        ESP_LOGE(TAG, "%s: error %s", __FUNCTION__, esp_err_to_name(ec));
        badgelink_status_int_err();
    } else {
        badgelink_send_packet();
    }
}

// Finish an AppFS transfer.
void badgelink_appfs_xfer_stop(bool abnormal) {
    if (badgelink_xfer_is_upload) {
        if (abnormal) {
            ESP_LOGE(TAG, "AppFS upload aborted");

            // AppFS can delete by fd so this is the workaround.
            char const* name_ptr;
            appfsEntryInfo(xfer_fd, &name_ptr, NULL);
            char name[64];
            strlcpy(name, name_ptr, sizeof(name));
            appfsDeleteFile(name);

        } else {
            uint32_t actual_crc32 = calc_app_crc32(xfer_fd);
            if (actual_crc32 != xfer_crc32) {
                ESP_LOGE(TAG, "AppFS upload CRC32 mismatch; expected %08" PRIx32 ", actual %08" PRIx32, xfer_crc32,
                         actual_crc32);
                badgelink_status_int_err();

                // AppFS can delete by fd so this is the workaround.
                char const* name_ptr;
                appfsEntryInfo(xfer_fd, &name_ptr, NULL);
                char name[64];
                strlcpy(name, name_ptr, sizeof(name));
                appfsDeleteFile(name);
            } else {
                ESP_LOGI(TAG, "AppFS upload finished");
                badgelink_status_ok();
            }
        }
    } else {
        if (abnormal) {
            ESP_LOGE(TAG, "AppFS download aborted");
        } else {
            ESP_LOGI(TAG, "AppFS download finished");
        }
    }
}

// Handle an AppFS list request.
void badgelink_appfs_list() {
    // Validate request.
    // Note: AppFS fds can be 0 so we don't check its presence here.
    badgelink_AppfsActionReq* req = &badgelink_packet.packet.request.req.appfs_action;
    if (req->crc32 || req->which_id != 0) {
        badgelink_status_malformed();
        return;
    }

    int skip = req->list_offset;

    // Format response.
    badgelink_packet.which_packet                = badgelink_Packet_response_tag;
    badgelink_packet.packet.response.status_code = badgelink_StatusCode_StatusOk;
    badgelink_packet.packet.response.which_resp  = badgelink_Response_appfs_resp_tag;
    badgelink_AppfsActionResp* resp              = &badgelink_packet.packet.response.resp.appfs_resp;
    resp->which_val                              = badgelink_AppfsActionResp_list_tag;
    resp->val.list.list_count                    = 0;
    resp->val.list.total_size                    = 0;

    // Read all handles in AppFS.
    appfs_handle_t handle  = appfsNextEntry(APPFS_INVALID_FD);
    int            index   = 0;
    size_t const   max_len = sizeof(resp->val.list.list) / sizeof(badgelink_AppfsMetadata);
    while (handle != APPFS_INVALID_FD) {
        resp->val.list.total_size++;
        if (skip) {
            // Skip adding this one.
            skip--;
        } else if (resp->val.list.list_count < max_len) {
            // Get metadata.
            char const* slug;
            char const* title;
            uint16_t    version;
            int         size;
            appfsEntryInfoExt(handle, &slug, &title, &version, &size);

            // Add metadata to the list.
            badgelink_AppfsMetadata* metadata = &resp->val.list.list[index];
            strlcpy(metadata->slug, slug, sizeof(metadata->slug));
            strlcpy(metadata->title, title, sizeof(metadata->title));
            metadata->version = version;
            metadata->size    = size;

            index++;
            resp->val.list.list_count = index;
        }

        // Go to next entry.
        handle = appfsNextEntry(handle);
    }

    // Send response.
    badgelink_send_packet();
}

// Handle an AppFS delete request.
void badgelink_appfs_delete() {
    // Validate request.
    badgelink_AppfsActionReq* req = &badgelink_packet.packet.request.req.appfs_action;
    if (req->crc32 || req->which_id != badgelink_AppfsActionReq_slug_tag) {
        badgelink_status_malformed();
        return;
    }

    if (!appfsExists(req->id.slug)) {
        badgelink_status_not_found();
        return;
    }
    esp_err_t ec = appfsDeleteFile(req->id.slug);
    if (ec) {
        ESP_LOGE(TAG, "%s: error %s", __FUNCTION__, esp_err_to_name(ec));
        badgelink_status_int_err();
    } else {
        badgelink_status_ok();
    }
}

// Handle an AppFS upload request.
void badgelink_appfs_upload() {
    // Validate request.
    badgelink_AppfsActionReq* req = &badgelink_packet.packet.request.req.appfs_action;
    if (req->which_id != badgelink_AppfsActionReq_metadata_tag) {
        badgelink_status_malformed();
        return;
    } else if (badgelink_xfer_type != BADGELINK_XFER_NONE) {
        badgelink_status_ill_state();
        return;
    }

    // Try to create the new file.
    esp_err_t ec = appfsCreateFileExt(req->id.metadata.slug, req->id.metadata.title, req->id.metadata.version,
                                      req->id.metadata.size, &xfer_fd);
    if (ec == ESP_ERR_NO_MEM) {
        ESP_LOGE(TAG, "Out of space for uploading");
        badgelink_status_no_space();
        return;
    } else if (ec) {
        ESP_LOGE(TAG, "%s: error %s", __FUNCTION__, esp_err_to_name(ec));
        badgelink_status_int_err();
        return;
    }

    // Erase the entire file explicitly because AppFS doesn't do that for some reason.
    uint32_t rounded_size = req->id.metadata.size;
    if (rounded_size % SPI_FLASH_MMU_PAGE_SIZE) {
        rounded_size += SPI_FLASH_MMU_PAGE_SIZE - rounded_size % SPI_FLASH_MMU_PAGE_SIZE;
    }
    appfsErase(xfer_fd, 0, rounded_size);

    // File created successfully, initiate transfer.
    badgelink_xfer_type      = BADGELINK_XFER_APPFS;
    badgelink_xfer_is_upload = true;
    badgelink_xfer_pos       = 0;
    badgelink_xfer_size      = req->id.metadata.size;
    xfer_crc32               = req->crc32;

    // This OK response officially starts the transfer.
    ESP_LOGI(TAG, "AppFS upload started");
    badgelink_status_ok();
}

// Handle an AppFS download request.
void badgelink_appfs_download() {
    // Validate request.
    badgelink_AppfsActionReq* req = &badgelink_packet.packet.request.req.appfs_action;
    if (req->crc32 || req->which_id != badgelink_AppfsActionReq_slug_tag) {
        badgelink_status_malformed();
        return;
    } else if (badgelink_xfer_type != BADGELINK_XFER_NONE) {
        badgelink_status_ill_state();
        return;
    }

    // Find the file.
    appfs_handle_t fd = appfsOpen(req->id.slug);
    if (fd == APPFS_INVALID_FD) {
        badgelink_status_not_found();
        return;
    }

    // Calculate file CRC32 and get size.
    uint32_t crc = calc_app_crc32(fd);
    int      size;
    appfsEntryInfo(fd, NULL, &size);

    // Set up transfer.
    badgelink_xfer_type      = BADGELINK_XFER_APPFS;
    badgelink_xfer_is_upload = false;
    badgelink_xfer_pos       = 0;
    badgelink_xfer_size      = size;
    xfer_fd                  = fd;

    // Format response.
    badgelink_packet.which_packet                = badgelink_Packet_response_tag;
    badgelink_packet.packet.response.status_code = badgelink_StatusCode_StatusOk;
    badgelink_packet.packet.response.which_resp  = badgelink_Response_appfs_resp_tag;
    badgelink_AppfsActionResp* resp              = &badgelink_packet.packet.response.resp.appfs_resp;
    resp->which_val                              = badgelink_AppfsActionResp_crc32_tag;
    resp->size                                   = size;
    resp->val.crc32                              = crc;

    // Send response.
    badgelink_send_packet();
}

// Handle an AppFS stat request.
void badgelink_appfs_stat() {
    // Validate request.
    badgelink_AppfsActionReq* req = &badgelink_packet.packet.request.req.appfs_action;
    if (req->crc32 || req->which_id != badgelink_AppfsActionReq_slug_tag) {
        badgelink_status_malformed();
        return;
    }

    // Find the file.
    appfs_handle_t fd = appfsOpen(req->id.slug);
    if (fd == APPFS_INVALID_FD) {
        badgelink_status_not_found();
        return;
    }

    // Get AppFS metadata.
    char const* name;
    char const* title;
    uint16_t    version;
    int         size;
    appfsEntryInfoExt(fd, &name, &title, &version, &size);

    // Format response.
    badgelink_packet.which_packet                = badgelink_Packet_response_tag;
    badgelink_packet.packet.response.status_code = badgelink_StatusCode_StatusOk;
    badgelink_packet.packet.response.which_resp  = badgelink_Response_appfs_resp_tag;
    badgelink_AppfsActionResp* resp              = &badgelink_packet.packet.response.resp.appfs_resp;
    resp->which_val                              = badgelink_AppfsActionResp_metadata_tag;
    strlcpy(resp->val.metadata.slug, name, sizeof(resp->val.metadata.slug));
    strlcpy(resp->val.metadata.title, title, sizeof(resp->val.metadata.title));
    resp->val.metadata.version = version;
    resp->val.metadata.size    = size;

    // Send response.
    badgelink_send_packet();
}

// Handle an AppFS crc32 request.
void badgelink_appfs_crc32() {
    // Validate request.
    badgelink_AppfsActionReq* req = &badgelink_packet.packet.request.req.appfs_action;
    if (req->crc32 || req->which_id != badgelink_AppfsActionReq_slug_tag) {
        badgelink_status_malformed();
        return;
    }

    // Find the file.
    appfs_handle_t fd = appfsOpen(req->id.slug);
    if (fd == APPFS_INVALID_FD) {
        badgelink_status_not_found();
        return;
    }

    // Calculate file CRC32.
    uint32_t crc = calc_app_crc32(fd);

    // Format response.
    badgelink_packet.which_packet                = badgelink_Packet_response_tag;
    badgelink_packet.packet.response.status_code = badgelink_StatusCode_StatusOk;
    badgelink_packet.packet.response.which_resp  = badgelink_Response_appfs_resp_tag;
    badgelink_AppfsActionResp* resp              = &badgelink_packet.packet.response.resp.appfs_resp;
    resp->which_val                              = badgelink_AppfsActionResp_crc32_tag;
    resp->val.crc32                              = crc;

    // Send response.
    badgelink_send_packet();
}

// Handle an AppFS usage statistics request.
void badgelink_appfs_usage() {
    // Validate request.
    badgelink_AppfsActionReq* req = &badgelink_packet.packet.request.req.appfs_action;
    if (req->crc32 || req->which_id != 0) {
        badgelink_status_malformed();
        return;
    }

    // Format response.
    badgelink_packet.which_packet                = badgelink_Packet_response_tag;
    badgelink_packet.packet.response.which_resp  = badgelink_Response_appfs_resp_tag;
    badgelink_packet.packet.response.status_code = badgelink_StatusCode_StatusOk;
    badgelink_AppfsActionResp* resp              = &badgelink_packet.packet.response.resp.appfs_resp;
    resp->which_val                              = badgelink_AppfsActionResp_usage_tag;
    resp->val.usage.size                         = appfsGetTotalMem();
    resp->val.usage.used                         = resp->val.usage.size - appfsGetFreeMem();

    badgelink_send_packet();
}
