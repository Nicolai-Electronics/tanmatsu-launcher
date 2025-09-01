#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t ntp_start_service(const char* server);
esp_err_t ntp_sync_wait(void);
bool      ntp_get_enabled(void);
void      ntp_set_enabled(bool enabled);
