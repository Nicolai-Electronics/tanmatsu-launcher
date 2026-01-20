#include "lora.h"
#include <string.h>
#include "bsp/tanmatsu.h"
#include "esp_err.h"
#include "esp_hosted_custom.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "lora_protocol.h"
#include "portmacro.h"
#include "tanmatsu_coprocessor.h"

static const char TAG[] = "lora";

QueueHandle_t     lora_packet_queue          = NULL;
SemaphoreHandle_t lora_mutex                 = NULL;
SemaphoreHandle_t lora_transaction_semaphore = NULL;
uint32_t          lora_sequence_number       = 0;
uint8_t           lora_packet_buffer[sizeof(uint32_t) + 512];
size_t            lora_packet_size = 0;

static esp_err_t lora_transaction(const uint8_t* request, size_t request_length, uint8_t* out_response,
                                  size_t* response_length, size_t max_response_length) {
    esp_err_t result = ESP_FAIL;
    xSemaphoreTake(lora_mutex, portMAX_DELAY);
    xSemaphoreTake(lora_transaction_semaphore, 0);  // Clear semaphore
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
    lora_sequence_number++;
    xSemaphoreGive(lora_mutex);
    return result;
}

esp_err_t lora_transaction_receive(uint8_t* packet, size_t length) {
    if (length > sizeof(lora_packet_buffer) || length < sizeof(lora_protocol_header_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    lora_protocol_header_t* header = (lora_protocol_header_t*)packet;
    if (header->type == LORA_PROTOCOL_TYPE_PACKET_RX) {
        // to-do add to queue
        printf("Received LoRa packet\r\n");
        for (size_t i = sizeof(lora_protocol_header_t); i < length - sizeof(lora_protocol_header_t); i++) {
            printf("%02X ", packet[i]);
        }
        printf("\r\n");
        tanmatsu_coprocessor_handle_t handle;
        bsp_tanmatsu_coprocessor_get_handle(&handle);
        tanmatsu_coprocessor_set_message(handle, true, false, false, true, false, false, false, false);
        vTaskDelay(pdMS_TO_TICKS(500));
        tanmatsu_coprocessor_set_message(handle, false, false, false, false, false, false, false, false);
        return ESP_OK;
    }
    // Else it's a response to a transaction
    memcpy(lora_packet_buffer, packet, length);
    lora_packet_size = length;
    xSemaphoreGive(lora_transaction_semaphore);
    return ESP_OK;
}

esp_err_t lora_init(QueueHandle_t packet_queue) {
    lora_packet_queue          = packet_queue;
    lora_mutex                 = xSemaphoreCreateMutex();
    lora_transaction_semaphore = xSemaphoreCreateBinary();
    if (lora_mutex == NULL || lora_transaction_semaphore == NULL) {
        if (lora_mutex != NULL) {
            vSemaphoreDelete(lora_mutex);
        }
        if (lora_transaction_semaphore != NULL) {
            vSemaphoreDelete(lora_transaction_semaphore);
        }
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
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
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t lora_receive_packet(uint8_t* out_buffer, uint8_t* out_length, uint16_t max_length) {
    return ESP_ERR_NOT_SUPPORTED;
}
