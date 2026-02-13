// SPDX-FileCopyrightText: 2026 Nicolai Electronics
// SPDX-License-Identifier: MIT

#include "bootloader_update.h"
#include "bsp/power.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "menu/message_dialog.h"

static const char* TAG = "Bootloader update";

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)

extern uint8_t const bootloader_bin_start[] asm("_binary_p4_bootloader_bin_start");
extern uint8_t const bootloader_bin_end[] asm("_binary_p4_bootloader_bin_end");

static const uint32_t bootloader_partition_address = 0x2000;
static const uint32_t bootloader_partition_size    = 24576;  // 24KB

static esp_err_t verify_bootloader(bool* out_match) {
    uint8_t buffer[1024] = {0};

    uint32_t bootloader_size = bootloader_bin_end - bootloader_bin_start;

    uint32_t read_position = 0;
    while (read_position < bootloader_size) {
        uint32_t read_size = MIN(sizeof(buffer), bootloader_size - read_position);

        // Read
        esp_err_t res =
            esp_flash_read(esp_flash_default_chip, buffer, bootloader_partition_address + read_position, read_size);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read flash: %s", esp_err_to_name(res));
            return res;
        }

        // Compare

        if (memcmp(buffer, &bootloader_bin_start[read_position], read_size) != 0) {
            *out_match = false;
            return ESP_OK;
        }

        read_position += read_size;
    }

    *out_match = true;
    return ESP_OK;
}

void bootloader_update(void) {
    uint32_t bootloader_size = bootloader_bin_end - bootloader_bin_start;
    if (bootloader_size > bootloader_partition_size) {
        ESP_LOGE(TAG, "Bootloader binary size (%d bytes) exceeds partition size (%d bytes)", bootloader_size,
                 bootloader_partition_size);
        return;
    }

    esp_err_t res = esp_flash_init(esp_flash_default_chip);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize flash: %s", esp_err_to_name(res));
        adv_dialog_ok(NULL, "Bootloader update failed", "Failed to initialize flash");
        return;
    }

    bool match = false;
    res        = verify_bootloader(&match);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to check current bootloader: %s", esp_err_to_name(res));
        adv_dialog_ok(NULL, "Bootloader update failed", "Failed to check current bootloader");
        return;
    }

    if (match) {
        startup_dialog("Bootloader is up-to-date");
        ESP_LOGI(TAG, "Bootloader is up to date, no update needed");
        return;
    }

    startup_dialog("Updating bootloader...");

    ESP_LOGW(TAG, "Bootloader is outdated, updating bootloader...");

    // Erase bootloader
    res = esp_flash_erase_region(esp_flash_default_chip, bootloader_partition_address, bootloader_partition_size);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase flash: %s", esp_err_to_name(res));
        adv_dialog_ok(NULL, "Bootloader update failed",
                      "Failed to erase flash\r\nThis may have left the device in an unusable state, go to "
                      "https://recovery.tanmatsu.cloud for recovery instructions");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Write new bootloader
    res = esp_flash_write(esp_flash_default_chip, bootloader_bin_start, bootloader_partition_address, bootloader_size);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write flash: %s", esp_err_to_name(res));
        adv_dialog_ok(NULL, "Bootloader update failed",
                      "Failed to write flash\r\nThis may have left the device in an unusable state, go to "
                      "https://recovery.tanmatsu.cloud for recovery instructions");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGI(TAG, "Bootloader update successful");
    busy_dialog(NULL, "Bootloader update successful",
                "The bootloader has been updated successfully.\r\nDevice will restart now.", true);
    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}

#else

void bootloader_update(void) {
}

#endif
