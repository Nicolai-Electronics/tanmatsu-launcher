#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

// Sign data using the ECDSA (SECP256R1) key stored in eFuse block KEY0
// The signature buffer must be 64 bytes long, the signature is stored in raw (r || s) format
// Returns ESP_ERR_NOT_SUPPORTED when the chip does not support ECDSA
esp_err_t signature_sign(uint8_t* data, size_t data_length, uint8_t* signature);

// Read the public key belonging to the ECDSA (SECP256R1) key stored in eFuse block KEY0
// The public key buffer must be 65 bytes long, the key is stored in uncompressed (0x04 || X || Y) format
// Returns ESP_ERR_NOT_SUPPORTED when the chip does not support ECDSA
esp_err_t signature_read_public_key(uint8_t* public_key);
