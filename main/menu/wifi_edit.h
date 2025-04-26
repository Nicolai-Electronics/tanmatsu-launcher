#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_wifi_types_generic.h"
#include "gui_style.h"
#include "pax_types.h"

bool menu_wifi_edit(pax_buf_t* buffer, gui_theme_t* theme, uint8_t index, bool new_entry, char* new_ssid,
                    wifi_auth_mode_t authmode);
