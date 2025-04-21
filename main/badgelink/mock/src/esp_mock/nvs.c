
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include "nvs.h"
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

/* ==== NVS read ==== */

esp_err_t nvs_open(char const* namespace, bool writeable, nvs_handle_t* out_handle) {
    printf("TODO\n");
    abort();
}
void nvs_close(nvs_handle_t handle) {
    printf("TODO\n");
    abort();
}

esp_err_t nvs_entry_find(char const* part, char const* namespace, nvs_type_t type, nvs_iterator_t* out_iter) {
    printf("TODO\n");
    abort();
}
void nvs_release_iterator(nvs_iterator_t iter) {
    printf("TODO\n");
    abort();
}
esp_err_t nvs_entry_info(nvs_iterator_t iter, nvs_entry_info_t* out_info) {
    printf("TODO\n");
    abort();
}
esp_err_t nvs_entry_next(nvs_iterator_t* iter) {
    printf("TODO\n");
    abort();
}

static mock__ent_t* nvs_lookup(nvs_handle_t handle, char const* key) {
    mock__ent_t* cur = handle.sentinel->next;
    while (*cur->key) {
        if (!strcmp(cur->key, key)) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

static esp_err_t nvs_get_scalar(nvs_handle_t handle, char const* key, void* out, nvs_type_t type) {
    mock__ent_t* ent = nvs_lookup(handle, key);
    if (!ent || ent->type != type) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    memcpy(out, &ent->data.scalar, 1 << (type & 3));
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t handle, char const* key, uint8_t* out_value) {
    return nvs_get_scalar(handle, key, out_value, NVS_TYPE_U8);
}
esp_err_t nvs_get_u16(nvs_handle_t handle, char const* key, uint16_t* out_value) {
    return nvs_get_scalar(handle, key, out_value, NVS_TYPE_U16);
}
esp_err_t nvs_get_u32(nvs_handle_t handle, char const* key, uint32_t* out_value) {
    return nvs_get_scalar(handle, key, out_value, NVS_TYPE_U32);
}
esp_err_t nvs_get_u64(nvs_handle_t handle, char const* key, uint64_t* out_value) {
    return nvs_get_scalar(handle, key, out_value, NVS_TYPE_U64);
}
esp_err_t nvs_get_i8(nvs_handle_t handle, char const* key, int8_t* out_value) {
    return nvs_get_scalar(handle, key, out_value, NVS_TYPE_I8);
}
esp_err_t nvs_get_i16(nvs_handle_t handle, char const* key, int16_t* out_value) {
    return nvs_get_scalar(handle, key, out_value, NVS_TYPE_I16);
}
esp_err_t nvs_get_i32(nvs_handle_t handle, char const* key, int32_t* out_value) {
    return nvs_get_scalar(handle, key, out_value, NVS_TYPE_I32);
}
esp_err_t nvs_get_i64(nvs_handle_t handle, char const* key, int64_t* out_value) {
    return nvs_get_scalar(handle, key, out_value, NVS_TYPE_I64);
}

esp_err_t nvs_get_str(nvs_handle_t handle, char const* key, char* buffer, size_t* inout_length) {
    mock__ent_t* ent = nvs_lookup(handle, key);
    if (!ent || ent->type != NVS_TYPE_STR) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if (*inout_length > ent->data.var.size) {
        *inout_length = ent->data.var.size;
    }
    if (*inout_length) {
        memcpy(buffer, ent->data.var.data, *inout_length);
        buffer[*inout_length] = 0;
    }
    *inout_length = ent->data.var.size;
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, char const* key, void* buffer, size_t* inout_size) {
    mock__ent_t* ent = nvs_lookup(handle, key);
    if (!ent || ent->type != NVS_TYPE_BLOB) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if (*inout_size > ent->data.var.size) {
        *inout_size = ent->data.var.size;
    }
    if (*inout_size) {
        memcpy(buffer, ent->data.var.data, *inout_size);
    }
    *inout_size = ent->data.var.size;
    return ESP_OK;
}

/* ==== NVS write ==== */

static mock__ent_t* nvs_alloc(nvs_handle_t handle, char const* key) {
    printf("TODO\n");
    abort();
}

static esp_err_t nvs_set_scalar(nvs_handle_t handle, char const* key, void const* value, nvs_type_t type) {
    mock__ent_t* ent = nvs_alloc(handle, key);
    if (ent->type == NVS_TYPE_STR || ent->type == NVS_TYPE_BLOB) {
        free(ent->data.var.data);
    }
    ent->type = type;
    memcpy(&ent->data.scalar, value, 1 << (type & 3));
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t handle, char const* key, uint8_t value) {
    return nvs_set_scalar(handle, key, &value, NVS_TYPE_U8);
}
esp_err_t nvs_set_u16(nvs_handle_t handle, char const* key, uint16_t value) {
    return nvs_set_scalar(handle, key, &value, NVS_TYPE_U16);
}
esp_err_t nvs_set_u32(nvs_handle_t handle, char const* key, uint32_t value) {
    return nvs_set_scalar(handle, key, &value, NVS_TYPE_U32);
}
esp_err_t nvs_set_u64(nvs_handle_t handle, char const* key, uint64_t value) {
    return nvs_set_scalar(handle, key, &value, NVS_TYPE_U64);
}
esp_err_t nvs_set_i8(nvs_handle_t handle, char const* key, int8_t value) {
    return nvs_set_scalar(handle, key, &value, NVS_TYPE_I8);
}
esp_err_t nvs_set_i16(nvs_handle_t handle, char const* key, int16_t value) {
    return nvs_set_scalar(handle, key, &value, NVS_TYPE_I16);
}
esp_err_t nvs_set_i32(nvs_handle_t handle, char const* key, int32_t value) {
    return nvs_set_scalar(handle, key, &value, NVS_TYPE_I32);
}
esp_err_t nvs_set_i64(nvs_handle_t handle, char const* key, int64_t value) {
    return nvs_set_scalar(handle, key, &value, NVS_TYPE_I64);
}

esp_err_t nvs_set_str(nvs_handle_t handle, char const* key, char* value) {
    printf("TODO\n");
    abort();
    return 0;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, char const* key, void* data, size_t length) {
    printf("TODO\n");
    abort();
    return 0;
}

esp_err_t nvs_erase_key(nvs_handle_t handle, char const* key) {
    printf("TODO\n");
    abort();
    return 0;
}
