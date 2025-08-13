#include "menu_power_information.h"
#include <string.h>
#include "bsp/input.h"
#include "bsp/power.h"
#include "common/display.h"
#include "common/theme.h"
#include "driver/temperature_sensor.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    18
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{NULL, "🅱 Back"}}), 1
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#endif

static temperature_sensor_handle_t temp_handle = NULL;

static void render(void) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    int        header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int        footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);
    pax_vec2_t position      = {
             .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
             .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
             .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
             .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    render_base_screen_statusbar(buffer, theme, true, true, true,
                                 ((gui_element_icontext_t[]){{get_icon(ICON_DEVICE_INFO), "Power information"}}), 1,
                                 FOOTER_LEFT, FOOTER_RIGHT);
    char text_buffer[256];
    int  line = 0;

    bsp_power_battery_information_t information = {0};

    esp_err_t res = bsp_power_get_battery_information(&information);

    if (res != ESP_OK) {
        snprintf(text_buffer, sizeof(text_buffer), "Unavailable (%s)", esp_err_to_name(res));
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        display_blit_buffer(buffer);
        return;
    }

    if (information.battery_available) {
        snprintf(text_buffer, sizeof(text_buffer), "Battery type:             %s",
                 information.type ? information.type : "Unknown");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Charging disabled:        %s",
                 information.charging_disabled ? "Yes" : "No");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Battery charging:         %s",
                 information.battery_charging ? "Yes" : "No");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Max charging current:     %" PRIu16 " mA",
                 information.maximum_charging_current);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Current charging current: %" PRIu16 " mA",
                 information.current_charging_current);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Battery voltage:          %" PRIu16 " mV", information.voltage);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Charging target voltage:  %" PRIu16 " mV",
                 information.charging_target_voltage);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Remaining charge:         %.2f%%",
                 information.remaining_percentage);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
    } else {
        snprintf(text_buffer, sizeof(text_buffer), "No battery detected");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
    }

    line++;
    snprintf(text_buffer, sizeof(text_buffer), "Power supply detected:    %s",
             information.power_supply_available ? "Yes" : "No");
    pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                  position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);

    uint16_t  system_voltage = 0;
    esp_err_t res_sys        = bsp_power_get_system_voltage(&system_voltage);
    if (res_sys == ESP_OK) {
        snprintf(text_buffer, sizeof(text_buffer), "System voltage:           %" PRIu16 " mV", system_voltage);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
    }

    uint16_t  input_voltage = 0;
    esp_err_t res_input     = bsp_power_get_input_voltage(&input_voltage);
    if (res_input == ESP_OK) {
        snprintf(text_buffer, sizeof(text_buffer), "Input voltage:            %" PRIu16 " mV", input_voltage);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
    }

    res = temperature_sensor_enable(temp_handle);
    if (res == ESP_OK) {
        float tsens_out;
        res = temperature_sensor_get_celsius(temp_handle, &tsens_out);
        if (res == ESP_OK) {
            line++;
            snprintf(text_buffer, sizeof(text_buffer), "SoC temperature:         ~%.2f *C", tsens_out);
            pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        };
        temperature_sensor_disable(temp_handle);
    }

    display_blit_buffer(buffer);
}

void menu_power_information(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    if (temp_handle == NULL) {
        temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 50);
        ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_handle));
    }

    render();
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
                                return;
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
            render();
        }
    }
}
