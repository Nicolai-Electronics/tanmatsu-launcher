#include "message_dialog.h"
#include <time.h>
#include "bsp/input.h"
#include "bsp/led.h"
#include "bsp/power.h"
#include "common/display.h"
#include "common/theme.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "gui_element_footer.h"
#include "gui_element_header.h"
#include "gui_element_icontext.h"
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
#include "plugin_manager.h"
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#include "bsp/tanmatsu.h"
#include "synthwave.h"
#include "tanmatsu_coprocessor.h"
#endif

static char clock_buffer[6] = {0};

static gui_element_icontext_t clock_indicator(void) {
    time_t     now      = time(NULL);
    struct tm* timeinfo = localtime(&now);
    strftime(clock_buffer, sizeof(clock_buffer), "%H:%M", timeinfo);
    return (gui_element_icontext_t){NULL, clock_buffer};
}

static char percentage_buffer[5] = {0};

static gui_element_icontext_t battery_indicator(void) {
    bsp_power_battery_information_t information = {0};
    bsp_power_get_battery_information(&information);

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
    tanmatsu_coprocessor_handle_t coprocessor_handle = NULL;
    bsp_tanmatsu_coprocessor_get_handle(&coprocessor_handle);
    tanmatsu_coprocessor_pmic_faults_t faults = {0};
    tanmatsu_coprocessor_get_pmic_faults(coprocessor_handle, &faults);
#endif

    if (!information.battery_available) {
        return (gui_element_icontext_t){get_icon(ICON_BATTERY_UNKNOWN), ""};
    }
    if (information.battery_charging) {
        return (gui_element_icontext_t){get_icon(ICON_BATTERY_BOLT), ""};
    }

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
    if (faults.watchdog || faults.chrg_input || faults.chrg_thermal || faults.chrg_safety || faults.batt_ovp ||
        faults.ntc_cold || faults.ntc_hot) {
        return (gui_element_icontext_t){get_icon(ICON_BATTERY_ALERT), ""};
    }
#endif

    // snprintf(percentage_buffer, sizeof(percentage_buffer), "%3u%%", (uint8_t)information.remaining_percentage);
    if (information.remaining_percentage >= 98) {
        return (gui_element_icontext_t){get_icon(ICON_BATTERY_FULL), percentage_buffer};
    }
    if (information.remaining_percentage >= 84) {
        return (gui_element_icontext_t){get_icon(ICON_BATTERY_6), percentage_buffer};
    }
    if (information.remaining_percentage >= 70) {
        return (gui_element_icontext_t){get_icon(ICON_BATTERY_5), percentage_buffer};
    }
    if (information.remaining_percentage >= 56) {
        return (gui_element_icontext_t){get_icon(ICON_BATTERY_4), percentage_buffer};
    }
    if (information.remaining_percentage >= 42) {
        return (gui_element_icontext_t){get_icon(ICON_BATTERY_3), percentage_buffer};
    }
    if (information.remaining_percentage >= 28) {
        return (gui_element_icontext_t){get_icon(ICON_BATTERY_2), percentage_buffer};
    }
    if (information.remaining_percentage >= 14) {
        return (gui_element_icontext_t){get_icon(ICON_BATTERY_1), percentage_buffer};
    }
    return (gui_element_icontext_t){get_icon(ICON_BATTERY_0), percentage_buffer};
}

static gui_element_icontext_t usb_indicator(void) {
    if (usb_mode_get() == USB_DEVICE) {
        return (gui_element_icontext_t){get_icon(ICON_USB), ""};
    } else {
        return (gui_element_icontext_t){get_icon(ICON_BUG_REPORT), ""};
    }
}

extern bool wifi_stack_get_initialized(void);

static wifi_ap_record_t connected_ap = {0};

static gui_element_icontext_t wifi_indicator(void) {
    bool              radio_initialized = wifi_stack_get_initialized();
    wifi_mode_t       mode              = WIFI_MODE_NULL;
    bsp_radio_state_t state;
    bsp_power_get_radio_state(&state);
    switch (state) {
        case BSP_POWER_RADIO_STATE_OFF:
            return (gui_element_icontext_t){NULL, ""};
        case BSP_POWER_RADIO_STATE_BOOTLOADER:
            return (gui_element_icontext_t){NULL, "BOOT"};
        case BSP_POWER_RADIO_STATE_APPLICATION:
        default:
            if (radio_initialized && esp_wifi_get_mode(&mode) == ESP_OK) {
                if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
                    if (wifi_connection_is_connected() && esp_wifi_sta_get_ap_info(&connected_ap) == ESP_OK) {
                        pax_buf_t* icon = get_icon(ICON_WIFI_0_BAR);
                        if (connected_ap.rssi > -50) {
                            icon = get_icon(ICON_WIFI_4_BAR);
                        } else if (connected_ap.rssi > -60) {
                            icon = get_icon(ICON_WIFI_3_BAR);
                        } else if (connected_ap.rssi > -70) {
                            icon = get_icon(ICON_WIFI_2_BAR);
                        } else if (connected_ap.rssi > -80) {
                            icon = get_icon(ICON_WIFI_1_BAR);
                        }
                        return (gui_element_icontext_t){icon, (char*)connected_ap.ssid};
                    } else {
                        return (gui_element_icontext_t){get_icon(ICON_WIFI_OFF), "Disconnected"};
                    }
                } else if (mode == WIFI_MODE_AP) {
                    return (gui_element_icontext_t){get_icon(ICON_WIFI_OFF), ""};  // AP mode is currently unused
                    // The device will be in AP mode by default until connection to a network is
                } else {
                    return (gui_element_icontext_t){get_icon(ICON_WIFI_UNKNOWN), "Other"};
                }
            } else {
                return (gui_element_icontext_t){get_icon(ICON_WIFI_ERROR), ""};
            }
            break;
    }
}

static gui_element_icontext_t sdcard_indicator(void) {
    switch (sd_status()) {
        case SD_STATUS_OK:
            return (gui_element_icontext_t){get_icon(ICON_SD_CARD), ""};
        case SD_STATUS_ERROR:
            return (gui_element_icontext_t){get_icon(ICON_SD_CARD_ALERT), ""};
        default:
            return (gui_element_icontext_t){NULL, ""};
    }
}

void render_base_screen(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                        gui_element_icontext_t* header_left, size_t header_left_count,
                        gui_element_icontext_t* header_right, size_t header_right_count,
                        gui_element_icontext_t* footer_left, size_t footer_left_count,
                        gui_element_icontext_t* footer_right, size_t footer_right_count) {
    if (background) {
        pax_background(buffer, theme->palette.color_background);
    }
    if (header) {
        if (!background) {
            pax_simple_rect(buffer, theme->palette.color_background, 0, 0, pax_buf_get_width(buffer),
                            theme->header.height + (theme->header.vertical_margin * 2));
        }
        gui_header_draw(buffer, theme, header_left, header_left_count, header_right, header_right_count);
    }
    if (footer) {
        if (!background) {
            pax_simple_rect(buffer, theme->palette.color_background, 0,
                            pax_buf_get_height(buffer) - theme->footer.height - (theme->footer.vertical_margin * 2),
                            pax_buf_get_width(buffer), theme->footer.height + (theme->footer.vertical_margin * 2));
        }
        gui_footer_draw(buffer, theme, footer_left, footer_left_count, footer_right, footer_right_count);
    }
}

void render_base_screen_statusbar(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                                  gui_element_icontext_t* header_left, size_t header_left_count,
                                  gui_element_icontext_t* footer_left, size_t footer_left_count,
                                  gui_element_icontext_t* footer_right, size_t footer_right_count) {
    gui_element_icontext_t header_right[5]    = {0};
    size_t                 header_right_count = 0;
    if (header) {
        header_right[0]    = clock_indicator();
        header_right[1]    = battery_indicator();
        header_right[2]    = usb_indicator();
        header_right[3]    = wifi_indicator();
        header_right[4]    = sdcard_indicator();
        header_right_count = 5;
    } else {
        header_right_count = 0;
    }
    render_base_screen(buffer, theme, background, header || footer, footer, header_left, header_left_count,
                       header_right, header_right_count, footer_left, footer_left_count, footer_right,
                       footer_right_count);

    // Render plugin status widgets in the header area (right-to-left, before system icons)
    if (header) {
        // Position widgets to the left of the system status icons
        // System icons start around x=400, so give plugins space before that
        int widget_x_right = 380;
        int widget_y = theme->header.vertical_margin;
        int widget_height = theme->header.height;
        plugin_api_render_status_widgets(buffer, widget_x_right, widget_y, widget_height);
    }

    // Update status LEDs (manual mode replaces coprocessor automatic mode)
    // LED 0: WiFi status indicator
    // LED 1: Power/battery status indicator
    // LEDs 2-5: Available for plugins
    // Only update LEDs if WiFi stack is initialized (system is ready)
    if (wifi_stack_get_initialized()) {
        // WiFi LED (LED 0): Green if connected, off if not
        if (wifi_connection_is_connected()) {
            bsp_led_set_pixel_rgb(0, 0, 64, 0);  // Dim green when connected
        } else {
            bsp_led_set_pixel_rgb(0, 0, 0, 0);   // Off when disconnected
        }

        // Power LED (LED 1): Color based on battery state
        bsp_power_battery_information_t bat_info = {0};
        bsp_power_get_battery_information(&bat_info);

        if (!bat_info.battery_available) {
            // No battery - dim blue for external power
            bsp_led_set_pixel_rgb(1, 0, 0, 64);
        } else if (bat_info.battery_charging) {
            // Charging - orange/amber
            bsp_led_set_pixel_rgb(1, 64, 32, 0);
        } else if (bat_info.remaining_percentage < 15) {
            // Low battery - red
            bsp_led_set_pixel_rgb(1, 64, 0, 0);
        } else if (bat_info.remaining_percentage < 30) {
            // Medium-low battery - yellow
            bsp_led_set_pixel_rgb(1, 64, 64, 0);
        } else {
            // Good battery - green
            bsp_led_set_pixel_rgb(1, 0, 64, 0);
        }

        // Apply plugin LED overlay (preserves plugin-controlled LEDs)
        plugin_api_apply_led_overlay();

        // Send LED data to coprocessor
        bsp_led_send();
    }
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, pax_buf_t* icon, const char* title,
                   const char* message, gui_element_icontext_t* footer, int footer_count, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{icon, (char*)title}}), 1, footer, footer_count, NULL,
                                     0);
    }
    if (!partial) {
        pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                      position.y0 + 18 * 0, message);
    }
    display_blit_buffer(buffer);
}

void message_dialog(pax_buf_t* icon, const char* title, const char* message, const char* action_text) {
    pax_buf_t*    buffer            = display_get_buffer();
    gui_theme_t*  theme             = get_theme();
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

    render(buffer, theme, position, icon, title, message, ADV_DIALOG_FOOTER_OK_TEXT((char*)action_text), false, true);
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
            render(buffer, theme, position, icon, title, message, ADV_DIALOG_FOOTER_OK_TEXT((char*)action_text), true,
                   true);
        }
    }
}

bsp_input_navigation_key_t adv_dialog(pax_buf_t* icon, const char* title, const char* message,
                                      gui_element_icontext_t* footer, int footer_count) {
    pax_buf_t*    buffer            = display_get_buffer();
    gui_theme_t*  theme             = get_theme();
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

    render(buffer, theme, position, icon, title, message, footer, footer_count, false, true);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) return event.args_navigation.key;
                    break;
                }
                default:
                    break;
            }
        } else {
            render(buffer, theme, position, icon, title, message, footer, footer_count, true, true);
        }
    }
}

message_dialog_return_type_t adv_dialog_ok(pax_buf_t* icon, const char* title, const char* message) {
    bsp_input_navigation_key_t key;
    while (1) {
        key = adv_dialog(icon, title, message, ADV_DIALOG_FOOTER_OK);
        switch (key) {
            case BSP_INPUT_NAVIGATION_KEY_ESC:
            case BSP_INPUT_NAVIGATION_KEY_F1:
            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                return MSG_DIALOG_RETURN_OK;
            default:
        }
    }
}

message_dialog_return_type_t adv_dialog_yes_no(pax_buf_t* icon, const char* title, const char* message) {
    bsp_input_navigation_key_t key;
    while (1) {
        key = adv_dialog(icon, title, message, ADV_DIALOG_FOOTER_YES_NO);
        switch (key) {
            case BSP_INPUT_NAVIGATION_KEY_ESC:
            case BSP_INPUT_NAVIGATION_KEY_F1:
            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                return MSG_DIALOG_RETURN_NO;
            case BSP_INPUT_NAVIGATION_KEY_F4:
            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                return MSG_DIALOG_RETURN_OK;
            default:
        }
    }
}

message_dialog_return_type_t adv_dialog_yes_no_cancel(pax_buf_t* icon, const char* title, const char* message) {
    bsp_input_navigation_key_t key;
    while (1) {
        key = adv_dialog(icon, title, message, ADV_DIALOG_FOOTER_YES_NO_CANCEL);
        switch (key) {
            case BSP_INPUT_NAVIGATION_KEY_ESC:
            case BSP_INPUT_NAVIGATION_KEY_F1:
            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                return MSG_DIALOG_RETURN_NO;
            case BSP_INPUT_NAVIGATION_KEY_F4:
            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                return MSG_DIALOG_RETURN_OK;
            case BSP_INPUT_NAVIGATION_KEY_F6:
            case BSP_INPUT_NAVIGATION_KEY_MENU:
                return MSG_DIALOG_RETURN_CANCEL;
            default:
        }
    }
}

void busy_dialog(pax_buf_t* icon, const char* title, const char* message, bool header) {
    printf("BUSY: [%s] %s\n", title, message);
    if (!display_is_initialized()) {
        return;
    }
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    render_base_screen_statusbar(buffer, theme, true, header, true, ((gui_element_icontext_t[]){{icon, (char*)title}}),
                                 1, NULL, 0, NULL, 0);

    pax_center_text(buffer, 0xFF000000, theme->menu.text_font, 24, pax_buf_get_width(buffer) / 2.0f,
                    (pax_buf_get_height(buffer) - 24) / 2.0f, message);

    display_blit_buffer(buffer);
}

void startup_dialog(const char* message) {
    printf("STARTUP: %s\n", message);
    if (!display_is_initialized()) {
        return;
    }
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
    static bool initialized = false;
    if (!initialized) {
        synthwave(buffer);
        initialized = true;
    }
    synthwave_step(buffer);
    pax_draw_text(buffer, 0xFFFFFFFF, theme->menu.text_font, theme->menu.text_height, theme->menu.horizontal_margin,
                  (pax_buf_get_height(buffer) - theme->menu.text_height - theme->menu.vertical_margin), message);
#else
    pax_background(buffer, theme->palette.color_background);
    pax_draw_text(buffer, theme->menu.palette.color_active_foreground, theme->menu.text_font, theme->menu.text_height,
                  theme->menu.horizontal_margin,
                  (pax_buf_get_height(buffer) - theme->menu.text_height - theme->menu.vertical_margin), message);
#endif
    display_blit_buffer(buffer);
}
