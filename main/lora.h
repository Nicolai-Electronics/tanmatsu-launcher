#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lora_protocol.h"

esp_err_t lora_transaction_receive(uint8_t* packet, size_t length);
esp_err_t lora_init(QueueHandle_t packet_queue);

esp_err_t lora_get_mode(lora_protocol_mode_t* out_mode);
esp_err_t lora_set_mode(const lora_protocol_mode_t mode);

esp_err_t lora_get_config(lora_protocol_config_params_t* out_config);
esp_err_t lora_set_config(const lora_protocol_config_params_t* config);

esp_err_t lora_get_status(lora_protocol_status_params_t* out_status);

esp_err_t lora_send_packet(const lora_protocol_lora_packet_t* packet);
esp_err_t lora_receive_packet(uint8_t* out_buffer, uint8_t* out_length, uint16_t max_length);
