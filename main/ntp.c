#include "ntp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "nvs.h"

static const char TAG[] = "NTP";

esp_err_t ntp_start_service(const char* server) {
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(server);
    return esp_netif_sntp_init(&config);
}

esp_err_t ntp_sync_wait(void) {
    return esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
}

// Settings

bool ntp_get_enabled(void) {
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open("system", NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return false;
    }
    uint8_t enabled = false;
    res             = nvs_get_u8(nvs_handle, "ntp", &enabled);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get NVS entry");
        nvs_close(nvs_handle);
        return false;
    }
    res = nvs_commit(nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit to NVS");
    }
    nvs_close(nvs_handle);
    return (enabled & 1);
}

void ntp_set_enabled(bool enabled) {
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open("system", NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return;
    }
    res = nvs_set_u8(nvs_handle, "ntp", enabled ? 1 : 0);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set NVS entry");
        nvs_close(nvs_handle);
        return;
    }
    res = nvs_commit(nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit to NVS");
    }
    nvs_close(nvs_handle);
}
