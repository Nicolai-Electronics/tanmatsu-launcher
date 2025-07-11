#include "settings.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "chakrapetchmedium.h"
#include "common/display.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/about.h"
#include "menu/message_dialog.h"
#include "menu/wifi.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
// #include "shapes/pax_misc.h"
#include "device_information.h"
#include "firmware_update.h"
#include "radio_update.h"
#include "settings_clock.h"

typedef enum {
    ACTION_NONE,
    ACTION_WIFI,
    ACTION_CLOCK,
    ACTION_FIRMWARE_UPDATE,
    ACTION_DEVICE_INFO,
    ACTION_ABOUT,
    ACTION_LAST,
    ACTION_RADIO_UPDATE,
} menu_home_action_t;

static void execute_action(pax_buf_t* fb, menu_home_action_t action, gui_theme_t* theme) {
    switch (action) {
        case ACTION_WIFI:
            menu_wifi(fb, theme);
            break;
        case ACTION_CLOCK:
            menu_settings_clock(fb, theme);
            break;
        case ACTION_FIRMWARE_UPDATE:
            menu_firmware_update(fb, theme);
            break;
        case ACTION_DEVICE_INFO:
            menu_device_information(fb, theme);
            break;
        case ACTION_ABOUT:
            menu_about(fb, theme);
            break;
        case ACTION_RADIO_UPDATE:
            radio_update(fb, theme, "/sd/firmware/radio/esp-hosted.zz", true, 1093760);
            break;
        default:
            break;
    }
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_SETTINGS), "Settings"}}), 1,
                                     ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}),
                                     2, ((gui_header_field_t[]){{NULL, "↑ / ↓ Navigate ⏎ Select"}}), 1);
    }
    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_settings(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_icon(&menu, "WiFi configuration", NULL, (void*)ACTION_WIFI, -1, get_icon(ICON_WIFI));
    menu_insert_item_icon(&menu, "Clock configuration", NULL, (void*)ACTION_CLOCK, -1, get_icon(ICON_CLOCK));
    menu_insert_item_icon(&menu, "Firmware update", NULL, (void*)ACTION_FIRMWARE_UPDATE, -1,
                          get_icon(ICON_SYSTEM_UPDATE));
    menu_insert_item_icon(&menu, "Device information", NULL, (void*)ACTION_DEVICE_INFO, -1, get_icon(ICON_DEVICE_INFO));
    menu_insert_item_icon(&menu, "About", NULL, (void*)ACTION_ABOUT, -1, get_icon(ICON_INFO));
    /*#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
        defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
        menu_insert_item_icon(&menu, "Update radio from SD card", NULL, (void*)ACTION_RADIO_UPDATE, -1,
                              get_icon(ICON_RELEASE_ALERT));
    #endif*/

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    render(buffer, theme, &menu, position, false, true);
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
                                render(buffer, theme, &menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                execute_action(buffer, (menu_home_action_t)arg, theme);
                                render(buffer, theme, &menu, position, false, true);
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
            render(buffer, theme, &menu, position, true, true);
        }
    }
}
