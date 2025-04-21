
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include "badgelink_nvs.h"
#include "esp_log.h"
#include "nvs.h"

static char const TAG[] = "badgelink_nvs";

// Handle an NVS request packet.
void badgelink_nvs_handle() {
    switch (badgelink_packet.packet.request.req.nvs_action.type) {
        case badgelink_NvsActionType_NvsActionList:
            badgelink_nvs_list();
            break;
        case badgelink_NvsActionType_NvsActionRead:
            badgelink_nvs_read();
            break;
        case badgelink_NvsActionType_NvsActionWrite:
            badgelink_nvs_write();
            break;
        case badgelink_NvsActionType_NvsActionDelete:
            badgelink_nvs_delete();
            break;
        default:
            badgelink_status_unsupported();
            break;
    }
}

// Handle an NVS list request.
void badgelink_nvs_list() {
    // Validate request.
    badgelink_NvsActionReq* req = &badgelink_packet.packet.request.req.nvs_action;
    if (req->has_wdata || req->key[0]) {
        badgelink_status_malformed();
        return;
    }

    // Try to open NVS.
    uint32_t       offset = req->list_offset;
    nvs_iterator_t iter;
    esp_err_t ec = nvs_entry_find(NVS_DEFAULT_PART_NAME, req->namespc[0] ? req->namespc : NULL, NVS_TYPE_ANY, &iter);
    if (ec == ESP_ERR_NVS_NOT_FOUND) {
        badgelink_status_ok();
        return;
    } else if (ec != ESP_OK) {
        ESP_LOGE(TAG, "nvs_entry_find error: %s", esp_err_to_name(ec));
        badgelink_status_int_err();
        return;
    }

    // Format response.
    badgelink_packet.which_packet                            = badgelink_Packet_response_tag;
    badgelink_packet.packet.response.status_code             = badgelink_StatusCode_StatusOk;
    badgelink_packet.packet.response.which_resp              = badgelink_Response_nvs_resp_tag;
    badgelink_packet.packet.response.resp.nvs_resp.which_val = badgelink_NvsActionResp_entries_tag;
    badgelink_NvsEntriesList* entries     = &badgelink_packet.packet.response.resp.nvs_resp.val.entries;
    size_t const              max_entries = sizeof(entries->entries) / sizeof(badgelink_NvsEntry);
    entries->entries_count                = 0;

    // Skip until the desired offset.
    while (offset) {
        ec = nvs_entry_next(&iter);
        if (ec == ESP_ERR_NVS_NOT_FOUND) {
            break;
        } else if (ec != ESP_OK) {
            ESP_LOGE(TAG, "nvs_entry_next error: %s", esp_err_to_name(ec));
            nvs_release_iterator(iter);
            badgelink_status_int_err();
            return;
        }
        entries->entries_count++;
        offset--;
    }

    // Read entries from NVS.
    while (1) {
        if (entries->entries_count < max_entries) {
            nvs_entry_info_t info;
            ec = nvs_entry_info(iter, &info);
            if (ec != ESP_OK) {
                ESP_LOGE(TAG, "nvs_entry_info error: %s", esp_err_to_name(ec));
                nvs_release_iterator(iter);
                badgelink_status_int_err();
                return;
            }

            // Translate NVS entry type.
            badgelink_NvsValueType type;
            switch (info.type) {
                case NVS_TYPE_U8:
                    type = badgelink_NvsValueType_NvsValueUint8;
                    break;
                case NVS_TYPE_I8:
                    type = badgelink_NvsValueType_NvsValueInt8;
                    break;
                case NVS_TYPE_U16:
                    type = badgelink_NvsValueType_NvsValueUint16;
                    break;
                case NVS_TYPE_I16:
                    type = badgelink_NvsValueType_NvsValueInt16;
                    break;
                case NVS_TYPE_U32:
                    type = badgelink_NvsValueType_NvsValueUint32;
                    break;
                case NVS_TYPE_I32:
                    type = badgelink_NvsValueType_NvsValueInt32;
                    break;
                case NVS_TYPE_U64:
                    type = badgelink_NvsValueType_NvsValueUint64;
                    break;
                case NVS_TYPE_I64:
                    type = badgelink_NvsValueType_NvsValueInt64;
                    break;
                case NVS_TYPE_STR:
                    type = badgelink_NvsValueType_NvsValueString;
                    break;
                case NVS_TYPE_BLOB:
                    type = badgelink_NvsValueType_NvsValueBlob;
                    break;
                default:
                    nvs_release_iterator(iter);
                    badgelink_status_int_err();
                    return;
            }

            // Copy the other fields.
            entries->entries[entries->entries_count].type = type;
            memcpy(entries->entries[entries->entries_count].namespc, info.namespace_name, NVS_NS_NAME_MAX_SIZE);
            memcpy(entries->entries[entries->entries_count].key, info.key, NVS_KEY_NAME_MAX_SIZE);
        }

        // Count total entries.
        entries->entries_count++;

        ec = nvs_entry_next(&iter);
        if (ec == ESP_ERR_NVS_NOT_FOUND) {
            break;
        } else if (ec != ESP_OK) {
            ESP_LOGE(TAG, "nvs_entry_next error: %s", esp_err_to_name(ec));
            nvs_release_iterator(iter);
            badgelink_status_int_err();
            return;
        }
    }

    // Send the response.
    badgelink_send_packet();
}

// Handle an NVS read request.
void badgelink_nvs_read() {
    // Validate request.
    badgelink_NvsActionReq* req = &badgelink_packet.packet.request.req.nvs_action;
    if (req->has_wdata || !req->key[0] || !req->namespc[0]) {
        badgelink_status_malformed();
    }

    // Try to open NVS.
    nvs_handle_t handle;
    esp_err_t    ec = nvs_open(req->namespc, NVS_READONLY, &handle);
    if (ec == ESP_ERR_NVS_NOT_FOUND) {
        badgelink_status_not_found();
        return;
    } else if (ec != ESP_OK) {
        ESP_LOGE(TAG, "Read error: %s", esp_err_to_name(ec));
        badgelink_status_int_err();
        return;
    }

    // Copy key out of request.
    badgelink_NvsValueType nvs_type                   = req->read_type;
    char                   key[NVS_KEY_NAME_MAX_SIZE] = {0};
    strlcpy(key, req->key, sizeof(key));

    // Format response.
    badgelink_packet.which_packet                            = badgelink_Packet_response_tag;
    badgelink_packet.packet.response.status_code             = badgelink_StatusCode_StatusOk;
    badgelink_packet.packet.response.which_resp              = badgelink_Response_nvs_resp_tag;
    badgelink_packet.packet.response.resp.nvs_resp.which_val = badgelink_NvsActionResp_rdata_tag;

    // Try to read from NVS.
    badgelink_NvsValue* rdata = &badgelink_packet.packet.response.resp.nvs_resp.val.rdata;
    rdata->type               = nvs_type;
    switch (nvs_type) {
        case badgelink_NvsValueType_NvsValueUint8: {
            uint8_t tmp;
            ec                    = nvs_get_u8(handle, key, &tmp);
            rdata->which_val      = badgelink_NvsValue_numericval_tag;
            rdata->val.numericval = tmp;
        } break;
        case badgelink_NvsValueType_NvsValueInt8: {
            int8_t tmp;
            ec                    = nvs_get_i8(handle, key, &tmp);
            rdata->which_val      = badgelink_NvsValue_numericval_tag;
            rdata->val.numericval = tmp;
        } break;
        case badgelink_NvsValueType_NvsValueUint16: {
            uint16_t tmp;
            ec                    = nvs_get_u16(handle, key, &tmp);
            rdata->which_val      = badgelink_NvsValue_numericval_tag;
            rdata->val.numericval = tmp;
        } break;
        case badgelink_NvsValueType_NvsValueInt16: {
            int16_t tmp;
            ec                    = nvs_get_i16(handle, key, &tmp);
            rdata->which_val      = badgelink_NvsValue_numericval_tag;
            rdata->val.numericval = tmp;
        } break;
        case badgelink_NvsValueType_NvsValueUint32: {
            uint32_t tmp;
            ec                    = nvs_get_u32(handle, key, &tmp);
            rdata->which_val      = badgelink_NvsValue_numericval_tag;
            rdata->val.numericval = tmp;
        } break;
        case badgelink_NvsValueType_NvsValueInt32: {
            int32_t tmp;
            ec                    = nvs_get_i32(handle, key, &tmp);
            rdata->which_val      = badgelink_NvsValue_numericval_tag;
            rdata->val.numericval = tmp;
        } break;
        case badgelink_NvsValueType_NvsValueUint64: {
            uint64_t tmp;
            ec                    = nvs_get_u64(handle, key, &tmp);
            rdata->which_val      = badgelink_NvsValue_numericval_tag;
            rdata->val.numericval = tmp;
        } break;
        case badgelink_NvsValueType_NvsValueInt64: {
            int64_t tmp;
            ec                    = nvs_get_i64(handle, key, &tmp);
            rdata->which_val      = badgelink_NvsValue_numericval_tag;
            rdata->val.numericval = tmp;
        } break;
        case badgelink_NvsValueType_NvsValueString: {
            rdata->which_val = badgelink_NvsValue_stringval_tag;
            size_t len       = sizeof(rdata->val.stringval) - 1;
            ec               = nvs_get_str(handle, key, rdata->val.stringval, &len);
            if (ec == ESP_OK) {
                if (len <= sizeof(rdata->val.stringval) - 1) {
                    rdata->val.stringval[len] = 0;
                } else {
                    ec = ESP_ERR_NO_MEM;
                }
            }
        } break;
        case badgelink_NvsValueType_NvsValueBlob: {
            rdata->which_val = badgelink_NvsValue_blobval_tag;
            size_t len       = sizeof(rdata->val.blobval.bytes);
            ec               = nvs_get_blob(handle, key, rdata->val.blobval.bytes, &len);
            if (ec == ESP_OK) {
                if (len <= sizeof(rdata->val.blobval.bytes)) {
                    rdata->val.blobval.size = len;
                } else {
                    ec = ESP_ERR_NO_MEM;
                }
            }
        } break;
        default:
            ec = ESP_FAIL;
            break;
    }
    nvs_close(handle);

    // Send response packet.
    if (ec == ESP_OK) {
        badgelink_send_packet();
    } else if (ec == ESP_ERR_NVS_NOT_FOUND) {
        badgelink_status_not_found();
    } else {
        ESP_LOGE(TAG, "Read error: %s", esp_err_to_name(ec));
        badgelink_status_int_err();
    }
}

// Handle an NVS write request.
void badgelink_nvs_write() {
    // Validate request.
    badgelink_NvsActionReq* req = &badgelink_packet.packet.request.req.nvs_action;
    if (!req->has_wdata || !req->key[0] || !req->namespc[0]) {
        ESP_LOGE(TAG, "Malformed NVS write: missing wdata, key or namespc");
        badgelink_status_malformed();
        return;
    }

    // Validate write data type.
    switch (req->wdata.type) {
        case badgelink_NvsValueType_NvsValueUint8:
        case badgelink_NvsValueType_NvsValueInt8:
        case badgelink_NvsValueType_NvsValueUint16:
        case badgelink_NvsValueType_NvsValueInt16:
        case badgelink_NvsValueType_NvsValueUint32:
        case badgelink_NvsValueType_NvsValueInt32:
        case badgelink_NvsValueType_NvsValueUint64:
        case badgelink_NvsValueType_NvsValueInt64:
            if (req->wdata.which_val != badgelink_NvsValue_numericval_tag) {
                ESP_LOGE(TAG, "Malformed NVS write: Expected numericval");
                badgelink_status_malformed();
                return;
            }
            break;
        case badgelink_NvsValueType_NvsValueString:
            if (req->wdata.which_val != badgelink_NvsValue_stringval_tag) {
                ESP_LOGE(TAG, "Malformed NVS write: Expected stringval");
                badgelink_status_malformed();
                return;
            }
            break;
        case badgelink_NvsValueType_NvsValueBlob:
            if (req->wdata.which_val != badgelink_NvsValue_blobval_tag) {
                ESP_LOGE(TAG, "Malformed NVS write: Expected blobval");
                badgelink_status_malformed();
                return;
            }
            break;
        default:
            ESP_LOGE(TAG, "Malformed NVS write: Invalid type");
            badgelink_status_malformed();
            return;
    }

    // Try to open NVS.
    nvs_handle_t handle;
    esp_err_t    ec = nvs_open(req->namespc, NVS_READWRITE, &handle);
    if (ec != ESP_OK) {
        ESP_LOGE(TAG, "Write error: %s", esp_err_to_name(ec));
        badgelink_status_int_err();
        return;
    }

    // Try to write NVS.
    switch (req->wdata.type) {
        case badgelink_NvsValueType_NvsValueUint8:
            ec = nvs_set_u8(handle, req->key, req->wdata.val.numericval);
            break;
        case badgelink_NvsValueType_NvsValueInt8:
            ec = nvs_set_i8(handle, req->key, req->wdata.val.numericval);
            break;
        case badgelink_NvsValueType_NvsValueUint16:
            ec = nvs_set_u16(handle, req->key, req->wdata.val.numericval);
            break;
        case badgelink_NvsValueType_NvsValueInt16:
            ec = nvs_set_i16(handle, req->key, req->wdata.val.numericval);
            break;
        case badgelink_NvsValueType_NvsValueUint32:
            ec = nvs_set_u32(handle, req->key, req->wdata.val.numericval);
            break;
        case badgelink_NvsValueType_NvsValueInt32:
            ec = nvs_set_i32(handle, req->key, req->wdata.val.numericval);
            break;
        case badgelink_NvsValueType_NvsValueUint64:
            ec = nvs_set_u64(handle, req->key, req->wdata.val.numericval);
            break;
        case badgelink_NvsValueType_NvsValueInt64:
            ec = nvs_set_i64(handle, req->key, req->wdata.val.numericval);
            break;
        case badgelink_NvsValueType_NvsValueString:
            ec = nvs_set_str(handle, req->key, req->wdata.val.stringval);
            break;
        case badgelink_NvsValueType_NvsValueBlob:
            ec = nvs_set_blob(handle, req->key, req->wdata.val.blobval.bytes, req->wdata.val.blobval.size);
            break;
        default:
            __builtin_unreachable();
    }

    // Clean up and report status.
    nvs_close(handle);
    if (ec != ESP_OK) {
        ESP_LOGE(TAG, "Write error: %s", esp_err_to_name(ec));
        badgelink_status_int_err();
    } else {
        badgelink_status_ok();
    }
}

// Handle an NVS delete request.
void badgelink_nvs_delete() {
    // Validate request.
    badgelink_NvsActionReq* req = &badgelink_packet.packet.request.req.nvs_action;
    if (req->has_wdata || !req->key[0] || !req->namespc[0]) {
        badgelink_status_malformed();
        return;
    }

    // Try to open NVS.
    nvs_handle_t handle;
    esp_err_t    ec = nvs_open(req->namespc, NVS_READWRITE, &handle);
    if (ec != ESP_OK) {
        ESP_LOGE(TAG, "Delete error: %s", esp_err_to_name(ec));
        badgelink_status_int_err();
        return;
    }

    // Try to delete the value.
    ec = nvs_erase_key(handle, req->key);
    nvs_close(handle);

    // Send status code.
    if (ec == ESP_ERR_NVS_NOT_FOUND) {
        badgelink_status_not_found();
    } else if (ec != ESP_OK) {
        ESP_LOGE(TAG, "Delete error: %s", esp_err_to_name(ec));
        badgelink_status_int_err();
    } else {
        badgelink_status_ok();
    }
}
