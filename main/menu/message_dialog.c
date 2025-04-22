#include "message_dialog.h"
#include <time.h>
#include "bsp/input.h"
#include "bsp/power.h"
#include "common/display.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_style.h"
#include "icons.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"
#include "sdcard.h"
#include "sdkconfig.h"
#include "usb_device.h"
#include "wifi_connection.h"
// #include "shapes/pax_misc.h"
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#include "bsp/tanmatsu.h"
#include "tanmatsu_coprocessor.h"
#endif

static char clock_buffer[6] = {0};

static gui_header_field_t clock_indicator(void) {
    time_t     now      = time(NULL);
    struct tm* timeinfo = localtime(&now);
    strftime(clock_buffer, sizeof(clock_buffer), "%H:%M", timeinfo);
    return (gui_header_field_t){NULL, clock_buffer};
}

static char percentage_buffer[5] = {0};

static gui_header_field_t battery_indicator(void) {
    bsp_power_battery_information_t information = {0};
    bsp_power_get_battery_information(&information);

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
    tanmatsu_coprocessor_handle_t coprocessor_handle = NULL;
    bsp_tanmatsu_coprocessor_get_handle(&coprocessor_handle);
    tanmatsu_coprocessor_pmic_faults_t faults = {0};
    tanmatsu_coprocessor_get_pmic_faults(coprocessor_handle, &faults);
#endif

    if (!information.battery_available) {
        return (gui_header_field_t){get_icon(ICON_BATTERY_UNKNOWN), ""};
    }
    if (information.battery_charging) {
        return (gui_header_field_t){get_icon(ICON_BATTERY_CHARGING), ""};
    }

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
    if (faults.watchdog || faults.chrg_input || faults.chrg_thermal || faults.chrg_safety || faults.batt_ovp ||
        faults.ntc_cold || faults.ntc_hot) {
        return (gui_header_field_t){get_icon(ICON_BATTERY_ERROR), ""};
    }
#endif

    // snprintf(percentage_buffer, sizeof(percentage_buffer), "%3u%%", (uint8_t)information.remaining_percentage);
    if (information.remaining_percentage >= 98) {
        return (gui_header_field_t){get_icon(ICON_BATTERY_7), percentage_buffer};
    }
    if (information.remaining_percentage >= 84) {
        return (gui_header_field_t){get_icon(ICON_BATTERY_6), percentage_buffer};
    }
    if (information.remaining_percentage >= 70) {
        return (gui_header_field_t){get_icon(ICON_BATTERY_5), percentage_buffer};
    }
    if (information.remaining_percentage >= 56) {
        return (gui_header_field_t){get_icon(ICON_BATTERY_4), percentage_buffer};
    }
    if (information.remaining_percentage >= 42) {
        return (gui_header_field_t){get_icon(ICON_BATTERY_3), percentage_buffer};
    }
    if (information.remaining_percentage >= 28) {
        return (gui_header_field_t){get_icon(ICON_BATTERY_2), percentage_buffer};
    }
    if (information.remaining_percentage >= 14) {
        return (gui_header_field_t){get_icon(ICON_BATTERY_1), percentage_buffer};
    }
    return (gui_header_field_t){get_icon(ICON_BATTERY_0), percentage_buffer};
}

static gui_header_field_t usb_indicator(void) {
    if (usb_mode_get() == USB_DEVICE) {
        return (gui_header_field_t){get_icon(ICON_USB), ""};
    } else {
        return (gui_header_field_t){get_icon(ICON_DEV), ""};
    }
}

extern bool wifi_stack_get_initialized(void);

static gui_header_field_t wifi_indicator(void) {
    bool radio_initialized = wifi_stack_get_initialized();
    if (radio_initialized) {
        wifi_ap_record_t connected_ap;
        if (wifi_connection_is_connected() && esp_wifi_sta_get_ap_info(&connected_ap) == ESP_OK) {
            pax_buf_t* icon = get_icon(ICON_WIFI_0);
            if (connected_ap.rssi > -50) {
                icon = get_icon(ICON_WIFI_4);
            } else if (connected_ap.rssi > -60) {
                icon = get_icon(ICON_WIFI_3);
            } else if (connected_ap.rssi > -70) {
                icon = get_icon(ICON_WIFI_2);
            } else if (connected_ap.rssi > -80) {
                icon = get_icon(ICON_WIFI_1);
            }
            return (gui_header_field_t){icon, (char*)connected_ap.ssid};
        } else {
            return (gui_header_field_t){get_icon(ICON_WIFI_ERROR), ""};
        }
    } else {
        return (gui_header_field_t){get_icon(ICON_WIFI_UNKNOWN), ""};
    }
}

static gui_header_field_t sdcard_indicator(void) {
    switch (sd_status()) {
        case SD_STATUS_OK:
            return (gui_header_field_t){get_icon(ICON_SD), ""};
        case SD_STATUS_ERROR:
            return (gui_header_field_t){get_icon(ICON_SD_ERROR), ""};
        default:
            return (gui_header_field_t){NULL, ""};
    }
}

void render_base_screen(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                        gui_header_field_t* header_left, size_t header_left_count, gui_header_field_t* header_right,
                        size_t header_right_count, gui_header_field_t* footer_left, size_t footer_left_count,
                        gui_header_field_t* footer_right, size_t footer_right_count) {
    if (background) {
        pax_background(buffer, theme->palette.color_background);
    }
    if (header) {
        if (!background) {
            pax_simple_rect(buffer, theme->palette.color_background, 0, 0, pax_buf_get_width(buffer),
                            theme->header.height + (theme->header.vertical_margin * 2));
        }
        gui_render_header_adv(buffer, theme, header_left, header_left_count, header_right, header_right_count);
    }
    if (footer) {
        if (!background) {
            pax_simple_rect(buffer, theme->palette.color_background, 0,
                            pax_buf_get_height(buffer) - theme->footer.height - (theme->footer.vertical_margin * 2),
                            pax_buf_get_width(buffer), theme->footer.height + (theme->footer.vertical_margin * 2));
        }
        gui_render_footer_adv(buffer, theme, footer_left, footer_left_count, footer_right, footer_right_count);
    }
}

void render_base_screen_statusbar(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                                  gui_header_field_t* header_left, size_t header_left_count,
                                  gui_header_field_t* footer_left, size_t footer_left_count,
                                  gui_header_field_t* footer_right, size_t footer_right_count) {
    gui_header_field_t header_right[5]    = {0};
    size_t             header_right_count = 0;
    if (header) {
        header_right[0]    = clock_indicator();
        header_right[1]    = battery_indicator();
        header_right[2]    = usb_indicator();
        header_right[3]    = wifi_indicator();
        header_right[4]    = sdcard_indicator();
        header_right_count = 5;
    }
    render_base_screen(buffer, theme, background, header, footer, header_left, header_left_count, header_right,
                       header_right_count, footer_left, footer_left_count, footer_right, footer_right_count);
}

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT  ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), (char*)action_text}}), 2
#define FOOTER_RIGHT NULL, 0
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define FOOTER_LEFT  ((gui_header_field_t[]){{NULL, "🅱"}, {NULL, (char*)action_text}}), 2
#define FOOTER_RIGHT NULL, 0
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#endif

static void render(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, const char* title, const char* message,
                   const char* action_text, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_ERROR), title}}), 1, FOOTER_LEFT,
                                     FOOTER_RIGHT);
    }
    if (!partial) {
        pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                      position.y0 + 18 * 0, message);
    }
    display_blit_buffer(buffer);
}

void message_dialog(pax_buf_t* buffer, gui_theme_t* theme, const char* title, const char* message,
                    const char* action_text) {
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

    render(buffer, theme, position, title, message, action_text, false, true);
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
            render(buffer, theme, position, title, message, action_text, true, true);
        }
    }
}
