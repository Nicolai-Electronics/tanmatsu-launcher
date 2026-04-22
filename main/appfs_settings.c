#include "appfs_settings.h"
#include <stdint.h>
#include "esp_err.h"
#include "nvs.h"

static const char* APPFS_NAMESPACE = "appfs";

esp_err_t appfs_settings_get_auto_cleanup(uint8_t* out_value) {
    if (out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(APPFS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (res != ESP_OK) {
        *out_value = DEFAULT_APPFS_AUTO_CLEANUP;
        return res;
    }
    uint8_t value;
    res = nvs_get_u8(nvs_handle, "auto_cleanup", &value);
    nvs_close(nvs_handle);
    if (res != ESP_OK) {
        *out_value = DEFAULT_APPFS_AUTO_CLEANUP;
        return res;
    }
    *out_value = value;
    return ESP_OK;
}

esp_err_t appfs_settings_set_auto_cleanup(uint8_t value) {
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(APPFS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }
    res = nvs_set_u8(nvs_handle, "auto_cleanup", value);
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }
    res = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return res;
}

esp_err_t appfs_settings_get_mismatch_reinstall(uint8_t* out_value) {
    if (out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(APPFS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (res != ESP_OK) {
        *out_value = DEFAULT_APPFS_MISMATCH_REINSTALL;
        return res;
    }
    uint8_t value;
    res = nvs_get_u8(nvs_handle, "mismatch_reinst", &value);
    nvs_close(nvs_handle);
    if (res != ESP_OK) {
        *out_value = DEFAULT_APPFS_MISMATCH_REINSTALL;
        return res;
    }
    *out_value = value;
    return ESP_OK;
}

esp_err_t appfs_settings_set_mismatch_reinstall(uint8_t value) {
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(APPFS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }
    res = nvs_set_u8(nvs_handle, "mismatch_reinst", value);
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }
    res = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return res;
}
