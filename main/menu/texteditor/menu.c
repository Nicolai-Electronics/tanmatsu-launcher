
// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2025 Julian Scheffers <julian@scheffers.net>

#include <esp_cache.h>
#include <freertos/FreeRTOS.h>
#include <math.h>
#include <sdkconfig.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "bsp/display.h"
#include "bsp/input.h"
#include "esp_err.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_types.h"
#include "gui_element_icontext.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "menu/texteditor.h"
#include "menu/texteditor/editing.h"
#include "menu/texteditor/rendering.h"
#include "menu/texteditor/saving.h"
#include "menu/texteditor/types.h"
#include "pax_gfx.h"

#ifdef TEXTEDITOR_USE_PPA
#include <driver/ppa.h>
#endif

#ifdef TEXTEDITOR_USE_PPA
// Helper function to do one scale-rotation-mirror with the PPA.
static void texteditor_do_srm(texteditor_t* editor, void* fb, pax_vec2i dest, pax_recti src) {
    pax_vec2i const raw_dims = pax_buf_get_dims_raw(editor->buffer);
    if (src.w <= 0 || src.h <= 0) {
        return;
    }

    pax_recti dest_ = {dest.x, dest.y, src.w, src.h};
    dest_           = pax_recti_abs(pax_orient_det_recti(editor->buffer, dest_));
    src             = pax_recti_abs(pax_orient_det_recti(editor->buffer, src));

    // TODO: Account for color modes that aren't RGB565.
    ppa_srm_oper_config_t const operation = {
        {
            pax_buf_get_pixels(editor->buffer),
            raw_dims.x,
            raw_dims.y,
            src.w,
            src.h,
            src.x,
            src.y,
            {.srm_cm = PPA_SRM_COLOR_MODE_RGB565},
            0,
            0,
        },
        {
            fb,
            pax_buf_get_size(editor->buffer),
            raw_dims.x,
            raw_dims.y,
            dest_.x,
            dest_.y,
            {.srm_cm = PPA_SRM_COLOR_MODE_RGB565},
            0,
            0,
        },
        PPA_SRM_ROTATION_ANGLE_0,
        1,
        1,
        false,
        false,
        false,
        false,
        PPA_ALPHA_FIX_VALUE,
        {0},
        PPA_TRANS_MODE_BLOCKING,
        NULL,
    };
    ppa_do_scale_rotate_mirror(editor->ppa_handle, &operation);
}
#endif

// Blit everything to the screen.
static void texteditor_blit(texteditor_t* editor) {
#ifdef TEXTEDITOR_USE_PPA
    pax_vec2i const dims = pax_buf_get_dims(editor->buffer);

    // Flush caches so the PPA can read what has been drawn.
    asm("fence rw,rw");
    size_t sync_addr = (size_t)pax_buf_get_pixels(editor->buffer);
    size_t sync_size = pax_buf_get_size(editor->buffer);
    if (sync_addr % CONFIG_CACHE_L2_CACHE_LINE_SIZE) {
        sync_size += sync_addr % CONFIG_CACHE_L2_CACHE_LINE_SIZE;
        sync_addr -= sync_addr % CONFIG_CACHE_L2_CACHE_LINE_SIZE;
    }
    if (sync_size % CONFIG_CACHE_L2_CACHE_LINE_SIZE) {
        sync_size += CONFIG_CACHE_L2_CACHE_LINE_SIZE - sync_size % CONFIG_CACHE_L2_CACHE_LINE_SIZE;
    }
    esp_cache_msync((void*)sync_addr, sync_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);

    // Use PPA for copying.
    esp_lcd_panel_handle_t panel;
    ESP_ERROR_CHECK(bsp_display_get_panel(&panel));
    void* fb;
    esp_lcd_dpi_panel_get_frame_buffer(panel, 1, &fb);

    // Blit the text area itself.
    pax_recti const bounds  = editor->textarea_bounds;
    pax_vec2i const scroll  = editor->scroll_offset;
    int const       split_x = scroll.x % bounds.w;
    int const       split_y = scroll.y % bounds.h;
    texteditor_do_srm(editor, fb, (pax_vec2i){bounds.x, bounds.y},
                      (pax_recti){bounds.x + split_x, bounds.y + split_y, bounds.w - split_x, bounds.h - split_y});
    texteditor_do_srm(editor, fb, (pax_vec2i){bounds.x, bounds.y + bounds.h - split_y},
                      (pax_recti){bounds.x + split_x, bounds.y, bounds.w - split_x, split_y});
    texteditor_do_srm(editor, fb, (pax_vec2i){bounds.x + bounds.w - split_x, bounds.y},
                      (pax_recti){bounds.x, bounds.y + split_y, split_x, bounds.h - split_y});
    texteditor_do_srm(editor, fb, (pax_vec2i){bounds.x + bounds.w - split_x, bounds.y + bounds.h - split_y},
                      (pax_recti){bounds.x, bounds.y, split_x, split_y});

    // Blit everything surrounding the text area.
    texteditor_do_srm(editor, fb, (pax_vec2i){0, 0}, (pax_recti){0, 0, dims.x, bounds.y});
    texteditor_do_srm(editor, fb, (pax_vec2i){0, bounds.h + bounds.y},
                      (pax_recti){0, 0, dims.x, dims.y - bounds.h - bounds.y});
    texteditor_do_srm(editor, fb, (pax_vec2i){0, bounds.y},
                      (pax_recti){0, bounds.y, bounds.x, dims.y - bounds.h - bounds.y});
    texteditor_do_srm(editor, fb, (pax_vec2i){bounds.x + bounds.w, bounds.y},
                      (pax_recti){bounds.x + bounds.w, bounds.y, bounds.x, dims.y - bounds.h - bounds.y});

    // TODO: Do async instead, then wait for PPA to finish?
#else
    // Uses non-PPA rendering; just blit the entire buffer.
    pax_vec2i const dims = pax_buf_get_dims_raw(editor->buffer);
    bsp_display_blit(0, 0, dims.x, dims.y, pax_buf_get_pixels(editor->buffer));
#endif
}

// Draw the text editor status bar.
static void draw_statusbar(texteditor_t* editor) {
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "Text editor: %s", editor->file_path);
    render_base_screen_statusbar(editor->buffer, editor->theme, true, true, false,
                                 ((gui_element_icontext_t[]){{get_icon(ICON_TEXTEDITOR), title_buf}}), 1, NULL, 0, NULL,
                                 0);
}

// Scroll by a certain number of pixels.
static void menu_texteditor_scroll(texteditor_t* editor, pax_vec2i offset) {
    editor->scroll_offset.x += offset.x;
    editor->scroll_offset.y += offset.y;

    float max_text_w = 0;
    for (size_t i = 0; i < editor->file_data.lines_len; i++) {
        max_text_w = fmaxf(max_text_w, texteditor_line_width(editor, i, false));
    }

    if (editor->scroll_offset.x > max_text_w) {
        editor->scroll_offset.x = max_text_w;
    }
    if (editor->scroll_offset.x < 0) {  // Note: Prev check may set x < 0, do not reorder.
        editor->scroll_offset.x = 0;
    }

    if (editor->scroll_offset.y >
        editor->theme->menu.text_height * (ptrdiff_t)editor->file_data.lines_len - editor->textarea_bounds.h) {
        editor->scroll_offset.y =
            editor->theme->menu.text_height * (ptrdiff_t)editor->file_data.lines_len - editor->textarea_bounds.h;
    }
    if (editor->scroll_offset.y < 0) {  // Note: Prev check may set y < 0, do not reorder.
        editor->scroll_offset.y = 0;
    }
}

// A simple text editor.
void menu_texteditor(pax_buf_t* buffer, gui_theme_t* theme, char const* file_path) {
    texteditor_t editor = {
        .buffer    = buffer,
        .theme     = theme,
        .file_path = file_path,
    };

#ifdef TEXTEDITOR_USE_PPA
    // Register PPA client for the menu blitting to display.
    const ppa_client_config_t ppa_cfg = {
        PPA_OPERATION_SRM,
        8,
        PPA_DATA_BURST_LENGTH_16,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_cfg, &editor.ppa_handle));
#endif

    // Compute the available editor space given the theme.
    editor.textarea_bounds.x = 5;
    editor.textarea_bounds.w = pax_buf_get_width(buffer) - 10;
    editor.textarea_bounds.y = theme->header.height + 3 * theme->header.vertical_margin;
    editor.textarea_bounds.h = pax_buf_get_height(buffer) - editor.textarea_bounds.y - 5;

    // Put some data in the file.
    static const char data[] = {
        // clang-format off
        "Hello, World!\n"
        "This text file\r"
        "Has many different line endings\r\n"
        "And also many lines\n"
        "I'm going to fill up\n"
        "The entire screen\n"
        "With text\n"
        "So I can test scrolling\n"
        "I'm also going to do this same thing horizontally so I can test scrolling horizontally too, so I need to make this sentence ridiculously long to run out of horizontal space\n"
        "Padding 0\n"
        "Padding 1\n"
        "Padding 2\n"
        "Padding 3\n"
        "Padding 4\n"
        "Padding 5\n"
        "Padding 6\n"
        "Padding 7\n"
        "Padding 8\n"
        "Padding 9\n"
        "Padding 10\n"
        "Padding 11\n"
        "Padding 12\n"
        "Padding 13\n"
        "Padding 14\n"
        "Padding 15\n"
        "Padding 16\n"
        "Padding 17\n"
        "Padding 18\n"
        "Padding 19\n"
        "Padding 20\n"
        "Padding 21\n"
        "Padding 22\n"
        "Padding 23\n"
        "Padding 24\n"
        "Padding 25\n"
        "Padding 26\n"
        "Padding 27\n"
        "Padding 28\n"
        "Padding 29\n"
        // clang-format on
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(text_file_from_blob(&editor.file_data, data, sizeof(data) - 1));

    draw_statusbar(&editor);
    texteditor_draw_text(&editor, true);
    texteditor_blit(&editor);

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    while (1) {
        bsp_input_event_t event;
        taskYIELD();
        xQueueReceive(input_event_queue, &event, portMAX_DELAY);

        if (event.type == INPUT_EVENT_TYPE_NAVIGATION && event.args_navigation.state) {
            bool const fn   = event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION;
            bool const ctrl = event.args_navigation.modifiers & BSP_INPUT_MODIFIER_CTRL;
            switch (event.args_navigation.key) {
                default:
                    break;
                case BSP_INPUT_NAVIGATION_KEY_ESC:
                    goto break_outer;
                case BSP_INPUT_NAVIGATION_KEY_UP:
                    texteditor_nav_up(&editor, fn);
                    break;
                case BSP_INPUT_NAVIGATION_KEY_DOWN:
                    texteditor_nav_down(&editor, fn);
                    break;
                case BSP_INPUT_NAVIGATION_KEY_LEFT:
                    texteditor_nav_left(&editor, ctrl, fn, false);
                    break;
                case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                    texteditor_nav_right(&editor, ctrl, fn, false);
                    break;
            }
        }

        texteditor_draw_text(&editor, true);
        texteditor_blit(&editor);
    }
break_outer:

#ifdef TEXTEDITOR_USE_PPA
    // Remove the PPA client again.
    ESP_ERROR_CHECK(ppa_unregister_client(editor.ppa_handle));
#endif
}
