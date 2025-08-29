
// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2025 Julian Scheffers <julian@scheffers.net>

#pragma once

#include "menu/texteditor/types.h"

// Get the width of a particular line.
float texteditor_line_width(texteditor_t* editor, size_t line_index, bool force_calc);
// Draw a single line's worth of text within relative horizontal bounds.
// The background color is drawn behind the text iff `erase` is `true`.
void  texteditor_draw_line_clip(texteditor_t* editor, size_t line_index, bool erase, int clip_x, int clip_w);
// Draw a single line's worth of text.
// The background color is drawn behind the text iff `erase` is `true`.
void  texteditor_draw_line(texteditor_t* editor, size_t line_index, bool erase);
// (Re-)draw the entire text area.
// If `redraw` is true, re-draw the entire text area instead of incrementally drawing.
void  texteditor_draw_text(texteditor_t* editor, bool redraw);
