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
#include "nvs.h"

static const char* NVS_NAMESPACE = "system";

// Default values for repository settings
#define DEFAULT_REPO_SERVER   "https://apps.tanmatsu.cloud"
#define DEFAULT_REPO_BASE_URI "/v1"

static esp_err_t device_settings_get_percentage(const char* key, uint8_t default_value, uint8_t minimum_value,
                                                uint8_t* out_percentage) {
    if (key == NULL || out_percentage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (res != ESP_OK) {
        *out_percentage = default_value;
        return res;
    }
    uint8_t value;
    res = nvs_get_u8(nvs_handle, key, &value);
    if (res != ESP_OK) {
        *out_percentage = default_value;
        nvs_close(nvs_handle);
        return res;
    }
    nvs_close(nvs_handle);

    if (value > 100) {
        value = 100;
    }

    if (value < minimum_value) {
        value = minimum_value;
    }

    *out_percentage = value;
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
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }
    res = nvs_set_u8(nvs_handle, key, percentage);
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }
    nvs_close(nvs_handle);
    return res;
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
    char                    device_name[64]  = {0};
    const esp_app_desc_t*   app_description  = esp_app_get_description();
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
