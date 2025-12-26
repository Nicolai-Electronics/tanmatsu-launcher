#include "gui_element_progressbar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_element_icontext.h"
#include "pax_gfx.h"
#include "shapes/pax_rects.h"

void gui_progressbar_draw(pax_buf_t* pax_buffer, gui_theme_t* theme, float x, float y, float width, float height,
                          float progress) {
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    gui_progressbar_style_t* style = &theme->progressbar;
    if (width < style->horizontal_margin) return;
    if (height < style->vertical_margin) return;
    width  -= style->horizontal_margin;
    height -= style->vertical_margin;
    x      += style->horizontal_margin / 2.0f;
    y      += style->vertical_margin / 2.0f;

    pax_draw_rect(pax_buffer, style->palette.color_background, x, y, width, height);
    pax_outline_rect(pax_buffer, style->palette.color_foreground, x, y, width, height);

    if (width < style->horizontal_padding) return;
    if (height < style->vertical_padding) return;
    width  -= style->horizontal_padding;
    height -= style->vertical_padding;
    x      += style->horizontal_padding / 2.0f;
    y      += style->vertical_padding / 2.0f;

    pax_draw_rect(pax_buffer, style->palette.color_active_background, x, y, width, height);
    pax_draw_rect(pax_buffer, style->palette.color_active_foreground, x, y, width * progress, height);
}

void gui_progressbar_vertical_draw(pax_buf_t* pax_buffer, gui_theme_t* theme, float x, float y, float width,
                                   float height, float progress) {
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    gui_progressbar_style_t* style = &theme->progressbar;
    if (width < style->vertical_margin) return;
    if (height < style->horizontal_margin) return;
    width  -= style->vertical_margin;
    height -= style->horizontal_margin;
    x      += style->vertical_margin / 2.0f;
    y      += style->horizontal_margin / 2.0f;

    pax_draw_rect(pax_buffer, style->palette.color_background, x, y, width, height);
    pax_outline_rect(pax_buffer, style->palette.color_foreground, x, y, width, height);

    if (width < style->vertical_padding) return;
    if (height < style->horizontal_padding) return;
    width  -= style->vertical_padding;
    height -= style->horizontal_padding;
    x      += style->vertical_padding / 2.0f;
    y      += style->horizontal_padding / 2.0f;

    pax_draw_rect(pax_buffer, style->palette.color_active_background, x, y, width, height);
    pax_draw_rect(pax_buffer, style->palette.color_active_foreground, x, y + height - (height * progress), width,
                  height * progress);
}
