
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define NVS_DEFAULT_PART_NAME "nvs"

#define NVS_PART_NAME_MAX_SIZE 16
#define NVS_KEY_NAME_MAX_SIZE  16
#define NVS_NS_NAME_MAX_SIZE   16

#define NVS_READONLY  false
#define NVS_READWRITE true

typedef enum {
    NVS_TYPE_U8,
    NVS_TYPE_U16,
    NVS_TYPE_U32,
    NVS_TYPE_U64,
    NVS_TYPE_I8,
    NVS_TYPE_I16,
    NVS_TYPE_I32,
    NVS_TYPE_I64,
    NVS_TYPE_STR,
    NVS_TYPE_BLOB,
    NVS_TYPE_MAX,
    NVS_TYPE_ANY,
} nvs_type_t;

typedef struct mock__ent mock__ent_t;
struct mock__ent {
    mock__ent_t* prev;
    mock__ent_t* next;
    char         key[NVS_KEY_NAME_MAX_SIZE];
    union {
        struct {
            void*  data;
            size_t size;
        } var;
        uint64_t scalar;
    } data;
    nvs_type_t type;
};

typedef struct {
    char* namespace;
    mock__ent_t* cur;
} nvs_iterator_t;

typedef struct {
    mock__ent_t* sentinel;
} nvs_handle_t;

typedef struct {
    nvs_type_t type;
    char       namespace_name[NVS_NS_NAME_MAX_SIZE];
    char       key[NVS_KEY_NAME_MAX_SIZE];
} nvs_entry_info_t;

esp_err_t nvs_open(char const* namespace, bool writeable, nvs_handle_t* out_handle);
void      nvs_close(nvs_handle_t handle);

esp_err_t nvs_entry_find(char const* part, char const* namespace, nvs_type_t type, nvs_iterator_t* out_iter);
void      nvs_release_iterator(nvs_iterator_t iter);
esp_err_t nvs_entry_info(nvs_iterator_t iter, nvs_entry_info_t* out_info);
esp_err_t nvs_entry_next(nvs_iterator_t* iter);

esp_err_t nvs_get_u8(nvs_handle_t handle, char const* key, uint8_t* out_value);
esp_err_t nvs_get_u16(nvs_handle_t handle, char const* key, uint16_t* out_value);
esp_err_t nvs_get_u32(nvs_handle_t handle, char const* key, uint32_t* out_value);
esp_err_t nvs_get_u64(nvs_handle_t handle, char const* key, uint64_t* out_value);
esp_err_t nvs_get_i8(nvs_handle_t handle, char const* key, int8_t* out_value);
esp_err_t nvs_get_i16(nvs_handle_t handle, char const* key, int16_t* out_value);
esp_err_t nvs_get_i32(nvs_handle_t handle, char const* key, int32_t* out_value);
esp_err_t nvs_get_i64(nvs_handle_t handle, char const* key, int64_t* out_value);
esp_err_t nvs_get_str(nvs_handle_t handle, char const* key, char* buffer, size_t* inout_length);
esp_err_t nvs_get_blob(nvs_handle_t handle, char const* key, void* buffer, size_t* inout_size);

esp_err_t nvs_set_u8(nvs_handle_t handle, char const* key, uint8_t value);
esp_err_t nvs_set_u16(nvs_handle_t handle, char const* key, uint16_t value);
esp_err_t nvs_set_u32(nvs_handle_t handle, char const* key, uint32_t value);
esp_err_t nvs_set_u64(nvs_handle_t handle, char const* key, uint64_t value);
esp_err_t nvs_set_i8(nvs_handle_t handle, char const* key, int8_t value);
esp_err_t nvs_set_i16(nvs_handle_t handle, char const* key, int16_t value);
esp_err_t nvs_set_i32(nvs_handle_t handle, char const* key, int32_t value);
esp_err_t nvs_set_i64(nvs_handle_t handle, char const* key, int64_t value);
esp_err_t nvs_set_str(nvs_handle_t handle, char const* key, char* value);
esp_err_t nvs_set_blob(nvs_handle_t handle, char const* key, void* data, size_t length);

esp_err_t nvs_erase_key(nvs_handle_t handle, char const* key);
