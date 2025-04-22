#include "about.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/power.h"
#include "common/display.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"

// #include "shapes/pax_misc.h"

static void render(pax_buf_t* buffer, gui_theme_t* theme, bsp_power_battery_information_t* information) {
    char percentage_string[14] = {0};
    snprintf(percentage_string, sizeof(percentage_string), "Battery: %u%%", (uint8_t)information->remaining_percentage);
    pax_background(buffer, 0xFF000000);
    pax_draw_text(buffer, 0xFFFFFFFF, theme->footer.text_font, 16, 0, 18 * 0, "Battery charging mode");
    pax_draw_text(buffer, 0xFFFFFFFF, theme->footer.text_font, 16, 0, 18 * 1, percentage_string);
    pax_draw_text(buffer, 0xFFFFFFFF, theme->footer.text_font, 16, 0, 18 * 2,
                  information->battery_charging ? "Charging..." : "Not charging");
    pax_draw_text(buffer, 0xFFFFFFFF, theme->footer.text_font, 16, 0, 18 * 3,
                  information->battery_available ? "" : "No battery detected");
    display_blit_buffer(buffer);
}

void charging_mode(pax_buf_t* buffer, gui_theme_t* theme) {
    bsp_power_battery_information_t information = {0};
    bsp_power_get_battery_information(&information);
    if (!information.power_supply_available) {
        return;
    }

    bsp_input_set_backlight_brightness(0);
    bsp_display_set_backlight_brightness(50);

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));
    render(buffer, theme, &information);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(500)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                                bsp_display_set_backlight_brightness(100);
                                return;
                            default:
                                break;
                        }
                    }
                    break;
                }
                case INPUT_EVENT_TYPE_ACTION:
                    switch (event.args_action.type) {
                        case BSP_INPUT_ACTION_TYPE_POWER_BUTTON:
                            if (!event.args_action.state) {
                                bsp_display_set_backlight_brightness(100);
                                return;
                            }
                            break;
                        default:
                            break;
                    }
                default:
                    break;
            }
        } else {
            bsp_power_get_battery_information(&information);
            if (!information.power_supply_available) {
                bsp_display_set_backlight_brightness(100);
                return;
            }
            render(buffer, theme, &information);

            if (information.battery_available && !information.battery_charging && information.power_supply_available &&
                information.remaining_percentage > 95.0) {
                bsp_power_off(false);
            }
        }
    }
}
