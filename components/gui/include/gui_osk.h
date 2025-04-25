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

#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_shapes.h"

// A number of inputs supported by the PAX keyboard.
typedef enum {
    // Represents no input being pressed.
    GUI_OSK_NO_INPUT,
    // Movement of the cursor.
    GUI_OSK_UP,
    GUI_OSK_DOWN,
    GUI_OSK_LEFT,
    GUI_OSK_RIGHT,
    // Delete to the left or the selection. Backspace key.
    GUI_OSK_DELETE_BEFORE,
    // Delete to the right or the selection. Delete key.
    GUI_OSK_DELETE_AFTER,
    // Switch between lower case, upper case and symbols.
    GUI_OSK_MODESELECT,
    // Enter a character.
    GUI_OSK_CHARSELECT,
    // The same thing as the shift key.
    // Goes between GUI_OSK_LOWERCASE and GUI_OSK_UPPERCASE or GUI_OSK_NUMBERS and GUI_OSK_SYMBOLS.
    GUI_OSK_SHIFT,
} gui_osk_input_t;

// The type of keyboard currently selected.
typedef enum {
    // Lowercase and .,
    GUI_OSK_LOWERCASE,
    // Uppercase and <>
    PBK_UPPERCASE,
    // Numbers and symbols 1/2
    GUI_OSK_NUMBERS,
    // Symbols 2/2
    GUI_OSK_SYMBOLS,
} gui_osk_keyboard_t;

// The PAX keyboard context used for drawing and alike.
typedef struct {
    // Position on screen of the keyboard.
    int x, y;
    // Maximum size of the keyboard.
    int width, height;

    // Content of the keyboard.
    char*  content;
    // Size in bytes of capacity of the content buffer.
    size_t content_cap;

    // Starting position of the selection in the text box.
    int selection;
    // Cursor position of the text box.
    int cursor;

    // Cursor position of the keyboard.
    int             key_x, key_y;
    // The currently held input.
    gui_osk_input_t held;
    // The time that holding the input started.
    int64_t         hold_start;
    // The last time gui_osk_press was called.
    int64_t         last_press;

    // Whether the keyboard is multi-line.
    bool               multiline;
    // Whether the keyboard is in insert mode.
    bool               insert;
    // The board that is currently selected.
    gui_osk_keyboard_t board_sel;

    // The font to use for the keyboard.
    const pax_font_t* kb_font;
    // The font size to use for the keyboard.
    float             kb_font_size;
    // The font to use for the text.
    const pax_font_t* text_font;
    // The font size to use for the text.
    float             text_font_size;
    // The text color to use.
    pax_col_t         text_col;
    // The text color to use when a character is being held down.
    pax_col_t         sel_text_col;
    // The selection color to use.
    pax_col_t         sel_col;
    // The background color to use.
    pax_col_t         bg_col;

    // Whether something has changed since last draw.
    bool dirty;
    // Whether the text has changed since last draw.
    bool text_dirty;
    // Whether the keyboard has changed since last draw.
    bool kb_dirty;
    // Whether just the selected character has changed since last draw.
    bool sel_dirty;
    // Previous cursor position of the keyboard.
    // Used for sel_dirty.
    int  last_key_x, last_key_y;

    // Indicates that the input has been accepted.
    bool input_accepted;
} gui_osk_ctx_t;

// Initialise the context with default settings.
void gui_osk_init(pax_buf_t* buf, gui_osk_ctx_t* ctx, size_t buffer_cap);
// Free any memory associated with the context.
void gui_osk_destroy(gui_osk_ctx_t* ctx);
// Replaces the text in the keyboard with the given text.
// Makes a copy of the given text.
void gui_osk_set_content(gui_osk_ctx_t* ctx, const char* content);

// Redraw the complete on-screen keyboard.
void gui_osk_render(pax_buf_t* buf, gui_osk_ctx_t* ctx);
// Redraw only the changed parts of the on-screen keyboard.
void gui_osk_redraw(pax_buf_t* buf, gui_osk_ctx_t* ctx);

// The loop that allows input repeating.
void gui_osk_loop(gui_osk_ctx_t* ctx);

// A pressing of the input.
void gui_osk_press(gui_osk_ctx_t* ctx, gui_osk_input_t input);
// A relealing of the input.
void gui_osk_release(gui_osk_ctx_t* ctx, gui_osk_input_t input);
