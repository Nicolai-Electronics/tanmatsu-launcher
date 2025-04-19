#include "gui_footer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"

static float render_icon_text(pax_buf_t* pax_buffer, gui_theme_t* theme, int x, int y, gui_icontext_t* content,
                              float padding) {
    float icon_width = 0;
    if (content->icon) {
        icon_width        = pax_buf_get_width(content->icon);
        float icon_height = pax_buf_get_height(content->icon);
        pax_draw_image(pax_buffer, content->icon, x, y + (theme->footer.height - icon_height) / 2);
    }
    pax_draw_text(pax_buffer, theme->palette.color_foreground, theme->footer.text_font, theme->footer.text_height,
                  x + icon_width + padding, y + (theme->footer.height - theme->footer.text_height) / 2, content->text);
    pax_vec2f text_size = pax_text_size(theme->footer.text_font, theme->footer.text_height, content->text);
    return icon_width + padding + text_size.x;
}

void gui_render_header(pax_buf_t* pax_buffer, gui_theme_t* theme, const char* text) {
    gui_element_style_t* style = &theme->header;

    int buffer_width = pax_buf_get_width(pax_buffer);
    int box_height   = style->height + (style->vertical_margin * 2);

    pax_draw_line(pax_buffer, theme->palette.color_foreground, style->horizontal_margin, box_height,
                  buffer_width - style->horizontal_margin, box_height);
    pax_draw_text(pax_buffer, theme->palette.color_foreground, style->text_font, style->text_height,
                  style->horizontal_margin + style->horizontal_padding, (box_height - style->text_height) / 2, text);
}

void gui_render_header_adv(pax_buf_t* pax_buffer, gui_theme_t* theme, gui_icontext_t* icontext, size_t icontext_count) {
    gui_element_style_t* style = &theme->header;

    int buffer_width = pax_buf_get_width(pax_buffer);
    int box_height   = style->height + (style->vertical_margin * 2);

    pax_draw_line(pax_buffer, theme->palette.color_foreground, style->horizontal_margin, box_height,
                  buffer_width - style->horizontal_margin, box_height);

    int x = style->horizontal_margin + style->horizontal_padding;
    for (size_t i = 0; i < icontext_count; i++) {
        x += render_icon_text(pax_buffer, theme, x, style->vertical_margin, &icontext[i], style->horizontal_padding);
    }
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
    /*pax_right_text(pax_buffer, theme->palette.color_foreground, style->text_font, style->text_height,
                   buffer_width - style->horizontal_margin - style->vertical_padding,
                   buffer_height - box_height + (box_height - style->text_height) / 2, right_text);*/
    pax_vec2f text_width = pax_text_size(style->text_font, style->text_height, right_text);
    pax_draw_text(pax_buffer, theme->palette.color_foreground, style->text_font, style->text_height,
                  buffer_width - style->horizontal_margin - style->vertical_padding - text_width.x,
                  buffer_height - box_height + (box_height - style->text_height) / 2, right_text);
}

void gui_render_footer_adv(pax_buf_t* pax_buffer, gui_theme_t* theme, gui_icontext_t* icontext, size_t icontext_count,
                           char* right_text) {
    gui_element_style_t* style         = &theme->footer;
    int                  buffer_width  = pax_buf_get_width(pax_buffer);
    int                  buffer_height = pax_buf_get_height(pax_buffer);
    int                  box_height    = style->height + (style->vertical_margin * 2);

    pax_draw_line(pax_buffer, theme->palette.color_foreground, style->horizontal_margin, buffer_height - box_height,
                  buffer_width - style->horizontal_margin, buffer_height - box_height);

    int x = style->horizontal_margin + style->horizontal_padding;
    for (size_t i = 0; i < icontext_count; i++) {
        x += render_icon_text(pax_buffer, theme, x, buffer_height - style->height - style->vertical_margin,
                              &icontext[i], style->horizontal_padding);
    }

    pax_vec2f text_width = pax_text_size(style->text_font, style->text_height, right_text);
    pax_draw_text(pax_buffer, theme->palette.color_foreground, style->text_font, style->text_height,
                  buffer_width - style->horizontal_margin - style->vertical_padding - text_width.x,
                  buffer_height - box_height + (box_height - style->text_height) / 2, right_text);
}