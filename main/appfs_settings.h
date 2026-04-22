#pragma once

#include <stdint.h>
#include "esp_err.h"

#define DEFAULT_APPFS_AUTO_CLEANUP        1
#define DEFAULT_APPFS_MISMATCH_REINSTALL  0

esp_err_t appfs_settings_get_auto_cleanup(uint8_t* out_value);
esp_err_t appfs_settings_set_auto_cleanup(uint8_t value);
esp_err_t appfs_settings_get_mismatch_reinstall(uint8_t* out_value);
esp_err_t appfs_settings_set_mismatch_reinstall(uint8_t value);
