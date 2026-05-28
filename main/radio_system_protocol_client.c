#include "radio_system_protocol_client.h"
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "esp_hosted.h"

static const char TAG[] = "radio system";

static SemaphoreHandle_t radio_system_protocol_mutex                 = NULL;
static SemaphoreHandle_t radio_system_protocol_transaction_semaphore = NULL;
static uint8_t           radio_system_protocol_packet_buffer[256]    = {0};
static uint32_t          radio_system_protocol_packet_size           = 0;
static uint32_t          radio_system_sequence_number                = 0;

static esp_err_t radio_system_protocol_transaction(const uint8_t* request, size_t request_length, uint8_t* out_response,
                                                   size_t* response_length, size_t max_response_length) {
    if (!radio_system_protocol_mutex || !radio_system_protocol_transaction_semaphore) {
        ESP_LOGE(TAG, "Invalid state");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result = ESP_FAIL;
    xSemaphoreTake(radio_system_protocol_mutex, portMAX_DELAY);
    xSemaphoreTake(radio_system_protocol_transaction_semaphore, 0);  // Clear semaphore
    result = esp_hosted_send_custom_data(0x05, (uint8_t*)request, request_length);
    if (result == ESP_OK) {
        if (xSemaphoreTake(radio_system_protocol_transaction_semaphore, pdMS_TO_TICKS(2000)) ==
            pdTRUE) {  // Wait for response
            if (radio_system_protocol_packet_size <= max_response_length) {
                memcpy(out_response, radio_system_protocol_packet_buffer, radio_system_protocol_packet_size);
                *response_length = radio_system_protocol_packet_size;
                result           = ESP_OK;
            } else {
                ESP_LOGE(TAG, "Invalid size");
                result = ESP_ERR_INVALID_SIZE;
            }
        } else {
            ESP_LOGE(TAG, "Timeout");
            result = ESP_ERR_TIMEOUT;
        }
    }
    radio_system_sequence_number++;
    xSemaphoreGive(radio_system_protocol_mutex);
    return result;
}

static void radio_system_protocol_transaction_receive(uint32_t msg_id, const uint8_t* packet, size_t length) {
    if (msg_id != 0x05) {
        ESP_LOGW(TAG, "Received system message with unknown ID: %u", msg_id);
        return;
    }

    if (!radio_system_protocol_mutex || !radio_system_protocol_transaction_semaphore) {
        ESP_LOGW(TAG, "Received system message but system not initialized");
        return;
    }
    if (length > sizeof(radio_system_protocol_packet_buffer) || length < sizeof(radio_system_protocol_header_t)) {
        ESP_LOGW(TAG, "Received radio system message but size incorrect");
        return;
    }
    radio_system_protocol_header_t* header = (radio_system_protocol_header_t*)packet;
    memcpy(radio_system_protocol_packet_buffer, packet, length);
    radio_system_protocol_packet_size = length;
    xSemaphoreGive(radio_system_protocol_transaction_semaphore);
}

esp_err_t radio_system_protocol_init(void) {
    radio_system_protocol_mutex                 = xSemaphoreCreateMutex();
    radio_system_protocol_transaction_semaphore = xSemaphoreCreateBinary();
    if (radio_system_protocol_mutex == NULL || radio_system_protocol_transaction_semaphore == NULL) {
        if (radio_system_protocol_mutex != NULL) {
            vSemaphoreDelete(radio_system_protocol_mutex);
        }
        if (radio_system_protocol_transaction_semaphore != NULL) {
            vSemaphoreDelete(radio_system_protocol_transaction_semaphore);
        }
        return ESP_ERR_NO_MEM;
    }

    esp_hosted_register_custom_callback(0x05, radio_system_protocol_transaction_receive);
    return ESP_OK;
}

esp_err_t radio_system_protocol_get_information(radio_system_protocol_information_t* out_information) {
    if (out_information == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t  request_length = sizeof(radio_system_protocol_header_t) + sizeof(radio_system_protocol_information_t);
    uint8_t request[request_length];
    radio_system_protocol_header_t* header = (radio_system_protocol_header_t*)request;
    header->sequence_number                = radio_system_sequence_number;
    header->type                           = RADIO_SYSTEM_PROTOCOL_TYPE_GET_INFORMATION;
    uint8_t   response[sizeof(radio_system_protocol_header_t) + sizeof(radio_system_protocol_information_t)] = {0};
    size_t    response_length                                                                                = 0;
    esp_err_t result =
        radio_system_protocol_transaction(request, request_length, response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Transaction failed");
        return result;
    }
    if (response_length < sizeof(radio_system_protocol_header_t) + sizeof(radio_system_protocol_information_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    radio_system_protocol_header_t* response_header = (radio_system_protocol_header_t*)response;
    if (response_header->sequence_number != header->sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", response_header->sequence_number);
        return ESP_FAIL;
    }
    if (response_header->type != RADIO_SYSTEM_PROTOCOL_TYPE_GET_INFORMATION) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", response_header->type);
        return ESP_FAIL;
    }
    memcpy(out_information, &response[sizeof(radio_system_protocol_header_t)],
           sizeof(radio_system_protocol_information_t));
    return ESP_OK;
}
#else
esp_err_t radio_system_protocol_init(uint32_t packet_queue_size) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t radio_system_protocol_get_information(system_protocol_information_t* out_information) {
    return ESP_ERR_NOT_SUPPORTED;
}
#endif