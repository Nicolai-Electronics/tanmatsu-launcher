#pragma once

#include "pax_types.h"

typedef struct {
    pax_col_t color_foreground;           // Foreground color for borders and text
    pax_col_t color_background;           // Background color for the entire screen
    pax_col_t color_active_foreground;    // Foreground color for focus elements
    pax_col_t color_active_background;    // Background color for focus elements
    pax_col_t color_highlight_primary;    // Color for highlighted text and borders (primary)
    pax_col_t color_highlight_secondary;  // Color for highlighted text and borders (secondary)
    pax_col_t color_highlight_tertiary;   // Color for highlighted text and borders (tertiary)
} gui_palette_t;

typedef struct {
    // Size
    int height;

    // Marigns
    int vertical_margin;
    int horizontal_margin;

    // Padding
    int vertical_padding;
    int horizontal_padding;

    // Text properties
    int               text_height;
    const pax_font_t* text_font;

    // List properties
    int list_entry_height;

    // Grid properties
    int grid_horizontal_count;
    int grid_vertical_count;
} gui_element_style_t;

typedef struct {
    bool visible;
    int  width;
} gui_scrollbar_style_t;

typedef struct {
    gui_palette_t         palette;  // Color palette
    gui_element_style_t   footer;
    gui_element_style_t   header;
    gui_element_style_t   menu;
    gui_scrollbar_style_t scrollbar;
} gui_theme_t;
