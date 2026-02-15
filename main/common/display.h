#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "pax_types.h"

esp_err_t  display_init(void);
pax_buf_t* display_get_buffer(void);
void       display_blit_buffer(pax_buf_t* fb);
void       display_blit(void);
bool       display_is_initialized(void);
bool       display_is_epaper(void);
