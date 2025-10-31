#include "device_information.h"
#include <string.h>
#include "bsp/device.h"
#include "esp_efuse.h"
#include "esp_efuse_custom_table.h"
#include "esp_efuse_table.h"
#include "esp_idf_version.h"

esp_err_t read_device_identity(device_identity_t* out_identity) {
    esp_err_t res;

    memset(out_identity, 0, sizeof(device_identity_t));

    res = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_NAME, out_identity->name, 16 * 8);
    if (res != ESP_OK) {
        return res;
    }

    res = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_VENDOR, out_identity->vendor, 10 * 8);
    if (res != ESP_OK) {
        return res;
    }

    res =
        esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_REVISION, &out_identity->revision, sizeof(uint8_t) * 8);
    if (res != ESP_OK) {
        return res;
    }

    uint8_t variant_radio = 0;
    res = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_VARIANT_RADIO, &variant_radio, sizeof(uint8_t) * 8);
    if (res != ESP_OK) {
        return res;
    }
    out_identity->radio = (device_variant_radio_t)variant_radio;

    uint8_t variant_color = 0;
    res = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_VARIANT_COLOR, &variant_color, sizeof(uint8_t) * 8);
    if (res != ESP_OK) {
        return res;
    }
    out_identity->color = (device_variant_radio_t)variant_color;

    res = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_REGION, out_identity->region, 2 * 8);
    if (res != ESP_OK) {
        return res;
    }

    res = esp_efuse_read_field_blob(ESP_EFUSE_MAC, &out_identity->mac, 6 * 8);
    if (res != ESP_OK) {
        return res;
    }
#ifdef CONFIG_IDF_TARGET_ESP32P4
    res = esp_efuse_read_field_blob(ESP_EFUSE_OPTIONAL_UNIQUE_ID, &out_identity->uid, 16 * 8);
    if (res != ESP_OK) {
        return res;
    }

#if ESP_IDF_VERSION_MAJOR >= 5 && ESP_IDF_VERSION_MINOR >= 6
    res = esp_efuse_read_field_blob(ESP_EFUSE_WAFER_VERSION_MAJOR_LO, &out_identity->waver_rev_major, 2);
    if (res != ESP_OK) {
        return res;
    }

    uint8_t version_major_hi = 0;

    res = esp_efuse_read_field_blob(ESP_EFUSE_WAFER_VERSION_MAJOR_HI, &version_major_hi, 1);
    if (res != ESP_OK) {
        return res;
    }

    out_identity->waver_rev_major |= (version_major_hi << 2);
#else
    // Fallback for ESP-IDF version 5.4 and earlier
    res = esp_efuse_read_field_blob(ESP_EFUSE_WAFER_VERSION_MAJOR, &out_identity->waver_rev_major, 2);
    if (res != ESP_OK) {
        return res;
    }
#endif

    res = esp_efuse_read_field_blob(ESP_EFUSE_WAFER_VERSION_MINOR, &out_identity->waver_rev_minor, 4);
    if (res != ESP_OK) {
        return res;
    }
#endif

    return res;
}

const char* get_radio_name(uint8_t radio_id) {
    switch (radio_id) {
        case DEVICE_VARIANT_RADIO_NONE:
            return "None";
        case DEVICE_VARIANT_RADIO_AI_THINKER_RA01S:
            return "Ai thinker RA-01S";
        case DEVICE_VARIANT_RADIO_AI_THINKER_RA01SH:
            return "Ai thinker RA-01SH";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_400M_22S:
            return "EBYTE E22-400M22S";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_900M_22S:
            return "EBYTE E22-900M22S";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_400M_30S:
            return "EBYTE E22-400M30S";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_900M_30S:
            return "EBYTE E22-900M30S";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_400M_33S:
            return "EBYTE E22-400M33S";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_900M_33S:
            return "EBYTE E22-900M33S";
        case DEVICE_VARIANT_RADIO_EBYTE_E220_400M_22S:
            return "EBYTE E220-400M22S";
        case DEVICE_VARIANT_RADIO_EBYTE_E220_900M_22S:
            return "EBYTE E220-900M22S";
        case DEVICE_VARIANT_RADIO_EBYTE_E220_400M_33S:
            return "EBYTE E220-400M33S";
        case DEVICE_VARIANT_RADIO_EBYTE_E220_900M_33S:
            return "EBYTE E220-900M33S";
        default:
            return "Unknown";
    }
}

const char* get_color_name(uint8_t color_id) {
    switch (color_id) {
        case DEVICE_VARIANT_COLOR_BLACK:
            return "Black";
        case DEVICE_VARIANT_COLOR_CYBERDECK:
            return "Cyberdeck";
        case DEVICE_VARIANT_COLOR_BLUE:
            return "Blue";
        case DEVICE_VARIANT_COLOR_RED:
            return "Red";
        case DEVICE_VARIANT_COLOR_GREEN:
            return "Green";
        case DEVICE_VARIANT_COLOR_PURPLE:
            return "Purple";
        case DEVICE_VARIANT_COLOR_YELLOW:
            return "Yellow";
        case DEVICE_VARIANT_COLOR_WHITE:
            return "White";
        case DEVICE_VARIANT_COLOR_ORANGE_BLACK:
            return "Orange - Black";
        case DEVICE_VARIANT_COLOR_ORANGE_CYBERDECK:
            return "Orange - Cyberdeck";
        default:
            return "Unknown";
    }
}
