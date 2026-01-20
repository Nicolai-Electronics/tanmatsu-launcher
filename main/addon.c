#include "addon.h"
#include "bsp/i2c.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "eeprom.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char TAG[] = "Add-on";

esp_err_t addon_detect_internal(void) {
    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    esp_err_t               res            = bsp_i2c_primary_bus_get_handle(&i2c_bus_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get I2C bus handle: %s", esp_err_to_name(res));
        return res;
    }
    SemaphoreHandle_t i2c_bus_semaphore = NULL;
    res                                 = bsp_i2c_primary_bus_get_semaphore(&i2c_bus_semaphore);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get I2C bus semaphore: %s", esp_err_to_name(res));
        return res;
    }

    eeprom_handle_t eeprom_handle;
    res = eeprom_init(&eeprom_handle, i2c_bus_handle, i2c_bus_semaphore, 0x50, 32, false);
    if (res != ESP_OK) {
        ESP_LOGI(TAG, "No internal add-on detected");
        return ESP_FAIL;
    }

    uint8_t eeprom_data[128];
    res = eeprom_read(&eeprom_handle, 0x00, eeprom_data, sizeof(eeprom_data));
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read from internal add-on EEPROM: %s", esp_err_to_name(res));
    }

    printf("Internal add-on EEPROM data:\n");
    for (size_t i = 0; i < sizeof(eeprom_data); i++) {
        printf("%02X ", eeprom_data[i]);
    }
    printf("\n");

    return ESP_OK;
}

void addon_detect_catt(void) {
#ifdef CONFIG_BSP_TARGET_TANMATSU
    i2c_master_bus_handle_t sao_i2c_bus_handle = NULL;
    SemaphoreHandle_t       sao_i2c_semaphore  = NULL;

    i2c_master_bus_config_t sao_i2c_master_config = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .i2c_port                     = 1,
        .scl_io_num                   = 13,
        .sda_io_num                   = 12,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };

    sao_i2c_semaphore = xSemaphoreCreateBinary();
    if (sao_i2c_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create SAO I2C semaphore");
        return;
    }
    esp_err_t res = i2c_new_master_bus(&sao_i2c_master_config, &sao_i2c_bus_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SAO I2C bus: %s", esp_err_to_name(res));
        return;
    }
    xSemaphoreGive(sao_i2c_semaphore);

    eeprom_handle_t sao_eeprom_handle;
    res = eeprom_init(&sao_eeprom_handle, sao_i2c_bus_handle, sao_i2c_semaphore, 0x50, 16, false);
    if (res != ESP_OK) {
        ESP_LOGI(TAG, "No CATT add-on detected");
    } else {
        uint8_t eeprom_data[200];
        res = eeprom_read(&sao_eeprom_handle, 0x00, eeprom_data, sizeof(eeprom_data));
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read from SAO EEPROM: %s", esp_err_to_name(res));
        } else {
            ESP_LOGI(TAG, "SAO EEPROM data:");
            for (size_t i = 0; i < sizeof(eeprom_data); i++) {
                printf("%02X ", eeprom_data[i]);
            }
            printf("\n");
        }
    }

    i2c_del_master_bus(sao_i2c_bus_handle);
    vSemaphoreDelete(sao_i2c_semaphore);
#endif
}