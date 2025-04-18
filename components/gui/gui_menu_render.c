#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_menu.h"
#include "gui_style.h"
#include "pax_gfx.h"
#include "shapes/pax_rects.h"

/*void menu_render_header(pax_buf_t* pax_buffer, float position_x, float position_y, float width, float height,
                        float text_height, pax_col_t text_color, pax_col_t bg_color, pax_buf_t* icon, pax_font_t* font,
                        const char* label) {
    pax_simple_rect(pax_buffer, bg_color, position_x, position_y, width, height);
    // pax_clip(pax_buffer, position_x + 1, position_y + ((height - text_height) / 2) + 1, width - 2, text_height);
    pax_draw_text(pax_buffer, text_color, font, text_height, position_x + ((icon != NULL) ? 32 : 0) + 1,
                  position_y + ((height - text_height) / 2) + 1, label);
    if (icon != NULL) {
        // pax_clip(pax_buffer, position_x, position_y, height, height);
        pax_draw_image(pax_buffer, icon, position_x, position_y);
    }
    // pax_noclip(pax_buffer);
}

void menu_render_dialog(pax_buf_t* pax_buffer, menu_t* menu, menu_style_t* style) {
    // pax_noclip(pax_buffer);

    pax_simple_rect(pax_buffer, style->palette.color_background, position.x0, position.y0, (position.x1 - position.x0),
                    style->height);
    pax_outline_rect(pax_buffer, style->palette.color_foreground, position.x0, position.y0,
                     (position.x1 - position.x0) - 1, style->height - 1);
}*/

void menu_render_item(pax_buf_t* pax_buffer, menu_item_t* item, menu_style_t* style, pax_vec2_t position,
                      float current_position_y, bool selected) {
    float text_offset = ((style->entry_height - style->text_height) / 2) + 1;

    float icon_width = 0;
    if (item->icon != NULL) {
        icon_width = item->icon->width + 1;
    }

    int horizontal_padding = 7;

    if (selected) {
        pax_simple_rect(pax_buffer, style->palette.color_active_background, position.x0 + 1, current_position_y,
                        (position.x1 - position.x0) - 2, style->entry_height);
        pax_outline_rect(pax_buffer, style->palette.color_highlight_primary, position.x0 + 1, current_position_y + 1,
                         (position.x1 - position.x0) - 3, style->entry_height - 1);
        // pax_clip(pax_buffer, position.x0 + 1, current_position_y + text_offset, (position.x1 - position.x0) -
        // 4, style->text_height);
        pax_draw_text(pax_buffer, style->palette.color_active_foreground, style->font, style->text_height,
                      position.x0 + horizontal_padding + icon_width, current_position_y + text_offset, item->label);
        // pax_noclip(pax_buffer);
    } else {
        pax_simple_rect(pax_buffer, style->palette.color_background, position.x0 + 1, current_position_y,
                        (position.x1 - position.x0) - 2, style->entry_height);
        // pax_clip(pax_buffer, position.x0 + 1, current_position_y + text_offset, (position.x1 - position.x0) -
        // 4, style->text_height);
        pax_draw_text(pax_buffer, style->palette.color_foreground, style->font, style->text_height,
                      position.x0 + horizontal_padding + icon_width, current_position_y + text_offset, item->label);
        // pax_noclip(pax_buffer);
    }

    if (item->icon != NULL) {
        // pax_clip(pax_buffer, position.x0 + 1, current_position_y, style->entry_height, style->entry_height);
        pax_draw_image(pax_buffer, item->icon, position.x0 + 1, current_position_y);
        // pax_noclip(pax_buffer);
    }
}

void menu_render(pax_buf_t* pax_buffer, menu_t* menu, pax_vec2_t position, menu_style_t* style, bool partial) {
    float  remaining_height = position.y1 - position.y0;
    size_t max_items        = remaining_height / style->entry_height;

    size_t item_offset = 0;
    if (menu->position >= max_items) {
        item_offset = menu->position - max_items + 1;
    }

    for (size_t index = item_offset; (index < item_offset + max_items) && (index < menu->length); index++) {
        if (partial && index != menu->previous_position && index != menu->position) {
            continue;
        }
        float        current_position_y = position.y0 + style->entry_height * index;
        menu_item_t* item               = menu_find_item(menu, index);
        if (item == NULL) continue;
        menu_render_item(pax_buffer, item, style, position, current_position_y, index == menu->position);
    }

    if (style->scrollbar) {
        // pax_clip(pax_buffer, position.x0 + (position.x1 - position.x0) - 5, position.y0 +
        // style->entry_height, 4, style->height - 1 - style->entry_height);
        float fractionStart = item_offset / (menu->length * 1.0);
        float fractionEnd   = (item_offset + max_items) / (menu->length * 1.0);
        if (fractionEnd > 1.0) fractionEnd = 1.0;
        float scrollbarHeight = (position.y1 - position.y0) - 2;
        float scrollbarStart  = scrollbarHeight * fractionStart;
        float scrollbarEnd    = scrollbarHeight * fractionEnd;
        pax_simple_rect(pax_buffer, style->scrollbarBgColor, position.x1 - 5, position.y0 + 1, 4, scrollbarHeight);
        pax_simple_rect(pax_buffer, style->scrollbarFgColor, position.x1 - 5, position.y0 + 1 + scrollbarStart, 4,
                        scrollbarEnd - scrollbarStart);
        // pax_noclip(pax_buffer);
    }
}

void menu_render_grid(pax_buf_t* pax_buffer, menu_t* menu, pax_vec2_t position, menu_style_t* style, gui_theme_t* theme,
                      bool partial) {
    int entry_count_x = style->grid_entry_count_x;
    int entry_count_y = style->grid_entry_count_y;

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
            pax_simple_rect(pax_buffer, theme->palette.color_active_background, item_position_x, item_position_y,
                            entry_width, entry_height);
            pax_outline_rect(pax_buffer, theme->palette.color_highlight_primary, item_position_x + 1,
                             item_position_y + 1, entry_width - 2, entry_height - 2);
            // pax_clip(pax_buffer, item_position_x, item_position_y, entry_width, entry_height);
            pax_draw_text(pax_buffer, theme->palette.color_active_foreground, theme->menu.text_font,
                          theme->menu.text_height, item_position_x + ((entry_width - text_size.x) / 2),
                          item_position_y + text_offset, item->label);
        } else {
            pax_simple_rect(pax_buffer, theme->palette.color_background, item_position_x, item_position_y, entry_width,
                            entry_height);
            // pax_clip(pax_buffer, item_position_x, item_position_y, entry_width, entry_height);
            pax_draw_text(pax_buffer, theme->palette.color_foreground, theme->menu.text_font, theme->menu.text_height,
                          item_position_x + ((entry_width - text_size.x) / 2), item_position_y + text_offset,
                          item->label);
        }

        if (item->icon != NULL) {
            // pax_clip(pax_buffer, item_position_x + ((entry_width - icon_size) / 2), item_position_y, icon_size,
            // icon_size);
            pax_draw_image(pax_buffer, item->icon, item_position_x + ((entry_width - icon_size) / 2), item_position_y);
        }

        // pax_noclip(pax_buffer);
    }

    // pax_noclip(pax_buffer);
}