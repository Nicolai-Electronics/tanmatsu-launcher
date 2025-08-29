
// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2025 Julian Scheffers <julian@scheffers.net>

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "gui_style.h"
#include "pax_gfx.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define TEXTEDITOR_USE_PPA
#include <driver/ppa.h>
#endif

// Specifies what character sequence separates a line from the next.
typedef enum {
    // Carriage return (0x0d).
    LINE_END_CR,
    // Line feed (0x0a).
    LINE_END_LF,
    // Carriage return (0x0d), line feed (0x0a).
    LINE_END_CRLF,
} line_end_t;

// Line of text for the editor.
typedef struct {
    // How this line is separated from the next.
    // Ignored if there isn't another line after this.
    line_end_t line_end;
    // Actual line length.
    size_t     data_len;
    // Line byte capacity.
    size_t     data_cap;
    // Bytes of data in this line.
    char*      data;
    // Visual line width.
    float      vis_w;
} text_line_t;

// File of text for the editor.
typedef struct {
    // Actual number of lines of text.
    size_t       lines_len;
    // Capacity for lines of text.
    size_t       lines_cap;
    // Lines of text in the open file.
    text_line_t* lines;
} text_file_t;

// Position of the cursor or a character in a text file.
typedef struct {
    // Byte offset from the start of the file.
    size_t offset;
    // 0-indexed line number.
    size_t line;
    // 0-indexed column number.
    size_t col;
} filepos_t;

// All context needed to implement the text editor.
typedef struct {
    // Framebuffer to draw the GUI and text lines into.
    pax_buf_t*   buffer;
    // GUI theme to apply.
    gui_theme_t* theme;
    // Path of the file being edited, if any.
    char const*  file_path;
    // Data in the file being edited.
    text_file_t  file_data;
    // Current scroll offset in pixels.
    pax_vec2i    scroll_offset;
    // Requested cursor X position.
    float        cursor_req_x;
    // Current cursor X position.
    float        cursor_vis_x;
    // Current cursor position.
    filepos_t    cursor_pos;
    // Selection start position; usually equal to the cursor indicating no selection.
    filepos_t    selection_start;
    // On-screen position of the text area.
    pax_recti    textarea_bounds;
    // Scroll offset at last time of render.
    pax_vec2i    visible_scroll;
#ifdef TEXTEDITOR_USE_PPA
    // PPA client handle to use for PPA operations.
    ppa_client_handle_t ppa_handle;
#endif
} texteditor_t;
