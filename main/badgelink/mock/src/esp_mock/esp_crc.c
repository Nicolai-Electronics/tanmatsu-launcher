
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include "esp_crc.h"
#include <zlib.h>

uint32_t esp_crc32_le(uint32_t init, void const* data, size_t len) {
    return crc32(init, data, len);
}
