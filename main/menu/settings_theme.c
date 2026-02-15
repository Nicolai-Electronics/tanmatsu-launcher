#include <stdbool.h>
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "device_information.h"
#include "device_settings.h"
#include "freertos/idf_additions.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "menu/textedit.h"
#include "owner.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"

static void render(menu_t* menu, bool partial, bool icons) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

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
            ((gui_element_icontext_t[]){{get_icon(ICON_COLORS), "Theme"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
            ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Select"}}), 1);
    }

    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void update_menu(menu_t* menu) {
    theme_setting_t theme = THEME_BLACK;
    device_settings_get_theme(&theme);
    if ((int)theme > menu_get_length(menu)) {
        theme = THEME_BLACK;
        device_settings_set_theme(theme);
    }
    for (size_t i = 0; i < menu_get_length(menu); i++) {
        if ((theme_setting_t)menu_get_callback_args(menu, i) == theme) {
            menu_set_value(menu, i, "X");
        } else {
            menu_set_value(menu, i, "");
        }
    }
}

void menu_settings_theme(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_value(&menu, "Light", "", NULL, (void*)THEME_BLACK, -1);
    menu_insert_item_value(&menu, "Dark", "", NULL, (void*)THEME_WHITE, -1);
    menu_insert_item_value(&menu, "Red", "", NULL, (void*)THEME_RED, -1);
    menu_insert_item_value(&menu, "Blue", "", NULL, (void*)THEME_BLUE, -1);
    menu_insert_item_value(&menu, "Purple", "", NULL, (void*)THEME_PURPLE, -1);
    menu_insert_item_value(&menu, "Orange", "", NULL, (void*)THEME_ORANGE, -1);
    menu_insert_item_value(&menu, "Green", "", NULL, (void*)THEME_GREEN, -1);
    menu_insert_item_value(&menu, "Yellow", "", NULL, (void*)THEME_YELLOW, -1);

    device_identity_t device_identity = {0};
    read_device_identity(&device_identity);
    if (device_identity.color == DEVICE_VARIANT_COLOR_COMMODORE) {
        menu_insert_item_value(&menu, "Commodore blue", "", NULL, (void*)THEME_C_BLUE, -1);
        menu_insert_item_value(&menu, "Commodore red", "", NULL, (void*)THEME_C_RED, -1);
    }

    update_menu(&menu);
    render(&menu, false, true);
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
                                render(&menu, false, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(&menu, false, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                theme_setting_t theme_setting =
                                    (theme_setting_t)menu_get_callback_args(&menu, menu_get_position(&menu));
                                device_settings_set_theme(theme_setting);
                                update_menu(&menu);
                                theme_initialize();
                                unload_icons();
                                load_icons();
                                render(&menu, false, true);
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
            render(&menu, true, true);
        }
    }
}
