#include "radio_update.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "bsp/power.h"
#include "cJSON.h"
#include "chakrapetchmedium.h"
#include "common/display.h"
#include "common/theme.h"
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
    pax_draw_rect(fb, 0xFFEEEAEE, 0, 65, pax_buf_get_width(fb), 32);
    pax_draw_text(fb, 0xFF340132, &chakrapetchmedium, 16, 20, 70, status_text);
    display_blit_buffer(fb);
}

static bool radio_prepare(void) {
    radio_update_callback("Stopping WiFi...");

    esp_wifi_stop();

    radio_update_callback("Starting updater...");

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

    radio_update_callback("Synchronizing with radio...");
    printf("Synchronizing with radio...\r\n");

    esp_err_t res = et2_sync();
    if (res != ESP_OK) {
        printf("Failed to sync with radio: %s\r\n", esp_err_to_name(res));
        radio_update_callback("Failed to sync with radio");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return false;
    }

    radio_update_callback("Detecting radio...");
    printf("Detecting radio...\r\n");

    uint32_t chip_id;
    res = et2_detect(&chip_id);
    if (res != ESP_OK) {
        printf("Failed to detect radio chip: %s\r\n", esp_err_to_name(res));
        radio_update_callback("Failed to detect radio chip");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return false;
    }
    radio_update_callback("Detected radio chip, starting stub...");
    printf("Detected chip id: 0x%08" PRIx32 "\r\n", chip_id);

    res = et2_run_stub();

    if (res != ESP_OK) {
        printf("Failed to run flashing stub: %s\r\n", esp_err_to_name(res));
        radio_update_callback("Failed to run flashing stub");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return false;
    }

    return true;
}

static esp_err_t radio_install_compressed(const char* path, size_t uncompressed_size, size_t offset) {
    radio_update_callback("Opening update file...");

    FILE* fd = fopen(path, "rb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", strerror(errno));
        radio_update_callback("Failed to open firmware file");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return ESP_FAIL;
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
        return ESP_FAIL;
    }

    esp_err_t res = et2_cmd_deflate_begin(uncompressed_size, compressed_size, offset);
    if (res != ESP_OK) {
        radio_update_callback("Failed to start transfer");
        vTaskDelay(pdMS_TO_TICKS(2000));
        free(data);
        fclose(fd);
        return res;
    }
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
            return ESP_FAIL;
        }
        char buffer[128] = {0};
        snprintf(buffer, sizeof(buffer), "Writing %zu bytes to radio (block %" PRIu32 ")...\r\n", block_length, seq);
        fputs(buffer, stdout);
        radio_update_callback(buffer);
        esp_err_t res = et2_cmd_deflate_data(data, block_length, seq);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write data to radio: %s", esp_err_to_name(res));
            radio_update_callback("Failed to write data to radio");
            vTaskDelay(pdMS_TO_TICKS(2000));
            free(data);
            fclose(fd);
            return ESP_FAIL;
        }
        seq++;
        position += block_length;
    }
    res = et2_cmd_deflate_finish(false);

    free(data);
    fclose(fd);

    return res;
}

static esp_err_t radio_install_raw(const char* path, size_t offset) {
    radio_update_callback("Opening update file...");
    printf("Opening update file...\r\n");

    FILE* fd = fopen(path, "rb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", strerror(errno));
        radio_update_callback("Failed to open firmware file");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return ESP_FAIL;
    }

    fseek(fd, 0, SEEK_END);
    size_t file_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    uint8_t* data = malloc(4096);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for firmware data");
        radio_update_callback("Failed to allocate memory");
        vTaskDelay(pdMS_TO_TICKS(2000));
        fclose(fd);
        return ESP_FAIL;
    }

    esp_err_t res = et2_cmd_flash_begin(file_size, offset);
    if (res != ESP_OK) {
        radio_update_callback("Failed to start transfer");
        vTaskDelay(pdMS_TO_TICKS(2000));
        free(data);
        fclose(fd);
        return res;
    }

    size_t   position = 0;
    uint32_t seq      = 0;
    while (position < file_size) {
        size_t block_length = file_size - position;
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
            return ESP_FAIL;
        }
        char buffer[128] = {0};
        snprintf(buffer, sizeof(buffer), "Writing %zu bytes to radio (block %" PRIu32 ")...\r\n", block_length, seq);
        fputs(buffer, stdout);
        radio_update_callback(buffer);
        esp_err_t res = et2_cmd_flash_data(data, block_length, seq);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write data to radio: %s", esp_err_to_name(res));
            radio_update_callback("Failed to write data to radio");
            vTaskDelay(pdMS_TO_TICKS(2000));
            free(data);
            fclose(fd);
            return ESP_FAIL;
        }
        seq++;
        position += block_length;
    }

    res = et2_cmd_flash_finish(false);

    free(data);
    fclose(fd);

    return res;
}

void radio_update(char* path, bool compressed, uint32_t uncompressed_size) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
    pax_background(buffer, theme->palette.color_background);
    gui_header_draw(buffer, theme, ((gui_element_icontext_t[]){{get_icon(ICON_SYSTEM_UPDATE), "Radio update"}}), 1,
                    NULL, 0);
    gui_footer_draw(buffer, theme, NULL, 0, NULL, 0);
    display_blit_buffer(buffer);

    if (!radio_prepare()) {
        return;
    }

    esp_err_t res;

    if (compressed) {
        radio_update_callback("Installing compressed firmware...");
        printf("Installing compressed firmware...\r\n");
        res = radio_install_compressed(path, uncompressed_size, 65536);
    } else {
        radio_update_callback("Installing raw firmware...");
        printf("Installing raw firmware...\r\n");
        res = radio_install_raw(path, 65536);
    }

    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install radio firmware: %s", esp_err_to_name(res));
        radio_update_callback("Failed to install radio firmware");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    radio_update_callback("Radio firmware installed successfully");
    printf("Radio firmware installed successfully\r\n");

    vTaskDelay(pdMS_TO_TICKS(1000));
#endif
}

void radio_install(const char* instructions_filename) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
    pax_background(buffer, theme->palette.color_background);
    gui_header_draw(buffer, theme,
                    ((gui_element_icontext_t[]){{get_icon(ICON_SYSTEM_UPDATE), "Radio firmware installation"}}), 1,
                    NULL, 0);
    gui_footer_draw(buffer, theme, NULL, 0, NULL, 0);
    display_blit_buffer(buffer);

    radio_update_callback("Connecting to radio...");
    if (!radio_prepare()) {
        radio_update_callback("Failed to initialize radio");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    radio_update_callback("Reading instructions...");
    FILE* fd = fopen(instructions_filename, "rb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", strerror(errno));
        radio_update_callback("Failed to open instructions");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    fseek(fd, 0, SEEK_END);
    size_t instructions_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    char* instructions_data = heap_caps_malloc(instructions_size, MALLOC_CAP_SPIRAM);
    if (instructions_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for instructions");
        radio_update_callback("Failed to allocate memory");
        vTaskDelay(pdMS_TO_TICKS(2000));
        fclose(fd);
        return;
    }
    size_t read_bytes = fread(instructions_data, 1, instructions_size, fd);
    if (read_bytes != instructions_size) {
        ESP_LOGE(TAG, "Failed to read instructions data: %s", strerror(errno));
        radio_update_callback("Failed to read instructions data");
        vTaskDelay(pdMS_TO_TICKS(2000));
        free(instructions_data);
        fclose(fd);
        return;
    }
    fclose(fd);

    cJSON* instructions_json = cJSON_ParseWithLength(instructions_data, instructions_size);

    cJSON* information = cJSON_GetObjectItem(instructions_json, "information");
    if (information == NULL || !cJSON_IsObject(information)) {
        ESP_LOGE(TAG, "No information found in instructions");
        radio_update_callback("No information found in instructions");
        vTaskDelay(pdMS_TO_TICKS(2000));
        cJSON_Delete(instructions_json);
        free(instructions_data);
        return;
    }

    cJSON* steps = cJSON_GetObjectItem(instructions_json, "steps");
    if (steps == NULL || !cJSON_IsArray(steps)) {
        ESP_LOGE(TAG, "No steps found in instructions");
        radio_update_callback("No steps found in instructions");
        vTaskDelay(pdMS_TO_TICKS(2000));
        cJSON_Delete(instructions_json);
        free(instructions_data);
        return;
    }

    radio_update_callback("Erasing radio flash...");
    esp_err_t res = et2_cmd_erase_flash();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase flash: %s", esp_err_to_name(res));
        radio_update_callback("Failed to erase flash");
        vTaskDelay(pdMS_TO_TICKS(2000));
        cJSON_Delete(instructions_json);
        free(instructions_data);
        return;
    }

    radio_update_callback("Installing radio firmware...");
    cJSON* step;
    cJSON_ArrayForEach(step, steps) {
        cJSON* file_obj       = cJSON_GetObjectItem(step, "file");
        cJSON* offset_obj     = cJSON_GetObjectItem(step, "offset");
        cJSON* hash_obj       = cJSON_GetObjectItem(step, "hash");
        cJSON* size_obj       = cJSON_GetObjectItem(step, "size");
        cJSON* compressed_obj = cJSON_GetObjectItem(step, "compressed");

        if (file_obj == NULL || !cJSON_IsString(file_obj) || offset_obj == NULL || !cJSON_IsNumber(offset_obj) ||
            hash_obj == NULL || !cJSON_IsString(hash_obj) || size_obj == NULL || !cJSON_IsNumber(size_obj) ||
            compressed_obj == NULL || !cJSON_IsBool(compressed_obj)) {
            ESP_LOGE(TAG, "Invalid step format");
            radio_update_callback("Invalid step format");
            vTaskDelay(pdMS_TO_TICKS(2000));
            cJSON_Delete(instructions_json);
            free(instructions_data);
            return;
        }

        const char* file_path  = file_obj->valuestring;
        uint32_t    offset     = (uint32_t)offset_obj->valueint;
        const char* hash       = hash_obj->valuestring;
        uint32_t    size       = (uint32_t)size_obj->valueint;
        bool        compressed = cJSON_IsTrue(compressed_obj);

        char text_buffer[256];
        snprintf(text_buffer, sizeof(text_buffer),
                 "Installing step: %s (offset: %" PRIu32 ", size: %" PRIu32 " compressed: %s)", file_path, offset, size,
                 compressed ? "yes" : "no");
        radio_update_callback(text_buffer);
        printf("%s\r\n", text_buffer);

        const char* last_slash = strrchr(instructions_filename, '/');
        size_t      dir_len    = last_slash - instructions_filename + 1;
        snprintf(text_buffer, sizeof(text_buffer), "%.*s/%s", (int)dir_len, instructions_filename, file_path);

        if (compressed) {
            res = radio_install_compressed(text_buffer, size, offset);
        } else {
            res = radio_install_raw(text_buffer, offset);
        }

        if (res != ESP_OK) {
            sprintf(text_buffer, "Failed to install step: %s", esp_err_to_name(res));
            radio_update_callback(text_buffer);
            vTaskDelay(pdMS_TO_TICKS(2000));
            cJSON_Delete(instructions_json);
            free(instructions_data);
            return;
        }
    }

    cJSON_Delete(instructions_json);
    free(instructions_data);

    radio_update_callback("Radio firmware installed successfully");
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif
}
