#pragma once

#include <stdio.h>
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"

// Fast file I/O for SD card - uses internal DMA RAM buffers
// Use these instead of fopen/fclose for SD card files (/sd/...)
FILE* sd_fopen(const char* path, const char* mode);
void  sd_fclose(FILE* f);

typedef enum {
    SD_STATUS_NOT_PRESENT,
    SD_STATUS_OK,
    SD_STATUS_ERROR,
    SD_STATUS_LAST,
} sd_status_t;

sd_pwr_ctrl_handle_t initialize_sd_ldo(void);
esp_err_t            sd_mount(sd_pwr_ctrl_handle_t pwr_ctrl_handle);
esp_err_t            sd_mount_spi(sd_pwr_ctrl_handle_t pwr_ctrl_handle);
void                 test_sd(void);
void                 sd_speedtest(void);
sd_status_t          sd_status(void);
