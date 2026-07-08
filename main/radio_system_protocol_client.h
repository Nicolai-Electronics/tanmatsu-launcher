#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define RADIO_SYSTEM_PROTOCOL_NVS_MAX_KEY_LENGTH 16

typedef enum {
    RADIO_SYSTEM_PROTOCOL_TYPE_ACK               = 0x00,
    RADIO_SYSTEM_PROTOCOL_TYPE_NACK              = 0x01,
    RADIO_SYSTEM_PROTOCOL_TYPE_GET_INFORMATION   = 0x02,
    RADIO_SYSTEM_PROTOCOL_TYPE_SET_CONFIGURATION = 0x03,
    RADIO_SYSTEM_PROTOCOL_TYPE_NVS_LIST          = 0x10,
    RADIO_SYSTEM_PROTOCOL_TYPE_NVS_READ          = 0x11,
    RADIO_SYSTEM_PROTOCOL_TYPE_NVS_WRITE         = 0x12,
    RADIO_SYSTEM_PROTOCOL_TYPE_NVS_DELETE        = 0x13,
} radio_system_protocol_packet_type_t;

typedef struct {
    uint32_t sequence_number;
    uint32_t type;  // radio_system_protocol_packet_type_t
} __attribute__((packed)) radio_system_protocol_header_t;

typedef struct {
    char    firmware_name[32];
    char    firmware_version[32];
    char    firmware_build_date[16];
    char    firmware_build_time[16];
    char    firmware_idf_version[32];
    char    firmware_sha256[32];
    uint8_t board_revision;
    char    country_code[2];
} __attribute__((packed)) radio_system_protocol_information_t;

typedef struct {
    uint8_t board_revision;
    char    country_code[2];
} __attribute__((packed)) radio_system_protocol_configuration_t;

typedef enum {
    RADIO_SYSTEM_PROTOCOL_NVS_VALUE_TYPE_UINT8  = 0,
    RADIO_SYSTEM_PROTOCOL_NVS_VALUE_TYPE_INT8   = 1,
    RADIO_SYSTEM_PROTOCOL_NVS_VALUE_TYPE_UINT16 = 2,
    RADIO_SYSTEM_PROTOCOL_NVS_VALUE_TYPE_INT16  = 3,
    RADIO_SYSTEM_PROTOCOL_NVS_VALUE_TYPE_UINT32 = 4,
    RADIO_SYSTEM_PROTOCOL_NVS_VALUE_TYPE_INT32  = 5,
    RADIO_SYSTEM_PROTOCOL_NVS_VALUE_TYPE_UINT64 = 6,
    RADIO_SYSTEM_PROTOCOL_NVS_VALUE_TYPE_INT64  = 7,
    RADIO_SYSTEM_PROTOCOL_NVS_VALUE_TYPE_STRING = 8,
    RADIO_SYSTEM_PROTOCOL_NVS_VALUE_TYPE_BLOB   = 9,
    RADIO_SYSTEM_PROTOCOL_NVS_VALUE_TYPE_ANY    = 10,
} radio_system_protocol_nvs_value_type_t;

typedef struct {
    char                                   key[RADIO_SYSTEM_PROTOCOL_NVS_MAX_KEY_LENGTH];
    radio_system_protocol_nvs_value_type_t type;
} __attribute__((packed)) radio_system_protocol_nvs_entry_t;

typedef struct {
    char                              namespace_name[RADIO_SYSTEM_PROTOCOL_NVS_MAX_KEY_LENGTH];
    radio_system_protocol_nvs_entry_t entry;
} __attribute__((packed)) radio_system_protocol_nvs_location_t;

typedef struct {
    radio_system_protocol_nvs_location_t location;
    uint32_t                             value_length;
    union {
        uint8_t  value_u8;
        int8_t   value_i8;
        uint16_t value_u16;
        int16_t  value_i16;
        uint32_t value_u32;
        int32_t  value_i32;
        uint64_t value_u64;
        int64_t  value_i64;
        char     value_string[0];
        uint8_t  value_blob[0];
    };
} __attribute__((packed)) radio_system_protocol_nvs_value_t;

// Functions

esp_err_t radio_system_protocol_init(void);
esp_err_t radio_system_protocol_get_information(radio_system_protocol_information_t* out_information);
esp_err_t radio_system_protocol_set_configuration(const radio_system_protocol_configuration_t* configuration);
esp_err_t radio_system_protocol_nvs_list(const char* namespace_name, const char* key_filter,
                                         radio_system_protocol_nvs_value_type_t type, uint32_t offset,
                                         radio_system_protocol_nvs_entry_t* out_entries, uint32_t max_entries,
                                         uint32_t* out_count);
esp_err_t radio_system_protocol_nvs_read(const char* namespace_name, const char* key,
                                         radio_system_protocol_nvs_value_type_t type,
                                         radio_system_protocol_nvs_value_t* out_value, size_t out_value_size);
esp_err_t radio_system_protocol_nvs_write(const radio_system_protocol_nvs_value_t* value, size_t value_size);
esp_err_t radio_system_protocol_nvs_delete(const char* namespace_name, const char* key,
                                           radio_system_protocol_nvs_value_type_t type);
