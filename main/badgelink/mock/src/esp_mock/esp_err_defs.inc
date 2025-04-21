
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#ifndef ESP_ERR_DEF
#define ESP_ERR_DEF(name)
#endif

// Must be first so it's zero.
ESP_ERR_DEF(ESP_OK)

// Common errors.
ESP_ERR_DEF(ESP_FAIL)
ESP_ERR_DEF(ESP_ERR_NO_MEM)
ESP_ERR_DEF(ESP_ERR_NOT_FOUND)

// NVS errors.
ESP_ERR_DEF(ESP_ERR_NVS_NOT_FOUND)

#undef ESP_ERR_DEF
