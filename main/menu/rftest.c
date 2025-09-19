#include "rftest.h"
#include "bsp/input.h"
#include "common/display.h"
#include "device_information.h"
#include "firmware_update.h"
#include "freertos/idf_additions.h"
#include "gui_element_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/about.h"
#include "menu/message_dialog.h"
#include "menu/terminal.h"
#include "menu/wifi.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "radio_update.h"
#include "settings_clock.h"

typedef enum {
    ACTION_NONE,
    ACTION_INSTALL_NORMAL_FIRMWARE,
    ACTION_INSTALL_RFTEST_REMOTE_FIRMWARE,
    ACTION_INSTALL_RFTEST_LOCAL_FIRMWARE,
    ACTION_TERMINAL,
} menu_home_action_t;

static void execute_action(pax_buf_t* fb, menu_home_action_t action, gui_theme_t* theme) {
    switch (action) {
        case ACTION_INSTALL_NORMAL_FIRMWARE:
            radio_update("/int/tanmatsu-radio.bin", false, 0);
            esp_restart();
            break;
        case ACTION_INSTALL_RFTEST_REMOTE_FIRMWARE:
            radio_update("/int/rftest_usb.bin", false, 0);
            esp_restart();
            break;
        case ACTION_INSTALL_RFTEST_LOCAL_FIRMWARE:
            radio_update("/int/rftest_local.bin", false, 0);
            esp_restart();
            break;
        case ACTION_TERMINAL:
            menu_terminal(fb, theme);
            break;
        default:
            break;
    }
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, bool partial, bool icons) {
    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    if (!partial || icons) {
        render_base_screen_statusbar(
            buffer, theme, !partial, !partial || icons, !partial,
            ((gui_element_icontext_t[]){{get_icon(ICON_SETTINGS), "Radio test"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
            ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Select"}}), 1);
    }
    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_rftest(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_icon(&menu, "Install normal radio firmware", NULL, (void*)ACTION_INSTALL_NORMAL_FIRMWARE, -1,
                          get_icon(ICON_SYSTEM_UPDATE));
    menu_insert_item_icon(&menu, "Install RF test firmware (USB)", NULL, (void*)ACTION_INSTALL_RFTEST_REMOTE_FIRMWARE,
                          -1, get_icon(ICON_SYSTEM_UPDATE));
    menu_insert_item_icon(&menu, "Install RF test firmware (local)", NULL, (void*)ACTION_INSTALL_RFTEST_LOCAL_FIRMWARE,
                          -1, get_icon(ICON_SYSTEM_UPDATE));
    menu_insert_item_icon(&menu, "Terminal for RF test local firmware", NULL, (void*)ACTION_TERMINAL, -1,
                          get_icon(ICON_DEVICE_INFO));

    render(buffer, theme, &menu, false, true);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                                menu_free(&menu);
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                execute_action(buffer, (menu_home_action_t)arg, theme);
                                render(buffer, theme, &menu, false, true);
                                break;
                            }
                            default:
                                break;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        } else {
            render(buffer, theme, &menu, true, true);
        }
    }
}
