#include "lora_settings_handler.h"
#include <stdint.h>
#include "device_settings.h"
#include "esp_err.h"
#include "lora.h"

esp_err_t lora_apply_settings(void) {
    lora_protocol_config_params_t config = {
        .frequency                  = 869618000,  // Hz
        .spreading_factor           = 8,          // SF8
        .bandwidth                  = 62,         // 62.5 kHz
        .coding_rate                = 8,          // 4/8
        .sync_word                  = 0x12,       // private
        .preamble_length            = 16,         // symbols
        .power                      = 22,         // +22 dBm
        .ramp_time                  = 40,         // us
        .crc_enabled                = true,       // CRC enabled
        .invert_iq                  = false,      // normal IQ
        .low_data_rate_optimization = false,      // disabled
    };

    uint32_t frequency = 0;
    device_settings_get_lora_frequency(&frequency);
    config.frequency = frequency;
    device_settings_get_lora_spreading_factor(&config.spreading_factor);
    uint16_t bandwidth = 0;
    device_settings_get_lora_bandwidth(&bandwidth);
    config.bandwidth = bandwidth;
    device_settings_get_lora_coding_rate(&config.coding_rate);
    device_settings_get_lora_power(&config.power);

    return lora_set_config(&config);
}