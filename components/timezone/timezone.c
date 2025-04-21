#include "timezone.h"
#include <string.h>
#include <time.h>
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "timezone";

extern const timezone_t timezones[];
extern const size_t     timezones_len;

// Database

size_t timezone_get_amount(void) {
    return timezones_len;
}

const timezone_t* timezone_get_index(size_t index) {
    if (index >= timezones_len) {
        return NULL;
    }
    return &timezones[index];
}

esp_err_t timezone_get_name(const char* name, const timezone_t** out_pointer) {
    size_t name_length = strlen(name);
    if (name_length < 2) {
        return ESP_FAIL;
    }
    for (size_t i = 0; i < timezones_len; i++) {
        const timezone_t* timezone = timezone_get_index(i);
        if (timezone->name[0] == name[0] && strlen(timezone->name) == name_length &&
            strcmp(timezone->name, name) == 0) {
            *out_pointer = timezone;
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

// Apply timezone to date library

esp_err_t timezone_apply_tzstring(const char* tzstring) {
    if (tzstring == NULL) {
        return ESP_FAIL;
    }
    // Set the timezone using the tz string
    setenv("TZ", tzstring, 1);
    tzset();
    return ESP_OK;
}

esp_err_t timezone_apply_timezone(const timezone_t* timezone) {
    if (timezone == NULL) {
        return ESP_FAIL;
    }
    return timezone_apply_tzstring(timezone->tz);
}

esp_err_t timezone_apply_index(size_t index) {
    const timezone_t* timezone = timezone_get_index(index);
    if (timezone == NULL) {
        return ESP_FAIL;
    }
    timezone_apply_timezone(timezone);
    return ESP_OK;
}

esp_err_t timezone_apply_name(const char* name) {
    const timezone_t* timezone = NULL;
    esp_err_t         res      = timezone_get_name(name, &timezone);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to find timezone \"%s\"", name);
        return res;
    }
    timezone_apply_timezone(timezone);
    return ESP_OK;
}

// NVS storage

esp_err_t timezone_nvs_get(const char* nvs_namespace, const char* nvs_key, char* out_name, size_t name_length) {
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(nvs_namespace, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return res;
    }
    size_t size = 0;
    res         = nvs_get_str(nvs_handle, nvs_key, NULL, &size);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to find NVS entry");
        nvs_close(nvs_handle);
        return res;
    }
    if (size > name_length) {
        ESP_LOGE(TAG, "Value in NVS is too long");
        nvs_close(nvs_handle);
        return ESP_ERR_NO_MEM;
    }
    res = nvs_get_str(nvs_handle, nvs_key, out_name, &size);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read NVS entry");
        nvs_close(nvs_handle);
        return res;
    }
    nvs_close(nvs_handle);
    return res;
}

esp_err_t timezone_nvs_set(const char* nvs_namespace, const char* nvs_key, const char* name) {
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(nvs_namespace, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return res;
    }

    res = nvs_set_str(nvs_handle, nvs_key, name);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set NVS entry");
        nvs_close(nvs_handle);
        return res;
    }

    res = nvs_commit(nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit to NVS");
        nvs_close(nvs_handle);
        return res;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t timezone_nvs_apply(const char* nvs_namespace, const char* nvs_key) {
    char      name[64];
    esp_err_t res = timezone_nvs_get(nvs_namespace, nvs_key, name, sizeof(name));
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get timezone from NVS");
        return res;
    }
    res = timezone_apply_name(name);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply timezone \"%s\"", name);
        return res;
    }
    return res;
}

// NVS raw TZ string storage for use in applications without this library

esp_err_t timezone_nvs_set_tzstring(const char* nvs_namespace, const char* nvs_key, const char* tzstring) {
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open(nvs_namespace, NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return res;
    }

    res = nvs_set_str(nvs_handle, nvs_key, tzstring);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set NVS entry");
        nvs_close(nvs_handle);
        return res;
    }

    res = nvs_commit(nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit to NVS");
        nvs_close(nvs_handle);
        return res;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t timezone_nvs_apply_tzstring(const char* nvs_namespace, const char* nvs_key) {
    char      tzstring[64];
    esp_err_t res = timezone_nvs_get(nvs_namespace, nvs_key, tzstring, sizeof(tzstring));
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get TZ string from NVS");
        return res;
    }
    res = timezone_apply_tzstring(tzstring);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply TZ string \"%s\"", tzstring);
        return res;
    }
    return res;
}
