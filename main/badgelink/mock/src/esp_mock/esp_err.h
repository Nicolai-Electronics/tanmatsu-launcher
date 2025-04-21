
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t esp_err_t;

enum {
#define ESP_ERR_DEF(name) name,
#include "esp_err_defs.inc"
};

char const* esp_err_to_name(esp_err_t err);
