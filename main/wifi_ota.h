#pragma once

#include <stdint.h>

typedef void (*ota_status_cb_t)(const char* status_text, uint8_t progress);

void ota_update(char* ota_url, ota_status_cb_t status_cb);
