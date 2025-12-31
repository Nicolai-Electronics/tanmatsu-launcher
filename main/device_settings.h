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
void      device_settings_get_default_http_user_agent(char* out_value, size_t max_length);
esp_err_t device_settings_get_http_user_agent(char* out_value, size_t max_length);
esp_err_t device_settings_set_http_user_agent(const char* value);

// Owner settings
esp_err_t device_settings_get_owner_nickname(char* out_value, size_t max_length);
esp_err_t device_settings_set_owner_nickname(const char* value);
esp_err_t device_settings_get_owner_birthday_day(uint8_t* out_day);
esp_err_t device_settings_set_owner_birthday_day(uint8_t day);
esp_err_t device_settings_get_owner_birthday_month(uint8_t* out_month);
esp_err_t device_settings_set_owner_birthday_month(uint8_t month);
