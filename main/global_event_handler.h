#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t global_event_handler_initialize(void);

// True when headphones are plugged into the audio jack, which is also the
// state that decides whether the speaker amplifier is enabled.
bool global_event_handler_headphones_inserted(void);

// Volume currently applied to the active output, in percent. Speaker and
// headphone volume are stored separately; this returns the one in use.
uint8_t global_event_handler_get_volume(void);
