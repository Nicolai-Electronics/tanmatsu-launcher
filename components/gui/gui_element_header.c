#include "gui_element_header.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_element_icontext.h"
#include "pax_gfx.h"

void gui_header_draw(pax_buf_t* pax_buffer, gui_theme_t* theme, gui_element_icontext_t* left, size_t left_count,
                     gui_element_icontext_t* right, size_t right_count) {
    gui_element_style_t* style = &theme->header;

    int buffer_width = pax_buf_get_width(pax_buffer);
    int box_height   = style->height + (style->vertical_margin * 2);

    pax_draw_rect(pax_buffer, style->palette.color_background, style->horizontal_margin, style->vertical_margin,
                  buffer_width - (style->horizontal_margin * 2), box_height);

    pax_draw_line(pax_buffer, style->palette.color_foreground, style->horizontal_margin, box_height,
                  buffer_width - (style->horizontal_margin * 2), box_height);

    int x = style->horizontal_margin + style->horizontal_padding;
    for (size_t i = 0; i < left_count; i++) {
        x += gui_icontext_draw(pax_buffer, style, x, 0, &left[i], style->horizontal_padding, box_height);
    }

    x = buffer_width - style->horizontal_margin;
    for (size_t i = 0; i < right_count; i++) {
        x -= gui_icontext_width(style, buffer_width, 0, &right[i], style->horizontal_padding);
        gui_icontext_draw(pax_buffer, style, x - style->horizontal_padding, 0, &right[i], style->horizontal_padding,
                          box_height);
    }
}
