// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include "esp_err.h"

// Initialize the airplane mode communication channel with the radio coprocessor.
esp_err_t airplane_mode_init(void);

// Query the current airplane mode state from the radio coprocessor.
esp_err_t airplane_mode_get(bool* out_enabled);

// Set the airplane mode state on the radio coprocessor.
esp_err_t airplane_mode_set(bool enabled);
