#pragma once

#include "gui_style.h"
#include "pax_types.h"

void menu_textbox(pax_buf_t* buffer, gui_theme_t* theme, const char* title, char* text, size_t size, bool multiline,
                   bool* out_accepted);
