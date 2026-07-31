#include "sdcard.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "sd_pwr_ctrl.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"

sd_status_t status = SD_STATUS_NOT_PRESENT;

static sdmmc_card_t*        card          = NULL;
static const char           mount_point[] = "/sd";
static sd_pwr_ctrl_handle_t sd_pwr_handle = NULL;

#if defined(CONFIG_BSP_TARGET_TANMATSU)
static char const TAG[] = "sdcard";

#if defined(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE) && (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
#define SDCARD_WORKAROUND_HOSTED_DOES_SDMMC_HOST_INIT 1
#else
#define SDCARD_WORKAROUND_HOSTED_DOES_SDMMC_HOST_INIT 0
#endif

#if SDCARD_WORKAROUND_HOSTED_DOES_SDMMC_HOST_INIT
static esp_err_t sdmmc_host_init_noop(void) {
    return ESP_OK;
}

static esp_err_t sdmmc_host_deinit_noop(void) {
    return ESP_OK;
}
#endif

static sd_pwr_ctrl_handle_t initialize_sd_ldo(void) {
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

static esp_err_t reset_sd_card(void) {
    if (sd_pwr_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Power cycling SD card...");

    // Pull all SDIO bus lines low
    gpio_config_t gpio_cfg = {
        .pin_bit_mask = BIT64(GPIO_NUM_39) | BIT64(GPIO_NUM_40) | BIT64(GPIO_NUM_41) | BIT64(GPIO_NUM_42) |
                        BIT64(GPIO_NUM_43) | BIT64(GPIO_NUM_44),
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_cfg);
    gpio_set_level(GPIO_NUM_39, 0);
    gpio_set_level(GPIO_NUM_40, 0);
    gpio_set_level(GPIO_NUM_41, 0);
    gpio_set_level(GPIO_NUM_42, 0);
    gpio_set_level(GPIO_NUM_43, 0);
    gpio_set_level(GPIO_NUM_44, 0);

    // Decrease LDO output voltage to minimum
    sd_pwr_ctrl_set_io_voltage(sd_pwr_handle, 0);
    vTaskDelay(pdMS_TO_TICKS(150));  // Wait 150ms for card to power down

    // Power on the SD card at 3.3V & release GPIOs
    gpio_cfg.mode = GPIO_MODE_INPUT;
    gpio_config(&gpio_cfg);
    sd_pwr_ctrl_set_io_voltage(sd_pwr_handle, 3300);
    vTaskDelay(pdMS_TO_TICKS(150));  // Wait 150ms for card to stabilize
    return ESP_OK;
}

esp_err_t sd_mount(void) {
    esp_err_t res;

    if (card != NULL) {
        ESP_LOGI(TAG, "SD card already mounted");
        return ESP_OK;
    }

    if (sd_pwr_handle == NULL) {
        ESP_LOGI(TAG, "Acquiring SD LDO power control handle");
        sd_pwr_handle = initialize_sd_ldo();
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 10, .allocation_unit_size = 16 * 1024};

    ESP_LOGI(TAG, "Initializing SD card");

    // Power cycle the SD card to ensure it's in a known state
    // This prevents issues when the card was left in SDMMC mode from a previous session
    res = reset_sd_card();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset SD card");
        return res;
    }

    sdmmc_host_t host    = SDMMC_HOST_DEFAULT();
    host.slot            = SDMMC_HOST_SLOT_0;     // Use SLOT0 for native IOMUX pins
    host.max_freq_khz    = SDMMC_FREQ_HIGHSPEED;  // 40MHz
    host.pwr_ctrl_handle = sd_pwr_handle;
#if SDCARD_WORKAROUND_HOSTED_DOES_SDMMC_HOST_INIT
    // ESP-Hosted already owns the shared SDMMC host controller.
    host.init   = &sdmmc_host_init_noop;
    host.deinit = &sdmmc_host_deinit_noop;
#endif

    static uint8_t* dma_buf = NULL;  // 2KB aligned buffer, allocated once and reused
    if (dma_buf == NULL) {
        dma_buf = heap_caps_malloc(512 * 4, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (dma_buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate DMA buffer for SD card");
            status = SD_STATUS_ERROR;
            return ESP_ERR_NO_MEM;
        }
    }
    host.dma_aligned_buffer = dma_buf;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk                 = GPIO_NUM_43;
    slot_config.cmd                 = GPIO_NUM_44;
    slot_config.d0                  = GPIO_NUM_39;
    slot_config.d1                  = GPIO_NUM_40;
    slot_config.d2                  = GPIO_NUM_41;
    slot_config.d3                  = GPIO_NUM_42;
    slot_config.width               = 4;  // 4-bit mode
    // slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

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

esp_err_t sd_unmount(void) {
    if (card == NULL) {
        ESP_LOGI(TAG, "SD card already unmounted");
        return ESP_OK;
    }
    esp_err_t res = esp_vfs_fat_sdcard_unmount(mount_point, card);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "SD card unmount returned error: %s", esp_err_to_name(res));
    }
    card   = NULL;
    status = SD_STATUS_NOT_PRESENT;
    ESP_LOGI(TAG, "SD card unmounted");
    return ESP_OK;
}

#elif defined(CONFIG_BSP_TARGET_ESP32_S31_KORVO_1)

static char const TAG[] = "sdcard";

static esp_err_t reset_sd_card(void) {
    ESP_LOGI(TAG, "Power cycling SD card...");

    // Pull all SDIO bus lines low
    gpio_config_t gpio_cfg = {
        .pin_bit_mask = BIT64(GPIO_NUM_20) | BIT64(GPIO_NUM_21) | BIT64(GPIO_NUM_22) | BIT64(GPIO_NUM_23) |
                        BIT64(GPIO_NUM_24) | BIT64(GPIO_NUM_25),
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_cfg);
    gpio_set_level(GPIO_NUM_20, 1);
    gpio_set_level(GPIO_NUM_21, 1);
    gpio_set_level(GPIO_NUM_22, 1);
    gpio_set_level(GPIO_NUM_23, 1);
    gpio_set_level(GPIO_NUM_24, 1);
    gpio_set_level(GPIO_NUM_25, 1);

    gpio_set_level(GPIO_NUM_39, 1);
    vTaskDelay(pdMS_TO_TICKS(150));  // Wait 150ms for card to power down
    gpio_set_level(GPIO_NUM_39, 0);
    vTaskDelay(pdMS_TO_TICKS(150));  // Wait 150ms for card to stabilize
    return ESP_OK;
}

esp_err_t sd_mount(void) {
    esp_err_t res;

    if (card != NULL) {
        ESP_LOGI(TAG, "SD card already mounted");
        return ESP_OK;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 10, .allocation_unit_size = 16 * 1024};

    ESP_LOGI(TAG, "Initializing SD card");

    gpio_config_t gpio_cfg = {
        .pin_bit_mask = BIT64(GPIO_NUM_39),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_cfg);

    // Power cycle the SD card to ensure it's in a known state
    // This prevents issues when the card was left in SDMMC mode from a previous session
    res = reset_sd_card();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset SD card");
        return res;
    }

    sdmmc_host_t host    = SDMMC_HOST_DEFAULT();
    host.slot            = SDMMC_HOST_SLOT_0;     // Use SLOT0 for native IOMUX pins
    host.max_freq_khz    = SDMMC_FREQ_HIGHSPEED;  // 40MHz

    static uint8_t* dma_buf = NULL;  // 2KB aligned buffer, allocated once and reused
    if (dma_buf == NULL) {
        dma_buf = heap_caps_malloc(512 * 4, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (dma_buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate DMA buffer for SD card");
            status = SD_STATUS_ERROR;
            return ESP_ERR_NO_MEM;
        }
    }
    host.dma_aligned_buffer = dma_buf;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk                 = GPIO_NUM_24;
    slot_config.cmd                 = GPIO_NUM_25;
    slot_config.d0                  = GPIO_NUM_20;
    slot_config.d1                  = GPIO_NUM_21;
    slot_config.d2                  = GPIO_NUM_22;
    slot_config.d3                  = GPIO_NUM_23;
    slot_config.width               = 4;  // 4-bit mode
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

esp_err_t sd_unmount(void) {
    if (card == NULL) {
        ESP_LOGI(TAG, "SD card already unmounted");
        return ESP_OK;
    }
    esp_err_t res = esp_vfs_fat_sdcard_unmount(mount_point, card);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "SD card unmount returned error: %s", esp_err_to_name(res));
    }
    card   = NULL;
    status = SD_STATUS_NOT_PRESENT;
    ESP_LOGI(TAG, "SD card unmounted");
    return ESP_OK;
}

#else

esp_err_t sd_mount(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sd_unmount(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

#endif

sd_status_t sd_status(void) {
    return status;
}
