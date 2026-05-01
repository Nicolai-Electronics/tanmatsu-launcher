#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t app_usage_get_last_used(const char* slug, uint32_t* out_timestamp);
esp_err_t app_usage_set_last_used(const char* slug, uint32_t timestamp);
esp_err_t app_usage_remove_last_used(const char* slug);
