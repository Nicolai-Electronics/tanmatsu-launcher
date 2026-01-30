#include "owner.h"
#include <stdbool.h>
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "device_settings.h"
#include "freertos/idf_additions.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"

typedef enum {
    SETTING_NONE,
    SETTING_NICKNAME,
    SETTING_BIRTHDAY_DAY,
    SETTING_BIRTHDAY_MONTH,
} menu_setting_t;

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
            ((gui_element_icontext_t[]){{get_icon(ICON_TAG), "Owner"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
            ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ← / → Change brightness | ⏎ Select"}}), 1);
    }

    char nickname[64] = {0};
    device_settings_get_owner_nickname(nickname, sizeof(nickname));
    uint8_t birthday_day = 1;
    device_settings_get_owner_birthday_day(&birthday_day);
    uint8_t birthday_month = 1;
    device_settings_get_owner_birthday_month(&birthday_month);

    size_t position_index = 0;
    char   value_buffer[16];
    menu_set_value(menu, position_index++, nickname);
    snprintf(value_buffer, sizeof(value_buffer), "%u%%", birthday_day);
    menu_set_value(menu, position_index++, value_buffer);
    snprintf(value_buffer, sizeof(value_buffer), "%u%%", birthday_month);
    menu_set_value(menu, position_index++, value_buffer);

    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_settings_owner(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_value(&menu, "Nickname", "", NULL, (void*)SETTING_NICKNAME, -1);
    // menu_insert_item_value(&menu, "Birthday day", "", NULL, (void*)SETTING_BIRTHDAY_DAY, -1);
    // menu_insert_item_value(&menu, "Birthday month", "", NULL, (void*)SETTING_BIRTHDAY_MONTH, -1);

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
                                render(&menu, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(&menu, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                render(&menu, false, true);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_LEFT: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                render(&menu, false, true);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
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
