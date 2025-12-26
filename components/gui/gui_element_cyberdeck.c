#include "gui_element_cyberdeck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_element_icontext.h"
#include "pax_gfx.h"
#include "shapes/pax_rects.h"

void gui_cyberdeck_draw_a(pax_buf_t* pax_buffer, gui_theme_t* theme, pax_col_t fill_color, pax_col_t line_color,
                          float x, float y, float width, float height, float tab_height, float tab_padding,
                          float corner_padding) {

    if (height < tab_height) {
        tab_height = height;
    }

    if (height < corner_padding) {
        corner_padding = height;
    }

    pax_vec2f points[] = {
        {x, y},                                           // Top left corner
        {x, y + tab_height},                              // Top corner of tab
        {x + tab_padding, y + tab_height + tab_padding},  // Bottom corner of tab
        {x + tab_padding, y + height},                    // Bottom left corner
        {x + width - corner_padding, y + height},         // Bottom right corner
        {x + width, y + height - corner_padding},         // Right corner of tab
        {x + width, y},                                   // Top right corner
    };
    pax_draw_shape(pax_buffer, fill_color, sizeof(points) / sizeof(pax_vec2f), points);

    pax_simple_line(pax_buffer, line_color, x, y, x, y + tab_height);
    pax_simple_line(pax_buffer, line_color, x, y + tab_height, x + tab_padding, y + tab_height + tab_padding);
    pax_simple_line(pax_buffer, line_color, x + tab_padding, y + tab_height + tab_padding, x + tab_padding, y + height);
    pax_simple_line(pax_buffer, line_color, x + tab_padding, y + height, x + width - corner_padding, y + height);
    pax_simple_line(pax_buffer, line_color, x + width - corner_padding, y + height, x + width,
                    y + height - corner_padding);
    pax_simple_line(pax_buffer, line_color, x + width, y + height - corner_padding, x + width, y);
    pax_simple_line(pax_buffer, line_color, x + width, y, x, y);
}

void gui_cyberdeck_draw_b(pax_buf_t* pax_buffer, gui_theme_t* theme, pax_col_t fill_color, pax_col_t line_color,
                          float x, float y, float width, float height, float corner_padding) {
    if (height < corner_padding) {
        corner_padding = height;
    }

    pax_vec2f points[] = {
        {x, y},                                    // Top left corner
        {x, y + height},                           // Bottom left corner
        {x + width - corner_padding, y + height},  // Bottom right corner
        {x + width, y + height - corner_padding},  // Right corner of tab
        {x + width, y},                            // Top right corner
    };
    pax_draw_shape(pax_buffer, fill_color, sizeof(points) / sizeof(pax_vec2f), points);

    pax_simple_line(pax_buffer, line_color, x, y, x, y + height);
    pax_simple_line(pax_buffer, line_color, x, y + height, x + width - corner_padding, y + height);
    pax_simple_line(pax_buffer, line_color, x + width - corner_padding, y + height, x + width,
                    y + height - corner_padding);
    pax_simple_line(pax_buffer, line_color, x + width, y + height - corner_padding, x + width, y);
    pax_simple_line(pax_buffer, line_color, x + width, y, x, y);
}

void gui_cyberdeck_draw_rect_corners(pax_buf_t* pax_buffer, gui_theme_t* theme, pax_col_t fill_color,
                                     pax_col_t line_color, float x, float y, float width, float height,
                                     float corner_size) {
    pax_simple_rect(pax_buffer, fill_color, x, y, width, height);

    pax_simple_line(pax_buffer, line_color, x, y, x + corner_size, y);
    pax_simple_line(pax_buffer, line_color, x, y, x, y + corner_size);

    pax_simple_line(pax_buffer, line_color, x + width - corner_size, y, x + width, y);
    pax_simple_line(pax_buffer, line_color, x + width, y, x + width, y + corner_size);

    pax_simple_line(pax_buffer, line_color, x, y + height, x + corner_size, y + height);
    pax_simple_line(pax_buffer, line_color, x, y + height, x, y + height - corner_size);

    pax_simple_line(pax_buffer, line_color, x + width - corner_size, y + height, x + width, y + height);
    pax_simple_line(pax_buffer, line_color, x + width, y + height - corner_size, x + width, y + height);
}
