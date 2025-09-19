#include "bsp/input.h"
#include "common/display.h"
#include "freertos/idf_additions.h"
#include "gui_edit.h"
#include "gui_osk_edit.h"
#include "gui_style.h"
#include "icons.h"
#include "message_dialog.h"
#include "pax_gfx.h"
#include "pax_types.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "⏎ Accept"}}), 1
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "🅰 Accept"}}), 1
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#endif

void render(pax_buf_t* buffer, gui_theme_t* theme, bool partial, bool icons, char* title) {
    render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                 ((gui_element_icontext_t[]){{get_icon(ICON_EXTENSION), title}}), 1, FOOTER_LEFT,
                                 FOOTER_RIGHT);
}

void menu_textbox(pax_buf_t* buffer, gui_theme_t* theme, const char* title, char* text, size_t size, bool multiline,
                   bool* out_accepted) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));
    render(buffer, theme, false, true, title);

    if (bsp_input_needs_on_screen_keyboard()) {
        gui_osk_ctx_t kb_ctx = {0};
        float         w      = 260;
        float         h      = 180;
        float         x      = (pax_buf_get_width(buffer) - w) / 2;
        float         y      = (pax_buf_get_height(buffer) - h) / 2;
        gui_osk_edit_init(&kb_ctx, buffer, x, y, w, h, title, "", text, size);
        gui_osk_edit_loop(&kb_ctx, buffer, NULL);
        display_blit_buffer(buffer);

        while (1) {
            bool              force_flush = false;
            bsp_input_event_t event;
            if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
                switch (event.type) {
                    case INPUT_EVENT_TYPE_NAVIGATION: {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                            case BSP_INPUT_NAVIGATION_KEY_HOME: {
                                if (event.args_navigation.state) {
                                    gui_osk_edit_destroy(&kb_ctx, out_accepted, text, size);
                                    return;
                                }
                            }
                            case BSP_INPUT_NAVIGATION_KEY_F2:
                            case BSP_INPUT_NAVIGATION_KEY_SELECT:
                                gui_osk_edit_navigation_event(&kb_ctx, GUI_OSK_MODESELECT, event.args_navigation.state);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                gui_osk_edit_navigation_event(&kb_ctx, GUI_OSK_UP, event.args_navigation.state);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN: {
                                gui_osk_edit_navigation_event(&kb_ctx, GUI_OSK_DOWN, event.args_navigation.state);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_LEFT:
                                gui_osk_edit_navigation_event(&kb_ctx, GUI_OSK_LEFT, event.args_navigation.state);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT: {
                                gui_osk_edit_navigation_event(&kb_ctx, GUI_OSK_RIGHT, event.args_navigation.state);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                            case BSP_INPUT_NAVIGATION_KEY_BACKSPACE: {
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_SHIFT) {
                                    gui_osk_edit_navigation_event(&kb_ctx, GUI_OSK_DELETE_AFTER,
                                                                  event.args_navigation.state);
                                } else {
                                    gui_osk_edit_navigation_event(&kb_ctx, GUI_OSK_DELETE_BEFORE,
                                                                  event.args_navigation.state);
                                }
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                gui_osk_edit_navigation_event(&kb_ctx, GUI_OSK_CHARSELECT, event.args_navigation.state);
                                break;
                            }
                            default:
                                break;
                        }
                        break;
                    }
                    default:
                        break;
                }
            } else {
                force_flush = true;
                render(buffer, theme, true, true, title);
            }
            bool flush = false;
            gui_osk_edit_loop(&kb_ctx, buffer, &flush);
            if (flush || force_flush) {
                display_blit_buffer(buffer);
            }

            bool accepted = false;
            gui_osk_edit_get_accepted(&kb_ctx, &accepted);
            if (accepted) {
                gui_osk_edit_destroy(&kb_ctx, out_accepted, text, size);
                return;
            }
        }
    } else {
        float w = 260;
        float h = 180;
        float x = (pax_buf_get_width(buffer) - w) / 2;
        float y = (pax_buf_get_height(buffer) - h) / 2;

        gui_edit_context_t context = {0};
        gui_edit_init(buffer, &context, x, y, w, h, text, size);

        gui_edit_render(buffer, &context);
        display_blit_buffer(buffer);

        while (1) {
            bsp_input_event_t event;
            if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
                switch (event.type) {
                    case INPUT_EVENT_TYPE_NAVIGATION: {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B: {
                                if (event.args_navigation.state) {
                                    gui_edit_destroy(&context, NULL, 0);
                                    *out_accepted = false;
                                    return;
                                }
                            }
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                                if (event.args_navigation.state) {
                                    gui_edit_destroy(&context, text, size);
                                    *out_accepted = true;
                                    return;
                                }
                            default:
                                gui_edit_handle_navigation_event(&context, event.args_navigation);
                                break;
                        }
                        break;
                    }
                    case INPUT_EVENT_TYPE_KEYBOARD: {
                        gui_edit_handle_keyboard_event(&context, event.args_keyboard);
                        break;
                    }
                    default:
                        break;
                }

                if (context.dirty) {
                    gui_edit_redraw(buffer, &context);
                    display_blit_buffer(buffer);
                }
            } else {
                render(buffer, theme, true, true, title);
                gui_edit_render(buffer, &context);
                display_blit_buffer(buffer);
            }
        }
    }
}