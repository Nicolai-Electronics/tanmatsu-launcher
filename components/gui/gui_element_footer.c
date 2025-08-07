#include "gui_element_footer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_element_icontext.h"
#include "pax_gfx.h"

void gui_footer_draw(pax_buf_t* pax_buffer, gui_theme_t* theme, gui_element_icontext_t* left, size_t left_count,
                     gui_element_icontext_t* right, size_t right_count) {
    gui_element_style_t* style         = &theme->footer;
    int                  buffer_width  = pax_buf_get_width(pax_buffer);
    int                  buffer_height = pax_buf_get_height(pax_buffer);
    int                  box_height    = style->height + (style->vertical_margin * 2);

    pax_draw_line(pax_buffer, theme->palette.color_foreground, style->horizontal_margin, buffer_height - box_height,
                  buffer_width - style->horizontal_margin, buffer_height - box_height);

    int x = style->horizontal_margin + style->horizontal_padding;
    for (size_t i = 0; i < left_count; i++) {
        x += gui_icontext_draw(pax_buffer, theme, x, buffer_height - box_height, &left[i], style->horizontal_padding,
                               box_height);
    }

    x = buffer_width - style->horizontal_margin;
    for (size_t i = 0; i < right_count; i++) {
        x -= gui_icontext_width(theme, buffer_width, buffer_height, &right[i], style->horizontal_padding);
        gui_icontext_draw(pax_buffer, theme, x - style->horizontal_padding, buffer_height - box_height, &right[i],
                          style->horizontal_padding, box_height);
    }
}