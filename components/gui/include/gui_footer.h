#pragma once

#include <sys/_intsup.h>
#include "gui_style.h"
#include "pax_types.h"
#ifdef __cplusplus
extern "C" {
#endif  //__cplusplus

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "gui_style.h"
#include "pax_gfx.h"

typedef struct {
    pax_buf_t* icon;
    char*      text;
} gui_header_field_t;

void gui_render_header_adv(pax_buf_t* pax_buffer, gui_theme_t* theme, gui_header_field_t* left, size_t left_count,
                           gui_header_field_t* right, size_t right_count);
void gui_render_footer_adv(pax_buf_t* pax_buffer, gui_theme_t* theme, gui_header_field_t* left, size_t left_count,
                           gui_header_field_t* right, size_t right_count);

#ifdef __cplusplus
}
#endif  //__cplusplus
