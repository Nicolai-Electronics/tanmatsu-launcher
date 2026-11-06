#pragma once

#include <stddef.h>
#include "gui_element_icontext.h"
#include "gui_style.h"
#include "pax_types.h"

void gui_screen_draw(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                     gui_element_icontext_t* header_left, size_t header_left_count,
                     gui_element_icontext_t* header_right, size_t header_right_count,
                     gui_element_icontext_t* footer_left, size_t footer_left_count,
                     gui_element_icontext_t* footer_right, size_t footer_right_count);
