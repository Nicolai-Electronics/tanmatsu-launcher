#pragma once

#include "pax_types.h"

void       display_init(void);
pax_buf_t* display_get_buffer(void);
void       display_blit_buffer(pax_buf_t* fb);
void       display_blit(void);