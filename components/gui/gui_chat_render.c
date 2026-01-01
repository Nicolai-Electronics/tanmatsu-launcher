#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_element_cyberdeck.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"

void chat_render_message(pax_buf_t* pax_buffer, menu_item_t* item, gui_theme_t* theme, pax_vec2_t position,
                         float current_position_y, bool selected) {
    float icon_width = 32.0f;
    /*if (item->icon != NULL) {
        icon_width = item->icon->width + 1;
    }*/

    int horizontal_padding = 7;

    float width = position.x1 - position.x0;

    float x              = position.x0 + icon_width + (theme->chat.horizontal_margin / 2.0f) + 10.0f;
    float contact_name_x = x + 8.0f;
    float y              = current_position_y + (theme->chat.vertical_padding / 2.0f) + 18.0f;
    float contact_name_y = current_position_y + (theme->chat.vertical_padding / 2.0f);
    float w              = width - icon_width - theme->chat.horizontal_margin;
    float h              = theme->chat.list_entry_height - theme->chat.vertical_padding - 18.0f;
    float contact_name_h = theme->chat.list_entry_height - theme->chat.vertical_padding;

    pax_vec2f text_size   = pax_text_size(theme->chat.text_font, theme->chat.text_height, item->label);
    float     text_offset = ((h - text_size.y) / 2) + 1;

    pax_simple_rect(pax_buffer, theme->chat.palette.color_background, x, contact_name_y, w, contact_name_h);
    if (item->value != NULL) {
        pax_draw_text(pax_buffer, theme->chat.palette.color_active_foreground, theme->chat.text_font, 16.0f,
                      contact_name_x, contact_name_y, item->value);
    }

    if (selected) {
        gui_cyberdeck_draw_a(pax_buffer, theme, theme->chat.palette.color_active_background,
                             theme->chat.palette.color_highlight_primary, x, y, w, h, 12.0f, 10.0f, 24.0f);
        pax_draw_text(pax_buffer, theme->chat.palette.color_active_foreground, theme->chat.text_font,
                      theme->chat.text_height, position.x0 + horizontal_padding + icon_width + 32.0f, y + text_offset,
                      item->label);
    } else {
        gui_cyberdeck_draw_a(pax_buffer, theme, theme->chat.palette.color_background,
                             theme->chat.palette.color_highlight_secondary, x, y, w, h, 24.0f, 10.0f, 24.0f);
        pax_draw_text(pax_buffer, theme->chat.palette.color_foreground, theme->chat.text_font, theme->chat.text_height,
                      position.x0 + horizontal_padding + icon_width + 32.0f, y + text_offset, item->label);
    }

    if (item->icon != NULL) {
        pax_draw_image(pax_buffer, item->icon, position.x0 + 1 + (theme->chat.horizontal_margin / 2.0f),
                       y + ((h - pax_buf_get_height(item->icon)) / 2));
    }
}

void chat_render(pax_buf_t* pax_buffer, menu_t* menu, pax_vec2_t position, gui_theme_t* theme, bool partial) {
    float  remaining_height = position.y1 - position.y0;
    size_t max_items        = remaining_height / theme->chat.list_entry_height;

    size_t previous_navigation_position = menu->navigation_position;

    size_t first_visible_item = menu->navigation_position;
    size_t last_visible_item  = menu->navigation_position + max_items - 1;

    if (menu->position < first_visible_item) {
        menu->navigation_position = menu->position;
    }
    if (menu->position > menu->navigation_position + max_items - 1) {
        menu->navigation_position = menu->position - max_items + 1;
    }

    size_t item_offset = menu->navigation_position;

    pax_vec2_t position_item = position;
    if (menu->length > max_items) {
        position_item.x1 -= 8;
    }

    for (size_t index = item_offset; (index < item_offset + max_items) && (index < menu->length); index++) {
        if (partial && index != menu->previous_position && index != menu->position &&
            previous_navigation_position == menu->navigation_position && menu->length <= max_items) {
            continue;
        }
        float        current_position_y = position_item.y0 + theme->chat.list_entry_height * (index - item_offset);
        menu_item_t* item               = menu_find_item(menu, index);
        if (item == NULL) continue;
        chat_render_message(pax_buffer, item, theme, position_item, current_position_y, index == menu->position);
    }

    if (menu->length > max_items) {
        // pax_clip(pax_buffer, position.x0 + (position.x1 - position.x0) - 5, position.y0 +
        // theme->chat.height, 4, style->height - 1 - theme->chat.height);
        float fractionStart = item_offset / (menu->length * 1.0);
        float fractionEnd   = (item_offset + max_items) / (menu->length * 1.0);
        if (fractionEnd > 1.0) fractionEnd = 1.0;
        float scrollbarHeight = (position.y1 - position.y0) - 2;
        float scrollbarStart  = scrollbarHeight * fractionStart;
        float scrollbarEnd    = scrollbarHeight * fractionEnd;
        pax_simple_rect(pax_buffer, theme->chat.palette.color_active_background, position.x1 - 5, position.y0 + 1, 4,
                        scrollbarHeight);
        pax_simple_rect(pax_buffer, theme->chat.palette.color_highlight_primary, position.x1 - 5,
                        position.y0 + 1 + scrollbarStart, 4, scrollbarEnd - scrollbarStart);
        // pax_noclip(pax_buffer);
    }
}
