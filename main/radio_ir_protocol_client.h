#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    IR_PROTOCOL_CLIENT_TYPE_ACK           = 0x00,
    IR_PROTOCOL_CLIENT_TYPE_NACK          = 0x01,
    IR_PROTOCOL_CLIENT_TYPE_GET_SUPPORTED = 0x02,
    IR_PROTOCOL_CLIENT_TYPE_SET_CONFIG    = 0x03,
    IR_PROTOCOL_CLIENT_TYPE_SEND_NEC      = 0x10,
} ir_protocol_client_packet_type_t;

typedef struct {
    uint32_t sequence_number;
    uint32_t type;  // ir_protocol_client_packet_type_t
} __attribute__((packed)) ir_protocol_client_header_t;

typedef struct {
    uint32_t frequency_hz;
    uint8_t  duty_cycle;
} __attribute__((packed)) ir_protocol_client_config_t;

typedef struct {
    uint16_t address;
    uint16_t command;
} __attribute__((packed)) ir_protocol_client_nec_scan_code_t;

// Functions

esp_err_t ir_protocol_client_init(void);
esp_err_t ir_protocol_client_get_supported(void);
esp_err_t ir_protocol_client_set_config(const ir_protocol_client_config_t* config);
esp_err_t ir_protocol_client_send_nec(const ir_protocol_client_nec_scan_code_t* scan_code);
