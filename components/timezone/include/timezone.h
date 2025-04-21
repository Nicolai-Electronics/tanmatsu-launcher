#pragma once
#include <stddef.h>
#include "esp_err.h"

#define TIMEZONE_NAME_LEN 32
#define TIMEZONE_TZ_LEN   64

typedef struct {
    char name[TIMEZONE_NAME_LEN];
    char tz[TIMEZONE_TZ_LEN];
} timezone_t;

size_t            timezone_get_amount(void);
const timezone_t* timezone_get_index(size_t index);
esp_err_t         timezone_get_name(const char* name, const timezone_t** out_pointer);
esp_err_t         timezone_apply_timezone(const timezone_t* timezone);
esp_err_t         timezone_apply_index(size_t index);
esp_err_t         timezone_apply_name(const char* name);
esp_err_t         timezone_nvs_get(const char* nvs_namespace, const char* nvs_key, char* out_name, size_t name_length);
esp_err_t         timezone_nvs_set(const char* nvs_namespace, const char* nvs_key, const char* name);
esp_err_t         timezone_nvs_apply(const char* nvs_namespace, const char* nvs_key);
esp_err_t         timezone_nvs_set_tzstring(const char* nvs_namespace, const char* nvs_key, const char* tzstring);
esp_err_t         timezone_nvs_apply_tzstring(const char* nvs_namespace, const char* nvs_key);