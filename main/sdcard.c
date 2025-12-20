#include "sdcard.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sd_pwr_ctrl.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"

sd_status_t status = SD_STATUS_NOT_PRESENT;

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
static char const    TAG[] = "sdcard";
sd_pwr_ctrl_handle_t initialize_sd_ldo(void) {
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = 4,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
    esp_err_t            res             = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return NULL;
    }
    // Don't set voltage here - let SDMMC driver set it via host.io_voltage (3.3V default)
    return pwr_ctrl_handle;
}

esp_err_t sd_mount(sd_pwr_ctrl_handle_t pwr_ctrl_handle) {
    esp_err_t res;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 10, .allocation_unit_size = 16 * 1024};

    sdmmc_card_t* card;
    const char    mount_point[] = "/sd";
    ESP_LOGI(TAG, "Initializing SD card");

    // Power cycle the SD card to ensure it's in a known state
    // This prevents issues when the card was left in SDMMC mode from a previous session
    if (pwr_ctrl_handle != NULL) {
        ESP_LOGI(TAG, "Power cycling SD card...");
        sd_pwr_ctrl_set_io_voltage(pwr_ctrl_handle, 0);      // Power off
        vTaskDelay(pdMS_TO_TICKS(150));                      // Wait 150ms
        sd_pwr_ctrl_set_io_voltage(pwr_ctrl_handle, 3300);   // Power on at 3.3V
        vTaskDelay(pdMS_TO_TICKS(150));                      // Wait 150ms for card to stabilize
        ESP_LOGI(TAG, "SD card power cycle complete");
    }

    sdmmc_host_t host    = SDMMC_HOST_DEFAULT();
    host.slot            = SDMMC_HOST_SLOT_0;     // Use SLOT0 for native IOMUX pins
    host.max_freq_khz    = SDMMC_FREQ_HIGHSPEED;  // 40MHz
    host.pwr_ctrl_handle = pwr_ctrl_handle;

    // Allocate DMA buffer in internal RAM to avoid PSRAM cache sync overhead
    static DRAM_DMA_ALIGNED_ATTR uint8_t dma_buf[512 * 4];  // 2KB aligned buffer
    host.dma_aligned_buffer = dma_buf;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk   = GPIO_NUM_43;
    slot_config.cmd   = GPIO_NUM_44;
    slot_config.d0    = GPIO_NUM_39;
    slot_config.d1    = GPIO_NUM_40;
    slot_config.d2    = GPIO_NUM_41;
    slot_config.d3    = GPIO_NUM_42;
    slot_config.width = 4;  // 4-bit mode
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting filesystem");
    res = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (res != ESP_OK) {
        if (res == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount SD card filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the SD card (%s). ", esp_err_to_name(res));
        }
        status = SD_STATUS_ERROR;
        return res;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    sdmmc_card_print_info(stdout, card);
    status = SD_STATUS_OK;
    return ESP_OK;
}

esp_err_t sd_mount_spi(sd_pwr_ctrl_handle_t pwr_ctrl_handle) {
    esp_err_t res;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 10, .allocation_unit_size = 16 * 1024};

    sdmmc_card_t* card;
    const char    mount_point[] = "/sd";
    ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_host_t host    = SDSPI_HOST_DEFAULT();
    host.max_freq_khz    = SDMMC_FREQ_HIGHSPEED;  // 40MHz for better throughput
    host.pwr_ctrl_handle = pwr_ctrl_handle;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = GPIO_NUM_44,
        .miso_io_num     = GPIO_NUM_39,
        .sclk_io_num     = GPIO_NUM_43,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4000,
    };

    res = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        status = SD_STATUS_ERROR;
        return res;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs               = GPIO_NUM_42;
    slot_config.host_id               = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    res = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (res != ESP_OK) {
        if (res == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount SD card filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the SD card (%s). ", esp_err_to_name(res));
        }
        status = SD_STATUS_ERROR;
        return res;
    }
    ESP_LOGI(TAG, "Filesystem mounted");
    status = SD_STATUS_OK;
    return ESP_OK;
}

#define SPEEDTEST_SIZE (20 * 1024 * 1024)  // 20MB
#define SPEEDTEST_CHUNK_SIZE (32 * 1024)  // 32KB chunks for efficient I/O

static void run_speedtest_posix(const char* label, uint8_t* buffer, size_t chunk_size) {
    const char* test_file = "/sd/test2.dat";
    int64_t start_time, end_time;
    size_t bytes_written = 0;
    size_t bytes_read = 0;

    // Fill buffer with pattern
    for (size_t i = 0; i < chunk_size; i++) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }

    // === Write Test ===
    ESP_LOGI(TAG, "Speedtest [%s POSIX]: Writing %d bytes", label, SPEEDTEST_SIZE);
    int fd = open(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        ESP_LOGE(TAG, "Speedtest: Failed to open file for writing");
        return;
    }

    start_time = esp_timer_get_time();
    while (bytes_written < SPEEDTEST_SIZE) {
        size_t to_write = SPEEDTEST_SIZE - bytes_written;
        if (to_write > chunk_size) {
            to_write = chunk_size;
        }
        ssize_t written = write(fd, buffer, to_write);
        if (written <= 0) {
            ESP_LOGE(TAG, "Speedtest: Write error");
            break;
        }
        bytes_written += written;
    }
    fsync(fd);
    close(fd);
    end_time = esp_timer_get_time();

    int64_t write_time_us = end_time - start_time;
    float write_speed_mbps = (float)bytes_written / (float)write_time_us;
    ESP_LOGI(TAG, "Speedtest [%s POSIX]: Write - %.2f MB/s", label, write_speed_mbps);

    // === Read Test ===
    fd = open(test_file, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Speedtest: Failed to open file for reading");
        return;
    }

    start_time = esp_timer_get_time();
    while (bytes_read < SPEEDTEST_SIZE) {
        size_t to_read = SPEEDTEST_SIZE - bytes_read;
        if (to_read > chunk_size) {
            to_read = chunk_size;
        }
        ssize_t read_count = read(fd, buffer, to_read);
        if (read_count <= 0) {
            if (read_count == 0) break;
            ESP_LOGE(TAG, "Speedtest: Read error");
            break;
        }
        bytes_read += read_count;
    }
    close(fd);
    end_time = esp_timer_get_time();

    int64_t read_time_us = end_time - start_time;
    float read_speed_mbps = (float)bytes_read / (float)read_time_us;
    ESP_LOGI(TAG, "Speedtest [%s POSIX]: Read - %.2f MB/s", label, read_speed_mbps);

    remove(test_file);
}

static void run_speedtest_stdio(void) {
    const char* test_file = "/sd/test2.dat";
    int64_t start_time, end_time;
    size_t bytes_written = 0;
    size_t bytes_read = 0;

    // Allocate internal DMA buffer for stdio
    uint8_t* stdio_buf = heap_caps_malloc(8192, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    uint8_t* buffer = heap_caps_malloc(1024, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (buffer == NULL || stdio_buf == NULL) {
        ESP_LOGW(TAG, "Could not allocate stdio test buffers");
        if (buffer) free(buffer);
        if (stdio_buf) free(stdio_buf);
        return;
    }

    for (size_t i = 0; i < 1024; i++) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }

    // === stdio Write Test with internal RAM buffer ===
    ESP_LOGI(TAG, "Speedtest [stdio+DMA]: Writing %d bytes", SPEEDTEST_SIZE);
    FILE* f = fopen(test_file, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Speedtest: Failed to open file");
        free(buffer);
        free(stdio_buf);
        return;
    }
    // Set internal RAM buffer for stdio
    setvbuf(f, (char*)stdio_buf, _IOFBF, 8192);

    start_time = esp_timer_get_time();
    while (bytes_written < SPEEDTEST_SIZE) {
        size_t written = fwrite(buffer, 1, 1024, f);
        if (written == 0) break;
        bytes_written += written;
    }
    fflush(f);
    fclose(f);
    end_time = esp_timer_get_time();

    float write_speed = (float)bytes_written / (float)(end_time - start_time);
    ESP_LOGI(TAG, "Speedtest [stdio+DMA]: Write - %.2f MB/s", write_speed);

    // === stdio Read Test with internal RAM buffer ===
    bytes_read = 0;
    f = fopen(test_file, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Speedtest: Failed to open file");
        free(buffer);
        free(stdio_buf);
        return;
    }
    setvbuf(f, (char*)stdio_buf, _IOFBF, 8192);

    start_time = esp_timer_get_time();
    while (bytes_read < SPEEDTEST_SIZE) {
        size_t rd = fread(buffer, 1, 1024, f);
        if (rd == 0) break;
        bytes_read += rd;
    }
    fclose(f);
    end_time = esp_timer_get_time();

    float read_speed = (float)bytes_read / (float)(end_time - start_time);
    ESP_LOGI(TAG, "Speedtest [stdio+DMA]: Read - %.2f MB/s", read_speed);

    free(buffer);
    free(stdio_buf);
    remove(test_file);
}

void sd_speedtest(void) {
    ESP_LOGI(TAG, "=== SD Card Speed Test ===");

    // Test POSIX with 8KB DMA buffer (optimal from previous tests)
    uint8_t* dma_buffer = heap_caps_malloc(8192, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (dma_buffer != NULL) {
        run_speedtest_posix("8KB DMA", dma_buffer, 8192);
        free(dma_buffer);
    } else {
        ESP_LOGW(TAG, "Could not allocate DMA buffer");
    }

    // Test stdio with small app buffer (relies on FATFS internal 8KB buffer)
    run_speedtest_stdio();

    ESP_LOGI(TAG, "=== Speed Test Complete ===");
}

void test_sd(void) {
    DIR* dir = opendir("/sd/apps");
    if (dir == NULL) {
        ESP_LOGW(TAG, "Directory not found");
        return;
    }
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_REG) continue;  // Skip files, only parse directories
        printf("Directory: %s\r\n", ent->d_name);
    }
    closedir(dir);
}
#endif

sd_status_t sd_status(void) {
    return status;
}