#include <sys/time.h>
#include <time.h>
#include "about.h"
#include "bsp/input.h"
#include "bsp/rtc.h"
#include "common/display.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "nvs.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"
#include "settings_clock_timezone.h"
#include "timezone.h"
// #include "shapes/pax_misc.h"

static const char* TAG = "clock";

static void render(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, const timezone_t* zone, bool ntp,
                   bool partial, bool icons, uint8_t selection) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_CLOCK), "Date and time configuration"}}), 1,
                                     ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"},
                                                             {get_icon(ICON_F1), "Back"},
                                                             {get_icon(ICON_F2), "Set timezone"},
                                                             {get_icon(ICON_F3), "Toggle NTP"}}),
                                     4, ((gui_header_field_t[]){{NULL, " ← / → Navigate ↑ / ↓ Modify value"}}), 1);
    }
    char text_buffer[80];

    time_t     now      = time(NULL);
    struct tm* timeinfo = localtime(&now);

    float box_width = position.x1 - position.x0;
    // float box_height = position.y1 - position.y0;

    float     date_font_size = 45;
    pax_vec2f date_selection_size =
        (selection < 3) ? pax_text_size(pax_font_sky_mono, date_font_size, selection < 2 ? "00" : "0000")
                        : ((pax_vec2f){0});
    pax_vec2f date_selection_offset_element = pax_text_size(pax_font_sky_mono, date_font_size, "00-");
    float     date_selection_offset         = date_selection_offset_element.x * selection;

    pax_vec2f date_size = pax_text_size(pax_font_sky_mono, date_font_size, "00-00-0000");
    strftime(text_buffer, sizeof(text_buffer), "%d-%m-%Y", timeinfo);

    pax_rect_t date_box = {
        .x = position.x0 + (box_width / 2) - (date_size.x / 2),
        .y = position.y0,
        .w = date_size.x,
        .h = date_size.y,
    };

    pax_draw_rect(buffer, theme->palette.color_active_background, date_box.x, date_box.y, date_box.w, date_box.h);
    if (selection < 3) {
        pax_draw_rect(buffer, theme->palette.color_highlight_secondary, date_box.x + date_selection_offset, date_box.y,
                      date_selection_size.x, date_box.h);
    }
    pax_draw_text(buffer, theme->palette.color_active_foreground, pax_font_sky_mono, date_font_size, date_box.x,
                  date_box.y, text_buffer);

    float     time_font_size = 90;
    pax_vec2f time_selection_size =
        (selection >= 3) ? pax_text_size(pax_font_sky_mono, time_font_size, "00") : ((pax_vec2f){0});
    pax_vec2f time_selection_offset_element = pax_text_size(pax_font_sky_mono, time_font_size, "00:");
    float     time_selection_offset         = time_selection_offset_element.x * (selection - 3);
    pax_vec2f time_size                     = pax_text_size(pax_font_sky_mono, time_font_size, "00:00:00");
    strftime(text_buffer, sizeof(text_buffer), "%H:%M:%S", timeinfo);

    pax_rect_t time_box = {
        .x = position.x0 + (box_width / 2) - (time_size.x / 2),
        .y = position.y0 + time_size.y + theme->menu.vertical_padding,
        .w = time_size.x,
        .h = time_size.y,
    };

    pax_draw_rect(buffer, theme->palette.color_active_background, time_box.x, time_box.y, time_box.w, time_box.h);
    if (selection >= 3) {
        pax_draw_rect(buffer, theme->palette.color_highlight_secondary, time_box.x + time_selection_offset, time_box.y,
                      time_selection_size.x, time_box.h);
    }
    pax_draw_text(buffer, theme->palette.color_foreground, pax_font_sky_mono, time_font_size, time_box.x, time_box.y,
                  text_buffer);

    if (!partial) {
        if (zone != NULL) {
            snprintf(text_buffer, sizeof(text_buffer), "Timezone: %s", zone->name);
            pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                          time_box.y + time_box.h + theme->menu.vertical_padding + 40, text_buffer);
        }
        snprintf(text_buffer, sizeof(text_buffer), "NTP: %s", ntp ? "Enabled" : "Disabled");
        pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                      time_box.y + time_box.h + theme->menu.vertical_padding * 2 + 16 + 40, text_buffer);
    }
    display_blit_buffer(buffer);
}

void adjust_date_time(uint8_t selection, int8_t delta) {
    time_t     now      = time(NULL);
    struct tm* timeinfo = localtime(&now);

    switch (selection) {
        case 0:
            timeinfo->tm_mday += delta;
            break;
        case 1:
            timeinfo->tm_mon += delta;
            break;
        case 2:
            timeinfo->tm_year += delta;
            break;
        case 3:
            timeinfo->tm_hour += delta;
            break;
        case 4:
            timeinfo->tm_min += delta;
            break;
        case 5:
            timeinfo->tm_sec += delta;
            break;
        default:
            break;
    }
    time_t         new_time    = mktime(timeinfo);
    struct timeval rtc_timeval = {
        .tv_sec  = new_time,
        .tv_usec = 0,
    };
    settimeofday(&rtc_timeval, NULL);
    bsp_rtc_set_time(new_time);
}

static bool get_ntp(void) {
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open("system", NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return false;
    }
    uint8_t enabled = false;
    res             = nvs_get_u8(nvs_handle, "ntp", &enabled);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get NVS entry");
        nvs_close(nvs_handle);
        return false;
    }
    res = nvs_commit(nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit to NVS");
    }
    nvs_close(nvs_handle);
    return (enabled & 1);
}

static void set_ntp(bool enabled) {
    nvs_handle_t nvs_handle;
    esp_err_t    res = nvs_open("system", NVS_READWRITE, &nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return;
    }
    res = nvs_set_u8(nvs_handle, "ntp", enabled ? 1 : 0);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set NVS entry");
        nvs_close(nvs_handle);
        return;
    }
    res = nvs_commit(nvs_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit to NVS");
    }
    nvs_close(nvs_handle);
}

static void get_timezone(const timezone_t** zone) {
    char timezone_name[32] = {0};
    timezone_nvs_get("system", "timezone", timezone_name, sizeof(timezone_name));
    timezone_get_name(timezone_name, zone);
}

void menu_settings_clock(pax_buf_t* buffer, gui_theme_t* theme) {
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

    const timezone_t* zone = NULL;

    get_timezone(&zone);

    bool ntp = get_ntp();

    uint8_t selection = 0;
    render(buffer, theme, position, zone, ntp, false, true, selection);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(500)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_LEFT:
                                if (selection > 0) {
                                    selection--;
                                }
                                render(buffer, theme, position, zone, ntp, true, false, selection);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                                if (selection < 5) {
                                    selection++;
                                }
                                render(buffer, theme, position, zone, ntp, true, false, selection);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                adjust_date_time(selection, 1);
                                render(buffer, theme, position, zone, ntp, true, false, selection);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                adjust_date_time(selection, -1);
                                render(buffer, theme, position, zone, ntp, true, false, selection);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F2:
                                // Set timezone
                                menu_clock_timezone(buffer, theme);
                                get_timezone(&zone);
                                render(buffer, theme, position, zone, ntp, false, true, selection);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F3:
                                // Toggle NTP
                                ntp = !ntp;
                                set_ntp(ntp);
                                render(buffer, theme, position, zone, ntp, false, true, selection);
                                break;
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
            render(buffer, theme, position, zone, ntp, true, true, selection);
        }
    }
}
