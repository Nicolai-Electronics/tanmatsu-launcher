// SPDX-FileCopyrightText: 2026 Nicolai Electronics
// SPDX-FileCopyrightText: 2026 Julian Scheffers
// SPDX-License-Identifier: MIT

#include "gui_edit_v2.h"
#include <malloc.h>
#include <string.h>
#include "bsp/input.h"
#include "esp_err.h"

esp_err_t gui_edit_v2_init(gui_edit_v2_context_t* context, char* text, size_t buffer_cap) {
    context->buffer_size = buffer_cap;
    context->cursor      = 0;
    context->dirty       = true;

    context->buffer = calloc(buffer_cap, 1);
    if (!context->buffer) {
        return ESP_ERR_NO_MEM;
    }

    gui_edit_v2_set_content(context, text);

    return ESP_OK;
}

void gui_edit_v2_destroy(gui_edit_v2_context_t* context, char* output, size_t output_size) {
    gui_edit_v2_get_content(context, output, output_size);
    free(context->buffer);
    context->buffer      = NULL;
    context->buffer_size = 0;
}

void gui_edit_v2_get_content(gui_edit_v2_context_t* context, char* output, size_t output_size) {
    if (output && output_size > 0) {
        memset(output, 0, output_size);
        strncpy(output, context->buffer, output_size - 1);
    }
}

void gui_edit_v2_set_content(gui_edit_v2_context_t* context, const char* content) {
    strncpy(context->buffer, content, context->buffer_size - 1);
    context->buffer[context->buffer_size - 1] = 0;
    context->cursor                           = strlen(content);
}

static void gui_edit_v2_render_text(pax_buf_t* buf, gui_edit_v2_context_t* context, bool do_bg, float aPosX,
                                    float aPosY, float aWidth, float aHeight, pax_col_t text_col, pax_col_t cursor_col,
                                    pax_col_t bg_col, const pax_font_t* font, float font_size) {
    // Draw background.
    if (do_bg) {
        pax_draw_rect(buf, bg_col, aPosX - 0.01, aPosY - 0.01, aWidth + 0.02, aHeight + 0.02);
    }

    // Some setup.
    float x      = aPosX + 2;
    float y      = aPosY + 2;
    char  tmp[2] = {0, 0};

    // Draw everything.
    for (int i = 0; i < strlen(context->buffer); i++) {
        if (context->cursor == i) {
            // The cursor in between the input.
            pax_draw_line(buf, cursor_col, x, y, x, y + font_size - 1);
        }

        // The character of the input.
        tmp[0]          = context->buffer[i];
        pax_vec1_t dims = pax_text_size(font, font_size, tmp);

        if (x + dims.x > aPosX + aWidth - 4) {
            // Word wrap.
            x  = aPosX + 2;
            y += font_size;
        }
        pax_draw_text(buf, text_col, font, font_size, x, y, tmp);
        x += dims.x;
    }
    if (context->cursor == strlen(context->buffer)) {
        // The cursor after the input.
        pax_draw_line(buf, cursor_col, x, y, x, y + font_size - 1);
    }
}

// Redraw the complete on-screen keyboard.
void gui_edit_v2_render(pax_buf_t* pax_buffer, gui_edit_v2_context_t* context, float aPosX, float aPosY, float aWidth,
                        float aHeight, pax_col_t text_col, pax_col_t cursor_col, pax_col_t bg_col,
                        const pax_font_t* font, float font_size, bool force) {
    // if (context->dirty || force) {
    printf("RECT %u %f %f %f %f\r\n", (unsigned int)bg_col, aPosX, aPosY, aWidth, aHeight);
    pax_draw_rect(pax_buffer, bg_col, aPosX, aPosY, aWidth, aHeight);
    pax_outline_rect(pax_buffer, cursor_col, aPosX, aPosY, aWidth - 1, aHeight - 1);
    gui_edit_v2_render_text(pax_buffer, context, false, aPosX, aPosY, aWidth, aHeight, text_col, cursor_col, bg_col,
                            font, font_size);
    //}

    // Mark as not dirty.
    context->dirty = false;
}

// Handling of delete or backspace.
static void gui_edit_v2_delete(gui_edit_v2_context_t* context, bool is_backspace) {
    size_t oldlen = strlen(context->buffer);
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
        context->buffer[i] = context->buffer[i + 1];
    }

    context->dirty = true;
}

// Handling of normal input.
static void gui_edit_v2_append(gui_edit_v2_context_t* context, char value) {
    size_t oldlen = strlen(context->buffer);
    if (oldlen + 2 >= context->buffer_size) {
        // That's too big.
        return;
    }

    // Copy over the remainder of the buffer.
    // If there's no text this still copies the null terminator.
    for (int i = oldlen; i >= context->cursor; i--) {
        context->buffer[i + 1] = context->buffer[i];
    }

    // And finally insert at the character.
    context->buffer[context->cursor] = value;
    context->cursor++;
    context->dirty = true;
}

void gui_edit_v2_input_left(gui_edit_v2_context_t* context) {
    if (context->cursor > 0) context->cursor--;
    context->dirty = true;
}

void gui_edit_v2_input_right(gui_edit_v2_context_t* context) {
    if (context->cursor < strlen(context->buffer)) context->cursor++;
    context->dirty = true;
}

void gui_edit_v2_input_backspace(gui_edit_v2_context_t* context) {
    if (context->cursor < strlen(context->buffer)) context->cursor++;
    gui_edit_v2_delete(context, true);
}

void gui_edit_v2_input_text(gui_edit_v2_context_t* context, char character) {
    if (character >= ' ' && character <= '~') {
        gui_edit_v2_append(context, character);
    }
    context->dirty = true;
}
