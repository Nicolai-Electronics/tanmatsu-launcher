
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Calculates how long some binary data could maximally be when COBS-encoded.
#define COBS_ENCODED_MAX_LENGTH(len) ((len) + (((len) + 253) / 254) + 1)

// Calculates how long some binary data could maximally be when COBS-decoded.
#define COBS_DECODED_MAX_LENGTH(len) ((len) - 1)

// Encode some binary data with COBS.
// Adds a null-terminator at the end of the output.
size_t cobs_encode(uint8_t* output, uint8_t const* input, size_t input_len);

// Decode some binary data with COBS.
// Assumes the null-terminator is still present.
size_t cobs_decode(uint8_t* output, uint8_t const* input, size_t input_len);

#ifdef __cplusplus
}
#endif
