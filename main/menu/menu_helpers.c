#include "menu_helpers.h"
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "freertos/idf_additions.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"

pax_vec2_t menu_calc_position(pax_buf_t* buffer, gui_theme_t* theme) {
    int        header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int        footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);
    pax_vec2_t position      = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };
    return position;
}

typedef enum {
    MENU_RUN_MODE_LIST,
    MENU_RUN_MODE_GRID,
} menu_run_mode_t;

static void menu_run_internal(menu_t* menu, gui_element_icontext_t* header, size_t header_count,
                              gui_element_icontext_t* footer_left, size_t footer_left_count,
                              gui_element_icontext_t* footer_right, size_t footer_right_count,
                              menu_action_cb_t action_cb, void* user_ctx, bool home_is_back, menu_run_mode_t mode) {
    pax_buf_t*   buffer   = display_get_buffer();
    gui_theme_t* theme    = get_theme();
    pax_vec2_t   position = menu_calc_position(buffer, theme);

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    bool do_full_render = true;
    bool do_icons       = true;

    while (1) {
        if (do_full_render || do_icons) {
            render_base_screen_statusbar(buffer, theme, do_full_render, do_full_render || do_icons, do_full_render,
                                         header, header_count, footer_left, footer_left_count, footer_right,
                                         footer_right_count);
        }
        if (mode == MENU_RUN_MODE_GRID) {
            menu_render_grid(buffer, menu, position, theme, !do_full_render);
        } else {
            menu_render(buffer, menu, position, theme, !do_full_render);
        }
        display_blit_buffer(buffer);

        do_full_render = false;
        do_icons       = false;

        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (event.type == INPUT_EVENT_TYPE_NAVIGATION && event.args_navigation.state) {
                switch (event.args_navigation.key) {
                    case BSP_INPUT_NAVIGATION_KEY_ESC:
                    case BSP_INPUT_NAVIGATION_KEY_F1:
                    case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                        menu_free(menu);
                        return;
                    case BSP_INPUT_NAVIGATION_KEY_HOME:
                        if (home_is_back) {
                            menu_free(menu);
                            return;
                        }
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_UP:
                        if (mode == MENU_RUN_MODE_GRID) {
                            menu_navigate_previous_row(menu, theme);
                        } else {
                            menu_navigate_previous(menu);
                        }
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_DOWN:
                        if (mode == MENU_RUN_MODE_GRID) {
                            menu_navigate_next_row(menu, theme);
                        } else {
                            menu_navigate_next(menu);
                        }
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_LEFT:
                        if (mode == MENU_RUN_MODE_GRID) {
                            menu_navigate_previous(menu);
                        }
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                        if (mode == MENU_RUN_MODE_GRID) {
                            menu_navigate_next(menu);
                        }
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_RETURN:
                    case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                    case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                        void* arg = menu_get_callback_args(menu, menu_get_position(menu));
                        if (action_cb) {
                            bool should_exit = action_cb(arg, user_ctx);
                            if (should_exit) {
                                menu_free(menu);
                                return;
                            }
                        }
                        do_full_render = true;
                        do_icons       = true;
                        break;
                    }
                    default:
                        break;
                }
            }
        } else {
            do_icons = true;
        }
    }
}

void menu_run_list(menu_t* menu, gui_element_icontext_t* header, size_t header_count,
                   gui_element_icontext_t* footer_left, size_t footer_left_count, gui_element_icontext_t* footer_right,
                   size_t footer_right_count, menu_action_cb_t action_cb, void* user_ctx, bool home_is_back) {
    menu_run_internal(menu, header, header_count, footer_left, footer_left_count, footer_right, footer_right_count,
                      action_cb, user_ctx, home_is_back, MENU_RUN_MODE_LIST);
}

void menu_run_grid(menu_t* menu, gui_element_icontext_t* header, size_t header_count,
                   gui_element_icontext_t* footer_left, size_t footer_left_count, gui_element_icontext_t* footer_right,
                   size_t footer_right_count, menu_action_cb_t action_cb, void* user_ctx, bool home_is_back) {
    menu_run_internal(menu, header, header_count, footer_left, footer_left_count, footer_right, footer_right_count,
                      action_cb, user_ctx, home_is_back, MENU_RUN_MODE_GRID);
}
