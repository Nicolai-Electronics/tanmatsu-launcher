/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#include "os_header.h"
#include "os_wrapper.h"
#include "esp_log.h"
#include "esp_hosted_api.h"
#include "esp_private/startup_internal.h"

DEFINE_LOG_TAG(host_init);

#ifdef CONFIG_ESP_HOSTED_INITIALIZE_ON_STARTUP
#define ESP_HOSTED_CONSTRUCTOR __attribute__((constructor))
#define ESP_HOSTED_DESTRUCTOR
#define ESP_HOSTED_CONSTRUCTOR_TYPE static
#else
#define ESP_HOSTED_CONSTRUCTOR
#define ESP_HOSTED_DESTRUCTOR
#define ESP_HOSTED_CONSTRUCTOR_TYPE
#endif

//ESP_SYSTEM_INIT_FN(esp_hosted_host_init, BIT(0), 120)
ESP_HOSTED_CONSTRUCTOR_TYPE esp_err_t ESP_HOSTED_CONSTRUCTOR esp_hosted_host_init(void)
{
	ESP_LOGI(TAG, "ESP Hosted : Host chip_ip[%d]", CONFIG_IDF_FIRMWARE_CHIP_ID);
#ifdef CONFIG_ESP_HOSTED_INITIALIZE_ON_STARTUP
	ESP_ERR_CHECK(esp_hosted_init());
#else
	return esp_hosted_init();
#endif
}

ESP_HOSTED_CONSTRUCTOR_TYPE esp_err_t ESP_HOSTED_DESTRUCTOR esp_hosted_host_deinit(void)
{
	ESP_LOGI(TAG, "ESP Hosted deinit");
	esp_hosted_deinit();
	return ESP_OK;
}
