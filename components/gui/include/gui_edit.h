/*
    MIT License

    Copyright (c) 2022 Julian Scheffers

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#pragma once

#include "bsp/input.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_shapes.h"

typedef struct {
    int               x, y;
    int               width, height;
    char*             content;
    size_t            content_cap;
    int               selection;
    int               cursor;
    int64_t           hold_start;
    int64_t           last_press;
    bool              multiline;
    bool              insert;
    const pax_font_t* text_font;
    float             text_font_size;
    pax_col_t         text_col;
    pax_col_t         sel_text_col;
    pax_col_t         sel_col;
    pax_col_t         bg_col;

    bool dirty;
    int  last_key_x, last_key_y;
} gui_edit_context_t;

void gui_edit_init(pax_buf_t* pax_buffer, gui_edit_context_t* context, float aPosX, float aPosY, float aWidth,
                   float aHeight, char* text, size_t buffer_cap);
void gui_edit_destroy(gui_edit_context_t* context, char* output, size_t output_size);
void gui_edit_set_content(gui_edit_context_t* context, const char* content);
void gui_edit_render(pax_buf_t* pax_buffer, gui_edit_context_t* context);
void gui_edit_redraw(pax_buf_t* buf, gui_edit_context_t* context);
void gui_edit_handle_navigation_event(gui_edit_context_t* context, bsp_input_event_args_navigation_t event);
void gui_edit_handle_keyboard_event(gui_edit_context_t* context, bsp_input_event_args_keyboard_t event);
