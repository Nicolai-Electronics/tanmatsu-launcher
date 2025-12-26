#pragma once

#include "gui_style.h"
#include "pax_types.h"

void gui_cyberdeck_draw_a(pax_buf_t* pax_buffer, gui_theme_t* theme, pax_col_t fill_color, pax_col_t line_color,
                          float x, float y, float width, float height, float tab_height, float tab_padding,
                          float corner_padding);
void gui_cyberdeck_draw_b(pax_buf_t* pax_buffer, gui_theme_t* theme, pax_col_t fill_color, pax_col_t line_color,
                          float x, float y, float width, float height, float corner_padding);

void gui_cyberdeck_draw_rect_corners(pax_buf_t* pax_buffer, gui_theme_t* theme, pax_col_t fill_color,
                                     pax_col_t line_color, float x, float y, float width, float height,
                                     float corner_size);
