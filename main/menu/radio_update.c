#include "radio_update.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "bsp/power.h"
#include "chakrapetchmedium.h"
#include "common/display.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "esp_wifi.h"
#include "freertos/idf_additions.h"
#include "gui_element_footer.h"
#include "gui_element_header.h"
#include "gui_style.h"
#include "icons.h"
#include "pax_gfx.h"
#include "pax_types.h"

#ifdef CONFIG_IDF_TARGET_ESP32P4
#include "esptoolsquared.h"
#endif

static const char* TAG = "radio_update";

#define BSP_UART_TX_C6 53  // UART TX going to ESP32-C6
#define BSP_UART_RX_C6 54  // UART RX coming from ESP32-C6

static void radio_update_callback(const char* status_text) {
    printf("Radio update status changed: %s\r\n", status_text);
    pax_buf_t* fb = display_get_buffer();
    pax_draw_rect(fb, 0xFFEEEAEE, 0, 65, fb->width, 32);
    pax_draw_text(fb, 0xFF340132, &chakrapetchmedium, 16, 20, 70, status_text);
    display_blit_buffer(fb);
}

void radio_update(pax_buf_t* buffer, gui_theme_t* theme, char* path, bool compressed, uint32_t uncompressed_size) {
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
    pax_background(buffer, theme->palette.color_background);
    gui_header_draw(buffer, theme, ((gui_element_icontext_t[]){{get_icon(ICON_SYSTEM_UPDATE), "Radio update"}}), 1,
                    NULL, 0);
    gui_footer_draw(buffer, theme, NULL, 0, NULL, 0);
    display_blit_buffer(buffer);

    radio_update_callback("Stopping WiFi...");

    esp_wifi_stop();

    radio_update_callback("Starting updater...");

    printf("Install UART driver...\r\n");
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 256, 0, NULL, 0));
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

    radio_update_callback("Synchronizing with radio...");
    printf("Synchronizing with radio...\r\n");

    esp_err_t res = et2_sync();
    if (res != ESP_OK) {
        printf("Failed to sync with radio: %s\r\n", esp_err_to_name(res));
        radio_update_callback("Failed to sync with radio");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    radio_update_callback("Detecting radio...");
    printf("Detecting radio...\r\n");

    uint32_t chip_id;
    res = et2_detect(&chip_id);
    if (res != ESP_OK) {
        printf("Failed to detect radio chip: %s\r\n", esp_err_to_name(res));
        radio_update_callback("Failed to detect radio chip");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }
    radio_update_callback("Detected radio chip, starting stub...");
    printf("Detected chip id: 0x%08" PRIx32 "\r\n", chip_id);

    res = et2_run_stub();

    if (res != ESP_OK) {
        printf("Failed to run flashing stub: %s\r\n", esp_err_to_name(res));
        radio_update_callback("Failed to run flashing stub");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    radio_update_callback("Opening update file...");
    printf("Opening update file...\r\n");

    FILE* fd = fopen(path, "rb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", strerror(errno));
        radio_update_callback("Failed to open firmware file");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    fseek(fd, 0, SEEK_END);
    size_t compressed_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    uint8_t* data = malloc(4096);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for firmware data");
        radio_update_callback("Failed to allocate memory");
        vTaskDelay(pdMS_TO_TICKS(2000));
        fclose(fd);
        return;
    }

    if (compressed) {
        ESP_ERROR_CHECK(et2_cmd_deflate_begin(uncompressed_size, compressed_size, 0x10000));

        size_t   position = 0;
        uint32_t seq      = 0;
        while (position < compressed_size) {
            size_t block_length = compressed_size - position;
            if (block_length > 4096) {
                block_length = 4096;
            }

            size_t read_bytes = fread(data, 1, block_length, fd);
            if (read_bytes != block_length) {
                ESP_LOGE(TAG, "Failed to read firmware data: %s", strerror(errno));
                radio_update_callback("Failed to read firmware data");
                vTaskDelay(pdMS_TO_TICKS(2000));
                free(data);
                fclose(fd);
                return;
            }
            char buffer[128] = {0};
            snprintf(buffer, sizeof(buffer), "Writing %zu bytes to radio (block %" PRIu32 ")...\r\n", block_length,
                     seq);
            fputs(buffer, stdout);
            radio_update_callback(buffer);
            res = et2_cmd_deflate_data(data, block_length, seq);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write data to radio: %s", esp_err_to_name(res));
                radio_update_callback("Failed to write data to radio");
                vTaskDelay(pdMS_TO_TICKS(2000));
                free(data);
                fclose(fd);
                return;
            }
            seq++;
            position += block_length;
        }
        ESP_ERROR_CHECK(et2_cmd_deflate_finish(true));
    } else {
        ESP_ERROR_CHECK(et2_cmd_flash_begin(compressed_size, 0x10000));

        size_t   position = 0;
        uint32_t seq      = 0;
        while (position < compressed_size) {
            size_t block_length = compressed_size - position;
            if (block_length > 4096) {
                block_length = 4096;
            }

            size_t read_bytes = fread(data, 1, block_length, fd);
            if (read_bytes != block_length) {
                ESP_LOGE(TAG, "Failed to read firmware data: %s", strerror(errno));
                radio_update_callback("Failed to read firmware data");
                vTaskDelay(pdMS_TO_TICKS(2000));
                free(data);
                fclose(fd);
                return;
            }
            char buffer[128] = {0};
            snprintf(buffer, sizeof(buffer), "Writing %zu bytes to radio (block %" PRIu32 ")...\r\n", block_length,
                     seq);
            fputs(buffer, stdout);
            radio_update_callback(buffer);
            res = et2_cmd_flash_data(data, block_length, seq);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write data to radio: %s", esp_err_to_name(res));
                radio_update_callback("Failed to write data to radio");
                vTaskDelay(pdMS_TO_TICKS(2000));
                free(data);
                fclose(fd);
                return;
            }
            seq++;
            position += block_length;
        }
        ESP_ERROR_CHECK(et2_cmd_flash_finish(true));
    }

    free(data);
    fclose(fd);
#endif
}
