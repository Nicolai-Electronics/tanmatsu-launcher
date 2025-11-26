#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t device_settings_get_display_brightness(uint8_t* out_percentage);
esp_err_t device_settings_set_display_brightness(uint8_t percentage);

esp_err_t device_settings_get_keyboard_brightness(uint8_t* out_percentage);
esp_err_t device_settings_set_keyboard_brightness(uint8_t percentage);

esp_err_t device_settings_get_led_brightness(uint8_t* out_percentage);
esp_err_t device_settings_set_led_brightness(uint8_t percentage);

esp_err_t device_settings_apply(void);
