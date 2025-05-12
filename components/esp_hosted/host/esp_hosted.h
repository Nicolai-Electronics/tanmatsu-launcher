/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#ifndef __ESP_HOSTED_H__
#define __ESP_HOSTED_H__

#include "esp_hosted_api.h"
#include "esp_hosted_config.h"
#include "esp_hosted_bt_config.h"
#include "esp_hosted_transport_config.h"

typedef struct esp_hosted_transport_config esp_hosted_config_t;

#ifndef CONFIG_ESP_HOSTED_INITIALIZE_ON_STARTUP
esp_err_t esp_hosted_host_init(void);
esp_err_t esp_hosted_host_deinit(void);
#endif

#endif /* __ESP_HOSTED_H__ */
