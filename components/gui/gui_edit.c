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

#include "gui_edit.h"
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include "bsp/input.h"
#include "esp_timer.h"

void gui_edit_init(pax_buf_t* pax_buffer, gui_edit_context_t* context, float aPosX, float aPosY, float aWidth,
                   float aHeight, char* text, size_t buffer_cap) {
    context->x              = aPosX + 1;
    context->y              = aPosY + 1;
    context->width          = aWidth - 2;
    context->height         = aHeight - 2;
    context->content_cap    = buffer_cap;
    context->selection      = 0;
    context->cursor         = 0;
    context->hold_start     = 0;
    context->last_press     = 0;
    context->multiline      = false;
    context->insert         = false;
    context->text_font      = pax_font_saira_regular;
    context->text_font_size = 18;
    context->text_col       = 0xFF0000AA;
    context->sel_text_col   = 0xff000000;
    context->sel_col        = 0xff007fff;
    context->bg_col         = 0xFFFFFFFF;
    context->dirty          = true;

    char* buffer = malloc(buffer_cap);
    assert(buffer != NULL);
    memset(buffer, 0, buffer_cap);
    context->content = buffer;
    gui_edit_set_content(context, text);

    pax_noclip(pax_buffer);
    pax_simple_rect(pax_buffer, context->bg_col, aPosX, aPosY, aWidth, aHeight);
    pax_outline_rect(pax_buffer, context->text_col, aPosX, aPosY, aWidth, aHeight);
}

void gui_edit_destroy(gui_edit_context_t* context, char* output, size_t output_size) {
    if (output && output_size > 0) {
        memset(output, 0, output_size);
        strncpy(output, context->content, output_size - 1);
    }
    free(context->content);
    context->content     = NULL;
    context->content_cap = 0;
}

void gui_edit_set_content(gui_edit_context_t* context, const char* content) {
    strncpy(context->content, content, context->content_cap - 1);
    context->content[context->content_cap - 1] = 0;
    context->cursor                            = strlen(content);
}

// Draw just the text part.
static void gui_edit_render_text(pax_buf_t* buf, gui_edit_context_t* context, bool do_bg) {
    // Draw background.
    if (do_bg) {
        pax_draw_rect(buf, context->bg_col, context->x - 0.01, context->y - 0.01, context->width + 0.02,
                      context->height + 0.02);
    }

    // Some setup.
    float x      = context->x + 2;
    float y      = context->y + 2;
    char  tmp[2] = {0, 0};

    // Draw everything.
    for (int i = 0; i < strlen(context->content); i++) {
        if (context->cursor == i) {
            // The cursor in between the input.
            pax_draw_line(buf, context->sel_col, x, y, x, y + context->text_font_size - 1);
        }

        // The character of the input.
        tmp[0]          = context->content[i];
        pax_vec1_t dims = pax_text_size(context->text_font, context->text_font_size, tmp);

        if (x + dims.x > context->x + context->width - 4) {
            // Word wrap.
            x  = context->x + 2;
            y += context->text_font_size;
        }
        pax_draw_text(buf, context->text_col, context->text_font, context->text_font_size, x, y, tmp);
        x += dims.x;
    }
    if (context->cursor == strlen(context->content)) {
        // The cursor after the input.
        pax_draw_line(buf, context->sel_col, x, y, x, y + context->text_font_size - 1);
    }
}

// Redraw the complete on-screen keyboard.
void gui_edit_render(pax_buf_t* pax_buffer, gui_edit_context_t* context) {
    if (matrix_2d_is_identity(pax_buffer->stack_2d.value) && context->x == 0 && context->y == 0 &&
        context->width == pax_buffer->width && context->height == pax_buffer->height) {
        // We can just fill the entire screen.
        pax_background(pax_buffer, context->bg_col);
    } else {
        // We'll need to fill a rectangle.
        pax_draw_rect(pax_buffer, context->bg_col, context->x, context->y, context->width, context->height);
    }

    pax_outline_rect(pax_buffer, context->sel_col, context->x, context->y, context->width - 1, context->height - 1);

    gui_edit_render_text(pax_buffer, context, false);

    // Mark as not dirty.
    context->dirty = false;
}

// Redraw only the changed parts of the on-screen keyboard.
void gui_edit_redraw(pax_buf_t* buf, gui_edit_context_t* context) {
    if (context->dirty) {
        gui_edit_render_text(buf, context, true);
    }

    // Mark as not dirty.
    context->dirty = false;
}

/* ==== Text editing ==== */

// Handling of delete or backspace.
static void gui_edit_delete(gui_edit_context_t* context, bool is_backspace) {
    size_t oldlen = strlen(context->content);
    if (!is_backspace && context->cursor == oldlen) {
        // No forward deleting at the end of the line.
        return;
    } else if (is_backspace && context->cursor == 0) {
        // No backward deleting at the start of the line.
        return;
    } else if (!is_backspace) {
        // Advanced backspace.
        context->cursor++;
    }

    // Copy back everything including null terminator.
    context->cursor--;
    for (int i = context->cursor; i < oldlen; i++) {
        context->content[i] = context->content[i + 1];
    }

    context->dirty = true;
}

// Handling of normal input.
static void gui_edit_append(gui_edit_context_t* context, char value) {
    size_t oldlen = strlen(context->content);
    if (oldlen + 2 >= context->content_cap) {
        // That's too big.
        return;
    }

    // Copy over the remainder of the buffer.
    // If there's no text this still copies the null terminator.
    for (int i = oldlen; i >= context->cursor; i--) {
        context->content[i + 1] = context->content[i];
    }

    // And finally insert at the character.
    context->content[context->cursor] = value;
    context->cursor++;
    context->dirty = true;
}

void gui_edit_handle_navigation_event(gui_edit_context_t* context, bsp_input_event_args_navigation_t event) {
    context->last_press = esp_timer_get_time();
    if (event.state) {
        switch (event.key) {
            case BSP_INPUT_NAVIGATION_KEY_LEFT:
                if (context->cursor > 0) context->cursor--;
                context->dirty = true;
                break;
            case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                if (context->cursor < strlen(context->content)) context->cursor++;
                context->dirty = true;
                break;
            case BSP_INPUT_NAVIGATION_KEY_UP:
                break;
            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                break;
            case BSP_INPUT_NAVIGATION_KEY_BACKSPACE:
                gui_edit_delete(context, true);
                break;
            default:
                break;
        }
    }
}

void gui_edit_handle_keyboard_event(gui_edit_context_t* context, bsp_input_event_args_keyboard_t event) {
    if (event.ascii >= ' ' && event.ascii <= '~') {
        gui_edit_append(context, event.ascii);
    }
    context->dirty = true;
}
