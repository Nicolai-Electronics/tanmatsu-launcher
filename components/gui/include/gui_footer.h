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
} gui_icontext_t;

void gui_render_header(pax_buf_t* pax_buffer, gui_theme_t* theme, const char* text);
void gui_render_header_adv(pax_buf_t* pax_buffer, gui_theme_t* theme, gui_icontext_t* icontext, size_t icontext_count);
void gui_render_footer(pax_buf_t* pax_buffer, gui_theme_t* theme, const char* left_text, const char* right_text);
void gui_render_footer_adv(pax_buf_t* pax_buffer, gui_theme_t* theme, gui_icontext_t* icontext, size_t icontext_count,
                           char* right_text);

#ifdef __cplusplus
}
#endif  //__cplusplus
