#include "radio_ota.h"
#include <stdio.h>
#include <string.h>
#include "appfs.h"
#include "bsp/device.h"
#include "bsp/power.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "filesystem_utils.h"
#include "http_download.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "wifi_connection.h"

#ifdef CONFIG_BSP_TARGET_TANMATSU
#include "esptoolsquared.h"

static const char* TAG = "Radio OTA";

#define BSP_UART_TX_C6 53  // UART TX going to ESP32-C6
#define BSP_UART_RX_C6 54  // UART RX coming from ESP32-C6

extern bool wifi_stack_get_initialized(void);

typedef struct {
    uint8_t* compressed_data;
    size_t   compressed_size;
    size_t   uncompressed_size;
    uint32_t offset;
} ota_step_t;

static void download_callback(size_t download_position, size_t file_size, const char* status_text) {
    uint8_t        percentage      = 100 * download_position / file_size;
    static uint8_t last_percentage = 0;
    if (percentage == last_percentage) {
        return;  // No change, no need to update
    }
    last_percentage = percentage;
    char text[64];
    sprintf(text, "%s (%u%%)", status_text, percentage);
    busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", text, true);
};

static bool radio_prepare(void) {
    busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Stopping WiFi...", false);

    esp_wifi_stop();

    busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Starting updater...", false);

    printf("Install UART driver...\r\n");
    uart_driver_install(UART_NUM_0, 256, 256, 0, NULL, 0);
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, BSP_UART_TX_C6, BSP_UART_RX_C6, -1, -1));
    ESP_ERROR_CHECK(uart_set_baudrate(UART_NUM_0, 115200));

    printf("Switching radio off...\r\n");
    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
    vTaskDelay(pdMS_TO_TICKS(50));
    printf("Switching radio to bootloader mode...\r\n");
    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_BOOTLOADER);
    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_log_level_set("et2", ESP_LOG_DEBUG);
    ESP_ERROR_CHECK(et2_setif_uart(UART_NUM_0));

    busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Synchronizing with radio...", false);
    printf("Synchronizing with radio...\r\n");

    esp_err_t res = et2_sync();
    if (res != ESP_OK) {
        printf("Failed to sync with radio: %s\r\n", esp_err_to_name(res));
        busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Failed to sync with radio", false);
        vTaskDelay(pdMS_TO_TICKS(2000));
        return false;
    }

    busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Detecting radio...", false);
    printf("Detecting radio...\r\n");

    uint32_t chip_id;
    res = et2_detect(&chip_id);
    if (res != ESP_OK) {
        printf("Failed to detect radio chip: %s\r\n", esp_err_to_name(res));
        busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Failed to detect radio chip", false);
        vTaskDelay(pdMS_TO_TICKS(2000));
        return false;
    }
    busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Detected radio chip, starting stub...", false);
    printf("Detected chip id: 0x%08" PRIx32 "\r\n", chip_id);

    res = et2_run_stub();

    if (res != ESP_OK) {
        printf("Failed to run flashing stub: %s\r\n", esp_err_to_name(res));
        busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Failed to run flashing stub", false);
        vTaskDelay(pdMS_TO_TICKS(2000));
        return false;
    }

    return true;
}

esp_err_t radio_ota_update(void) {
    busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Connecting to WiFi...", true);

    if (!wifi_stack_get_initialized()) {
        ESP_LOGE(TAG, "WiFi stack not initialized");
        message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "WiFi stack not initialized", "Quit");
        return ESP_FAIL;
    }

    if (!wifi_connection_is_connected()) {
        if (wifi_connect_try_all() != ESP_OK) {
            ESP_LOGE(TAG, "Not connected to WiFi");
            message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Failed to connect to WiFi network", "Quit");
            return ESP_FAIL;
        }
    }

    busy_dialog(get_icon(ICON_REPOSITORY), "Radio update", "Starting download...", true);

    uint8_t* instructions_data = NULL;
    size_t   instructions_size = 0;
    bool     dl_res            = download_ram("https://ota.tanmatsu.cloud/radio/instructions.json", &instructions_data,
                                              &instructions_size, download_callback, "Downloading instructions...");
    if (!dl_res) {
        message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Failed to download instructions", "Quit");
        return ESP_FAIL;
    }

    cJSON* instructions_json = cJSON_ParseWithLength((const char*)instructions_data, instructions_size);

    ota_step_t steps[16]  = {0};
    int        step_count = cJSON_GetArraySize(instructions_json);

    for (int i = 0; i < step_count; i++) {
        cJSON* step_json           = cJSON_GetArrayItem(instructions_json, i);
        steps[i].offset            = (uint32_t)cJSON_GetObjectItem(step_json, "offset")->valueint;
        steps[i].uncompressed_size = (size_t)cJSON_GetObjectItem(step_json, "size")->valueint;
        const char* filename       = cJSON_GetObjectItem(step_json, "file")->valuestring;

        char url[256] = {0};
        sprintf(url, "https://ota.tanmatsu.cloud/radio/%s", filename);
        char message[256] = {0};
        sprintf(message, "Downloading firmware part %d of %d...", i + 1, step_count);

        dl_res = download_ram(url, &steps[i].compressed_data, &steps[i].compressed_size, download_callback, message);
        if (!dl_res) {
            for (int j = 0; j < i; j++) {
                free(steps[j].compressed_data);
            }
            cJSON_Delete(instructions_json);
            free(instructions_data);
            message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Failed to download radio data", "Quit");
            return ESP_FAIL;
        }
    }

    busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Disabling radio stack...", false);
    if (!radio_prepare()) {
        bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
        for (int j = 0; j < step_count; j++) {
            free(steps[j].compressed_data);
        }
        cJSON_Delete(instructions_json);
        free(instructions_data);
        message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Failed to connect to radio in bootloader mode",
                       "Quit");
        esp_restart();
        return ESP_FAIL;
    }

    busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Erasing...", false);

    esp_err_t res = et2_cmd_erase_flash();
    /*if (res != ESP_OK) {
        bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
        for (int j = 0; j < step_count; j++) {
            free(steps[j].compressed_data);
        }
        cJSON_Delete(instructions_json);
        free(instructions_data);
        message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Failed to erase flash", "Quit");
        return res;
    }*/

    for (int i = 0; i < step_count; i++) {
        busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Preparing...", false);
        while (et2_cmd_deflate_begin(steps[i].uncompressed_size, steps[i].compressed_size, steps[i].offset) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        /*if (res != ESP_OK) {
            bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
            for (int j = 0; j < i; j++) {
                free(steps[j].compressed_data);
            }
            cJSON_Delete(instructions_json);
            free(instructions_data);
            message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Failed to start flashing", "Quit");
            return res;
        }*/

        size_t   position = 0;
        uint32_t seq      = 0;
        while (position < steps[i].compressed_size) {
            size_t block_length = steps[i].compressed_size - position;
            if (block_length > 4096) {
                block_length = 4096;
            }

            char buffer[128] = {0};
            snprintf(buffer, sizeof(buffer), "Part %d of %d:\nWriting %zu bytes to radio (block %" PRIu32 ")...\r\n",
                     i + 1, step_count, block_length, seq);
            fputs(buffer, stdout);
            busy_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", buffer, false);
            while (et2_cmd_deflate_data(steps[i].compressed_data + position, block_length, seq) != ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            /*if (res != ESP_OK) {
                bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
                for (int j = 0; j < step_count; j++) {
                    free(steps[j].compressed_data);
                }
                cJSON_Delete(instructions_json);
                free(instructions_data);
                message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Failed to write data", "Quit");
                return ESP_FAIL;
            }*/
            seq++;
            position += block_length;
        }
        while (et2_cmd_deflate_finish(false) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        /*if (res != ESP_OK) {
            bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
            for (int j = 0; j < step_count; j++) {
                free(steps[j].compressed_data);
            }
            cJSON_Delete(instructions_json);
            free(instructions_data);
            message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Failed to finalize flashing", "Quit");
            return res;
        }*/
    }

    for (int j = 0; j < step_count; j++) {
        free(steps[j].compressed_data);
    }
    cJSON_Delete(instructions_json);
    free(instructions_data);
    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
    message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Radio update completed successfully!", "Quit");

    esp_restart();
    return ESP_OK;
}

#else

esp_err_t radio_ota_update(void) {
    message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Radio update", "Radio update not supported on this platform", "Quit");
    return ESP_FAIL;
}

#endif