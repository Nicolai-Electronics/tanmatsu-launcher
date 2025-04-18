#pragma once

#include "pax_matrix.h"
#ifdef __cplusplus
extern "C" {
#endif  //__cplusplus

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "gui_style.h"
#include "pax_gfx.h"

typedef struct {
    // List entry
    float entry_height;
    float text_height;

    // Grid entry
    float grid_margin_x;
    float grid_margin_y;
    float grid_entry_count_x;
    float grid_entry_count_y;

    // Colors
    gui_palette_t palette;

    pax_col_t fgColor;
    pax_col_t bgColor;
    pax_col_t selectedItemColor;
    pax_col_t bgTextColor;
    pax_col_t borderColor;
    pax_col_t scrollbarBgColor;
    pax_col_t scrollbarFgColor;

    // Scrollbar
    bool scrollbar;

    // Font
    pax_font_t* font;
} menu_style_t;

typedef bool (*menu_callback_t)();

typedef struct _menu_item {
    char*           label;
    menu_callback_t callback;
    void*           callback_arguments;

    pax_buf_t* icon;

    // Linked list
    struct _menu_item* previousItem;
    struct _menu_item* nextItem;
} menu_item_t;

typedef struct menu {
    // Header
    char*        title;
    pax_buf_t*   icon;
    menu_item_t* firstItem;
    size_t       length;
    size_t       position;
    size_t       previous_position;
    menu_style_t style;
} menu_t;

void         menu_initialize(menu_t* menu, char* title, pax_buf_t* icon);
void         menu_free(menu_t* menu);
menu_item_t* menu_find_item(menu_t* menu, size_t position);
menu_item_t* menu_find_last_item(menu_t* menu);
bool         menu_insert_item(menu_t* menu, const char* label, menu_callback_t callback, void* callback_arguments,
                              size_t position);
bool         menu_insert_item_icon(menu_t* menu, const char* label, menu_callback_t callback, void* callback_arguments,
                                   size_t position, pax_buf_t* icon);
bool         menu_remove_item(menu_t* menu, size_t position);
bool         menu_navigate_to(menu_t* menu, size_t position);
void         menu_navigate_previous(menu_t* menu);
void         menu_navigate_next(menu_t* menu);
void         menu_navigate_previous_row(menu_t* menu, menu_style_t* style);
void         menu_navigate_next_row(menu_t* menu, menu_style_t* style);
size_t       menu_get_position(menu_t* menu);
void         menu_set_position(menu_t* menu, size_t position);
size_t       menu_get_length(menu_t* menu);
void*        menu_get_callback_args(menu_t* menu, size_t position);
pax_buf_t*   menu_get_icon(menu_t* menu, size_t position);

void menu_render(pax_buf_t* pax_buffer, menu_t* menu, pax_vec2_t position, menu_style_t* style, bool partial);
void menu_render_grid(pax_buf_t* pax_buffer, menu_t* menu, pax_vec2_t position, menu_style_t* style, gui_theme_t* theme,
                      bool partial);

#ifdef __cplusplus
}
#endif  //__cplusplus
