// SPDX-FileCopyrightText: 2025 Nicolai Electronics
// SPDX-License-Identifier: MIT

#include "coprocessor_management.h"
#include <stdbool.h>
#include "bsp/device.h"
#include "bsp/i2c.h"
#include "esp_log.h"
#include "icons.h"
#include "menu/message_dialog.h"

static const char* TAG = "Coprocessor management";

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)

#include "bsp/tanmatsu.h"
#include "rvswd_ch32v20x.h"
#include "tanmatsu_coprocessor.h"

extern uint8_t const coprocessor_firmware_start[] asm("_binary_tanmatsu_coprocessor_bin_start");
extern uint8_t const coprocessor_firmware_end[] asm("_binary_tanmatsu_coprocessor_bin_end");

static void callback(char const* status_text, uint8_t percentage) {
    static uint8_t last_percentage = 0xFF;
    if (percentage == last_percentage) {
        return;  // No change, no need to update
    }
    last_percentage = percentage;
    char text[128];
    sprintf(text, "%s (%u%%)", status_text, percentage);
    busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Coprocessor update", text, false);
}

void coprocessor_flash(bool force) {
    uint16_t coprocessor_firmware_target = 6;

    tanmatsu_coprocessor_handle_t coprocessor_handle = NULL;

    if (!force) {
        if (bsp_device_get_initialized_without_coprocessor()) {
            ESP_LOGI(TAG, "Coprocessor recovery required, installing firmware version %u\r\n",
                     coprocessor_firmware_target);
        } else {

            if (bsp_tanmatsu_coprocessor_get_handle(&coprocessor_handle) != ESP_OK) {
                ESP_LOGE(TAG, "Could not get coprocessor handle");
                // To-do: display error message
                while (1) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }

            uint16_t coprocessor_firmware_version;
            ESP_ERROR_CHECK(
                tanmatsu_coprocessor_get_firmware_version(coprocessor_handle, &coprocessor_firmware_version));

            if (coprocessor_firmware_version == coprocessor_firmware_target) {
                return;
            }

            ESP_LOGI(TAG, "Coprocessor update required, current version %u will be updated to version %u\r\n",
                     coprocessor_firmware_version, coprocessor_firmware_target);
        }
    }

    rvswd_handle_t handle = {
        .swdio = 22,
        .swclk = 23,
    };

    // To-do: display progress screen

    ch32v20x_program(&handle, coprocessor_firmware_start, coprocessor_firmware_end - coprocessor_firmware_start,
                     callback);

    vTaskDelay(pdMS_TO_TICKS(1000));

    i2c_master_bus_handle_t i2c_bus_handle_internal;
    ESP_ERROR_CHECK(bsp_i2c_primary_bus_get_handle(&i2c_bus_handle_internal));
    if (i2c_master_probe(i2c_bus_handle_internal, 0x5f, 50) != ESP_OK) {
        ESP_LOGE(TAG, "Coprocessor does not respond after flashing");
        // To-do: display error message
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    esp_restart();
}

#else

void coprocessor_flash(bool force) {
}

#endif
