#include "lora_settings_handler.h"
#include <stdint.h>
#include "device_settings.h"
#include "esp_err.h"
#include "lora.h"
#include "nvs_settings.h"

extern lora_handle_t* lora_get_handle(void);

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
        .rx_boost                   = true,       // enabled
        .use_dcdc                   = true,       // enabled
        .use_automatic_correction   = false,      // disabled
    };

    uint32_t frequency = 0;
    device_settings_get_lora_frequency(&frequency);
    config.frequency = frequency;
    nvs_settings_get_lora_spreading_factor(&config.spreading_factor);
    uint16_t bandwidth = 0;
    nvs_settings_get_lora_bandwidth(&bandwidth);
    config.bandwidth = bandwidth;
    nvs_settings_get_lora_coding_rate(&config.coding_rate);
    nvs_settings_get_lora_power(&config.power);
    nvs_settings_get_lora_automatic_offset(&config.use_automatic_correction);
    nvs_settings_get_lora_low_data_rate_optimization(&config.low_data_rate_optimization);

    esp_err_t res = lora_set_config(lora_get_handle(), &config);
    if (res != ESP_OK) {
        return res;
    }

    return res;

    int32_t offset = 0;
    nvs_settings_get_lora_offset(&offset);
    return lora_set_frequency_offset(lora_get_handle(), offset);
}
