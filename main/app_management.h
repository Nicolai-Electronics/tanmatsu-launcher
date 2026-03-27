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

// AppFS cache management
bool      app_mgmt_has_binary_in_install_dir(const char* slug);
bool      app_mgmt_can_uncache(const char* slug);
esp_err_t app_mgmt_copy_appfs_to_install_dir(const char* slug);
esp_err_t app_mgmt_remove_from_appfs(const char* slug);
esp_err_t app_mgmt_ensure_in_appfs(const char* slug, const char* name, uint32_t revision, const char* firmware_path);
esp_err_t app_mgmt_cache_to_appfs(const char* slug, const char* name, uint32_t revision, const char* firmware_path);
esp_err_t app_mgmt_appfs_evict_lru(size_t needed_bytes);
char*     app_mgmt_find_firmware_path(const char* slug);
esp_err_t app_mgmt_move(const char* slug, app_mgmt_location_t from, app_mgmt_location_t to);
bool      app_mgmt_can_move(const char* slug, app_mgmt_location_t from, app_mgmt_location_t to);