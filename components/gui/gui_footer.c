#include "gui_footer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pax_gfx.h"

void gui_render_header(pax_buf_t* pax_buffer, gui_theme_t* theme, const char* text) {
    gui_element_style_t* style = &theme->header;

    int buffer_width = pax_buf_get_width(pax_buffer);
    int box_height   = style->height + (style->vertical_margin * 2);

    pax_draw_line(pax_buffer, theme->palette.color_foreground, style->horizontal_margin, box_height,
                  buffer_width - style->horizontal_margin, box_height);
    pax_draw_text(pax_buffer, theme->palette.color_foreground, style->text_font, style->text_height,
                  style->horizontal_margin + style->vertical_padding, (box_height - style->text_height) / 2, text);
}

void gui_render_footer(pax_buf_t* pax_buffer, gui_theme_t* theme, const char* left_text, const char* right_text) {
    gui_element_style_t* style         = &theme->footer;
    int                  buffer_width  = pax_buf_get_width(pax_buffer);
    int                  buffer_height = pax_buf_get_height(pax_buffer);

    int box_height = style->height + (style->vertical_margin * 2);
    pax_draw_line(pax_buffer, theme->palette.color_foreground, style->horizontal_margin, buffer_height - box_height,
                  buffer_width - style->horizontal_margin, buffer_height - box_height);
    pax_draw_text(pax_buffer, theme->palette.color_foreground, style->text_font, style->text_height,
                  style->horizontal_margin + style->vertical_padding,
                  buffer_height - box_height + (box_height - style->text_height) / 2, left_text);
    pax_right_text(pax_buffer, theme->palette.color_foreground, style->text_font, style->text_height,
                   buffer_width - style->horizontal_margin - style->vertical_padding,
                   buffer_height - box_height + (box_height - style->text_height) / 2, right_text);
}
