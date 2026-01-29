#include "lora.h"
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "portmacro.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "esp_hosted_custom.h"
#endif

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#include "bsp/tanmatsu.h"
#include "tanmatsu_coprocessor.h"
#endif

static const char TAG[] = "lora";

QueueHandle_t                      lora_packet_queue          = NULL;
SemaphoreHandle_t                  lora_mutex                 = NULL;
SemaphoreHandle_t                  lora_transaction_semaphore = NULL;
uint32_t                           lora_sequence_number       = 0;
uint8_t                            lora_packet_buffer[sizeof(uint32_t) + 512];
size_t                             lora_packet_size = 0;
static lora_protocol_lora_packet_t lora_packet      = {0};

static esp_err_t lora_transaction(const uint8_t* request, size_t request_length, uint8_t* out_response,
                                  size_t* response_length, size_t max_response_length) {
    esp_err_t result = ESP_FAIL;
    xSemaphoreTake(lora_mutex, portMAX_DELAY);
    xSemaphoreTake(lora_transaction_semaphore, 0);  // Clear semaphore
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    result = esp_hosted_send_custom(1, (uint8_t*)request, request_length);
    if (result == ESP_OK) {
        if (xSemaphoreTake(lora_transaction_semaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {  // Wait for response
            if (lora_packet_size <= max_response_length) {
                memcpy(out_response, lora_packet_buffer, lora_packet_size);
                *response_length = lora_packet_size;
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
    lora_sequence_number++;
    xSemaphoreGive(lora_mutex);
    return result;
}

esp_err_t lora_transaction_receive(uint8_t* packet, size_t length) {
    if (!lora_mutex || !lora_transaction_semaphore || !lora_packet_queue) {
        return ESP_ERR_INVALID_STATE;
    }
    if (length > sizeof(lora_packet_buffer) || length < sizeof(lora_protocol_header_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    lora_protocol_header_t* header = (lora_protocol_header_t*)packet;
    if (header->type == LORA_PROTOCOL_TYPE_PACKET_RX) {
        // Received LoRa packet
        lora_packet.length = length - sizeof(lora_protocol_header_t);
        memcpy(lora_packet.data, packet + sizeof(lora_protocol_header_t), lora_packet.length);
        xQueueSend(lora_packet_queue, &lora_packet, 0);
    } else {
        // Response to a transaction
        memcpy(lora_packet_buffer, packet, length);
        lora_packet_size = length;
        xSemaphoreGive(lora_transaction_semaphore);
    }
    return ESP_OK;
}

esp_err_t lora_init(uint32_t packet_queue_size) {
    lora_mutex                 = xSemaphoreCreateMutex();
    lora_transaction_semaphore = xSemaphoreCreateBinary();
    lora_packet_queue          = xQueueCreate(packet_queue_size, sizeof(lora_protocol_lora_packet_t));
    if (lora_mutex == NULL || lora_transaction_semaphore == NULL || lora_packet_queue == NULL) {
        if (lora_mutex != NULL) {
            vSemaphoreDelete(lora_mutex);
        }
        if (lora_transaction_semaphore != NULL) {
            vSemaphoreDelete(lora_transaction_semaphore);
        }
        if (lora_packet_queue != NULL) {
            vQueueDelete(lora_packet_queue);
        }
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

QueueHandle_t lora_get_packet_queue(void) {
    return lora_packet_queue;
}

esp_err_t lora_get_mode(lora_protocol_mode_t* out_mode) {
    if (out_mode == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lora_protocol_header_t request = {
        .sequence_number = lora_sequence_number,
        .type            = LORA_PROTOCOL_TYPE_GET_MODE,
    };
    uint8_t   response[sizeof(lora_protocol_header_t) + sizeof(lora_protocol_mode_params_t)] = {0};
    size_t    response_length                                                                = 0;
    esp_err_t result =
        lora_transaction((uint8_t*)&request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < sizeof(lora_protocol_header_t) + sizeof(lora_protocol_mode_params_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    lora_protocol_header_t* header = (lora_protocol_header_t*)response;
    if (header->sequence_number != request.sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", header->sequence_number);
        return ESP_FAIL;
    }
    if (header->type != LORA_PROTOCOL_TYPE_GET_MODE) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", header->type);
        return ESP_FAIL;
    }
    lora_protocol_mode_params_t* params = (lora_protocol_mode_params_t*)(response + sizeof(lora_protocol_header_t));
    *out_mode                           = params->mode;
    return ESP_OK;
}

esp_err_t lora_set_mode(const lora_protocol_mode_t mode) {
    size_t                  request_length = sizeof(lora_protocol_header_t) + sizeof(lora_protocol_mode_params_t);
    uint8_t                 request[request_length];
    lora_protocol_header_t* header      = (lora_protocol_header_t*)request;
    header->sequence_number             = lora_sequence_number;
    header->type                        = LORA_PROTOCOL_TYPE_SET_MODE;
    lora_protocol_mode_params_t* params = (lora_protocol_mode_params_t*)(request + sizeof(lora_protocol_header_t));
    params->mode                        = mode;
    uint8_t   response[sizeof(lora_protocol_header_t)] = {0};
    size_t    response_length                          = 0;
    esp_err_t result = lora_transaction(request, request_length, response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < sizeof(lora_protocol_header_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    lora_protocol_header_t* response_header = (lora_protocol_header_t*)response;
    if (response_header->sequence_number != header->sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", response_header->sequence_number);
        return ESP_FAIL;
    }
    if (response_header->type != LORA_PROTOCOL_TYPE_ACK) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", response_header->type);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t lora_get_config(lora_protocol_config_params_t* out_config) {
    if (out_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lora_protocol_header_t request = {
        .sequence_number = lora_sequence_number,
        .type            = LORA_PROTOCOL_TYPE_GET_CONFIG,
    };
    uint8_t   response[sizeof(lora_protocol_header_t) + sizeof(lora_protocol_config_params_t)] = {0};
    size_t    response_length                                                                  = 0;
    esp_err_t result =
        lora_transaction((uint8_t*)&request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < sizeof(lora_protocol_header_t) + sizeof(lora_protocol_config_params_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    lora_protocol_header_t* header = (lora_protocol_header_t*)response;
    if (header->sequence_number != request.sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", header->sequence_number);
        return ESP_FAIL;
    }
    if (header->type != LORA_PROTOCOL_TYPE_GET_CONFIG) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", header->type);
        return ESP_FAIL;
    }
    lora_protocol_config_params_t* params = (lora_protocol_config_params_t*)(response + sizeof(lora_protocol_header_t));
    memcpy(out_config, params, sizeof(lora_protocol_config_params_t));
    return ESP_OK;
}

esp_err_t lora_set_config(const lora_protocol_config_params_t* config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t                  request_length = sizeof(lora_protocol_header_t) + sizeof(lora_protocol_config_params_t);
    uint8_t                 request[request_length];
    lora_protocol_header_t* header        = (lora_protocol_header_t*)request;
    header->sequence_number               = lora_sequence_number;
    header->type                          = LORA_PROTOCOL_TYPE_SET_CONFIG;
    lora_protocol_config_params_t* params = (lora_protocol_config_params_t*)(request + sizeof(lora_protocol_header_t));
    memcpy(params, config, sizeof(lora_protocol_config_params_t));
    uint8_t   response[sizeof(lora_protocol_header_t)] = {0};
    size_t    response_length                          = 0;
    esp_err_t result = lora_transaction(request, request_length, response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < sizeof(lora_protocol_header_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    lora_protocol_header_t* response_header = (lora_protocol_header_t*)response;
    if (response_header->sequence_number != header->sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", response_header->sequence_number);
        return ESP_FAIL;
    }
    if (response_header->type != LORA_PROTOCOL_TYPE_ACK) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", response_header->type);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t lora_get_status(lora_protocol_status_params_t* out_status) {
    if (out_status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lora_protocol_header_t request = {
        .sequence_number = lora_sequence_number,
        .type            = LORA_PROTOCOL_TYPE_GET_STATUS,
    };
    uint8_t   response[sizeof(lora_protocol_header_t) + sizeof(lora_protocol_status_params_t)] = {0};
    size_t    response_length                                                                  = 0;
    esp_err_t result =
        lora_transaction((uint8_t*)&request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < sizeof(lora_protocol_header_t) + sizeof(lora_protocol_status_params_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    lora_protocol_header_t* header = (lora_protocol_header_t*)response;
    if (header->sequence_number != request.sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", header->sequence_number);
        return ESP_FAIL;
    }
    if (header->type != LORA_PROTOCOL_TYPE_GET_STATUS) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", header->type);
        return ESP_FAIL;
    }
    lora_protocol_status_params_t* params = (lora_protocol_status_params_t*)(response + sizeof(lora_protocol_header_t));
    memcpy(out_status, params, sizeof(lora_protocol_status_params_t));
    return ESP_OK;
}

esp_err_t lora_send_packet(const lora_protocol_lora_packet_t* packet) {
    size_t                  request_length = sizeof(lora_protocol_header_t) + packet->length;
    uint8_t                 request[request_length];
    lora_protocol_header_t* header = (lora_protocol_header_t*)request;
    header->sequence_number        = lora_sequence_number;
    header->type                   = LORA_PROTOCOL_TYPE_PACKET_TX;
    uint8_t* data_ptr              = (uint8_t*)(request + sizeof(lora_protocol_header_t));
    memcpy(data_ptr, packet->data, packet->length);
    uint8_t   response[sizeof(lora_protocol_header_t)] = {0};
    size_t    response_length                          = 0;
    esp_err_t result = lora_transaction(request, request_length, response, &response_length, sizeof(response));
    if (result != ESP_OK) {
        return result;
    }
    if (response_length < sizeof(lora_protocol_header_t)) {
        ESP_LOGE(TAG, "Invalid response length: %u\r\n", response_length);
        return ESP_FAIL;
    }
    lora_protocol_header_t* response_header = (lora_protocol_header_t*)response;
    if (response_header->sequence_number != header->sequence_number) {
        ESP_LOGE(TAG, "Invalid response sequence number: %u\r\n", response_header->sequence_number);
        return ESP_FAIL;
    }
    if (response_header->type != LORA_PROTOCOL_TYPE_ACK) {
        ESP_LOGE(TAG, "Invalid response type: %u\r\n", response_header->type);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t lora_receive_packet(lora_protocol_lora_packet_t* out_packet, TickType_t timeout) {
    return xQueueReceive(lora_packet_queue, out_packet, timeout) ? ESP_OK : ESP_FAIL;
}
