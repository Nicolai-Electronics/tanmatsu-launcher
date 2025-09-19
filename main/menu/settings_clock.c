#include <sys/time.h>
#include <time.h>
#include "about.h"
#include "bsp/input.h"
#include "bsp/rtc.h"
#include "common/display.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "gui_element_footer.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "ntp.h"
#include "nvs.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"
#include "sdkconfig.h"
#include "settings_clock_timezone.h"
#include "timezone.h"

static const char* TAG = "clock";

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT                                                  \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},           \
                                {get_icon(ICON_F1), "Back"},         \
                                {get_icon(ICON_F2), "Set timezone"}, \
                                {get_icon(ICON_F3), "Toggle NTP"}}), \
        4
#define FOOTER_RIGHT   ((gui_element_icontext_t[]){{NULL, " ← / → Navigate ↑ / ↓ Modify value"}}), 1
#define DATE_TEXT_SIZE 45
#define TIME_TEXT_SIZE 90
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define FOOTER_LEFT    ((gui_element_icontext_t[]){{NULL, "🅱Back"}}), 1
#define FOOTER_RIGHT   ((gui_element_icontext_t[]){{NULL, "🆂Set timezone"}, {NULL, "🅴Toggle NTP"}}), 2
#define DATE_TEXT_SIZE 32
#define TIME_TEXT_SIZE 32
#else
#define FOOTER_LEFT    NULL, 0
#define FOOTER_RIGHT   NULL, 0
#define DATE_TEXT_SIZE 32
#define TIME_TEXT_SIZE 32
#endif

static void render(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, const timezone_t* zone, bool ntp,
                   bool partial, bool icons, uint8_t selection) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_CLOCK), "Clock configuration"}}), 1,
                                     FOOTER_LEFT, FOOTER_RIGHT);
    }

    float date_font_size = DATE_TEXT_SIZE;
    float time_font_size = TIME_TEXT_SIZE;

    char text_buffer[80];

    time_t     now      = time(NULL);
    struct tm* timeinfo = localtime(&now);

    float box_width = position.x1 - position.x0;
    // float box_height = position.y1 - position.y0;

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
    time_t new_time = mktime(timeinfo);

    if (new_time < 0) {
        new_time = 0;
    }

    struct timeval rtc_timeval = {
        .tv_sec  = new_time,
        .tv_usec = 0,
    };
    settimeofday(&rtc_timeval, NULL);
    bsp_rtc_set_time(new_time);
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

    bool ntp = ntp_get_enabled();

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
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
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
                            case BSP_INPUT_NAVIGATION_KEY_MENU:
                                // Set timezone
                                menu_clock_timezone(buffer, theme);
                                get_timezone(&zone);
                                render(buffer, theme, position, zone, ntp, false, true, selection);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F3:
                            case BSP_INPUT_NAVIGATION_KEY_SELECT:
                                // Toggle NTP
                                ntp = !ntp;
                                ntp_set_enabled(ntp);
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
