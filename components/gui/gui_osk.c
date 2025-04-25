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

#include "gui_osk.h"
#include <malloc.h>
#include <string.h>
#include "esp_timer.h"

// Initialise the context with default settings.
void gui_osk_init(pax_buf_t* buf, gui_osk_ctx_t* ctx, size_t buffer_cap) {
    // Allocate a bufffer.
    char* buffer = malloc(buffer_cap);
    memset(buffer, 0, buffer_cap);

    // Some defaults.
    *ctx = (gui_osk_ctx_t){
        // Position on screen of the keyboard.
        .x      = 0,
        .y      = 0,
        // Maximum size of the keyboard.
        .width  = buf->width,
        .height = buf->height,

        // Content of the keyboard.
        .content     = buffer,
        // Size in bytes of capacity of the content buffer.
        .content_cap = buffer_cap,

        // Starting position of the selection in the text box.
        .selection = 0,
        // Cursor position of the text box.
        .cursor    = 0,

        // Cursor position of the keyboard.
        .key_x      = 3,
        .key_y      = 1,
        // The currently held input.
        .held       = GUI_OSK_NO_INPUT,
        // The time that holding the input started.
        .hold_start = 0,
        // The last time gui_osk_press was called.
        .last_press = 0,

        // Whether the keyboard is multi-line.
        .multiline = false,
        // Whether the keyboard is in insert mode.
        .insert    = false,
        // The board that is currently selected.
        .board_sel = GUI_OSK_LOWERCASE,

        // The font to use for the keyboard.
        .kb_font        = PAX_FONT_DEFAULT,
        // The font size to use for the keyboard.
        .kb_font_size   = 27,
        // The font to use for the text.
        .text_font      = PAX_FONT_DEFAULT,
        // The font size to use for the text.
        .text_font_size = 18,
        // The text color to use.
        .text_col       = 0xffffffff,
        // The text color to use when a character is being held down.
        .sel_text_col   = 0xff000000,
        // The selection color to use.
        .sel_col        = 0xff007fff,
        // The background color to use.
        .bg_col         = 0xff000000,

        // Whether something has changed since last draw.
        .dirty      = true,
        // Whether the text has changed since last draw.
        .text_dirty = true,
        // Whether the keyboard has changed since last draw.
        .kb_dirty   = true,
        // Whether just the selected character has changed since last draw.
        .sel_dirty  = true,
        // Previous cursor position of the keyboard.
        // Used for sel_dirty.
        .last_key_x = 3,
        .last_key_y = 1,

        // Indicates that the input has been accepted.
        .input_accepted = false,
    };

    // TODO: Pick fancier text sizes.
}

// Free any memory associated with the context.
void gui_osk_destroy(gui_osk_ctx_t* ctx) {
    free(ctx->content);
    ctx->content     = NULL;
    ctx->content_cap = 0;
}

// Replaces the text in the keyboard with the given text.
// Makes a copy of the given text.
void gui_osk_set_content(gui_osk_ctx_t* ctx, const char* content) {
    // Replace the content.
    strncpy(ctx->content, content, ctx->content_cap - 1);
    ctx->content[ctx->content_cap - 1] = 0;
    ctx->cursor                        = strlen(content);
}

const char* uppercase_board[] = {
    "QWERTYUIOP", "ASDFGHJKL", " ZXCVBNM ", " <     > ", "<", ">",
};
const char* lowercase_board[] = {
    "qwertyuiop", "asdfghjkl", " zxcvbnm ", " ,     . ", ",", ".",
};
const char* number_board[] = {
    "1234567890", "@#$_&-+()/", " *\"';:!? ", " =      \\ ", "=", "\\",
};
const char* symbols_board[] = {
    "()[]{}`~", "/|\\+-_=", " ^%<>'\" ", " ,     . ", ",", ".",
};
const char** boards[] = {
    lowercase_board,
    uppercase_board,
    number_board,
    symbols_board,
};

/* ==== Special key art ==== */

// Art for the accept key.
// Expects x to be the horizontal center of the key.
static void gui_osk_art_accept(pax_buf_t* buf, gui_osk_ctx_t* ctx, float x, float y, float dx, bool selected) {
    pax_col_t col   = selected && ctx->held == GUI_OSK_CHARSELECT ? ctx->sel_text_col : ctx->text_col;
    float     scale = fminf(ctx->kb_font_size - 4, dx - 4);

    pax_push_2d(buf);
    pax_apply_2d(buf, matrix_2d_translate(x - dx / 2 + 2, y + 2));
    pax_apply_2d(buf, matrix_2d_scale(scale, scale));

    pax_draw_line(buf, col, 0.25, 0.5, 0.5, 1);
    pax_draw_line(buf, col, 0.5, 1, 1, 0);

    pax_pop_2d(buf);
}

// Art for the backspace key.
// Expects x to be the horizontal center of the key.
static void gui_osk_art_bksp(pax_buf_t* buf, gui_osk_ctx_t* ctx, float x, float y, float dx, bool selected) {
    pax_col_t col   = selected && ctx->held == GUI_OSK_CHARSELECT ? ctx->sel_text_col : ctx->text_col;
    float     scale = fminf(ctx->kb_font_size - 4, dx - 4);

    pax_push_2d(buf);
    pax_apply_2d(buf, matrix_2d_translate(x - dx / 2 + 2, y + 2));
    pax_apply_2d(buf, matrix_2d_scale(scale, scale));

    // The stopper.
    pax_draw_line(buf, col, 0, 0.25, 0, 0.75);
    // The arrow.
    pax_draw_line(buf, col, 0.1, 0.5, 0.35, 0.25);
    pax_draw_line(buf, col, 0.1, 0.5, 0.35, 0.75);
    pax_draw_line(buf, col, 0.1, 0.5, 1, 0.5);

    pax_pop_2d(buf);
}

// Art for the shift key.
// Expects x to be the horizontal center of the key.
static void gui_osk_art_shift(pax_buf_t* buf, gui_osk_ctx_t* ctx, float x, float y, float dx, bool selected) {
    bool      active = ctx->board_sel & 1;
    pax_col_t col    = selected && ctx->held == GUI_OSK_CHARSELECT ? ctx->sel_text_col : ctx->text_col;
    float     scale  = fminf(ctx->kb_font_size - 4, dx - 4);

    pax_push_2d(buf);
    pax_apply_2d(buf, matrix_2d_translate(x, y + 2));
    pax_apply_2d(buf, matrix_2d_scale(scale, scale));

    if (active) {
        // Filled in shift key.
        pax_draw_tri(buf, col, -0.5, 0.5, 0, 0, 0.5, 0.5);
        pax_draw_rect(buf, col, -0.25, 0.5, 0.5, 0.5);
    } else {
        // Outlined shift key.
        pax_draw_line(buf, col, 0, 0, 0.5, 0.5);
        pax_draw_line(buf, col, 0.5, 0.5, 0.25, 0.5);
        pax_draw_line(buf, col, 0.25, 0.5, 0.25, 1);
        pax_draw_line(buf, col, 0.25, 1, -0.25, 1);
        pax_draw_line(buf, col, -0.25, 0.5, -0.25, 1);
        pax_draw_line(buf, col, -0.5, 0.5, -0.25, 0.5);
        pax_draw_line(buf, col, 0, 0, -0.5, 0.5);
    }

    pax_pop_2d(buf);
}

// Art for the letters/numbers key.
// Expects x to be the horizontal center of the key.
static void gui_osk_art_select(pax_buf_t* buf, gui_osk_ctx_t* ctx, float x, float y, float dx, bool selected) {
    pax_col_t col = selected && ctx->held == GUI_OSK_CHARSELECT ? ctx->sel_text_col : ctx->text_col;

    // Pick the stuff to show.
    char* short_str;
    char* long_str;
    if (ctx->board_sel == GUI_OSK_NUMBERS) {
        // Show some symbols.
        short_str = "%";
        long_str  = "%&~";
    } else if (ctx->board_sel == GUI_OSK_SYMBOLS) {
        // Show some letters.
        short_str = "A";
        long_str  = "Abc";
    } else {
        // Show some numbers.
        short_str = "#";
        long_str  = "123";
    }

    // Calculate font size.
    char*      str       = long_str;
    pax_vec1_t dims      = pax_text_size(ctx->kb_font, 0, str);
    int        font_size = 9;  //(dx - 4) / dims.x * dims.y;
    if (font_size < dims.y) {
        str       = short_str;
        dims      = pax_text_size(ctx->kb_font, 0, str);
        font_size = 9;  //(dx - 4) / dims.x * dims.y;
    }
    dims = pax_text_size(ctx->kb_font, font_size, str);

    // Now draw it.
    pax_push_2d(buf);
    pax_apply_2d(buf, matrix_2d_translate(x - dims.x / 2, y + (ctx->kb_font_size - font_size) / 2));
    pax_draw_text(buf, col, ctx->kb_font, font_size, 0, 0, str);
    pax_pop_2d(buf);
}

/* ==== Rendering ==== */

// Draw one key of the keyboard.
// Expects x to be the horizontal center of the key.
static void gui_osk_char(pax_buf_t* buf, gui_osk_ctx_t* ctx, float x, float y, float dx, const char* text,
                         bool selected) {
    pax_vec1_t dims = pax_text_size(ctx->kb_font, ctx->kb_font_size, text);

    if (selected && ctx->held == GUI_OSK_CHARSELECT) {
        // Infilll!
        pax_draw_rect(buf, ctx->sel_col, x - dx / 2, y, dx, ctx->kb_font_size);
        pax_draw_text(buf, ctx->sel_text_col, ctx->kb_font, ctx->kb_font_size, x - dims.x * 0.5, y, text);
    } else {
        pax_draw_text(buf, ctx->text_col, ctx->kb_font, ctx->kb_font_size, x - dims.x * 0.5, y, text);

        // Outline?
        if (selected) {
            pax_push_2d(buf);
            pax_apply_2d(buf, matrix_2d_translate(-dx / 2, 0));
            pax_draw_line(buf, ctx->sel_col, x, y, x + dx - 1, y);
            pax_draw_line(buf, ctx->sel_col, x, y + ctx->kb_font_size - 1, x + dx - 1, y + ctx->kb_font_size - 1);
            pax_draw_line(buf, ctx->sel_col, x + dx - 1, y, x + dx - 1, y + ctx->kb_font_size - 1);
            pax_draw_line(buf, ctx->sel_col, x, y, x, y + ctx->kb_font_size - 1);
            pax_pop_2d(buf);
        }
    }
}

// Draw one full row of the keyboard.
static void gui_osk_row(pax_buf_t* buf, gui_osk_ctx_t* ctx, int rownum, int selected, float dx, float y) {
    // Calculate some stuff.
    const char* row    = boards[ctx->board_sel][rownum];
    size_t      len    = strlen(row);
    int         x      = ctx->x + (ctx->width - len * dx + dx) / 2;
    char        tmp[2] = {0, 0};

    // Show all of them.
    for (int i = 0; i < len; i++) {
        // Draw a KEY.
        *tmp = row[i];
        gui_osk_char(buf, ctx, x, y, dx, tmp, selected == i);

        if (i == 0 && rownum == 2) {
            // Draw shift key art.
            gui_osk_art_shift(buf, ctx, x, y, dx, selected == i);
        } else if (rownum == 2 && i == len - 1) {
            // Draw the backspace key art.
            gui_osk_art_bksp(buf, ctx, x, y, dx, selected == i);
        }

        x += dx;
    }
}

// Draw a specific key in a row of the keyboard.
static void gui_osk_row_key(pax_buf_t* buf, gui_osk_ctx_t* ctx, int rownum, bool selected, float dx, float y,
                            int keyno) {
    // Calculate some stuff.
    const char* row    = boards[ctx->board_sel][rownum];
    size_t      len    = strlen(row);
    int         x      = ctx->x + (ctx->width - len * dx + dx) / 2 + dx * keyno;
    char        tmp[2] = {0, 0};

    // Show one of them.
    *tmp = row[keyno];
    pax_draw_rect(buf, ctx->bg_col, x - dx / 2 - 1, y, dx + 2, ctx->kb_font_size);
    gui_osk_char(buf, ctx, x, y, dx, tmp, selected);

    if (rownum == 2 && keyno == 0) {
        // Draw the shift key art.
        gui_osk_art_shift(buf, ctx, x, y, dx, selected);
    } else if (rownum == 2 && keyno == len - 1) {
        // Draw the backspace key art.
        gui_osk_art_bksp(buf, ctx, x, y, dx, selected);
    }
}

// Draw just the board part.
static void gui_osk_render_keyb(pax_buf_t* buf, gui_osk_ctx_t* ctx, bool do_bg) {
    // Draw background.
    if (do_bg) {
        pax_draw_rect(buf, ctx->bg_col, ctx->x, ctx->y + ctx->height - ctx->kb_font_size * 4, ctx->width,
                      ctx->kb_font_size * 4);
    }

    // Select the board to display.
    const char** board = boards[ctx->board_sel];
    float        dx    = ctx->width / 10;
    float        y     = ctx->y + ctx->height - ctx->kb_font_size * 4;

    // Draw the first three rows.
    for (int i = 0; i < 3; i++) {
        int sel = -1;
        if (i == ctx->key_y) {
            sel = ctx->key_x;
        }
        gui_osk_row(buf, ctx, i, sel, dx, y);
        y += ctx->kb_font_size;
    }

    // Spacebar row time.
    bool  space_sel = ctx->key_y == 3 && ctx->key_x > 1 && ctx->key_x < 7;
    float x         = ctx->x + (ctx->width - 8 * dx) / 2;

    // The thingy selector.
    gui_osk_char(buf, ctx, x, y, dx, " ", ctx->key_y == 3 && ctx->key_x == 0);
    gui_osk_art_select(buf, ctx, x, y, dx, ctx->key_y == 3 && ctx->key_x == 0);
    x += 1.0 * dx;
    // Left char.
    gui_osk_char(buf, ctx, x, y, dx, board[4], ctx->key_y == 3 && ctx->key_x == 1);
    x += 1.0 * dx;

    // SPACE.
    if (space_sel && ctx->held == GUI_OSK_CHARSELECT) {
        pax_draw_rect(buf, ctx->sel_col, x - dx / 2, y, dx * 5, ctx->kb_font_size);
        pax_draw_rect(buf, ctx->sel_text_col, x, y + ctx->kb_font_size / 3, dx * 4, ctx->kb_font_size / 3);
    } else {
        pax_draw_rect(buf, ctx->text_col, x, y + ctx->kb_font_size / 3, dx * 4, ctx->kb_font_size / 3);
        if (space_sel) {
            // Outline rect?
            pax_push_2d(buf);
            pax_apply_2d(buf, matrix_2d_translate(-dx / 2, 0));
            pax_draw_line(buf, ctx->sel_col, x, y, x + dx * 5 - 1, y);
            pax_draw_line(buf, ctx->sel_col, x, y + ctx->kb_font_size - 1, x + dx * 5 - 1, y + ctx->kb_font_size - 1);
            pax_draw_line(buf, ctx->sel_col, x + dx * 5 - 1, y, x + dx * 5 - 1, y + ctx->kb_font_size - 1);
            pax_draw_line(buf, ctx->sel_col, x, y, x, y + ctx->kb_font_size - 1);
            pax_pop_2d(buf);
        }
    }

    // Right char.
    x += 5.0 * dx;
    gui_osk_char(buf, ctx, x, y, dx, board[5], ctx->key_y == 3 && ctx->key_x == 7);
    x += 1.0 * dx;
    // The thingy acceptor.
    gui_osk_char(buf, ctx, x, y, dx, " ", ctx->key_y == 3 && ctx->key_x == 8);
    gui_osk_art_accept(buf, ctx, x, y, dx, ctx->key_y == 3 && ctx->key_x == 8);
}

// Draw just the text part.
static void gui_osk_render_text(pax_buf_t* buf, gui_osk_ctx_t* ctx, bool do_bg) {
    // Draw background.
    if (do_bg) {
        pax_draw_rect(buf, ctx->bg_col, ctx->x - 0.01, ctx->y - 0.01, ctx->width + 0.02,
                      ctx->height - ctx->kb_font_size * 4 + 0.02);
    }
    if (ctx->key_y == -1) {
        // Outline us.
        pax_outline_rect(buf, ctx->sel_col, ctx->x, ctx->y, ctx->width - 1, ctx->height - ctx->kb_font_size * 4 - 1);
    }

    // Some setup.
    float x      = ctx->x + 2;
    float y      = ctx->y + 2;
    char  tmp[2] = {0, 0};

    // Draw everything.
    for (int i = 0; i < strlen(ctx->content); i++) {
        if (ctx->cursor == i) {
            // The cursor in between the input.
            pax_draw_line(buf, ctx->sel_col, x, y, x, y + ctx->text_font_size - 1);
        }

        // The character of the input.
        tmp[0]          = ctx->content[i];
        pax_vec1_t dims = pax_text_size(ctx->text_font, ctx->text_font_size, tmp);

        if (x + dims.x > ctx->x + ctx->width - 4) {
            // Word wrap.
            x  = ctx->x + 2;
            y += ctx->text_font_size;
        }
        pax_draw_text(buf, ctx->text_col, ctx->text_font, ctx->text_font_size, x, y, tmp);
        x += dims.x;
    }
    if (ctx->cursor == strlen(ctx->content)) {
        // The cursor after the input.
        pax_draw_line(buf, ctx->sel_col, x, y, x, y + ctx->text_font_size - 1);
    }
}

// Draw one specific key.
static void gui_osk_render_key(pax_buf_t* buf, gui_osk_ctx_t* ctx, int key_x, int key_y) {
    if (key_y == -1) {
        // If key_y is -1, the text box is selected to render.
        gui_osk_render_text(buf, ctx, true);
        return;
    }

    // Select the board to display.
    const char** board = boards[ctx->board_sel];
    float        dx    = ctx->width / 10;
    float        y     = ctx->y + ctx->height - ctx->kb_font_size * 4;

    if (key_y < 3) {
        // Draw one of the first three rows.
        y += ctx->kb_font_size * key_y;
        gui_osk_row_key(buf, ctx, key_y, key_y == ctx->key_y && key_x == ctx->key_x, dx, y, key_x);
        y += ctx->kb_font_size;
    } else {
        // Spacebar row time.
        y               += ctx->kb_font_size * 3;
        bool  space_sel  = ctx->key_y == 3 && ctx->key_x > 1 && ctx->key_x < 7;
        float x          = ctx->x + (ctx->width - 8 * dx) / 2;

        // The thingy selector.
        if (key_x == 0) {
            pax_draw_rect(buf, ctx->bg_col, x - dx / 2, y, dx, ctx->kb_font_size);
            gui_osk_char(buf, ctx, x, y, dx, " ", ctx->key_y == 3 && ctx->key_x == 0);
            gui_osk_art_select(buf, ctx, x, y, dx, ctx->key_y == 3 && ctx->key_x == 0);
        }
        x += 1.0 * dx;
        if (key_x == 1) {
            pax_draw_rect(buf, ctx->bg_col, x - dx / 2, y, dx, ctx->kb_font_size);
            gui_osk_char(buf, ctx, x, y, dx, board[4], ctx->key_y == 3 && ctx->key_x == 1);
        }
        x += 1.0 * dx;

        // SPACE.
        if (space_sel && ctx->held == GUI_OSK_CHARSELECT) {
            pax_draw_rect(buf, ctx->sel_col, x - dx / 2, y, dx * 5, ctx->kb_font_size);
            pax_draw_rect(buf, ctx->sel_text_col, x, y + ctx->kb_font_size / 3, dx * 4, ctx->kb_font_size / 3);
        } else {
            pax_draw_rect(buf, ctx->bg_col, x - dx / 2, y, dx * 5, ctx->kb_font_size);
            pax_draw_rect(buf, ctx->text_col, x, y + ctx->kb_font_size / 3, dx * 4, ctx->kb_font_size / 3);
            if (space_sel) {
                // Outline rect?
                pax_push_2d(buf);
                pax_apply_2d(buf, matrix_2d_translate(-dx / 2, 0));
                pax_draw_line(buf, ctx->sel_col, x, y, x + dx * 5 - 1, y);
                pax_draw_line(buf, ctx->sel_col, x, y + ctx->kb_font_size - 1, x + dx * 5 - 1,
                              y + ctx->kb_font_size - 1);
                pax_draw_line(buf, ctx->sel_col, x + dx * 5 - 1, y, x + dx * 5 - 1, y + ctx->kb_font_size - 1);
                pax_draw_line(buf, ctx->sel_col, x, y, x, y + ctx->kb_font_size - 1);
                pax_pop_2d(buf);
            }
        }

        // Right char.
        x += 5.0 * dx;
        if (key_x == 7) {
            pax_draw_rect(buf, ctx->bg_col, x - dx / 2, y, dx, ctx->kb_font_size);
            gui_osk_char(buf, ctx, x, y, dx, board[5], ctx->key_y == 3 && ctx->key_x == 7);
        }
        x += 1.0 * dx;
        // The thingy acceptor.
        if (key_x == 8) {
            pax_draw_rect(buf, ctx->bg_col, x - dx / 2, y, dx, ctx->kb_font_size);
            gui_osk_char(buf, ctx, x, y, dx, " ", ctx->key_y == 3 && ctx->key_x == 8);
            gui_osk_art_accept(buf, ctx, x, y, dx, ctx->key_y == 3 && ctx->key_x == 8);
        }
    }
}

// Redraw the complete on-screen keyboard.
void gui_osk_render(pax_buf_t* buf, gui_osk_ctx_t* ctx) {
    if (matrix_2d_is_identity(buf->stack_2d.value) && ctx->x == 0 && ctx->y == 0 && ctx->width == buf->width &&
        ctx->height == buf->height) {
        // We can just fill the entire screen.
        pax_background(buf, ctx->bg_col);
    } else {
        // We'll need to fill a rectangle.
        pax_draw_rect(buf, ctx->bg_col, ctx->x, ctx->y, ctx->width, ctx->height);
    }

    // Draw the board.
    gui_osk_render_keyb(buf, ctx, false);
    // Time to draw some text.
    gui_osk_render_text(buf, ctx, false);

    // Mark as not dirty.
    ctx->dirty      = false;
    ctx->kb_dirty   = false;
    ctx->sel_dirty  = false;
    ctx->text_dirty = false;
    ctx->last_key_x = ctx->key_x;
    ctx->last_key_y = ctx->key_y;
}

// Redraw only the changed parts of the on-screen keyboard.
void gui_osk_redraw(pax_buf_t* buf, gui_osk_ctx_t* ctx) {
    if (ctx->text_dirty) {
        gui_osk_render_text(buf, ctx, true);
    }
    if (ctx->kb_dirty) {
        gui_osk_render_keyb(buf, ctx, true);
    } else if (ctx->sel_dirty) {
        gui_osk_render_key(buf, ctx, ctx->last_key_x, ctx->last_key_y);
        gui_osk_render_key(buf, ctx, ctx->key_x, ctx->key_y);
    }

    // Mark as not dirty.
    ctx->dirty      = false;
    ctx->kb_dirty   = false;
    ctx->sel_dirty  = false;
    ctx->text_dirty = false;
    ctx->last_key_x = ctx->key_x;
    ctx->last_key_y = ctx->key_y;
}

/* ==== Text editing ==== */

// Handling of delete or backspace.
static void gui_osk_delete(gui_osk_ctx_t* ctx, bool is_backspace) {
    size_t oldlen = strlen(ctx->content);
    if (!is_backspace && ctx->cursor == oldlen) {
        // No forward deleting at the end of the line.
        return;
    } else if (is_backspace && ctx->cursor == 0) {
        // No backward deleting at the start of the line.
        return;
    } else if (!is_backspace) {
        // Advanced backspace.
        ctx->cursor++;
    }

    // Copy back everything including null terminator.
    ctx->cursor--;
    for (int i = ctx->cursor; i < oldlen; i++) {
        ctx->content[i] = ctx->content[i + 1];
    }

    ctx->text_dirty = true;
}

// Handling of normal input.
static void gui_osk_append(gui_osk_ctx_t* ctx, char value) {
    size_t oldlen = strlen(ctx->content);
    if (oldlen + 2 >= ctx->content_cap) {
        // That's too big.
        return;
    }

    // Copy over the remainder of the buffer.
    // If there's no text this still copies the null terminator.
    for (int i = oldlen; i >= ctx->cursor; i--) {
        ctx->content[i + 1] = ctx->content[i];
    }

    // And finally insert at the character.
    ctx->content[ctx->cursor] = value;
    ctx->cursor++;
    ctx->text_dirty = true;
}

/* ==== Input handling ==== */

// The loop that allows input repeating.
void gui_osk_loop(gui_osk_ctx_t* ctx) {
    int64_t now = esp_timer_get_time();
    if (!ctx->held) return;
    bool is_dir = (ctx->held >= GUI_OSK_UP) && (ctx->held <= GUI_OSK_RIGHT);

    if ((ctx->hold_start + 1000000 < now) || (is_dir && ctx->hold_start + 250000 < now)) {
        // 8 repeats per second.
        if (ctx->last_press + 125000 < now) {
            gui_osk_press(ctx, ctx->held);
        }
    }
}

void gui_osk_press(gui_osk_ctx_t* ctx, gui_osk_input_t input) {
    const char** board = boards[ctx->board_sel];
    ctx->last_press    = esp_timer_get_time();
    switch (input) {
        // Cursor movements.
        size_t rowlen;
        case GUI_OSK_UP:
            ctx->key_y--;
            if (ctx->key_y < -1)
                ctx->key_y = 3;
            else if (ctx->key_y != -1) {
                rowlen = strlen(board[ctx->key_y]);
                if (ctx->key_x >= rowlen) ctx->key_x = rowlen - 1;
            }
            ctx->sel_dirty = true;
            break;

        case GUI_OSK_DOWN:
            ctx->key_y++;
            ctx->key_y %= 4;
            rowlen      = strlen(board[ctx->key_y]);
            if (ctx->key_x >= rowlen) ctx->key_x = rowlen - 1;
            ctx->sel_dirty = true;
            break;

        case GUI_OSK_LEFT:
            if (ctx->key_y == -1) {
                if (ctx->cursor > 0) ctx->cursor--;
            } else if (ctx->key_y == 3 && ctx->key_x > 1 && ctx->key_x < 7) {
                ctx->key_x = 1;
            } else {
                ctx->key_x--;
                if (ctx->key_x < 0) ctx->key_x = strlen(board[ctx->key_y]) - 1;
            }
            ctx->sel_dirty = true;
            break;

        case GUI_OSK_RIGHT:
            if (ctx->key_y == -1) {
                if (ctx->cursor < strlen(ctx->content)) ctx->cursor++;
            } else if (ctx->key_y == 3 && ctx->key_x > 1 && ctx->key_x < 7) {
                ctx->key_x = 7;
            } else {
                ctx->key_x++;
                ctx->key_x %= strlen(board[ctx->key_y]);
            }
            ctx->sel_dirty = true;
            break;

            // Enter a character.
        case GUI_OSK_CHARSELECT:
            ctx->sel_dirty |= ctx->held != GUI_OSK_CHARSELECT;
            if (ctx->key_y == 3) {
                switch (ctx->key_x) {
                    case 0:
                        // Board selector.
                        if (ctx->board_sel == GUI_OSK_NUMBERS)
                            ctx->board_sel = GUI_OSK_SYMBOLS;
                        else if (ctx->board_sel == GUI_OSK_SYMBOLS)
                            ctx->board_sel = GUI_OSK_LOWERCASE;
                        else
                            ctx->board_sel = GUI_OSK_NUMBERS;
                        ctx->kb_dirty = true;
                        break;
                    case 1:
                        // Magic.
                        gui_osk_append(ctx, *board[4]);
                        break;
                    default:
                        // Spacebar.
                        gui_osk_append(ctx, ' ');
                        break;
                    case 7:
                        // mAGIC.
                        gui_osk_append(ctx, *board[5]);
                        break;
                    case 8:
                        // Enter idk.
                        ctx->input_accepted = true;
                        break;
                }
            } else if (ctx->key_y == 2) {
                if (ctx->key_x == 0) {
                    // cAPS LOCK KEY.
                    if (ctx->held == GUI_OSK_CHARSELECT) ctx->held = GUI_OSK_NO_INPUT;
                    ctx->board_sel ^= 1;
                    ctx->kb_dirty   = true;
                } else if (ctx->key_x == strlen(board[2]) - 1) {
                    // Backspace.
                    gui_osk_delete(ctx, true);
                } else {
                    // nORMAL CHAR.
                    gui_osk_append(ctx, board[2][ctx->key_x]);
                }
            } else if (ctx->key_y != -1) {
                // Normal char.
                gui_osk_append(ctx, board[ctx->key_y][ctx->key_x]);
            }
            break;

            // Shift key, the pressening.
        case GUI_OSK_SHIFT:
            ctx->board_sel |= 1;
            ctx->kb_dirty   = true;
            break;

            // Next keyboard.
        case GUI_OSK_MODESELECT:
            ctx->board_sel++;
            ctx->board_sel %= 4;
            rowlen          = strlen(board[ctx->key_y]);
            if (ctx->key_x >= rowlen) ctx->key_x = rowlen - 1;
            ctx->kb_dirty = true;
            break;

            // Backspace.
        case GUI_OSK_DELETE_BEFORE:
            gui_osk_delete(ctx, true);
            break;

            // Delete.
        case GUI_OSK_DELETE_AFTER:
            gui_osk_delete(ctx, false);
            break;
        default:
            break;
    }
    if (input != GUI_OSK_SHIFT && input != ctx->held) {
        ctx->held       = input;
        ctx->hold_start = esp_timer_get_time();
    }
    ctx->dirty = true;
}

void gui_osk_release(gui_osk_ctx_t* ctx, gui_osk_input_t input) {
    switch (input) {
            // Shift key, the releasening.
        case GUI_OSK_SHIFT:
            ctx->dirty      = true;
            ctx->board_sel &= ~1;
            ctx->kb_dirty   = true;
            break;

            // Unpress them char.
        case GUI_OSK_CHARSELECT:
            ctx->sel_dirty = true;
            break;

        default:
            break;
    }
    if (ctx->held == input) {
        ctx->held  = GUI_OSK_NO_INPUT;
        ctx->dirty = true;
    }
}
