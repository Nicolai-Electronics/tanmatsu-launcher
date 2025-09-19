#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    DEVICE_VARIANT_RADIO_NONE                = 0,
    DEVICE_VARIANT_RADIO_AI_THINKER_RA01S    = 1,   // SX1268 410-525MHz
    DEVICE_VARIANT_RADIO_AI_THINKER_RA01SH   = 2,   // SX1262 803-930MHz
    DEVICE_VARIANT_RADIO_EBYTE_E22_400M_22S  = 3,   // SX1268 410-493MHz TCXO
    DEVICE_VARIANT_RADIO_EBYTE_E22_900M_22S  = 4,   // SX1262 850-930MHz TCXO
    DEVICE_VARIANT_RADIO_EBYTE_E22_400M_30S  = 5,   // SX1268 410-493MHz TCXO PA
    DEVICE_VARIANT_RADIO_EBYTE_E22_900M_30S  = 6,   // SX1262 850-930MHz TCXO PA
    DEVICE_VARIANT_RADIO_EBYTE_E22_400M_33S  = 7,   // SX1268 410-493MHz TCXO PA
    DEVICE_VARIANT_RADIO_EBYTE_E22_900M_33S  = 8,   // SX1262 850-930MHz TCXO PA
    DEVICE_VARIANT_RADIO_EBYTE_E220_400M_22S = 9,   // LLCC68 410-493 MHz
    DEVICE_VARIANT_RADIO_EBYTE_E220_900M_22S = 10,  // LLCC68 850-930 MHz
    DEVICE_VARIANT_RADIO_EBYTE_E220_400M_33S = 11,  // LLCC68 410-493 MHz PA
    DEVICE_VARIANT_RADIO_EBYTE_E220_900M_33S = 12,  // LLCC68 850-930 MHz PA
} device_variant_radio_t;

typedef enum {
    // Basic color variants (0x00 - 0x0F)
    DEVICE_VARIANT_COLOR_NONE             = 0x00,
    DEVICE_VARIANT_COLOR_BLACK            = 0x01,
    DEVICE_VARIANT_COLOR_CYBERDECK        = 0x02,
    DEVICE_VARIANT_COLOR_BLUE             = 0x03,
    DEVICE_VARIANT_COLOR_RED              = 0x04,
    DEVICE_VARIANT_COLOR_GREEN            = 0x05,
    DEVICE_VARIANT_COLOR_PURPLE           = 0x06,
    DEVICE_VARIANT_COLOR_YELLOW           = 0x07,
    DEVICE_VARIANT_COLOR_WHITE            = 0x08,
    // Custom color variants (0x80 - 0x8F)
    DEVICE_VARIANT_COLOR_ORANGE_BLACK     = 0x81,
    DEVICE_VARIANT_COLOR_ORANGE_CYBERDECK = 0x82,
} device_variant_color_t;

typedef struct {
    // User data
    char                   name[16 + sizeof('\0')];
    char                   vendor[10 + sizeof('\0')];
    uint8_t                revision;
    device_variant_radio_t radio;
    device_variant_color_t color;
    char                   region[2 + sizeof('\0')];
    // MAC address
    uint8_t                mac[6];
    // Unique identifier
    uint8_t                uid[16];
    // Waver revision
    uint8_t                waver_rev_major;
    uint8_t                waver_rev_minor;
} device_identity_t;

esp_err_t   read_device_identity(device_identity_t* out_identity);
const char* get_radio_name(uint8_t radio_id);
const char* get_color_name(uint8_t color_id);
