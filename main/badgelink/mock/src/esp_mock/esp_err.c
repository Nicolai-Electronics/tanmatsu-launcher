
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include "esp_err.h"

static char const* const esp_err_tab[] = {
#define ESP_ERR_DEF(name) [name] = #name,
#include "esp_err_defs.inc"
};

char const* esp_err_to_name(esp_err_t err) {
    if (err > sizeof(esp_err_tab) / sizeof(char const*)) {
        return NULL;
    } else {
        return esp_err_tab[err];
    }
}
