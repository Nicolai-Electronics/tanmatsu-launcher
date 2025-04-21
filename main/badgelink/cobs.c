
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include "cobs.h"
#include <string.h>

// Encode some binary data with COBS.
// Adds a null-terminator at the end of the output.
size_t cobs_encode(uint8_t* output, uint8_t const* input, size_t input_len) {
    bool   final_zero = true;
    size_t output_len = 0;
    size_t search_idx = 0;
    size_t i;

    for (i = 0; i < input_len; i++) {
        if (input[i] == 0) {
            // Zero byte found.
            final_zero           = true;
            output[output_len++] = i - search_idx + 1;
            memmove(output + output_len, input + search_idx, i - search_idx);
            output_len += i - search_idx;
            search_idx  = i + 1;
        } else if (i - search_idx == 253) {
            // Overhead byte needed.
            final_zero           = false;
            output[output_len++] = 255;
            memmove(output + output_len, input + search_idx, i - search_idx + 1);
            output_len += i - search_idx + 1;
            search_idx  = i + 1;
        }
    }

    if (i != search_idx || final_zero) {
        // No zero bytes until end of packet.
        output[output_len++] = i - search_idx + 1;
        memmove(output + output_len, input + search_idx, i - search_idx);
        output_len += i - search_idx;
    }

    output[output_len++] = 0;
    return output_len;
}

// Decode some binary data with COBS.
// Assumes the null-terminator is still present.
size_t cobs_decode(uint8_t* output, uint8_t const* input, size_t input_len) {
    if (input_len < 1) {
        return 0;
    }
    input_len--;

    size_t output_len = 0;
    size_t idx        = 0;
    while (1) {
        // Get length of non-zero byte span.
        uint8_t length = input[idx++];

        // Copy the non-zero bytes.
        memmove(output + output_len, input + idx, length - 1);
        output_len += length - 1;
        idx        += length - 1;
        if (idx >= input_len) {
            break;
        }
        if (length < 255) {
            // Add the zero byte at the end.
            output[output_len++] = 0;
        }
    }

    return output_len;
}
