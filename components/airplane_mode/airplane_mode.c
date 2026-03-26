// SPDX-License-Identifier: MIT

#include "airplane_mode.h"
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "portmacro.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "esp_hosted.h"
#endif

static const char TAG[] = "airplane_mode";

// Must match the event type defined in radio firmware's priv_events.h
#define TANMATSU_EVENT_AIRPLANE 0x05

// Protocol command bytes (must match radio firmware's airplane_mode.c)
#define AIRPLANE_CMD_GET  0x00
#define AIRPLANE_CMD_SET  0x01
#define AIRPLANE_RESP     0x02

static SemaphoreHandle_t airplane_mutex                 = NULL;
static SemaphoreHandle_t airplane_transaction_semaphore = NULL;
static uint8_t           airplane_response_buffer[4];
static size_t            airplane_response_size = 0;

static void airplane_transaction_receive(uint32_t msg_id, const uint8_t* packet, size_t length) {
    if (msg_id != TANMATSU_EVENT_AIRPLANE) {
        ESP_LOGW(TAG, "Received message with unexpected ID: %" PRIu32, msg_id);
        return;
    }
    if (!airplane_mutex || !airplane_transaction_semaphore) {
        ESP_LOGW(TAG, "Received message but not initialized");
        return;
    }
    if (length > sizeof(airplane_response_buffer) || length < 2) {
        ESP_LOGW(TAG, "Received message with unexpected size: %zu", length);
        return;
    }
    memcpy(airplane_response_buffer, packet, length);
    airplane_response_size = length;
    xSemaphoreGive(airplane_transaction_semaphore);
}

static esp_err_t airplane_transaction(const uint8_t* request, size_t request_length, uint8_t* out_response,
                                      size_t* response_length, size_t max_response_length) {
    if (!airplane_mutex || !airplane_transaction_semaphore) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = ESP_FAIL;
    xSemaphoreTake(airplane_mutex, portMAX_DELAY);
    xSemaphoreTake(airplane_transaction_semaphore, 0);  // Clear semaphore
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    result = esp_hosted_send_custom_data(TANMATSU_EVENT_AIRPLANE, (uint8_t*)request, request_length);
    if (result == ESP_OK) {
        if (xSemaphoreTake(airplane_transaction_semaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
            if (airplane_response_size <= max_response_length) {
                memcpy(out_response, airplane_response_buffer, airplane_response_size);
                *response_length = airplane_response_size;
                result           = ESP_OK;
            } else {
                result = ESP_ERR_INVALID_SIZE;
            }
        } else {
            result = ESP_ERR_TIMEOUT;
        }
    }
#else
    result = ESP_OK;
#endif
    xSemaphoreGive(airplane_mutex);
    return result;
}

esp_err_t airplane_mode_init(void) {
    airplane_mutex                 = xSemaphoreCreateMutex();
    airplane_transaction_semaphore = xSemaphoreCreateBinary();
    if (airplane_mutex == NULL || airplane_transaction_semaphore == NULL) {
        if (airplane_mutex != NULL) {
            vSemaphoreDelete(airplane_mutex);
        }
        if (airplane_transaction_semaphore != NULL) {
            vSemaphoreDelete(airplane_transaction_semaphore);
        }
        return ESP_ERR_NO_MEM;
    }

#if defined(CONFIG_IDF_TARGET_ESP32P4)
    esp_hosted_register_custom_callback(TANMATSU_EVENT_AIRPLANE, airplane_transaction_receive);
#endif

    return ESP_OK;
}

esp_err_t airplane_mode_get(bool* out_enabled) {
    if (out_enabled == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t request[1] = {AIRPLANE_CMD_GET};
    uint8_t response[2] = {0};
    size_t  response_length = 0;

    esp_err_t result = airplane_transaction(request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < 2 || response[0] != AIRPLANE_RESP) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    *out_enabled = response[1] != 0;
    return ESP_OK;
}

esp_err_t airplane_mode_set(bool enabled) {
    uint8_t request[2] = {AIRPLANE_CMD_SET, enabled ? 1 : 0};
    uint8_t response[2] = {0};
    size_t  response_length = 0;

    esp_err_t result = airplane_transaction(request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < 2 || response[0] != AIRPLANE_RESP) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
