#include "settings_lora.h"
#include <stdbool.h>
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "device_settings.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "lora.h"
#include "lora_settings_handler.h"
#include "menu/message_dialog.h"
#include "menu/textedit.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"

static const char TAG[] = "LoRa settings menu";

typedef enum {
    SETTING_NONE,
    SETTING_FREQUENCY,
    SETTING_SPREADING_FACTOR,
    SETTING_BANDWIDTH,
    SETTING_CODING_RATE,
    SETTING_POWER,
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

    menu_setting_t setting = (menu_setting_t)menu_get_callback_args(menu, menu_get_position(menu));

    if (!partial || icons) {
        render_base_screen_statusbar(
            buffer, theme, !partial, !partial || icons, !partial,
            ((gui_element_icontext_t[]){{get_icon(ICON_CHAT), "LoRa radio"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
            (setting == SETTING_FREQUENCY) ? ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Edit"}})
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

void edit_frequency(menu_t* menu) {
    pax_buf_t*   buffer    = display_get_buffer();
    gui_theme_t* theme     = get_theme();
    char         temp[16]  = {0};
    bool         accepted  = false;
    uint32_t     frequency = 0;
    device_settings_get_lora_frequency(&frequency);
    snprintf(temp, sizeof(temp), "%" PRIu32, frequency);
    menu_textedit(buffer, theme, "Frequency (Hz)", temp, sizeof(temp) + sizeof('\0'), true, &accepted);
    if (accepted) {
        uint32_t new_frequency = strtoul(temp, NULL, 10);
        if (new_frequency < 1) {
            return;
        }
        device_settings_set_lora_frequency(new_frequency);
        snprintf(temp, sizeof(temp), "%" PRIu32, new_frequency);
        menu_set_value(menu, 0, temp);
    }
}

static void edit(menu_t* menu) {
    menu_setting_t setting = (menu_setting_t)menu_get_callback_args(menu, menu_get_position(menu));
    switch (setting) {
        case SETTING_FREQUENCY:
            edit_frequency(menu);
            break;
        default:
            break;
    }
}

static void edit_updown(menu_t* menu, bool up) {
    menu_setting_t setting = (menu_setting_t)menu_get_callback_args(menu, menu_get_position(menu));
    switch (setting) {
        case SETTING_FREQUENCY:
            break;
        case SETTING_SPREADING_FACTOR: {
            uint8_t spreading_factor = 0;
            device_settings_get_lora_spreading_factor(&spreading_factor);
            spreading_factor += up ? 1 : -1;
            if (spreading_factor < 7) spreading_factor = 7;
            if (spreading_factor > 12) spreading_factor = 12;
            device_settings_set_lora_spreading_factor(spreading_factor);
            break;
        }
        case SETTING_BANDWIDTH: {
            uint16_t bandwidth = 0;
            device_settings_get_lora_bandwidth(&bandwidth);
            switch (bandwidth) {
                case 7:
                    bandwidth = up ? 10 : 7;
                    break;
                case 10:
                    bandwidth = up ? 15 : 7;
                    break;
                case 15:
                    bandwidth = up ? 20 : 10;
                    break;
                case 20:
                    bandwidth = up ? 31 : 15;
                    break;
                case 31:
                    bandwidth = up ? 41 : 20;
                    break;
                case 41:
                    bandwidth = up ? 62 : 31;
                    break;
                case 62:
                    bandwidth = up ? 125 : 41;
                    break;
                case 125:
                    bandwidth = up ? 250 : 62;
                    break;
                case 250:
                    bandwidth = up ? 500 : 125;
                    break;
                case 500:
                    bandwidth = up ? 500 : 250;
                    break;
                default:
                    bandwidth = 125;
            }
            device_settings_set_lora_bandwidth(bandwidth);
            break;
        }
        case SETTING_CODING_RATE: {
            uint8_t coding_rate = 0;
            device_settings_get_lora_coding_rate(&coding_rate);
            coding_rate += up ? 1 : -1;
            if (coding_rate < 5) coding_rate = 5;
            if (coding_rate > 8) coding_rate = 8;
            device_settings_set_lora_coding_rate(coding_rate);
            break;
        }
        case SETTING_POWER: {
            uint8_t power = 0;
            device_settings_get_lora_power(&power);
            if (power > 0 && !up) power -= 1;
            if (up) {
                power++;
                if (power > 22) power = 22;
            }
            device_settings_set_lora_power(power);
            break;
        }
        default:
            break;
    }
}

static void apply(void) {
    esp_err_t res = lora_set_mode(LORA_PROTOCOL_MODE_STANDBY_RC);
    if (res == ESP_OK) {
        ESP_LOGI(TAG, "LoRa set to standby mode");
    } else {
        ESP_LOGE(TAG, "Failed to set LoRa mode: %s", esp_err_to_name(res));
    }

    res = lora_apply_settings();
    if (res == ESP_OK) {
        ESP_LOGI(TAG, "LoRa configuration set");
    } else {
        ESP_LOGE(TAG, "Failed to set LoRa configuration: %s", esp_err_to_name(res));
    }

    res = lora_set_mode(LORA_PROTOCOL_MODE_RX);
    if (res == ESP_OK) {
        ESP_LOGI(TAG, "LoRa set to RX mode");
    } else {
        ESP_LOGE(TAG, "Failed to set LoRa mode: %s", esp_err_to_name(res));
    }
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
                                apply();
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
                                edit(&menu);
                                render(&menu, false, true);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_LEFT: {
                                edit_updown(&menu, false);
                                render(&menu, false, true);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT: {
                                edit_updown(&menu, true);
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
