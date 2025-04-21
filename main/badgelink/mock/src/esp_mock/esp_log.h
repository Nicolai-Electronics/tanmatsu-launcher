
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include <inttypes.h>
#include <stdio.h>

#define ESP_LOGI(tag, ...) mock__log(MOCK__LOGLEVEL_INFO, tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) mock__log(MOCK__LOGLEVEL_WARN, tag, __VA_ARGS__)
#define ESP_LOGE(tag, ...) mock__log(MOCK__LOGLEVEL_ERROR, tag, __VA_ARGS__)
#define ESP_LOGD(tag, ...) mock__log(MOCK__LOGLEVEL_DEBUG, tag, __VA_ARGS__)

typedef enum {
    MOCK__LOGLEVEL_INFO,
    MOCK__LOGLEVEL_WARN,
    MOCK__LOGLEVEL_ERROR,
    MOCK__LOGLEVEL_DEBUG,
} mock__loglevel_t;

__attribute__((format(printf, 3, 4))) void mock__log(mock__loglevel_t level, char const* tag, char const* fmt, ...);
