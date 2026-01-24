#pragma once

#include <stdint.h>
#include "esp_err.h"

// Add-on descriptor

typedef enum {
    ADDON_LOCATION_INTERNAL               = 0x00,  // Internal add-on
    ADDON_LOCATION_EXTERNAL               = 0x10,  // CATT or SAO add-on
    ADDON_LOCATION_INTERNAL_MULTIPLEXER_0 = 0xE0,  // Internal add-on behind I2C multiplexer channel 0
    ADDON_LOCATION_INTERNAL_MULTIPLEXER_1 = 0xE1,  // Internal add-on behind I2C multiplexer channel 1
    ADDON_LOCATION_INTERNAL_MULTIPLEXER_2 = 0xE2,  // Internal add-on behind I2C multiplexer channel 2
    ADDON_LOCATION_INTERNAL_MULTIPLEXER_3 = 0xE3,  // Internal add-on behind I2C multiplexer channel 3
    ADDON_LOCATION_INTERNAL_MULTIPLEXER_4 = 0xE4,  // Internal add-on behind I2C multiplexer channel 4
    ADDON_LOCATION_INTERNAL_MULTIPLEXER_5 = 0xE5,  // Internal add-on behind I2C multiplexer channel 5
    ADDON_LOCATION_INTERNAL_MULTIPLEXER_6 = 0xE6,  // Internal add-on behind I2C multiplexer channel 6
    ADDON_LOCATION_INTERNAL_MULTIPLEXER_7 = 0xE7,  // Internal add-on behind I2C multiplexer channel 7
    ADDON_LOCATION_EXTERNAL_MULTIPLEXER_0 = 0xF0,  // External add-on behind I2C multiplexer channel 0
    ADDON_LOCATION_EXTERNAL_MULTIPLEXER_1 = 0xF1,  // External add-on behind I2C multiplexer channel 1
    ADDON_LOCATION_EXTERNAL_MULTIPLEXER_2 = 0xF2,  // External add-on behind I2C multiplexer channel 2
    ADDON_LOCATION_EXTERNAL_MULTIPLEXER_3 = 0xF3,  // External add-on behind I2C multiplexer channel 3
    ADDON_LOCATION_EXTERNAL_MULTIPLEXER_4 = 0xF4,  // External add-on behind I2C multiplexer channel 4
    ADDON_LOCATION_EXTERNAL_MULTIPLEXER_5 = 0xF5,  // External add-on behind I2C multiplexer channel 5
    ADDON_LOCATION_EXTERNAL_MULTIPLEXER_6 = 0xF6,  // External add-on behind I2C multiplexer channel 6
    ADDON_LOCATION_EXTERNAL_MULTIPLEXER_7 = 0xF7,  // External add-on behind I2C multiplexer channel 7
} addon_location_t;

typedef enum {
    ADDON_TYPE_UNINITIALIZED = 0,  // Uninitialized EEPROM
    ADDON_TYPE_UNKNOWN       = 1,  // Unknown magic value
    ADDON_TYPE_BINARY_SAO    = 2,  // LIFE (https://badge.team/docs/standards/sao/binary_descriptor)
    ADDON_TYPE_JSON          = 3,  // JSON (https://github.com/urish/badge-addon-id)
    ADDON_TYPE_HEXPANSION    = 4,  // THEX (https://tildagon.badge.emfcamp.org/hexpansions/eeprom)
    ADDON_TYPE_CATT          = 5,  // CATT (Nicolai Electronics add-on descriptor)
} addon_descriptor_type_t;

typedef struct {
    char*    name;
    uint8_t  data_length;
    uint8_t* data;
} addon_binary_sao_driver_t;

typedef struct {
    addon_location_t        location;
    addon_descriptor_type_t descriptor_type;
    union {
        struct {
            // Binary SAO specific fields
            char*                      name;
            uint8_t                    amount_of_drivers;
            addon_binary_sao_driver_t* drivers;
        } binary_sao;
        struct {
            // JSON specific fields
            char* json_text;
        } json;
        struct {
            // CATT / Hexpansion specific fields
            char manifest_version[4];
            struct {
                uint16_t offset;
                uint16_t page_size;
                uint32_t total_size;
            } filesystem_info;
            uint16_t vendor_id;
            uint16_t product_id;
            uint16_t unique_id;
            char     name[9 + sizeof('\0')];
        } catt;
    };
} addon_descriptor_t;

/*#define SAO_MAX_FIELD_LENGTH 256
#define SAO_MAX_NUM_DRIVERS  8

typedef struct _SAO_DRIVER {
    char    name[SAO_MAX_FIELD_LENGTH + 1];
    uint8_t data[SAO_MAX_FIELD_LENGTH];
    uint8_t data_length;
} sao_driver_t;

typedef struct _SAO {
    uint8_t      type;  // sao_type_t;
    char         name[SAO_MAX_FIELD_LENGTH + 1];
    uint8_t      amount_of_drivers;
    sao_driver_t drivers[SAO_MAX_NUM_DRIVERS];
} SAO;*/

// Functions

esp_err_t addon_initialize(void);
esp_err_t addon_get_descriptor(addon_location_t location, addon_descriptor_t** out_descriptor);
