#pragma once

#include <stddef.h>
#include "gui_element_icontext.h"
#include "gui_style.h"
#include "pax_types.h"

void gui_header_draw(pax_buf_t* pax_buffer, gui_theme_t* theme, gui_element_icontext_t* left, size_t left_count,
                     gui_element_icontext_t* right, size_t right_count);
