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
#include "esp_system.h"
#include "esp_wifi.h"
#include "gui_footer.h"
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

void menu_radio_update(pax_buf_t* buffer, gui_theme_t* theme) {
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
    pax_background(buffer, theme->palette.color_background);
    gui_render_header_adv(buffer, theme, ((gui_header_field_t[]){{get_icon(ICON_SYSTEM_UPDATE), "Radio update"}}), 1,
                          NULL, 0);
    gui_render_footer_adv(buffer, theme, NULL, 0, NULL, 0);
    display_blit_buffer(buffer);

    esp_wifi_stop();

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

    printf("Connecting to radio...\r\n");
    esp_log_level_set("et2", ESP_LOG_DEBUG);
    ESP_ERROR_CHECK(et2_setif_uart(UART_NUM_0));
    ESP_ERROR_CHECK(et2_sync());

    uint32_t chip_id;
    ESP_ERROR_CHECK(et2_detect(&chip_id));
    printf("Detected chip id: 0x%08" PRIx32 "\r\n", chip_id);

    ESP_ERROR_CHECK(et2_run_stub());

    FILE* fd = fopen("/sd/firmware/radio/esp-hosted.zz", "rb");
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open file: %s", strerror(errno));
        return;
    }

    fseek(fd, 0, SEEK_END);
    size_t compressed_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    uint8_t* data = malloc(4096);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for firmware data");
        fclose(fd);
        return;
    }

    uint32_t uncompressed_size = 1093760;

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
            free(data);
            fclose(fd);
            return;
        }
        printf("Writing %zu bytes to radio...\r\n", block_length);
        ESP_ERROR_CHECK(et2_cmd_deflate_data(data, block_length, seq));
        seq++;
        position += block_length;
    }

    free(data);
    fclose(fd);

    ESP_ERROR_CHECK(et2_cmd_deflate_finish(true));

    esp_restart();
#endif
}
