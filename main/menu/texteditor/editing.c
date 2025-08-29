
// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2025 Julian Scheffers <julian@scheffers.net>

#include "menu/texteditor/editing.h"
#include <math.h>
#include "arrays.h"
#include "menu/texteditor/rendering.h"
#include "menu/texteditor/types.h"
#include "pax_text.h"
#include "pax_types.h"

// Insert a new text line in a specific position.
bool text_file_new_line(text_file_t* file, size_t line_index) {
    assert(line_index <= file->lines_len);
    text_line_t new_line = {0};
    if (file->lines_len) {
        new_line.line_end = file->lines[file->lines_len - 1].line_end;
    } else {
        new_line.line_end = LINE_END_LF;
    }
    return array_lencap_insert(&file->lines, sizeof(text_line_t), &file->lines_len, &file->lines_cap, &new_line,
                               line_index);
}

// Insert new text in the middle of a text line.
bool text_line_insert(text_file_t* file, size_t line_index, size_t offset, char const* data, size_t data_len) {
    assert(line_index < file->lines_len);
    text_line_t* line = &file->lines[line_index];
    assert(offset <= line->data_len);
    return array_lencap_insert_n(&line->data, 1, &line->data_len, &line->data_cap, data, offset, data_len);
}

// Helper function that moves the cursor for `texteditor_nav_up` and `texteditor_nav_down`.
static void texteditor_nav_v_helper(texteditor_t* editor) {
    text_line_t const* line      = &editor->file_data.lines[editor->cursor_pos.line];
    pax_font_t const*  font      = editor->theme->menu.text_font;
    int                font_size = editor->theme->menu.text_height;

    // Iterate through the line to find the closest horizontal position to the desired one.
    float const req_x  = editor->cursor_req_x;
    float       vis_x  = 0;
    size_t      col    = 0;
    size_t      offset = 0;
    while (1) {
        // Measure the next character.
        size_t new_offset = pax_utf8_seeknext_l(line->data, line->data_len, offset);
        size_t len        = new_offset - offset;
        float w = pax_text_size_adv(font, font_size, line->data + offset, len, PAX_ALIGN_BEGIN, PAX_ALIGN_BEGIN, -1).x0;

        // If the next character would put the cursor further away, stop here.
        if (fabsf(vis_x - req_x) < fabsf(vis_x + w - req_x)) {
            break;
        }

        // Otherwise, this is the new best cursor position.
        vis_x += w;
        col++;
        offset = new_offset;
    }

    editor->cursor_pos.col    = col;
    editor->cursor_pos.offset = offset;
    editor->cursor_vis_x      = vis_x;
}

// Logic for moving the cursor up.
void texteditor_nav_up(texteditor_t* editor, bool is_pgup) {
    if (is_pgup) {
        size_t page_size = editor->textarea_bounds.h / editor->theme->menu.text_height;
        if (editor->cursor_pos.line <= page_size) {
            editor->cursor_pos.line = 0;
        } else {
            editor->cursor_pos.line -= page_size;
        }
        texteditor_nav_v_helper(editor);
    } else if (editor->cursor_pos.line > 0) {
        editor->cursor_pos.line--;
        texteditor_nav_v_helper(editor);
    }
}

// Logic for moving the cursor down.
void texteditor_nav_down(texteditor_t* editor, bool is_pgdn) {
    if (is_pgdn) {
        size_t page_size = editor->textarea_bounds.h / editor->theme->menu.text_height;
        if (editor->cursor_pos.line + page_size >= editor->file_data.lines_len) {
            editor->cursor_pos.line = editor->file_data.lines_len - 1;
        } else {
            editor->cursor_pos.line += page_size;
        }
        texteditor_nav_v_helper(editor);
    } else if (editor->cursor_pos.line < editor->file_data.lines_len) {
        editor->cursor_pos.line++;
        texteditor_nav_v_helper(editor);
    }
}

// Helper function that moves the cursor left one column.
// Returns whether the cursor was moved.
static bool texteditor_nav_left_helper(texteditor_t* editor) {
}

// Logic for moving the cursor left.
void texteditor_nav_left(texteditor_t* editor, bool ctrl, bool is_home, bool is_backspace) {
    if (is_home) {
        editor->cursor_req_x      = 0;
        editor->cursor_vis_x      = 0;
        editor->cursor_pos.col    = 0;
        editor->cursor_pos.offset = 0;
        return;
    }
}

// Helper function that moves the cursor right one column.
// Returns whether the cursor was moved.
static bool texteditor_nav_right_helper(texteditor_t* editor) {
}

// Logic for moving the cursor right.
void texteditor_nav_right(texteditor_t* editor, bool ctrl, bool is_end, bool is_del) {
    text_line_t* line = &editor->file_data.lines[editor->cursor_pos.line];
    if (is_end) {
        editor->cursor_pos.col    = pax_utf8_strlen_l(line->data, line->data_len);
        editor->cursor_pos.offset = line->data_len;
        texteditor_line_width(editor, editor->cursor_pos.line, true);  // Recalculate visual X position of cursor.
        editor->cursor_req_x = editor->cursor_vis_x;
        return;
    }
}
