#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "http_download.h"

typedef enum {
    APP_MGMT_LOCATION_INTERNAL = 0,
    APP_MGMT_LOCATION_SD,
} app_mgmt_location_t;

esp_err_t app_mgmt_install(const char* repository_url, const char* slug, app_mgmt_location_t location,
                           download_callback_t download_callback);
esp_err_t app_mgmt_uninstall(const char* slug, app_mgmt_location_t location);
esp_err_t app_mgmt_install_from_file(const char* slug, const char* name, uint32_t revision, char* firmware_path);