#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char*    name;              // Name of the preset
    float    frequency;         // Frequency in MHz
    uint16_t bandwidth;         // 7, 10,15, 20, 31, 41, 62, 125, 250, 500 kHz
    uint8_t  spreading_factor;  // 5-12
    uint8_t  coding_rate;       // 5-8 (4/5 to 4/8)
    bool     low_frequency;     // True if the preset is for 433 MHz variant
} mesh_lora_preset_t;

extern const mesh_lora_preset_t lora_presets[];
extern const size_t             lora_presets_count;
