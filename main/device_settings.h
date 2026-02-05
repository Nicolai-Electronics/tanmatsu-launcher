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

esp_err_t device_settings_get_lora_frequency(uint32_t* out_value);
esp_err_t device_settings_set_lora_frequency(uint32_t frequency);
esp_err_t device_settings_get_lora_spreading_factor(uint8_t* out_sf);
esp_err_t device_settings_set_lora_spreading_factor(uint8_t sf);
esp_err_t device_settings_get_lora_bandwidth(uint16_t* out_bandwidth);
esp_err_t device_settings_set_lora_bandwidth(uint16_t bandwidth);
esp_err_t device_settings_get_lora_coding_rate(uint8_t* out_coding_rate);
esp_err_t device_settings_set_lora_coding_rate(uint8_t coding_rate);
esp_err_t device_settings_get_lora_power(uint8_t* out_power);
esp_err_t device_settings_set_lora_power(uint8_t power);

// Meshcore settings

esp_err_t device_settings_get_meshcore_name(char* out_value, size_t max_length);
esp_err_t device_settings_set_meshcore_name(const char* value);
esp_err_t device_settings_get_meshcore_private_key(char* out_value, size_t max_length);
esp_err_t device_settings_set_meshcore_private_key(const char* value);
esp_err_t device_settings_get_meshcore_public_key(char* out_value, size_t max_length);
esp_err_t device_settings_set_meshcore_public_key(const char* value);
