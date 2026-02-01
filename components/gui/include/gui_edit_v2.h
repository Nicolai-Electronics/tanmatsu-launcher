// SPDX-FileCopyrightText: 2026 Nicolai Electronics
// SPDX-FileCopyrightText: 2026 Julian Scheffers
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp/input.h"
#include "esp_err.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_shapes.h"

typedef struct {
    char*  buffer;
    size_t buffer_size;
    int    cursor;
    bool   dirty;
} gui_edit_v2_context_t;

esp_err_t gui_edit_v2_init(gui_edit_v2_context_t* context, char* text, size_t buffer_cap);
void      gui_edit_v2_destroy(gui_edit_v2_context_t* context, char* output, size_t output_size);
void      gui_edit_v2_set_content(gui_edit_v2_context_t* context, const char* content);
void      gui_edit_v2_get_content(gui_edit_v2_context_t* context, char* output, size_t output_size);
void gui_edit_v2_render(pax_buf_t* pax_buffer, gui_edit_v2_context_t* context, float aPosX, float aPosY, float aWidth,
                        float aHeight, pax_col_t text_col, pax_col_t cursor_col, pax_col_t bg_col,
                        const pax_font_t* font, float font_size, bool force);
void gui_edit_v2_input_left(gui_edit_v2_context_t* context);
void gui_edit_v2_input_right(gui_edit_v2_context_t* context);
void gui_edit_v2_input_backspace(gui_edit_v2_context_t* context);
void gui_edit_v2_input_text(gui_edit_v2_context_t* context, char character);
