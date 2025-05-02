
// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2025 Julian Scheffers <julian@scheffers.net>

#include "texteditor.h"
#include <stdio.h>
#include <string.h>
#include "icons.h"
#include "message_dialog.h"
#include "pax_text.h"
#include "pax_types.h"

// Minimum capacity kept per line in the file.
#define MIN_LINE_DATA_CAP 64
// Minimum capacity for lines of the file.
#define MIN_FILE_LINE_CAP 16

typedef struct {
    // Actual line length.
    size_t len;
    // Line byte capacity.
    size_t cap;
    // Bytes of data in this line.
    char*  data;
} text_line_t;

typedef struct {
    // Actual number of lines of text.
    size_t       len;
    // Capacity for lines of text.
    size_t       cap;
    // Lines of text in the open file.
    text_line_t* lines;
} text_file_t;

typedef struct {
    // 0-indexed line number.
    size_t line;
    // 0-indexed column number.
    size_t col;
} filepos_t;

typedef struct {
    // Framebuffer to draw the GUI and text lines into.
    pax_buf_t*   buffer;
    // GUI theme to apply.
    gui_theme_t* theme;
    // Path of the file being edited.
    char const*  file_path;
    // Data in the file being edited.
    text_file_t  file_data;
    // Current scroll offset.
    pax_vec2i    scroll_offset;
    // Current cursor position.
    filepos_t    cursor_pos;
    // Selection start position; usually equal to the cursor indicating no selection.
    filepos_t    selection_start;
    // On-screen position of the text area.
    pax_recti    textarea_bounds;
    // Number of lines that fit in the text area.
} texteditor_t;

// Allocate at least a certain capacity of bytes for a line of text.
bool text_line_alloc(text_line_t* line, size_t min_data_cap) {
    if (min_data_cap == 0) {
        // Empty; free its memory.
        free(line->data);
        line->data = NULL;
        line->cap  = 0;

    } else if (min_data_cap & (min_data_cap - 1)) {
        // Get the smallest power of two large enough.
        line->cap = MIN_LINE_DATA_CAP;
        while (line->cap < min_data_cap) {
            line->cap <<= 1;
        }
    }

    // Try to allocate the memory required.
    void* mem = realloc(line->data, line->cap);
    if (!mem) {
        return false;
    }
    line->data = mem;

    return true;
}

// Allocate at least a certain capacity of lines for a text file.
bool text_file_alloc(text_file_t* file, size_t min_data_cap) {
    if (min_data_cap == 0) {
        // Empty; free its memory.
        free(file->lines);
        file->lines = NULL;
        file->cap   = 0;

    } else if (min_data_cap & (min_data_cap - 1)) {
        // Get the smallest power of two large enough.
        file->cap = MIN_FILE_LINE_CAP;
        while (file->cap < min_data_cap) {
            file->cap <<= 1;
        }
    }

    // Try to allocate the memory required.
    void* mem = realloc(file->lines, file->cap * sizeof(text_line_t));
    if (!mem) {
        return false;
    }
    file->lines = mem;

    return true;
}

// Draw the text editor status bar.
static void draw_statusbar(texteditor_t* editor) {
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "Text editor: %s", editor->file_path);
    render_base_screen_statusbar(editor->buffer, editor->theme, true, true, false,
                                 ((gui_header_field_t[]){{get_icon(ICON_TEXTEDITOR), title_buf}}), 1, NULL, 0, NULL, 0);
}

// Style and draw a line of text given position.
static void draw_text_line_at(texteditor_t* editor, size_t line_index, pax_vec2i pos) {
    text_line_t line      = editor->file_data.lines[line_index];
    ptrdiff_t   cursorpos = -1;
    if (editor->cursor_pos.line == line_index) cursorpos = editor->cursor_pos.col;
    pax_draw_text_adv(editor->buffer, editor->theme->palette.color_foreground, editor->theme->menu.text_font,
                      editor->theme->menu.text_height, pos.x, pos.y, line.data, line.len, PAX_ALIGN_BEGIN,
                      PAX_ALIGN_BEGIN, cursorpos);
}

// Draw a single line's worth of text.
// The background color is drawn behind the text iff `erase` is `true`.
static void draw_text_line(texteditor_t* editor, size_t line_index, bool erase) {
}

// Scroll the view to a certain line and column.
// The line number is that of the top-most line displayed.
static void scroll_to(texteditor_t* editor, pax_vec2i new_scroll) {
}

// A simple text editor.
void menu_texteditor(pax_buf_t* buffer, gui_theme_t* theme, char const* file_path) {
    texteditor_t editor = {
        .buffer    = buffer,
        .theme     = theme,
        .file_path = file_path,
    };
    draw_statusbar(&editor);
}
