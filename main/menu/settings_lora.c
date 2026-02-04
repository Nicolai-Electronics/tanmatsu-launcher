#include "settings_lora.h"
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
#include "menu/textedit.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"

typedef enum {
    SETTING_NONE,
    SETTING_FREQUENCY,
    SETTING_SPREADING_FACTOR,
    SETTING_BANDWIDTH,
    SETTING_CODING_RATE,
    SETTING_POWER,
} menu_setting_t;

static void edit_nickname(menu_t* menu) {
    pax_buf_t*   buffer    = display_get_buffer();
    gui_theme_t* theme     = get_theme();
    char         temp[129] = {0};
    bool         accepted  = false;
    memset(temp, 0, sizeof(temp));
    device_settings_get_owner_nickname(temp, sizeof(temp));
    menu_textedit(buffer, theme, "Nickname", temp, sizeof(temp) + sizeof('\0'), true, &accepted);
    if (accepted) {
        device_settings_set_owner_nickname(temp);
        menu_set_value(menu, 0, temp);
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

    menu_setting_t setting = (menu_setting_t)menu_get_callback_args(menu, menu_get_position(menu));

    if (!partial || icons) {
        render_base_screen_statusbar(
            buffer, theme, !partial, !partial || icons, !partial,
            ((gui_element_icontext_t[]){{get_icon(ICON_GLOBE), "LoRa radio"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
            (false) ? ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Edit"}})
                    : ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ← / → Edit"}}),
            1);
    }

    uint32_t frequency = 0;
    device_settings_get_lora_frequency(&frequency);
    uint8_t spreading_factor = 0;
    device_settings_get_lora_spreading_factor(&spreading_factor);
    uint16_t bandwidth = 0;
    device_settings_get_lora_bandwidth(&bandwidth);
    float bandwidth_float = bandwidth;
    if (bandwidth == 7) bandwidth_float = 7.8;
    if (bandwidth == 10) bandwidth_float = 10.4;
    if (bandwidth == 15) bandwidth_float = 15.6;
    if (bandwidth == 20) bandwidth_float = 20.8;
    if (bandwidth == 31) bandwidth_float = 31.25;
    if (bandwidth == 41) bandwidth_float = 41.7;
    if (bandwidth == 62) bandwidth_float = 62.5;
    uint8_t coding_rate = 0;
    device_settings_get_lora_coding_rate(&coding_rate);
    uint8_t power = 0;
    device_settings_get_lora_power(&power);

    size_t position_index = 0;
    char   value_buffer[16];
    snprintf(value_buffer, sizeof(value_buffer), "%3.3f MHz", (float)(frequency) / 1000000.0);
    menu_set_value(menu, position_index++, value_buffer);
    snprintf(value_buffer, sizeof(value_buffer), "%u", spreading_factor);
    menu_set_value(menu, position_index++, value_buffer);
    snprintf(value_buffer, sizeof(value_buffer), "%.1f kHz", bandwidth_float);
    menu_set_value(menu, position_index++, value_buffer);
    snprintf(value_buffer, sizeof(value_buffer), "%u", coding_rate);
    menu_set_value(menu, position_index++, value_buffer);
    snprintf(value_buffer, sizeof(value_buffer), "%u dBm", power);
    menu_set_value(menu, position_index++, value_buffer);

    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_settings_lora(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);

    menu_insert_item_value(&menu, "Frequency", "", NULL, (void*)SETTING_FREQUENCY, -1);
    menu_insert_item_value(&menu, "Spreading factor", "", NULL, (void*)SETTING_SPREADING_FACTOR, -1);
    menu_insert_item_value(&menu, "Bandwidth", "", NULL, (void*)SETTING_BANDWIDTH, -1);
    menu_insert_item_value(&menu, "Coding rate", "", NULL, (void*)SETTING_CODING_RATE, -1);
    menu_insert_item_value(&menu, "Transmit power", "", NULL, (void*)SETTING_POWER, -1);

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
                                menu_setting_t setting =
                                    (menu_setting_t)menu_get_callback_args(&menu, menu_get_position(&menu));
                                switch (setting) {
                                    default:
                                        break;
                                }
                                render(&menu, false, true);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_LEFT: {
                                menu_setting_t setting =
                                    (menu_setting_t)menu_get_callback_args(&menu, menu_get_position(&menu));
                                switch (setting) {
                                    default:
                                        break;
                                }
                                render(&menu, false, true);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT: {
                                menu_setting_t setting =
                                    (menu_setting_t)menu_get_callback_args(&menu, menu_get_position(&menu));
                                switch (setting) {
                                    break;
                                    default:
                                        break;
                                }
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
