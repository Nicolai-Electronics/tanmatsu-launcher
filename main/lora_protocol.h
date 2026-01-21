#pragma once
#include <stdbool.h>
#include <stdint.h>

#define LORA_PROTOCOL_VERSION_STRING_LENGTH 16

typedef enum {
    LORA_PROTOCOL_TYPE_ACK        = 0x00,
    LORA_PROTOCOL_TYPE_NACK       = 0x01,
    LORA_PROTOCOL_TYPE_GET_MODE   = 0x02,
    LORA_PROTOCOL_TYPE_SET_MODE   = 0x03,
    LORA_PROTOCOL_TYPE_GET_CONFIG = 0x04,
    LORA_PROTOCOL_TYPE_SET_CONFIG = 0x05,
    LORA_PROTOCOL_TYPE_GET_STATUS = 0x06,
    LORA_PROTOCOL_TYPE_PACKET_RX  = 0x07,
    LORA_PROTOCOL_TYPE_PACKET_TX  = 0x08,
} lora_protocol_packet_type_t;

typedef enum {
    LORA_PROTOCOL_MODE_UNKNOWN      = 0x00,
    LORA_PROTOCOL_MODE_STANDBY_RC   = 0x01,
    LORA_PROTOCOL_MODE_STANDBY_XOSC = 0x02,
    LORA_PROTOCOL_MODE_FS           = 0x03,
    LORA_PROTOCOL_MODE_TX           = 0x04,
    LORA_PROTOCOL_MODE_RX           = 0x05,
} lora_protocol_mode_t;

typedef enum {
    LORA_PROTOCOL_CHIP_SX1262 = 0x00,
    LORA_PROTOCOL_CHIP_SX1268 = 0x01,
} lora_protocol_chip_t;

typedef struct {
    lora_protocol_mode_t mode;
} __attribute__((packed)) lora_protocol_mode_params_t;

typedef struct {
    float    frequency;                   // Frequency in MHz
    uint8_t  spreading_factor;            // 5-12
    uint16_t bandwidth;                   // 7, 10,15, 20, 31, 41, 62, 125, 250, 500 kHz
    uint8_t  coding_rate;                 // 5-8 (4/5 to 4/8)
    uint8_t  sync_word;                   // Sync word
    uint16_t preamble_length;             // Preamble length in symbols
    uint8_t  power;                       // TX Power in dBm
    uint8_t  ramp_time;                   // Microseconds
    bool     crc_enabled;                 // CRC enabled/disabled
    bool     invert_iq;                   // Invert IQ enabled/disabled
    bool     low_data_rate_optimization;  // Low data rate optimization enabled/disabled
} __attribute__((packed)) lora_protocol_config_params_t;

typedef struct {
    uint16_t             errors;
    lora_protocol_chip_t chip_type;
    char                 version_string[LORA_PROTOCOL_VERSION_STRING_LENGTH];
} __attribute__((packed)) lora_protocol_status_params_t;

typedef struct {
    uint8_t length;
    uint8_t data[];
} __attribute__((packed)) lora_protocol_lora_packet_t;

typedef struct {
    uint32_t sequence_number;
    uint32_t type;  // lora_protocol_packet_type_t
} __attribute__((packed)) lora_protocol_header_t;
