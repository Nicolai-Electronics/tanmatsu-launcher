#pragma once

#include <stddef.h>
#include "gui_style.h"
#include "pax_types.h"

typedef struct {
    pax_buf_t* icon;
    char*      text;
} gui_element_icontext_t;

float gui_icontext_width(gui_element_style_t* style, int x, int y, gui_element_icontext_t* content, float padding);
float gui_icontext_draw(pax_buf_t* pax_buffer, gui_element_style_t* style, int x, int y,
                        gui_element_icontext_t* content, float padding, int box_height);
