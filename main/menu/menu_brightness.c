#include "menu_brightness.h"
#include <stdbool.h>
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/power.h"
#include "common/display.h"
#include "common/theme.h"
#include "device_settings.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/menu_helpers.h"
#include "menu/message_dialog.h"
#include "nvs_settings.h"
#include "nvs_settings_helpers.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"

typedef enum {
    SETTING_NONE,
    SETTING_DISPLAY_BACKLIGHT_BRIGHTNESS,
    SETTING_KEYBOARD_BACKLIGHT_BRIGHTNESS,
    SETTING_LED_BRIGHTNESS,
    SETTING_LCD_VCOM,
} menu_setting_t;

#define NVS_KEY_LCD_VCOM "lcd_vcom"

static void render(menu_t* menu, bool partial, bool icons) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    pax_vec2_t position = menu_calc_position(buffer, theme);

    // Test pattern in the bottom left corner, above the footer. The menu area is reduced so it doesn't cover it
    const int pattern_size    = 128;
    const int pattern_spacing = 32;
    int       pattern_x       = position.x0;
    int       pattern_y       = position.y1 - pattern_size;
    position.y1               = pattern_y;

    if (!partial || icons) {
        render_base_screen_statusbar(
            buffer, theme, !partial, !partial || icons, !partial,
            ((gui_element_icontext_t[]){{get_icon(ICON_BRIGHTNESS), "Brightness"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
            ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ← / → Change brightness | ⏎ Select"}}), 1);
    }

    uint8_t display_brightness = 100;
    nvs_settings_get_display_brightness(&display_brightness, DEFAULT_DISPLAY_BRIGHTNESS);

    uint8_t keyboard_brightness = 0;
    nvs_settings_get_keyboard_brightness(&keyboard_brightness, DEFAULT_KEYBOARD_BRIGHTNESS);

    uint8_t led_brightness = 100;
    nvs_settings_get_led_brightness(&led_brightness, DEFAULT_LED_BRIGHTNESS);

    size_t position_index = 0;
    char   value_buffer[16];
    snprintf(value_buffer, sizeof(value_buffer), "%u%%", display_brightness);
    menu_set_value(menu, position_index++, value_buffer);
    snprintf(value_buffer, sizeof(value_buffer), "%u%%", keyboard_brightness);
    menu_set_value(menu, position_index++, value_buffer);
    snprintf(value_buffer, sizeof(value_buffer), "%u%%", led_brightness);
    menu_set_value(menu, position_index++, value_buffer);
    uint8_t lcd_vcom = 0;
    if (position_index < menu_get_length(menu) && bsp_display_get_vcom(&lcd_vcom) == ESP_OK) {
        snprintf(value_buffer, sizeof(value_buffer), "%u", lcd_vcom);
        menu_set_value(menu, position_index++, value_buffer);
    }

    menu_render(buffer, menu, position, theme, partial);

    if (!partial) {
        pax_simple_rect(buffer, 0xFF552075, pattern_x, pattern_y, pattern_size, pattern_size);
        int lines_x = pattern_x + pattern_size + pattern_spacing;
        for (int line = 0; line < pattern_size; line++) {
            pax_simple_rect(buffer, (line & 1) ? 0xFFFFFFFF : 0xFF000000, lines_x, pattern_y + line, pattern_size, 1);
        }
    }

    display_blit_buffer(buffer);
}

static void adjust_lcd_vcom(int8_t direction) {
    uint8_t value = 0;
    if (bsp_display_get_vcom(&value) != ESP_OK) {
        return;
    }

    if (direction > 0 && value < 255) {
        value++;
    } else if (direction < 0 && value > 0) {
        value--;
    } else {
        return;
    }

    if (bsp_display_set_vcom(value) == ESP_OK) {
        nvs_settings_set_u8(NVS_KEY_LCD_VCOM, value);
    }
}

void adjust_setting(menu_setting_t setting, int8_t direction) {
    uint8_t value = 0;
    switch (setting) {
        case SETTING_DISPLAY_BACKLIGHT_BRIGHTNESS:
            nvs_settings_get_display_brightness(&value, DEFAULT_DISPLAY_BRIGHTNESS);
            break;
        case SETTING_KEYBOARD_BACKLIGHT_BRIGHTNESS:
            nvs_settings_get_keyboard_brightness(&value, DEFAULT_KEYBOARD_BRIGHTNESS);
            break;
        case SETTING_LED_BRIGHTNESS:
            nvs_settings_get_led_brightness(&value, DEFAULT_LED_BRIGHTNESS);
            break;
        case SETTING_LCD_VCOM:
            adjust_lcd_vcom(direction);
            return;
        default:
            return;
    }

    if (direction > 0 && value < 100) {
        value += (value >= 5) ? 5 : 1;
        if (value > 100) {
            value = 100;
        }
    } else if (direction < 0 && value > 0) {
        value -= (value > 5) ? 5 : 1;
    }

    switch (setting) {
        case SETTING_DISPLAY_BACKLIGHT_BRIGHTNESS:
            nvs_settings_set_display_brightness(value);
            break;
        case SETTING_KEYBOARD_BACKLIGHT_BRIGHTNESS:
            nvs_settings_set_keyboard_brightness(value);
            break;
        case SETTING_LED_BRIGHTNESS:
            nvs_settings_set_led_brightness(value);
            break;
        default:
            break;
    }
    device_settings_apply();
}

void menu_settings_brightness(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_value(&menu, "Display backlight brightness", "", NULL, (void*)SETTING_DISPLAY_BACKLIGHT_BRIGHTNESS,
                           -1);
    menu_insert_item_value(&menu, "Keyboard backlight brightness", "", NULL,
                           (void*)SETTING_KEYBOARD_BACKLIGHT_BRIGHTNESS, -1);
    menu_insert_item_value(&menu, "LED brightness", "", NULL, (void*)SETTING_LED_BRIGHTNESS, -1);
    uint8_t lcd_vcom = 0;
    if (bsp_display_get_vcom(&lcd_vcom) == ESP_OK) {
        menu_insert_item_value(&menu, "Display VCOM", "", NULL, (void*)SETTING_LCD_VCOM, -1);
    }

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
                                render(&menu, false, true);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_LEFT: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                adjust_setting((menu_setting_t)arg, -1);
                                render(&menu, false, true);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                adjust_setting((menu_setting_t)arg, 1);
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
