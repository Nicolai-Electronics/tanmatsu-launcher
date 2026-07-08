#include "radio_system_protocol_client.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
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
static uint8_t           radio_system_protocol_packet_buffer[512]    = {0};
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
    memcpy(radio_system_protocol_packet_buffer, packet, length);
    radio_system_protocol_packet_size = length;
    xSemaphoreGive(radio_system_protocol_transaction_semaphore);
}

// Validates a received response: checks minimum length, sequence number, and type.
// Returns ESP_FAIL if the remote sent NACK, ESP_FAIL for any other mismatch.
static esp_err_t validate_response(const uint8_t* response, size_t response_length, uint32_t expected_seq,
                                   uint32_t expected_type, size_t min_body_size) {
    if (response_length < sizeof(radio_system_protocol_header_t) + min_body_size) {
        ESP_LOGE(TAG, "Response too short: %zu bytes", response_length);
        return ESP_FAIL;
    }
    const radio_system_protocol_header_t* h = (const radio_system_protocol_header_t*)response;
    if (h->sequence_number != expected_seq) {
        ESP_LOGE(TAG, "Response sequence number mismatch: %u != %u", h->sequence_number, expected_seq);
        return ESP_FAIL;
    }
    if (h->type == RADIO_SYSTEM_PROTOCOL_TYPE_NACK) {
        return ESP_FAIL;
    }
    if (h->type != expected_type) {
        ESP_LOGE(TAG, "Response type mismatch: %u != %u", h->type, expected_type);
        return ESP_FAIL;
    }
    return ESP_OK;
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

esp_err_t radio_system_protocol_set_configuration(const radio_system_protocol_configuration_t* configuration) {
    if (configuration == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t request[sizeof(radio_system_protocol_header_t) + sizeof(radio_system_protocol_configuration_t)] = {0};
    radio_system_protocol_header_t* header = (radio_system_protocol_header_t*)request;
    header->sequence_number                = radio_system_sequence_number;
    header->type                           = RADIO_SYSTEM_PROTOCOL_TYPE_SET_CONFIGURATION;
    memcpy(request + sizeof(radio_system_protocol_header_t), configuration,
           sizeof(radio_system_protocol_configuration_t));

    uint8_t   response[sizeof(radio_system_protocol_header_t)] = {0};
    size_t    response_length                                  = 0;
    esp_err_t result =
        radio_system_protocol_transaction(request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) return result;
    return validate_response(response, response_length, header->sequence_number, RADIO_SYSTEM_PROTOCOL_TYPE_ACK, 0);
}

// NVS

typedef struct {
    char                                   namespace_name[RADIO_SYSTEM_PROTOCOL_NVS_MAX_KEY_LENGTH];
    char                                   key[RADIO_SYSTEM_PROTOCOL_NVS_MAX_KEY_LENGTH];
    radio_system_protocol_nvs_value_type_t type;
    uint32_t                               offset;
} __attribute__((packed)) nvs_list_request_t;

typedef struct {
    uint32_t                          count;
    radio_system_protocol_nvs_entry_t entries[0];
} __attribute__((packed)) nvs_list_response_t;

esp_err_t radio_system_protocol_nvs_list(const char* namespace_name, const char* key_filter,
                                         radio_system_protocol_nvs_value_type_t type, uint32_t offset,
                                         radio_system_protocol_nvs_entry_t* out_entries, uint32_t max_entries,
                                         uint32_t* out_count) {
    if (out_entries == NULL || out_count == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t                         request[sizeof(radio_system_protocol_header_t) + sizeof(nvs_list_request_t)] = {0};
    radio_system_protocol_header_t* header = (radio_system_protocol_header_t*)request;
    header->sequence_number                = radio_system_sequence_number;
    header->type                           = RADIO_SYSTEM_PROTOCOL_TYPE_NVS_LIST;
    nvs_list_request_t* body               = (nvs_list_request_t*)(request + sizeof(radio_system_protocol_header_t));
    if (namespace_name) strlcpy(body->namespace_name, namespace_name, sizeof(body->namespace_name));
    if (key_filter) strlcpy(body->key, key_filter, sizeof(body->key));
    body->type   = type;
    body->offset = offset;

    uint8_t   response[512]   = {0};
    size_t    response_length = 0;
    esp_err_t result =
        radio_system_protocol_transaction(request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) return result;
    result = validate_response(response, response_length, header->sequence_number, RADIO_SYSTEM_PROTOCOL_TYPE_NVS_LIST,
                               sizeof(nvs_list_response_t));
    if (result != ESP_OK) return result;

    nvs_list_response_t* resp_body = (nvs_list_response_t*)(response + sizeof(radio_system_protocol_header_t));
    *out_count                     = resp_body->count < max_entries ? resp_body->count : max_entries;
    memcpy(out_entries, resp_body->entries, *out_count * sizeof(radio_system_protocol_nvs_entry_t));
    return ESP_OK;
}

esp_err_t radio_system_protocol_nvs_read(const char* namespace_name, const char* key,
                                         radio_system_protocol_nvs_value_type_t type,
                                         radio_system_protocol_nvs_value_t* out_value, size_t out_value_size) {
    if (namespace_name == NULL || key == NULL || out_value == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t request[sizeof(radio_system_protocol_header_t) + sizeof(radio_system_protocol_nvs_location_t)] = {0};
    radio_system_protocol_header_t* header = (radio_system_protocol_header_t*)request;
    header->sequence_number                = radio_system_sequence_number;
    header->type                           = RADIO_SYSTEM_PROTOCOL_TYPE_NVS_READ;
    radio_system_protocol_nvs_location_t* body =
        (radio_system_protocol_nvs_location_t*)(request + sizeof(radio_system_protocol_header_t));
    strlcpy(body->namespace_name, namespace_name, sizeof(body->namespace_name));
    strlcpy(body->entry.key, key, sizeof(body->entry.key));
    body->entry.type = type;

    uint8_t   response[512]   = {0};
    size_t    response_length = 0;
    esp_err_t result =
        radio_system_protocol_transaction(request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) return result;
    result = validate_response(response, response_length, header->sequence_number, RADIO_SYSTEM_PROTOCOL_TYPE_NVS_READ,
                               sizeof(radio_system_protocol_nvs_value_t));
    if (result != ESP_OK) return result;

    size_t body_length = response_length - sizeof(radio_system_protocol_header_t);
    size_t copy_length = body_length < out_value_size ? body_length : out_value_size;
    memcpy(out_value, response + sizeof(radio_system_protocol_header_t), copy_length);
    return ESP_OK;
}

esp_err_t radio_system_protocol_nvs_write(const radio_system_protocol_nvs_value_t* value, size_t value_size) {
    if (value == NULL) return ESP_ERR_INVALID_ARG;

    size_t request_length = sizeof(radio_system_protocol_header_t) + value_size;
    if (request_length > 512) {
        ESP_LOGE(TAG, "NVS write value too large: %zu bytes", value_size);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t                         request[512] = {0};
    radio_system_protocol_header_t* header       = (radio_system_protocol_header_t*)request;
    header->sequence_number                      = radio_system_sequence_number;
    header->type                                 = RADIO_SYSTEM_PROTOCOL_TYPE_NVS_WRITE;
    memcpy(request + sizeof(radio_system_protocol_header_t), value, value_size);

    uint8_t   response[sizeof(radio_system_protocol_header_t)] = {0};
    size_t    response_length                                  = 0;
    esp_err_t result =
        radio_system_protocol_transaction(request, request_length, response, &response_length, sizeof(response));
    if (result != ESP_OK) return result;
    return validate_response(response, response_length, header->sequence_number, RADIO_SYSTEM_PROTOCOL_TYPE_ACK, 0);
}

esp_err_t radio_system_protocol_nvs_delete(const char* namespace_name, const char* key,
                                           radio_system_protocol_nvs_value_type_t type) {
    if (namespace_name == NULL || key == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t request[sizeof(radio_system_protocol_header_t) + sizeof(radio_system_protocol_nvs_location_t)] = {0};
    radio_system_protocol_header_t* header = (radio_system_protocol_header_t*)request;
    header->sequence_number                = radio_system_sequence_number;
    header->type                           = RADIO_SYSTEM_PROTOCOL_TYPE_NVS_DELETE;
    radio_system_protocol_nvs_location_t* body =
        (radio_system_protocol_nvs_location_t*)(request + sizeof(radio_system_protocol_header_t));
    strlcpy(body->namespace_name, namespace_name, sizeof(body->namespace_name));
    strlcpy(body->entry.key, key, sizeof(body->entry.key));
    body->entry.type = type;

    uint8_t   response[sizeof(radio_system_protocol_header_t)] = {0};
    size_t    response_length                                  = 0;
    esp_err_t result =
        radio_system_protocol_transaction(request, sizeof(request), response, &response_length, sizeof(response));
    if (result != ESP_OK) return result;
    return validate_response(response, response_length, header->sequence_number, RADIO_SYSTEM_PROTOCOL_TYPE_ACK, 0);
}

esp_err_t radio_system_protocol_set_board_revision(uint8_t board_revision) {
    return ESP_ERR_NOT_SUPPORTED;
}
#else

esp_err_t radio_system_protocol_init(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t radio_system_protocol_get_information(radio_system_protocol_information_t* out_information) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t radio_system_protocol_set_configuration(const radio_system_protocol_configuration_t* configuration) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t radio_system_protocol_nvs_list(const char* namespace_name, const char* key_filter,
                                         radio_system_protocol_nvs_value_type_t type, uint32_t offset,
                                         radio_system_protocol_nvs_entry_t* out_entries, uint32_t max_entries,
                                         uint32_t* out_count) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t radio_system_protocol_nvs_read(const char* namespace_name, const char* key,
                                         radio_system_protocol_nvs_value_type_t type,
                                         radio_system_protocol_nvs_value_t* out_value, size_t out_value_size) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t radio_system_protocol_nvs_write(const radio_system_protocol_nvs_value_t* value, size_t value_size) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t radio_system_protocol_nvs_delete(const char* namespace_name, const char* key,
                                           radio_system_protocol_nvs_value_type_t type) {
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
