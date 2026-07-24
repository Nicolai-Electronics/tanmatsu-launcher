#pragma once

#include <stdbool.h>
#include "esp_err.h"

bool app_favorite_get(const char* slug);
void app_favorite_set(const char* slug, bool favorite);

esp_err_t app_autostart_get(char* out_slug);
esp_err_t app_autostart_set(const char* slug);
