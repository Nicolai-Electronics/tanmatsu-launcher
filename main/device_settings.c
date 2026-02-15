#include "device_settings.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/led.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "lora.h"
#include "nvs.h"

static const char* NVS_NAMESPACE = "system";

// Default values for repository settings
#define DEFAULT_REPO_SERVER   "https://apps.tanmatsu.cloud"
#define DEFAULT_REPO_BASE_URI "/v1"

static esp_err_t device_settings_get_u8(const char* key, uint8_t default_value, uint8_t* out_value) {
    if (key == NULL || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (res != ESP_OK) {
        *out_value = default_value;
        return res;
    }
    uint8_t value;
    res = nvs_get_u8(nvs_handle, key, &value);
    if (res != ESP_OK) {
        *out_value = default_value;
        nvs_close(nvs_handle);
        return res;
    }
    nvs_close(nvs_handle);

    *out_value = value;
    return res;
}

static esp_err_t device_settings_set_u8(const char* key, uint8_t value) {
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }
    res = nvs_set_u8(nvs_handle, key, value);
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }
    nvs_close(nvs_handle);
    return res;
}

static esp_err_t device_settings_get_u16(const char* key, uint16_t default_value, uint16_t* out_value) {
    if (key == NULL || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (res != ESP_OK) {
        *out_value = default_value;
        return res;
    }
    uint16_t value;
    res = nvs_get_u16(nvs_handle, key, &value);
    if (res != ESP_OK) {
        *out_value = default_value;
        nvs_close(nvs_handle);
        return res;
    }
    nvs_close(nvs_handle);

    *out_value = value;
    return res;
}

static esp_err_t device_settings_set_u16(const char* key, uint16_t value) {
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }
    res = nvs_set_u16(nvs_handle, key, value);
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }
    nvs_close(nvs_handle);
    return res;
}

static esp_err_t device_settings_get_u32(const char* key, uint32_t default_value, uint32_t* out_value) {
    if (key == NULL || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (res != ESP_OK) {
        *out_value = default_value;
        return res;
    }
    uint32_t value;
    res = nvs_get_u32(nvs_handle, key, &value);
    if (res != ESP_OK) {
        *out_value = default_value;
        nvs_close(nvs_handle);
        return res;
    }
    nvs_close(nvs_handle);

    *out_value = value;
    return res;
}

static esp_err_t device_settings_set_u32(const char* key, uint32_t value) {
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }
    res = nvs_set_u32(nvs_handle, key, value);
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }
    nvs_close(nvs_handle);
    return res;
}

static esp_err_t device_settings_get_percentage(const char* key, uint8_t default_value, uint8_t minimum_value,
                                                uint8_t* out_percentage) {
    esp_err_t res = device_settings_get_u8(key, default_value, out_percentage);
    if (res != ESP_OK) {
        return res;
    }

    if (*out_percentage > 100) {
        *out_percentage = 100;
    }

    if (*out_percentage < minimum_value) {
        *out_percentage = minimum_value;
    }

    return res;
}

static esp_err_t device_settings_set_percentage(const char* key, uint8_t minimum_value, uint8_t percentage) {
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (percentage > 100) {
        percentage = 100;
    }
    if (percentage < minimum_value) {
        percentage = minimum_value;
    }
    return device_settings_set_u8(key, percentage);
}

static esp_err_t device_settings_get_string(const char* key, const char* default_value, char* out_value,
                                            size_t max_length) {
    if (key == NULL || out_value == NULL || max_length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (res != ESP_OK) {
        if (default_value != NULL) {
            strncpy(out_value, default_value, max_length - 1);
            out_value[max_length - 1] = '\0';
        }
        return res;
    }
    size_t size = 0;
    res         = nvs_get_str(nvs_handle, key, NULL, &size);
    if (res != ESP_OK || size > max_length) {
        if (default_value != NULL) {
            strncpy(out_value, default_value, max_length - 1);
            out_value[max_length - 1] = '\0';
        }
        nvs_close(nvs_handle);
        return (res != ESP_OK) ? res : ESP_ERR_NO_MEM;
    }
    res = nvs_get_str(nvs_handle, key, out_value, &size);
    if (res != ESP_OK) {
        if (default_value != NULL) {
            strncpy(out_value, default_value, max_length - 1);
            out_value[max_length - 1] = '\0';
        }
    }
    nvs_close(nvs_handle);
    return res;
}

static esp_err_t device_settings_set_string(const char* key, const char* value) {
    if (key == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }
    res = nvs_set_str(nvs_handle, key, value);
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }
    res = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return res;
}

esp_err_t device_settings_get_display_brightness(uint8_t* out_percentage) {
    return device_settings_get_percentage("disp.brightness", 100, 3, out_percentage);
}

esp_err_t device_settings_set_display_brightness(uint8_t percentage) {
    return device_settings_set_percentage("disp.brightness", 3, percentage);
}

esp_err_t device_settings_get_keyboard_brightness(uint8_t* out_percentage) {
    return device_settings_get_percentage("kb.brightness", 0, 0, out_percentage);
}

esp_err_t device_settings_set_keyboard_brightness(uint8_t percentage) {
    return device_settings_set_percentage("kb.brightness", 0, percentage);
}

esp_err_t device_settings_get_led_brightness(uint8_t* out_percentage) {
    return device_settings_get_percentage("led.brightness", 100, 0, out_percentage);
}

esp_err_t device_settings_set_led_brightness(uint8_t percentage) {
    return device_settings_set_percentage("led.brightness", 0, percentage);
}

esp_err_t device_settings_apply(void) {
    uint8_t display_brightness = 100;
    device_settings_get_display_brightness(&display_brightness);
    bsp_display_set_backlight_brightness(display_brightness);

    uint8_t keyboard_brightness = 0;
    device_settings_get_keyboard_brightness(&keyboard_brightness);
    bsp_input_set_backlight_brightness(keyboard_brightness);

    uint8_t led_brightness = 100;
    device_settings_get_led_brightness(&led_brightness);
    bsp_led_set_brightness(led_brightness);
    return ESP_OK;
}

// Firmware settings
esp_err_t device_settings_get_firmware_patch_level(uint8_t* out_level) {
    return device_settings_get_u8("fw.patch", 0, out_level);
}

esp_err_t device_settings_set_firmware_patch_level(uint8_t level) {
    return device_settings_set_u8("fw.patch", level);
}

// Repository settings

esp_err_t device_settings_get_repo_server(char* out_value, size_t max_length) {
    return device_settings_get_string("repo.server", DEFAULT_REPO_SERVER, out_value, max_length);
}

esp_err_t device_settings_set_repo_server(const char* value) {
    return device_settings_set_string("repo.server", value);
}

esp_err_t device_settings_get_repo_base_uri(char* out_value, size_t max_length) {
    return device_settings_get_string("repo.baseuri", DEFAULT_REPO_BASE_URI, out_value, max_length);
}

esp_err_t device_settings_set_repo_base_uri(const char* value) {
    return device_settings_set_string("repo.baseuri", value);
}

void device_settings_get_default_http_user_agent(char* out_value, size_t max_length) {
    char                  device_name[64] = {0};
    const esp_app_desc_t* app_description = esp_app_get_description();
    bsp_device_get_name(device_name, sizeof(device_name));
    snprintf(out_value, max_length, "%s/%s", device_name, app_description->version);
}

esp_err_t device_settings_get_http_user_agent(char* out_value, size_t max_length) {
    char default_ua[128] = {0};
    device_settings_get_default_http_user_agent(default_ua, sizeof(default_ua));
    return device_settings_get_string("http.ua", default_ua, out_value, max_length);
}

esp_err_t device_settings_set_http_user_agent(const char* value) {
    return device_settings_set_string("http.ua", value);
}

// Owner settings

esp_err_t device_settings_get_owner_nickname(char* out_value, size_t max_length) {
    return device_settings_get_string("owner.nickname", "John Smith", out_value, max_length);
}

esp_err_t device_settings_set_owner_nickname(const char* value) {
    return device_settings_set_string("owner.nickname", value);
}

esp_err_t device_settings_get_owner_birthday_day(uint8_t* out_day) {
    return device_settings_get_u8("owner.bday.day", 1, out_day);
}

esp_err_t device_settings_set_owner_birthday_day(uint8_t day) {
    return device_settings_set_u8("owner.bday.day", day);
}

esp_err_t device_settings_get_owner_birthday_month(uint8_t* out_month) {
    return device_settings_get_u8("owner.bday.mon", 1, out_month);
}

esp_err_t device_settings_set_owner_birthday_month(uint8_t month) {
    return device_settings_set_u8("owner.bday.mon", month);
}

// LoRa settings

esp_err_t device_settings_get_lora_frequency(uint32_t* out_value) {
    esp_err_t res = device_settings_get_u32("lora.freq", 0, out_value);
    if (res != ESP_OK || out_value == 0) {
        lora_protocol_status_params_t status = {0};
        lora_get_status(&status);
        *out_value = status.chip_type == LORA_PROTOCOL_CHIP_SX1268 ? 433875000 : 869618000;
    }
    return res;
}

esp_err_t device_settings_set_lora_frequency(uint32_t frequency) {
    return device_settings_set_u32("lora.freq", frequency);
}

esp_err_t device_settings_get_lora_spreading_factor(uint8_t* out_sf) {
    return device_settings_get_u8("lora.sf", 8, out_sf);
}

esp_err_t device_settings_set_lora_spreading_factor(uint8_t sf) {
    return device_settings_set_u8("lora.sf", sf);
}

esp_err_t device_settings_get_lora_bandwidth(uint16_t* out_bandwidth) {
    return device_settings_get_u16("lora.bandwidth", 62, out_bandwidth);
}

esp_err_t device_settings_set_lora_bandwidth(uint16_t bandwidth) {
    return device_settings_set_u16("lora.bandwidth", bandwidth);
}

esp_err_t device_settings_get_lora_coding_rate(uint8_t* out_coding_rate) {
    return device_settings_get_u8("lora.codingrate", 8, out_coding_rate);
}

esp_err_t device_settings_set_lora_coding_rate(uint8_t coding_rate) {
    return device_settings_set_u8("lora.codingrate", coding_rate);
}

esp_err_t device_settings_get_lora_power(uint8_t* out_power) {
    return device_settings_get_u8("lora.power", 22, out_power);
}

esp_err_t device_settings_set_lora_power(uint8_t power) {
    return device_settings_set_u8("lora.power", power);
}

// Meshcore settings

esp_err_t device_settings_get_meshcore_name(char* out_value, size_t max_length) {
    return device_settings_get_string("mc.name", "Tanmatsu", out_value, max_length);
}

esp_err_t device_settings_set_meshcore_name(const char* value) {
    return device_settings_set_string("mc.name", value);
}

esp_err_t device_settings_get_meshcore_private_key(char* out_value, size_t max_length) {
    return device_settings_get_string("mc.private_key", "", out_value, max_length);
}

esp_err_t device_settings_set_meshcore_private_key(const char* value) {
    return device_settings_set_string("mc.private_key", value);
}

esp_err_t device_settings_get_meshcore_public_key(char* out_value, size_t max_length) {
    return device_settings_get_string("mc.public_key", "", out_value, max_length);
}

esp_err_t device_settings_set_meshcore_public_key(const char* value) {
    return device_settings_set_string("mc.public_key", value);
}

// Theme

esp_err_t device_settings_get_theme(theme_setting_t* out_theme) {
    return device_settings_get_u32("owner.theme", THEME_BLACK, (uint32_t*)out_theme);
}

esp_err_t device_settings_set_theme(theme_setting_t theme) {
    return device_settings_set_u32("owner.theme", (uint32_t)theme);
}
