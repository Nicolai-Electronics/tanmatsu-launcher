#include <stdint.h>
#include "esp_err.h"

typedef enum {
    RADIO_SYSTEM_PROTOCOL_TYPE_ACK             = 0x00,
    RADIO_SYSTEM_PROTOCOL_TYPE_NACK            = 0x01,
    RADIO_SYSTEM_PROTOCOL_TYPE_GET_INFORMATION = 0x02,
} radio_system_protocol_packet_type_t;

typedef struct {
    uint32_t sequence_number;
    uint32_t type;  // radio_system_protocol_packet_type_t
} __attribute__((packed)) radio_system_protocol_header_t;

typedef struct {
    char firmware_name[32];
    char firmware_version[32];
    char firmware_build_date[16];
    char firmware_build_time[16];
    char firmware_idf_version[32];
    char firmware_sha256[32];
} __attribute__((packed)) radio_system_protocol_information_t;

esp_err_t radio_system_protocol_init(void);
esp_err_t radio_system_protocol_get_information(radio_system_protocol_information_t* out_information);
