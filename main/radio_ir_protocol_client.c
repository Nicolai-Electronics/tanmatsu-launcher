#include "radio_ir_protocol_client.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "esp_hosted.h"

static const char TAG[] = "ir client";

static SemaphoreHandle_t ir_protocol_client_mutex                 = NULL;
static SemaphoreHandle_t ir_protocol_client_transaction_semaphore = NULL;
static uint8_t           ir_protocol_client_packet_buffer[512]    = {0};
static uint32_t          ir_protocol_client_packet_size           = 0;
static uint32_t          ir_protocol_client_sequence_number       = 0;

static esp_err_t ir_protocol_client_transaction(const uint8_t* request, size_t request_length, uint8_t* out_response,
                                                size_t* response_length, size_t max_response_length) {
    if (!ir_protocol_client_mutex || !ir_protocol_client_transaction_semaphore) {
        ESP_LOGE(TAG, "Invalid state");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result = ESP_FAIL;
    xSemaphoreTake(ir_protocol_client_mutex, portMAX_DELAY);
    xSemaphoreTake(ir_protocol_client_transaction_semaphore, 0);  // Clear semaphore
    result = esp_hosted_send_custom_data(0x04, (uint8_t*)request, request_length);
    if (result == ESP_OK) {
        if (xSemaphoreTake(ir_protocol_client_transaction_semaphore, pdMS_TO_TICKS(2000)) ==
            pdTRUE) {  // Wait for response
            if (ir_protocol_client_packet_size <= max_response_length) {
                memcpy(out_response, ir_protocol_client_packet_buffer, ir_protocol_client_packet_size);
                *response_length = ir_protocol_client_packet_size;
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
    ir_protocol_client_sequence_number++;
    xSemaphoreGive(ir_protocol_client_mutex);
    return result;
}

static void ir_protocol_client_transaction_receive(uint32_t msg_id, const uint8_t* packet, size_t length) {
    if (msg_id != 0x04) {
        ESP_LOGW(TAG, "Received IR message with unknown ID: %u", msg_id);
        return;
    }

    if (!ir_protocol_client_mutex || !ir_protocol_client_transaction_semaphore) {
        ESP_LOGW(TAG, "Received IR message but client not initialized");
        return;
    }
    if (length > sizeof(ir_protocol_client_packet_buffer) || length < sizeof(ir_protocol_client_header_t)) {
        ESP_LOGW(TAG, "Received IR message but size incorrect");
        return;
    }
    memcpy(ir_protocol_client_packet_buffer, packet, length);
    ir_protocol_client_packet_size = length;
    xSemaphoreGive(ir_protocol_client_transaction_semaphore);
}

static esp_err_t validate_response(const uint8_t* response, size_t response_length, uint32_t expected_seq,
                                   uint32_t expected_type, size_t min_body_size) {
    if (response_length < sizeof(ir_protocol_client_header_t) + min_body_size) {
        ESP_LOGE(TAG, "Response too short: %zu bytes", response_length);
        return ESP_FAIL;
    }
    const ir_protocol_client_header_t* h = (const ir_protocol_client_header_t*)response;
    if (h->sequence_number != expected_seq) {
        ESP_LOGE(TAG, "Response sequence number mismatch: %u != %u", h->sequence_number, expected_seq);
        return ESP_FAIL;
    }
    if (h->type == IR_PROTOCOL_CLIENT_TYPE_NACK) {
        return ESP_FAIL;
    }
    if (h->type != expected_type) {
        ESP_LOGE(TAG, "Response type mismatch: %u != %u", h->type, expected_type);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t ir_protocol_client_init(void) {
    ir_protocol_client_mutex                 = xSemaphoreCreateMutex();
    ir_protocol_client_transaction_semaphore = xSemaphoreCreateBinary();
    if (ir_protocol_client_mutex == NULL || ir_protocol_client_transaction_semaphore == NULL) {
        if (ir_protocol_client_mutex != NULL) {
            vSemaphoreDelete(ir_protocol_client_mutex);
        }
        if (ir_protocol_client_transaction_semaphore != NULL) {
            vSemaphoreDelete(ir_protocol_client_transaction_semaphore);
        }
        return ESP_ERR_NO_MEM;
    }

    esp_hosted_register_custom_callback(0x04, ir_protocol_client_transaction_receive);
    return ESP_OK;
}

esp_err_t ir_protocol_client_get_supported(void) {
    uint8_t                      request[sizeof(ir_protocol_client_header_t)] = {0};
    ir_protocol_client_header_t* header                                       = (ir_protocol_client_header_t*)request;
    header->sequence_number                                                   = ir_protocol_client_sequence_number;
    header->type                                                              = IR_PROTOCOL_CLIENT_TYPE_GET_SUPPORTED;

    uint8_t   response[sizeof(ir_protocol_client_header_t)] = {0};
    size_t    response_length                               = 0;
    esp_err_t result =
        ir_protocol_client_transaction(request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) return result;
    return validate_response(response, response_length, header->sequence_number, IR_PROTOCOL_CLIENT_TYPE_ACK, 0);
}

esp_err_t ir_protocol_client_set_config(const ir_protocol_client_config_t* config) {
    if (config == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t request[sizeof(ir_protocol_client_header_t) + sizeof(ir_protocol_client_config_t)] = {0};
    ir_protocol_client_header_t* header = (ir_protocol_client_header_t*)request;
    header->sequence_number             = ir_protocol_client_sequence_number;
    header->type                        = IR_PROTOCOL_CLIENT_TYPE_SET_CONFIG;
    memcpy(request + sizeof(ir_protocol_client_header_t), config, sizeof(ir_protocol_client_config_t));

    uint8_t   response[sizeof(ir_protocol_client_header_t)] = {0};
    size_t    response_length                               = 0;
    esp_err_t result =
        ir_protocol_client_transaction(request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) return result;
    return validate_response(response, response_length, header->sequence_number, IR_PROTOCOL_CLIENT_TYPE_ACK, 0);
}

esp_err_t ir_protocol_client_send_nec(const ir_protocol_client_nec_scan_code_t* scan_code) {
    if (scan_code == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t request[sizeof(ir_protocol_client_header_t) + sizeof(ir_protocol_client_nec_scan_code_t)] = {0};
    ir_protocol_client_header_t* header = (ir_protocol_client_header_t*)request;
    header->sequence_number             = ir_protocol_client_sequence_number;
    header->type                        = IR_PROTOCOL_CLIENT_TYPE_SEND_NEC;
    memcpy(request + sizeof(ir_protocol_client_header_t), scan_code, sizeof(ir_protocol_client_nec_scan_code_t));

    uint8_t   response[sizeof(ir_protocol_client_header_t)] = {0};
    size_t    response_length                               = 0;
    esp_err_t result =
        ir_protocol_client_transaction(request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) return result;
    return validate_response(response, response_length, header->sequence_number, IR_PROTOCOL_CLIENT_TYPE_ACK, 0);
}

#endif
