#pragma once

typedef struct {
    float    frequency;         // Frequency in MHz
    uint16_t bandwidth;         // 7, 10,15, 20, 31, 41, 62, 125, 250, 500 kHz
    uint8_t  spreading_factor;  // 5-12
    uint8_t  coding_rate;       // 5-8 (4/5 to 4/8)
    uint8_t  power;             // TX Power in dBm
} mesh_lora_settings_t;

void menu_lora_settings(void);
