#pragma once

#include <stdbool.h>
#include "esp_err.h"

bool      wifi_remote_get_initialized(void);
esp_err_t wifi_remote_initialize(void);
