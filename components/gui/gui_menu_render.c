#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_menu.h"
#include "gui_style.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
// #include "shapes/pax_rects.h"

void menu_render_item(pax_buf_t* pax_buffer, menu_item_t* item, gui_theme_t* theme, pax_vec2_t position,
                      float current_position_y, bool selected) {
    float text_offset = ((theme->menu.list_entry_height - theme->menu.text_height) / 2) + 1;

    float icon_width = 0;
    if (item->icon != NULL) {
        icon_width = item->icon->width + 1;
    }

    int horizontal_padding = 7;

    float width = position.x1 - position.x0;

    if (item->value != NULL) {
        float xl = position.x0 + 1;
        float xv = xl + (width / 2);
        float y  = current_position_y;
        float w  = (width / 2);
        float h  = theme->menu.list_entry_height;
        pax_simple_rect(pax_buffer, theme->menu.palette.color_background, xl, y, w, h);
        pax_draw_text(pax_buffer, theme->menu.palette.color_foreground, theme->menu.text_font, theme->menu.text_height,
                      xl + horizontal_padding + icon_width, y + text_offset, item->label);
        if (selected) {
            pax_simple_rect(pax_buffer, theme->menu.palette.color_active_background, xv, y, w, h);
            pax_outline_rect(pax_buffer, theme->menu.palette.color_highlight_primary, xv + 1, y + 1, w - 2, h - 2);
            pax_draw_text(pax_buffer, theme->menu.palette.color_active_foreground, theme->menu.text_font,
                          theme->menu.text_height, xv + horizontal_padding, y + text_offset, item->value);
        } else {
            pax_simple_rect(pax_buffer, theme->menu.palette.color_background, xv, y, w, h);
            pax_draw_text(pax_buffer, theme->menu.palette.color_foreground, theme->menu.text_font,
                          theme->menu.text_height, xv + horizontal_padding, y + text_offset, item->value);
        }
    } else {
        if (selected) {
            pax_simple_rect(pax_buffer, theme->menu.palette.color_active_background, position.x0 + 1,
                            current_position_y, (width)-2, theme->menu.list_entry_height);
            pax_outline_rect(pax_buffer, theme->menu.palette.color_highlight_primary, position.x0 + 1,
                             current_position_y + 1, (width)-3, theme->menu.list_entry_height - 1);
            // pax_clip(pax_buffer, position.x0 + 1, current_position_y + text_offset, (width) -
            // 4, theme->menu.text_height);
            pax_draw_text(pax_buffer, theme->menu.palette.color_active_foreground, theme->menu.text_font,
                          theme->menu.text_height, position.x0 + horizontal_padding + icon_width,
                          current_position_y + text_offset, item->label);
            // pax_noclip(pax_buffer);
        } else {
            pax_simple_rect(pax_buffer, theme->menu.palette.color_background, position.x0 + 1, current_position_y,
                            (width)-2, theme->menu.list_entry_height);
            // pax_clip(pax_buffer, position.x0 + 1, current_position_y + text_offset, (width) -
            // 4, theme->menu.text_height);
            pax_draw_text(pax_buffer, theme->menu.palette.color_foreground, theme->menu.text_font,
                          theme->menu.text_height, position.x0 + horizontal_padding + icon_width,
                          current_position_y + text_offset, item->label);
            // pax_noclip(pax_buffer);
        }
    }

    if (item->icon != NULL) {
        // pax_clip(pax_buffer, position.x0 + 1, current_position_y, theme->menu.list_entry_height,
        // theme->menu.list_entry_height);
        pax_draw_image(pax_buffer, item->icon, position.x0 + 1,
                       current_position_y + ((theme->menu.list_entry_height - pax_buf_get_height(item->icon)) / 2));
        // pax_noclip(pax_buffer);
    }
}

void menu_render(pax_buf_t* pax_buffer, menu_t* menu, pax_vec2_t position, gui_theme_t* theme, bool partial) {
    float  remaining_height = position.y1 - position.y0;
    size_t max_items        = remaining_height / theme->menu.list_entry_height;

    /*size_t item_offset = 0;
    if (menu->position >= max_items) {
        item_offset = menu->position - max_items + 1;
    }*/

    size_t previous_navigation_position = menu->navigation_position;

    size_t first_visible_item = menu->navigation_position;
    size_t last_visible_item  = menu->navigation_position + max_items - 1;

    /*printf("Position: %zu, Navigation Position: %zu, First Visible Item: %zu, Last Visible Item: %zu\n",
       menu->position, menu->navigation_position, first_visible_item, last_visible_item);*/

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
        float        current_position_y = position_item.y0 + theme->menu.list_entry_height * (index - item_offset);
        menu_item_t* item               = menu_find_item(menu, index);
        if (item == NULL) continue;
        menu_render_item(pax_buffer, item, theme, position_item, current_position_y, index == menu->position);
    }

    if (menu->length > max_items) {
        // pax_clip(pax_buffer, position.x0 + (position.x1 - position.x0) - 5, position.y0 +
        // theme->menu.height, 4, style->height - 1 - theme->menu.height);
        float fractionStart = item_offset / (menu->length * 1.0);
        float fractionEnd   = (item_offset + max_items) / (menu->length * 1.0);
        if (fractionEnd > 1.0) fractionEnd = 1.0;
        float scrollbarHeight = (position.y1 - position.y0) - 2;
        float scrollbarStart  = scrollbarHeight * fractionStart;
        float scrollbarEnd    = scrollbarHeight * fractionEnd;
        pax_simple_rect(pax_buffer, theme->menu.palette.color_active_background, position.x1 - 5, position.y0 + 1, 4,
                        scrollbarHeight);
        pax_simple_rect(pax_buffer, theme->menu.palette.color_highlight_primary, position.x1 - 5,
                        position.y0 + 1 + scrollbarStart, 4, scrollbarEnd - scrollbarStart);
        // pax_noclip(pax_buffer);
    }
}

void menu_get_draw_position(menu_t* menu, pax_vec2_t position, gui_theme_t* theme, int index, pax_vec2_t* output) {
    float  remaining_height = position.y1 - position.y0;
    size_t max_items        = remaining_height / theme->menu.list_entry_height;

    size_t first_visible_item = menu->navigation_position;

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

    output->x0 = position_item.x0 + ((position_item.x1 - position_item.x0) / 2);
    output->y0 = position_item.y0 + theme->menu.list_entry_height * (index - item_offset);
    output->x1 = position_item.x1;
    output->y1 = position_item.y0 + theme->menu.list_entry_height * (index - item_offset + 1);
}

void menu_render_grid(pax_buf_t* pax_buffer, menu_t* menu, pax_vec2_t position, gui_theme_t* theme, bool partial) {
    int entry_count_x = theme->menu.grid_horizontal_count;
    int entry_count_y = theme->menu.grid_vertical_count;

    float entry_width =
        ((position.x1 - position.x0) - (theme->menu.horizontal_margin * (entry_count_x + 1))) / entry_count_x;
    float entry_height =
        ((position.y1 - position.y0) - (theme->menu.vertical_margin * (entry_count_y + 1))) / entry_count_y;

    size_t max_items = entry_count_x * entry_count_y;

    // pax_noclip(pax_buffer);

    size_t item_offset = 0;
    if (menu->position >= max_items) {
        item_offset = menu->position - max_items + 1;
    }

    for (size_t index = item_offset; (index < item_offset + max_items) && (index < menu->length); index++) {
        if (partial && index != menu->previous_position && index != menu->position) {
            continue;
        }

        menu_item_t* item = menu_find_item(menu, index);
        if (item == NULL) {
            printf("Render error: item is NULL at %u\n", index);
            break;
        }

        size_t item_position = index - item_offset;

        float item_position_x = position.x0 + theme->menu.horizontal_margin +
                                ((item_position % entry_count_x) * (entry_width + theme->menu.horizontal_margin));
        float item_position_y = position.y0 + theme->menu.vertical_margin +
                                ((item_position / entry_count_x) * (entry_height + theme->menu.vertical_margin));

        float icon_size   = (item->icon != NULL) ? 33 : 0;
        float text_offset = ((entry_height - theme->menu.text_height - icon_size) / 2) + icon_size + 1;

        pax_vec1_t text_size = pax_text_size(theme->menu.text_font, theme->menu.text_height, item->label);
        if (index == menu->position) {
            pax_simple_rect(pax_buffer, theme->menu.palette.color_active_background, item_position_x, item_position_y,
                            entry_width, entry_height);
            pax_outline_rect(pax_buffer, theme->menu.palette.color_highlight_primary, item_position_x + 1,
                             item_position_y + 1, entry_width - 2, entry_height - 2);
            // pax_clip(pax_buffer, item_position_x, item_position_y, entry_width, entry_height);
            pax_draw_text(pax_buffer, theme->menu.palette.color_active_foreground, theme->menu.text_font,
                          theme->menu.text_height, item_position_x + ((entry_width - text_size.x) / 2),
                          item_position_y + text_offset, item->label);
        } else {
            pax_simple_rect(pax_buffer, theme->menu.palette.color_background, item_position_x, item_position_y,
                            entry_width, entry_height);
            // pax_clip(pax_buffer, item_position_x, item_position_y, entry_width, entry_height);
            pax_draw_text(pax_buffer, theme->menu.palette.color_foreground, theme->menu.text_font,
                          theme->menu.text_height, item_position_x + ((entry_width - text_size.x) / 2),
                          item_position_y + text_offset, item->label);
        }

        if (item->icon != NULL) {
            // pax_clip(pax_buffer, item_position_x + ((entry_width - icon_size) / 2), item_position_y, icon_size,
            // icon_size);
            pax_draw_image(pax_buffer, item->icon, item_position_x + ((entry_width - icon_size) / 2),
                           item_position_y + ((text_offset - icon_size - 1) / 2));
        }

        // pax_noclip(pax_buffer);
    }

    // pax_noclip(pax_buffer);
}