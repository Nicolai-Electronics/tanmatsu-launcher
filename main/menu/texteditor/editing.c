
// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2025 Julian Scheffers <julian@scheffers.net>

#include "menu/texteditor/editing.h"
#include <math.h>
#include "arrays.h"
#include "menu/texteditor/rendering.h"
#include "menu/texteditor/types.h"
#include "pax_text.h"
#include "pax_types.h"

// Get the codepoint of the character immediately before the cursor.
bool texteditor_char_before_cursor(texteditor_t* editor, uint32_t* codepoint) {
    if (editor->cursor_pos.line == 0 && editor->cursor_pos.col == 0) {
        return false;
    } else if (editor->cursor_pos.col == 0) {
        *codepoint = '\n';
        return true;
    }
    text_line_t* line   = &editor->file_data.lines[editor->cursor_pos.line];
    size_t       offset = pax_utf8_seekprev_l(line->data, line->data_len, editor->cursor_pos.offset);
    pax_utf8_getch_l(line->data + offset, line->data_len - offset, codepoint);
    return true;
}

// Get the codepoint of the character immediately after the cursor.
bool texteditor_char_after_cursor(texteditor_t* editor, uint32_t* codepoint) {
    text_line_t* line = &editor->file_data.lines[editor->cursor_pos.line];
    if (editor->cursor_pos.offset >= line->data_len) {
        if (editor->cursor_pos.line + 1 >= editor->file_data.lines_len) {
            return false;
        }
        *codepoint = '\n';
        return true;
    }
    pax_utf8_getch_l(line->data + editor->cursor_pos.offset, line->data_len - editor->cursor_pos.offset, codepoint);
    return true;
}

// Is a whitespace character?
static bool is_whitespace(uint32_t codepoint) {
    switch (codepoint) {
        case ' ':
        case '\t':
        case 0xA0:  // NBSP
            return true;
        default:
            return false;
    }
}

// Is a punctuation character?
static bool is_punctuation(uint32_t codepoint) {
    switch (codepoint) {
        case '.':
        case ',':
        case ';':
        case ':':
        case '!':
        case '?':
        case '-':
        case '_':
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case '<':
        case '>':
        case '/':
        case '\\':
        case '\'':
        case '\"':
            return true;
        default:
            return false;
    }
}

// Is a regular character?
static bool is_regular(uint32_t codepoint) {
    return !(is_whitespace(codepoint) || is_punctuation(codepoint));
}

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
    if (editor->cursor_pos.col == 0) {
        // Can't move left if already at the start of the line.
        return false;
    }

    // Find the start of the previous character.
    text_line_t* line       = &editor->file_data.lines[editor->cursor_pos.line];
    size_t       new_offset = pax_utf8_seekprev_l(line->data, line->data_len, editor->cursor_pos.offset);
    assert(new_offset < editor->cursor_pos.offset);
    editor->cursor_pos.col--;
    editor->cursor_pos.offset = new_offset;

    // Recalculate visual X position of cursor.
    texteditor_line_width(editor, editor->cursor_pos.line, true);
    editor->cursor_req_x = editor->cursor_vis_x;
    return true;
}

// Logic for moving the cursor left.
void texteditor_nav_left(texteditor_t* editor, bool ctrl, bool is_home, bool is_backspace) {
    if (is_home) {
        editor->cursor_req_x      = 0;
        editor->cursor_vis_x      = 0;
        editor->cursor_pos.col    = 0;
        editor->cursor_pos.offset = 0;
        return;
    } else if (editor->cursor_pos.col == 0) {
        // Move to end of previous line if possible.
        if (editor->cursor_pos.line != 0) {
            editor->cursor_pos.line--;
            text_line_t* line         = &editor->file_data.lines[editor->cursor_pos.line];
            editor->cursor_pos.col    = pax_utf8_strlen_l(line->data, line->data_len);
            editor->cursor_pos.offset = line->data_len;
        }
    }

    // Always move left at least once.
    uint32_t codepoint;
    texteditor_char_before_cursor(editor, &codepoint);
    if (texteditor_nav_left_helper(editor)) {
        uint32_t next_codepoint;
        if (!texteditor_char_before_cursor(editor, &next_codepoint)) {
            // Can't skip multiple characters if there aren't multiple.
        } else if (is_backspace) {
            uint32_t next_codepoint = 0;
            if (!is_whitespace(next_codepoint)) {
                // Skip an optional whitespace and a cluster of similar characters.
                bool punctuation = is_punctuation(next_codepoint);
                while (punctuation == is_punctuation(next_codepoint)) {
                    texteditor_nav_left_helper(editor);
                    if (!texteditor_char_before_cursor(editor, &next_codepoint)) {
                        break;
                    }
                }
            } else if (is_whitespace(codepoint)) {
                // Or: A cluster of multiple whitespace characters.
                do {
                    texteditor_nav_left_helper(editor);
                } while (texteditor_char_before_cursor(editor, &next_codepoint) && is_whitespace(next_codepoint));
            }
        } else if (ctrl) {
            // Skip a cluster of whitespace.
            bool has_next;
            do {
                texteditor_nav_left_helper(editor);
                has_next = texteditor_char_before_cursor(editor, &next_codepoint);
            } while (has_next && is_whitespace(next_codepoint));

            if (has_next && !is_punctuation(next_codepoint)) {
                // A cluster of regular characters.
                do {
                    texteditor_nav_left_helper(editor);
                } while (texteditor_char_before_cursor(editor, &next_codepoint) && is_regular(next_codepoint));

            } else if (has_next) {
                texteditor_nav_left_helper(editor);
                has_next = texteditor_char_before_cursor(editor, &next_codepoint);

                if (has_next && is_regular(codepoint)) {
                    // An optional punctuation, and a cluster of regular characters.
                    do {
                        texteditor_nav_left_helper(editor);
                    } while (texteditor_char_before_cursor(editor, &next_codepoint) && is_regular(next_codepoint));

                } else if (has_next && is_punctuation(codepoint)) {
                    // Or: Multiple punctuation characters.
                    do {
                        texteditor_nav_left_helper(editor);
                    } while (texteditor_char_before_cursor(editor, &next_codepoint) && is_punctuation(next_codepoint));
                }
            }
        }
    }

    // Recalculate visual X position of cursor.
    texteditor_line_width(editor, editor->cursor_pos.line, true);
    editor->cursor_req_x = editor->cursor_vis_x;
}

// Helper function that moves the cursor right one column.
// Returns whether the cursor was moved.
static bool texteditor_nav_right_helper(texteditor_t* editor) {
    text_line_t* line = &editor->file_data.lines[editor->cursor_pos.line];
    if (editor->cursor_pos.offset >= line->data_len) {
        // Can't move right if already at the end of the line.
        return false;
    }

    // Find the start of the next character.
    size_t new_offset = pax_utf8_seeknext_l(line->data, line->data_len, editor->cursor_pos.offset);
    assert(new_offset > editor->cursor_pos.offset);
    editor->cursor_pos.col++;
    editor->cursor_pos.offset = new_offset;

    // Recalculate visual X position of cursor.
    texteditor_line_width(editor, editor->cursor_pos.line, true);
    editor->cursor_req_x = editor->cursor_vis_x;
    return true;
}

// Logic for moving the cursor right.
void texteditor_nav_right(texteditor_t* editor, bool ctrl, bool is_end, bool is_del) {
    text_line_t* line = &editor->file_data.lines[editor->cursor_pos.line];
    if (is_end) {
        editor->cursor_pos.col    = pax_utf8_strlen_l(line->data, line->data_len);
        editor->cursor_pos.offset = line->data_len;

        // Recalculate visual X position of cursor.
        texteditor_line_width(editor, editor->cursor_pos.line, true);
        editor->cursor_req_x = editor->cursor_vis_x;
        return;
    } else if (editor->cursor_pos.offset >= line->data_len) {
        // Move to beginning of next line if possible.
        if (editor->cursor_pos.line < editor->file_data.lines_len - 1) {
            editor->cursor_pos.line++;
            editor->cursor_pos.col    = 0;
            editor->cursor_pos.offset = 0;
        }
    }

    // Always move left at least once.
    uint32_t codepoint;
    texteditor_char_after_cursor(editor, &codepoint);
    if (texteditor_nav_right_helper(editor)) {
        uint32_t next_codepoint;
        if (!texteditor_char_after_cursor(editor, &next_codepoint)) {
            // Can't skip multiple characters if there aren't multiple.
        } else if (is_del) {
            uint32_t next_codepoint = 0;
            if (!is_whitespace(next_codepoint)) {
                // Skip an optional whitespace and a cluster of similar characters.
                bool punctuation = is_punctuation(next_codepoint);
                while (punctuation == is_punctuation(next_codepoint)) {
                    texteditor_nav_right_helper(editor);
                    if (!texteditor_char_after_cursor(editor, &next_codepoint)) {
                        break;
                    }
                }
            } else if (is_whitespace(codepoint)) {
                // Or: A cluster of multiple whitespace characters.
                do {
                    texteditor_nav_right_helper(editor);
                } while (texteditor_char_after_cursor(editor, &next_codepoint) && is_whitespace(next_codepoint));
            }
        } else if (ctrl) {
            // Skip a cluster of whitespace.
            bool has_next;
            do {
                texteditor_nav_right_helper(editor);
                has_next = texteditor_char_after_cursor(editor, &next_codepoint);
            } while (has_next && is_whitespace(next_codepoint));

            if (has_next && !is_punctuation(next_codepoint)) {
                // A cluster of regular characters.
                do {
                    texteditor_nav_right_helper(editor);
                } while (texteditor_char_after_cursor(editor, &next_codepoint) && is_regular(next_codepoint));

            } else if (has_next) {
                texteditor_nav_right_helper(editor);
                has_next = texteditor_char_after_cursor(editor, &next_codepoint);

                if (has_next && is_regular(codepoint)) {
                    // An optional punctuation, and a cluster of regular characters.
                    do {
                        texteditor_nav_right_helper(editor);
                    } while (texteditor_char_after_cursor(editor, &next_codepoint) && is_regular(next_codepoint));

                } else if (has_next && is_punctuation(codepoint)) {
                    // Or: Multiple punctuation characters.
                    do {
                        texteditor_nav_right_helper(editor);
                    } while (texteditor_char_after_cursor(editor, &next_codepoint) && is_punctuation(next_codepoint));
                }
            }
        }
    }

    // Recalculate visual X position of cursor.
    texteditor_line_width(editor, editor->cursor_pos.line, true);
    editor->cursor_req_x = editor->cursor_vis_x;
}
