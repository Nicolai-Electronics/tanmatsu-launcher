#pragma once

#include <stdbool.h>
#include "bsp/device.h"
#include "esp_err.h"

bool device_has_lora(void);
bool device_has_provisioning(void);
