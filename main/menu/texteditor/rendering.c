
// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2025 Julian Scheffers <julian@scheffers.net>

#include "menu/texteditor/rendering.h"
#include <limits.h>
#include <stdio.h>
#include "menu/texteditor/types.h"
#include "pax_gfx.h"
#include "pax_text.h"

// Get the width of a particular line.
float texteditor_line_width(texteditor_t* editor, size_t line_index, bool force_calc) {
    if (force_calc || isnan(editor->file_data.lines[line_index].vis_w)) {
        ptrdiff_t cursorpos = -1;
        if (editor->cursor_pos.line == line_index) cursorpos = editor->cursor_pos.col;
        pax_2vec2f dims = pax_text_size_adv(
            editor->theme->menu.text_font, editor->theme->menu.text_height, editor->file_data.lines[line_index].data,
            editor->file_data.lines[line_index].data_len, PAX_ALIGN_BEGIN, PAX_ALIGN_BEGIN, cursorpos);
        editor->file_data.lines[line_index].vis_w = dims.x0;
        if (cursorpos != -1) {
            editor->cursor_vis_x = dims.x1;
        }
    }
    return editor->file_data.lines[line_index].vis_w;
}

// Style and draw a line of text given position.
static void texteditor_draw_line_impl1(texteditor_t* editor, size_t line_index, pax_vec2i pos) {
    text_line_t line      = editor->file_data.lines[line_index];
    ptrdiff_t   cursorpos = -1;
    if (editor->cursor_pos.line == line_index) cursorpos = editor->cursor_pos.col;
    pax_2vec2f dims = pax_draw_text_adv(editor->buffer, editor->theme->palette.color_foreground,
                                        editor->theme->menu.text_font, editor->theme->menu.text_height, pos.x, pos.y,
                                        line.data, line.data_len, PAX_ALIGN_BEGIN, PAX_ALIGN_BEGIN, cursorpos);
    editor->file_data.lines[line_index].vis_w = dims.x0;
    if (cursorpos != -1) {
        editor->cursor_vis_x = dims.x1 - pos.x;
    }
}

// Helper function for `draw_text_line`, that correctly positions and clips the text vertically.
// Takes precalculated X position to pass through to `draw_text_line_at`, and X clip window.
static void texteditor_draw_line_impl0(texteditor_t* editor, size_t line_index, int x, int clip_x, int clip_w,
                                       bool erase) {
    pax_vec2i const scroll      = editor->scroll_offset;
    pax_col_t const bg_col      = editor->cursor_pos.line == line_index ? editor->theme->palette.color_active_background
                                                                        : editor->theme->palette.color_background;
    int const       line_height = editor->theme->menu.text_height;
    pax_recti const bounds      = editor->textarea_bounds;
    int const       cyclic_y    = line_index * line_height % bounds.h;

    // Compute clip rectangle.
    int clip_y0 = line_index * line_height;
    int clip_h  = line_height;
    if (clip_y0 < scroll.y) {
        clip_h  += scroll.y - clip_y0;
        clip_y0 += scroll.y - clip_y0;
    }
    if (clip_y0 + clip_h > bounds.h + scroll.y) {
        clip_h = bounds.h + scroll.y - clip_y0;
    }
    clip_y0 %= bounds.h;

    // Limit to visible area.
    int clip_h0 = clip_h;
    if (clip_y0 + clip_h0 > bounds.h) {
        clip_h0 = bounds.h - clip_y0;
    }

    // Draw the first time.
    if (clip_h0 > 0) {
        pax_clip(editor->buffer, clip_x, clip_y0 + bounds.y, clip_w, clip_h0);
        if (erase) {
            pax_draw_rect(editor->buffer, bg_col, clip_x, clip_y0 + bounds.y, clip_w, clip_h0);
        }
        texteditor_draw_line_impl1(editor, line_index, (pax_vec2i){x, cyclic_y + bounds.y});
    }

    // Draw twice because this wraps around to the top of the area.
    if (clip_h0 != clip_h) {
        int clip_h1 = (line_index + 1) * line_height % bounds.h;
        if (clip_h1 > scroll.y % bounds.h) {
            clip_h1 = scroll.y % bounds.h;
        }

        if (clip_h1 > 0) {
            pax_clip(editor->buffer, clip_x, bounds.y, clip_w, clip_h1);
            if (erase) {
                pax_draw_rect(editor->buffer, bg_col, clip_x, bounds.y, clip_w, clip_h1);
            }
            texteditor_draw_line_impl1(editor, line_index, (pax_vec2i){x, cyclic_y + bounds.y - bounds.h});
        }
    }
}

// Draw a single line's worth of text within relative horizontal bounds.
// The background color is drawn behind the text iff `erase` is `true`.
void texteditor_draw_line_clip(texteditor_t* editor, size_t line_index, bool erase, int clip_x, int clip_w) {
    pax_recti       orig_clip = pax_get_clip(editor->buffer);
    pax_recti const bounds    = editor->textarea_bounds;
    pax_vec2i const scroll    = editor->scroll_offset;

    int const subscroll = scroll.x % bounds.w;
    int const pos_x     = bounds.x - scroll.x + subscroll;

    int clip_x0 = clip_x % bounds.w;
    int clip_w0 = clip_w;
    if (clip_x0 < subscroll) {
        clip_w0 -= subscroll - clip_x0;
        clip_x0  = subscroll;
    }
    if (clip_x0 + clip_w0 > bounds.w) {
        clip_w0 = bounds.w - clip_x0;
    }
    if (clip_w0 > 0) {
        texteditor_draw_line_impl0(editor, line_index, pos_x, bounds.x + clip_x0, clip_w0, erase);
    }

    if (subscroll) {
        int clip_x1 = clip_x % bounds.w;
        int clip_w1 = clip_w;
        if (clip_x1 + clip_w1 > subscroll) {
            clip_w1 = subscroll - clip_x1;
        }
        if (clip_w1 > 0) {
            texteditor_draw_line_impl0(editor, line_index, pos_x - bounds.w, bounds.x + clip_x1, clip_w1, erase);
        }
    }

    pax_set_clip(editor->buffer, orig_clip);
}

// Draw a single line's worth of text.
// The background color is drawn behind the text iff `erase` is `true`.
void texteditor_draw_line(texteditor_t* editor, size_t line_index, bool erase) {
    texteditor_draw_line_clip(editor, line_index, erase, 0, INT_MAX);
}

// Re-draws the entire text area.
static void texteditor_redraw_text_impl(texteditor_t* editor) {
    // Draw text background all at once as an optimization.
    pax_draw_rect(editor->buffer, editor->theme->palette.color_background, editor->textarea_bounds.x,
                  editor->textarea_bounds.y, editor->textarea_bounds.w, editor->textarea_bounds.h);

    // Determine what lines are supposed to be on screen.
    int const line_height = editor->theme->menu.text_height;
    size_t    start       = editor->scroll_offset.y / line_height;
    size_t    end         = (editor->scroll_offset.y + editor->textarea_bounds.h - 1) / line_height;
    if (end > editor->file_data.lines_len - 1) {
        end = editor->file_data.lines_len - 1;
    }

    // Then draw all lines that are on screen.
    for (size_t i = start; i <= end; i++) {
        texteditor_draw_line(editor, i, i == editor->cursor_pos.line);
    }
}

// (Re-)draw the entire text area.
// If `redraw` is true, re-draw the entire text area instead of incrementally drawing.
void texteditor_draw_text(texteditor_t* editor, bool redraw) {
    if (redraw || abs(editor->visible_scroll.y - editor->scroll_offset.y) >= editor->textarea_bounds.h ||
        abs(editor->visible_scroll.x - editor->scroll_offset.x) >= editor->textarea_bounds.w) {
        // Forced redraw or scrolled so far that redrawing is better.
        texteditor_redraw_text_impl(editor);
        return;
    }

    // Determine what lines are supposed to be on screen.
    int const line_height = editor->theme->menu.text_height;
    size_t    start       = editor->scroll_offset.y / line_height;
    size_t    end         = (editor->scroll_offset.y + editor->textarea_bounds.h - 1) / line_height;
    if (end > editor->file_data.lines_len - 1) {
        end = editor->file_data.lines_len - 1;
    }
    size_t const visible_start = start, visible_end = end;

    if (editor->visible_scroll.y < editor->scroll_offset.y) {
        // Scrolling down.
        // Calculate the last line that would have been drawn previously.
        size_t prev_end = (editor->visible_scroll.y + editor->textarea_bounds.h - 1) / line_height;
        if (prev_end > editor->file_data.lines_len - 1) {
            prev_end = editor->file_data.lines_len - 1;
        }
        if (start < prev_end) {
            start = prev_end;
        }

    } else if (editor->visible_scroll.y < editor->scroll_offset.y) {
        // Scrolling up.
        // Calculate the first line that would have been drawn previously.
        size_t prev_start = editor->visible_scroll.y / line_height;
        if (end > prev_start) {
            end = prev_start;
        }
    }

    // Compute clip X and width.
    int clip_x = 0;
    int clip_w = INT_MAX;
    if (editor->visible_scroll.x > editor->scroll_offset.x) {
        // Scrolling left.
        clip_x = editor->scroll_offset.x;
        clip_w = editor->visible_scroll.x - editor->scroll_offset.x;
    } else if (editor->visible_scroll.x < editor->scroll_offset.x) {
        // Scrolling right.
        clip_x = editor->visible_scroll.x;
        clip_w = editor->scroll_offset.x - editor->visible_scroll.x;
    }

    // Draw the calculated region of lines.
    if (editor->visible_scroll.x != editor->scroll_offset.x && editor->visible_scroll.y != editor->scroll_offset.y) {
    } else if (editor->visible_scroll.x != editor->scroll_offset.x) {
        // Just horizontal scrolling.
        for (size_t i = visible_start; i <= visible_end; i++) {
            texteditor_draw_line_clip(editor, i, true, clip_x, clip_w);
            // texteditor_draw_line(editor, i, true);
        }
    } else if (editor->visible_scroll.y != editor->scroll_offset.y) {
        // Just vertical scrolling.
        for (size_t i = start; i <= end; i++) {
            texteditor_draw_line(editor, i, true);
        }
    }

    // Update what has been drawn.
    editor->visible_scroll = editor->scroll_offset;
}
