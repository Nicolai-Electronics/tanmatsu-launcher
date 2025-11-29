
// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2025 Julian Scheffers <julian@scheffers.net>

#pragma once

#include "menu/texteditor/types.h"

// Get the codepoint of the character immediately before the cursor.
bool texteditor_char_before_cursor(texteditor_t* editor, uint32_t* codepoint);
// Get the codepoint of the character immediately after the cursor.
bool texteditor_char_after_cursor(texteditor_t* editor, uint32_t* codepoint);

// Insert a new text line in a specific position.
bool text_file_new_line(text_file_t* file, size_t line_index);
// Remove a text line in a specific position.
bool text_file_remove_line(text_file_t* file, size_t line_index);
// Insert new text in the middle of a text line.
bool text_line_insert(text_file_t* file, size_t line_index, size_t offset, char const* data, size_t data_len);
// Remove text in the middle of a text line.
bool text_line_remove(text_file_t* file, size_t line_index, size_t offset, size_t data_len);

// Logic for moving the cursor up.
void texteditor_nav_up(texteditor_t* editor, bool is_pgup);
// Logic for moving the cursor down.
void texteditor_nav_down(texteditor_t* editor, bool is_pgdn);
// Logic for moving the cursor left.
void texteditor_nav_left(texteditor_t* editor, bool ctrl, bool is_home, bool is_backspace);
// Logic for moving the cursor right.
void texteditor_nav_right(texteditor_t* editor, bool ctrl, bool is_end, bool is_del);
