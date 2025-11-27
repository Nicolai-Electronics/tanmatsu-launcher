#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t device_settings_get_display_brightness(uint8_t* out_percentage);
esp_err_t device_settings_set_display_brightness(uint8_t percentage);

esp_err_t device_settings_get_keyboard_brightness(uint8_t* out_percentage);
esp_err_t device_settings_set_keyboard_brightness(uint8_t percentage);

esp_err_t device_settings_get_led_brightness(uint8_t* out_percentage);
esp_err_t device_settings_set_led_brightness(uint8_t percentage);

esp_err_t device_settings_apply(void);

// Repository settings
esp_err_t device_settings_get_repo_server(char* out_value, size_t max_length);
esp_err_t device_settings_set_repo_server(const char* value);
esp_err_t device_settings_get_repo_base_uri(char* out_value, size_t max_length);
esp_err_t device_settings_set_repo_base_uri(const char* value);
