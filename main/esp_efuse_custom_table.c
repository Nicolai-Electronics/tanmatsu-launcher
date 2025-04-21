/*
 * SPDX-FileCopyrightText: 2017-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_efuse.h"
#include <assert.h>
#include "esp_efuse_custom_table.h"

// md5_digest_table 0850fd9135bd482323a0339a42d4e160
// This file was generated from the file esp_efuse_custom_table.csv. DO NOT CHANGE THIS FILE MANUALLY.
// If you want to change some fields, you need to change esp_efuse_custom_table.csv file
// then run `efuse_common_table` or `efuse_custom_table` command it will generate this file.
// To show efuse_table run the command 'show_efuse_table'.

static const esp_efuse_desc_t USER_DATA_HARDWARE_NAME[] = {
    {EFUSE_BLK3, 0, 128}, 	 // ,
};

static const esp_efuse_desc_t USER_DATA_HARDWARE_VENDOR[] = {
    {EFUSE_BLK3, 128, 80}, 	 // ,
};

static const esp_efuse_desc_t USER_DATA_HARDWARE_RESERVED0[] = {
    {EFUSE_BLK3, 208, 8}, 	 // ,
};

static const esp_efuse_desc_t USER_DATA_HARDWARE_REVISION[] = {
    {EFUSE_BLK3, 216, 8}, 	 // ,
};

static const esp_efuse_desc_t USER_DATA_HARDWARE_VARIANT_RADIO[] = {
    {EFUSE_BLK3, 224, 8}, 	 // ,
};

static const esp_efuse_desc_t USER_DATA_HARDWARE_RESERVED1[] = {
    {EFUSE_BLK3, 232, 8}, 	 // ,
};

static const esp_efuse_desc_t USER_DATA_HARDWARE_REGION[] = {
    {EFUSE_BLK3, 240, 16}, 	 // ,
};





const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_HARDWARE_NAME[] = {
    &USER_DATA_HARDWARE_NAME[0],    		// 
    NULL
};

const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_HARDWARE_VENDOR[] = {
    &USER_DATA_HARDWARE_VENDOR[0],    		// 
    NULL
};

const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_HARDWARE_RESERVED0[] = {
    &USER_DATA_HARDWARE_RESERVED0[0],    		// 
    NULL
};

const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_HARDWARE_REVISION[] = {
    &USER_DATA_HARDWARE_REVISION[0],    		// 
    NULL
};

const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_HARDWARE_VARIANT_RADIO[] = {
    &USER_DATA_HARDWARE_VARIANT_RADIO[0],    		// 
    NULL
};

const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_HARDWARE_RESERVED1[] = {
    &USER_DATA_HARDWARE_RESERVED1[0],    		// 
    NULL
};

const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_HARDWARE_REGION[] = {
    &USER_DATA_HARDWARE_REGION[0],    		// 
    NULL
};
