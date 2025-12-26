#pragma once

#include "gui_style.h"
#include "pax_types.h"

void gui_progressbar_draw(pax_buf_t* pax_buffer, gui_theme_t* theme, float x, float y, float width, float height,
                          float progress);

void gui_progressbar_vertical_draw(pax_buf_t* pax_buffer, gui_theme_t* theme, float x, float y, float width,
                                   float height, float progress);
