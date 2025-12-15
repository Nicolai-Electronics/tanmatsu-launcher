#include <stdbool.h>
#include "bsp/input.h"
#include "bsp/orientation.h"
#include "bsp/power.h"
#include "common/display.h"
#include "common/theme.h"
#include "device_settings.h"
#include "freertos/idf_additions.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "menu_hardware_test.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "test_keyboard.h"
#include "test_keyboard_stuck_keys.h"

typedef enum {
    SETTING_NONE,
    SETTING_DISPLAY_BACKLIGHT_BRIGHTNESS,
    SETTING_KEYBOARD_BACKLIGHT_BRIGHTNESS,
    SETTING_LED_BRIGHTNESS,
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
            ((gui_element_icontext_t[]){{get_icon(ICON_BRIGHTNESS), "Brightness"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
            ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ← / → Change brightness | ⏎ Select"}}), 1);
    }

    uint8_t display_brightness = 100;
    device_settings_get_display_brightness(&display_brightness);

    uint8_t keyboard_brightness = 0;
    device_settings_get_keyboard_brightness(&keyboard_brightness);

    uint8_t led_brightness = 100;
    device_settings_get_led_brightness(&led_brightness);

    size_t position_index = 0;
    char   value_buffer[16];
    snprintf(value_buffer, sizeof(value_buffer), "%u%%", display_brightness);
    menu_set_value(menu, position_index++, value_buffer);
    snprintf(value_buffer, sizeof(value_buffer), "%u%%", keyboard_brightness);
    menu_set_value(menu, position_index++, value_buffer);
    snprintf(value_buffer, sizeof(value_buffer), "%u%%", led_brightness);
    menu_set_value(menu, position_index++, value_buffer);

    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void increase_setting(menu_setting_t setting) {
    uint8_t value = 0;
    switch (setting) {
        case SETTING_DISPLAY_BACKLIGHT_BRIGHTNESS:
            device_settings_get_display_brightness(&value);
            if (value < 100) {
                if (value >= 5) {
                    value += 5;
                } else {
                    value += 1;
                }
                if (value > 100) {
                    value = 100;
                }
                device_settings_set_display_brightness(value);
            }
            break;
        case SETTING_KEYBOARD_BACKLIGHT_BRIGHTNESS:
            device_settings_get_keyboard_brightness(&value);
            if (value < 100) {
                if (value >= 5) {
                    value += 5;
                } else {
                    value += 1;
                }
                if (value > 100) {
                    value = 100;
                }
                device_settings_set_keyboard_brightness(value);
            }
            break;
        case SETTING_LED_BRIGHTNESS:
            device_settings_get_led_brightness(&value);
            if (value < 100) {
                if (value >= 5) {
                    value += 5;
                } else {
                    value += 1;
                }
                if (value > 100) {
                    value = 100;
                }
                device_settings_set_led_brightness(value);
            }
            break;
        default:
            break;
    }
    device_settings_apply();
}

void decrease_setting(menu_setting_t setting) {
    uint8_t value = 0;
    switch (setting) {
        case SETTING_DISPLAY_BACKLIGHT_BRIGHTNESS:
            device_settings_get_display_brightness(&value);
            if (value > 0) {
                if (value > 5) {
                    value -= 5;
                } else {
                    value -= 1;
                }
                device_settings_set_display_brightness(value);
            }
            break;
        case SETTING_KEYBOARD_BACKLIGHT_BRIGHTNESS:
            device_settings_get_keyboard_brightness(&value);
            if (value > 0) {
                if (value > 5) {
                    value -= 5;
                } else {
                    value -= 1;
                }
                device_settings_set_keyboard_brightness(value);
            }
            break;
        case SETTING_LED_BRIGHTNESS:
            device_settings_get_led_brightness(&value);
            if (value > 0) {
                if (value > 5) {
                    value -= 5;
                } else {
                    value -= 1;
                }
                device_settings_set_led_brightness(value);
            }
            break;
        default:
            break;
    }
    device_settings_apply();
}

void menu_brightness(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_value(&menu, "Display backlight brightness", "", NULL, (void*)SETTING_DISPLAY_BACKLIGHT_BRIGHTNESS,
                           -1);
    menu_insert_item_value(&menu, "Keyboard backlight brightness", "", NULL,
                           (void*)SETTING_KEYBOARD_BACKLIGHT_BRIGHTNESS, -1);
    menu_insert_item_value(&menu, "LED brightness", "", NULL, (void*)SETTING_LED_BRIGHTNESS, -1);

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
                                decrease_setting((menu_setting_t)arg);
                                render(&menu, false, true);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                increase_setting((menu_setting_t)arg);
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
