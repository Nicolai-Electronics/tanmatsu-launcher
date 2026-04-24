#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define DEFAULT_DISPLAY_BRIGHTNESS  100
#define DEFAULT_KEYBOARD_BRIGHTNESS 100
#define DEFAULT_LED_BRIGHTNESS      100

#define DEFAULT_REPO_SERVER         "https://apps.tanmatsu.cloud"
#define DEFAULT_REPO_BASE_URI       "/v1"
#define DEFAULT_REPO_DOWNLOAD_ICONS 1

#define NVS_KEY_REPO_DOWNLOAD_ICONS "repo.dl_icons"

esp_err_t device_settings_apply(void);
void      device_settings_get_default_http_user_agent(char* out_value, size_t max_length);
esp_err_t device_settings_get_lora_frequency(uint32_t* out_value);
