#include "wifi.h"
#include "bsp/input.h"
#include "common/display.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "wifi_connection.h"
#include "wifi_edit.h"
#include "wifi_scan.h"
#include "wifi_settings.h"
// #include "shapes/pax_misc.h"

extern bool wifi_stack_get_initialized(void);

static bool populate_menu_from_wifi_entries(menu_t* menu) {
    bool empty = true;
    for (uint32_t index = 0; index < WIFI_SETTINGS_MAX; index++) {
        wifi_settings_t settings;
        esp_err_t       res = wifi_settings_get(index, &settings);
        if (res == ESP_OK) {
            menu_insert_item(menu, (char*)settings.ssid, NULL, (void*)index, -1);
            empty = false;
        }
    }
    return !empty;
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons,
                   bool loading) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_WIFI), "WiFi networks"}}), 1,
                                     ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"},
                                                             {get_icon(ICON_F1), "Back"},
                                                             {get_icon(ICON_F2), "Scan"},
                                                             {get_icon(ICON_F3), "Add manually"},
                                                             {get_icon(ICON_F5), "Remove"}}),
                                     5, ((gui_header_field_t[]){{NULL, "↑ / ↓ Navigate ⏎ Edit"}}), 1);
    }
    menu_render(buffer, menu, position, theme, partial);
    if (menu_find_item(menu, 0) == NULL) {
        if (loading) {
            pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                          position.y0 + 18 * 0, "Loading list of stored WiFi networks...");
        } else {
            pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                          position.y0 + 18 * 0, "No WiFi networks stored");
        }
    }
    display_blit_buffer(buffer);
}

static void add_manually(pax_buf_t* buffer, gui_theme_t* theme) {
    int index = wifi_settings_find_empty_slot();
    if (index == -1) {
        message_dialog(buffer, theme, "Error", "No empty slot, can not add another network",
                       MESSAGE_DIALOG_FOOTER_GOBACK);
    }
    menu_wifi_edit(buffer, theme, index, true, "", 0);
}

static bool _menu_wifi(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    menu_t menu = {0};
    menu_initialize(&menu);
    render(buffer, theme, &menu, position, false, true, true);
    populate_menu_from_wifi_entries(&menu);
    render(buffer, theme, &menu, position, false, true, false);
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
                                return false;
                            case BSP_INPUT_NAVIGATION_KEY_F2:
                                menu_wifi_scan(buffer, theme);
                                return true;
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F3:
                                add_manually(buffer, theme);
                                return true;
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F5: {
                                uint8_t index = (uint32_t)menu_get_callback_args(&menu, menu_get_position(&menu));
                                wifi_settings_erase(index);
                                return true;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, position, true, false, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, position, true, false, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                if (menu_find_item(&menu, 0) != NULL) {
                                    uint8_t index = (uint32_t)menu_get_callback_args(&menu, menu_get_position(&menu));
                                    menu_wifi_edit(buffer, theme, index, false, NULL, 0);
                                    render(buffer, theme, &menu, position, false, false, false);
                                }
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
            render(buffer, theme, &menu, position, true, true, false);
        }
    }
}

void menu_wifi(pax_buf_t* buffer, gui_theme_t* theme) {
    while (_menu_wifi(buffer, theme));
}
