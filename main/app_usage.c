#include "app_usage.h"
#include <stdint.h>
#include "esp_err.h"
#include "nvs.h"

static const char* APP_USAGE_NAMESPACE = "app_usage";

esp_err_t app_usage_get_last_used(const char* slug, uint32_t* out_timestamp) {
    if (slug == NULL || out_timestamp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(APP_USAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (res != ESP_OK) {
        *out_timestamp = 0;
        return res;
    }
    uint32_t value;
    res = nvs_get_u32(nvs_handle, slug, &value);
    if (res != ESP_OK) {
        *out_timestamp = 0;
        nvs_close(nvs_handle);
        return res;
    }
    nvs_close(nvs_handle);
    *out_timestamp = value;
    return res;
}

esp_err_t app_usage_set_last_used(const char* slug, uint32_t timestamp) {
    if (slug == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(APP_USAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }
    res = nvs_set_u32(nvs_handle, slug, timestamp);
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }
    res = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return res;
}

esp_err_t app_usage_remove_last_used(const char* slug) {
    if (slug == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(APP_USAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        return res;
    }
    res = nvs_erase_key(nvs_handle, slug);
    if (res == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs_handle);
        return ESP_OK;
    }
    if (res != ESP_OK) {
        nvs_close(nvs_handle);
        return res;
    }
    res = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return res;
}
