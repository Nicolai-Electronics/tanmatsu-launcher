#include "sdcard.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"

sd_status_t status = SD_STATUS_NOT_PRESENT;

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
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
    sd_pwr_ctrl_set_io_voltage(pwr_ctrl_handle, 3300);
    return pwr_ctrl_handle;
}

esp_err_t sd_mount(sd_pwr_ctrl_handle_t pwr_ctrl_handle) {
    esp_err_t res;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16 * 1024};

    sdmmc_card_t* card;
    const char    mount_point[] = "/sd";
    ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_host_t host    = SDMMC_HOST_DEFAULT();
    host.pwr_ctrl_handle = pwr_ctrl_handle;

    sdmmc_slot_config_t slot_config = {
        .clk   = GPIO_NUM_43,
        .cmd   = GPIO_NUM_44,
        .d0    = GPIO_NUM_39,
        .d1    = GPIO_NUM_40,
        .d2    = GPIO_NUM_41,
        .d3    = GPIO_NUM_42,
        .d4    = GPIO_NUM_NC,
        .d5    = GPIO_NUM_NC,
        .d6    = GPIO_NUM_NC,
        .d7    = GPIO_NUM_NC,
        .cd    = SDMMC_SLOT_NO_CD,
        .wp    = SDMMC_SLOT_NO_WP,
        .width = 4,
        .flags = 0,
    };

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
        .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16 * 1024};

    sdmmc_card_t* card;
    const char    mount_point[] = "/sd";
    ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_host_t host    = SDSPI_HOST_DEFAULT();
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