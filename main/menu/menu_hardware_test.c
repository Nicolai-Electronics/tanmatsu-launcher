#include "menu_hardware_test.h"
#include <stdbool.h>
#include "bsp/input.h"
#include "bsp/orientation.h"
#include "bsp/power.h"
#include "common/display.h"
#include "common/theme.h"
#include "freertos/idf_additions.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "test_keyboard.h"
#include "test_keyboard_stuck_keys.h"

typedef enum {
    ACTION_NONE,
    ACTION_KEYBOARD_STUCK_KEYS,
    ACTION_KEYBOARD,
    ACTION_TOGGLE_USB_HOST_POWER,
    ACTION_TOGGLE_KEYBOARD_BACKLIGHT,
    ACTION_TOGGLE_RADIO_MODE,
    ACTION_TOGGLE_GYROSCOPE,
    ACTION_TOGGLE_ACCELEROMETER,
} menu_home_action_t;

static void execute_action(menu_t* menu, menu_home_action_t action) {
    switch (action) {
        case ACTION_KEYBOARD_STUCK_KEYS: {
            char result_buffer[128];
            bool test_result = test_keyboard_stuck_keys(result_buffer, sizeof(result_buffer));
            if (test_result) {
                menu_set_value(menu, 0, "PASS");
            } else {
                menu_set_value(menu, 0, "FAIL");
                message_dialog(get_icon(ICON_ERROR), "Keyboard: stuck keys test", result_buffer, "OK");
            }
            break;
        }
        case ACTION_KEYBOARD: {
            test_keyboard();
            break;
        }
        case ACTION_TOGGLE_USB_HOST_POWER: {
            bool usb_enabled = false;
            bsp_power_get_usb_host_boost_enabled(&usb_enabled);
            usb_enabled = !usb_enabled;
            bsp_power_set_usb_host_boost_enabled(usb_enabled);
            printf("USB host port power: %s\r\n", usb_enabled ? "On" : "Off");
            break;
        }
        case ACTION_TOGGLE_KEYBOARD_BACKLIGHT: {
            uint8_t kb_backlight = 0;
            bsp_input_get_backlight_brightness(&kb_backlight);
            if (kb_backlight == 0) {
                kb_backlight = 100;
            } else {
                kb_backlight = 0;
            }
            bsp_input_set_backlight_brightness(kb_backlight);
            printf("Keyboard backlight: %u%%\r\n", kb_backlight);
            break;
        }
        case ACTION_TOGGLE_RADIO_MODE: {
            bsp_radio_state_t radio_state = BSP_POWER_RADIO_STATE_OFF;
            bsp_power_get_radio_state(&radio_state);
            switch (radio_state) {
                case BSP_POWER_RADIO_STATE_OFF:
                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_APPLICATION);
                    printf("Radio mode: Application\r\n");
                    break;
                case BSP_POWER_RADIO_STATE_APPLICATION:
                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_BOOTLOADER);
                    printf("Radio mode: Bootloader\r\n");
                    break;
                case BSP_POWER_RADIO_STATE_BOOTLOADER:
                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
                    printf("Radio mode: Off\r\n");
                    break;
                default:
                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
                    printf("Radio mode: Off\r\n");
                    break;
            }
            break;
        }
        case ACTION_TOGGLE_GYROSCOPE: {
            static bool gyro_enabled = false;
            gyro_enabled             = !gyro_enabled;
            if (gyro_enabled) {
                if (bsp_orientation_enable_gyroscope() == ESP_OK) {
                    printf("Gyroscope: Enabled\r\n");
                } else {
                    gyro_enabled = false;
                    message_dialog(get_icon(ICON_ERROR), "Error", "Failed to enable gyroscope", "OK");
                }
            } else {
                if (bsp_orientation_disable_gyroscope() == ESP_OK) {
                    printf("Gyroscope: Disabled\r\n");
                } else {
                    gyro_enabled = true;
                    message_dialog(get_icon(ICON_ERROR), "Error", "Failed to disable gyroscope", "OK");
                }
            }
            break;
        }
        case ACTION_TOGGLE_ACCELEROMETER: {
            static bool accel_enabled = false;
            accel_enabled             = !accel_enabled;
            if (accel_enabled) {
                if (bsp_orientation_enable_accelerometer() == ESP_OK) {
                    printf("Accelerometer: Enabled\r\n");
                } else {
                    accel_enabled = false;
                    message_dialog(get_icon(ICON_ERROR), "Error", "Failed to enable accelerometer", "OK");
                }
            } else {
                if (bsp_orientation_disable_accelerometer() == ESP_OK) {
                    printf("Accelerometer: Disabled\r\n");
                } else {
                    accel_enabled = true;
                    message_dialog(get_icon(ICON_ERROR), "Error", "Failed to disable accelerometer", "OK");
                }
            }
            break;
        }
        default:
            break;
    }
}

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
            ((gui_element_icontext_t[]){{get_icon(ICON_BUG_REPORT), "Hardware test"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
            ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Select"}}), 1);
    }

    uint8_t position_index = 2;

    bool usb_enabled = false;
    bsp_power_get_usb_host_boost_enabled(&usb_enabled);
    menu_set_value(menu, position_index++, usb_enabled ? "On" : "Off");

    uint8_t kb_backlight = 0;
    bsp_input_get_backlight_brightness(&kb_backlight);
    char kb_backlight_str[6];
    snprintf(kb_backlight_str, sizeof(kb_backlight_str), "%u%%", kb_backlight);
    menu_set_value(menu, position_index++, kb_backlight_str);

    bsp_radio_state_t radio_state = BSP_POWER_RADIO_STATE_OFF;
    bsp_power_get_radio_state(&radio_state);
    const char* radio_state_str = NULL;
    switch (radio_state) {
        case BSP_POWER_RADIO_STATE_OFF:
            radio_state_str = "Off";
            break;
        case BSP_POWER_RADIO_STATE_APPLICATION:
            radio_state_str = "Application";
            break;
        case BSP_POWER_RADIO_STATE_BOOTLOADER:
            radio_state_str = "Bootloader";
            break;
        default:
            radio_state_str = "Unknown";
            break;
    }
    // menu_set_value(menu, position_index++, radio_state_str);

    bool  gyro_enabled  = false;
    bool  accel_enabled = false;
    float gyro_x, gyro_y, gyro_z;
    float accel_x, accel_y, accel_z;
    bsp_orientation_get(&gyro_enabled, &accel_enabled, &gyro_x, &gyro_y, &gyro_z, &accel_x, &accel_y, &accel_z);
    char gyro_string[32] = {0};
    snprintf(gyro_string, sizeof(gyro_string), "%4.2f, %4.2f, %4.2f dps", gyro_x, gyro_y, gyro_z);
    char accel_string[32] = {0};
    snprintf(accel_string, sizeof(accel_string), "%4.2f, %4.2f, %4.2f m/s²", accel_x, accel_y, accel_z);
    menu_set_value(menu, position_index++, gyro_enabled ? gyro_string : "Disabled");
    menu_set_value(menu, position_index++, accel_enabled ? accel_string : "Disabled");

    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_hardware_test(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_value(&menu, "Keyboard: stuck keys", "", NULL, (void*)ACTION_KEYBOARD_STUCK_KEYS, -1);
    menu_set_value(&menu, 0, "Click to run");
    menu_insert_item_value(&menu, "Keyboard", "", NULL, (void*)ACTION_KEYBOARD, -1);
    menu_set_value(&menu, 1, "Click to run");
    menu_insert_item_value(&menu, "USB host port power", "", NULL, (void*)ACTION_TOGGLE_USB_HOST_POWER, -1);
    menu_insert_item_value(&menu, "Keyboard backlight", "", NULL, (void*)ACTION_TOGGLE_KEYBOARD_BACKLIGHT, -1);
    // menu_insert_item_value(&menu, "Radio mode", "", NULL, (void*)ACTION_TOGGLE_RADIO_MODE, -1);
    menu_insert_item_value(&menu, "Gyroscope", "", NULL, (void*)ACTION_TOGGLE_GYROSCOPE, -1);
    menu_insert_item_value(&menu, "Accelerometer", "", NULL, (void*)ACTION_TOGGLE_ACCELEROMETER, -1);

    render(&menu, false, true);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
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
                                execute_action(&menu, (menu_home_action_t)arg);
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
