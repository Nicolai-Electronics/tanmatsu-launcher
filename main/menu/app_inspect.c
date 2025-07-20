
#include "app_inspect.h"
#include <string.h>
#include "appfs.h"
#include "bsp/input.h"
#include "common/display.h"
#include "gui_footer.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/apps.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT                                              \
    ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"},           \
                            {get_icon(ICON_F1), "Back"},         \
                            {get_icon(ICON_F2), "Start"},        \
                            {get_icon(ICON_F5), "Delete App"}}), \
        4
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    18
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define FOOTER_LEFT  ((gui_header_field_t[]){{NULL, "🅱 Back"}}), 1
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#endif

static void render(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, bool partial, bool icons, app_t* app) {

    char text_buffer[256];
    int  line = 0;

    if (!partial || icons) {
        snprintf(text_buffer, sizeof(text_buffer), "App Info: %s", app->name);
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_DEVICE_INFO), text_buffer}}), 1,
                                     FOOTER_LEFT, FOOTER_RIGHT);
    }

    if (!partial) {

        if (app->name) {
            snprintf(text_buffer, sizeof(text_buffer), "Name: %s", app->name);
            pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        }

        if (app->description) {
            snprintf(text_buffer, sizeof(text_buffer), "Description: %s", app->description);
            pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        }

        if (app->author) {
            snprintf(text_buffer, sizeof(text_buffer), "Author: %s", app->author);
            pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        }

        if (app->version) {
            snprintf(text_buffer, sizeof(text_buffer), "Version: %ld", app->version);
            pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        }

        if (app->slug) {
            snprintf(text_buffer, sizeof(text_buffer), "Slug: %s", app->slug);
            pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        }
    }

    display_blit_buffer(buffer);
}

bool menu_app_inspect(pax_buf_t* buffer, gui_theme_t* theme, app_t* app) {
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

    render(buffer, theme, position, false, false, app);

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
                                return false;
                            case BSP_INPUT_NAVIGATION_KEY_F2:
                                execute_app(buffer, theme, position, app);
                                render(buffer, theme, position, false, false, app);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F5:
                                message_dialog_return_type_t msg_ret = message_dialog_yes_no(
                                    buffer, theme, "Delete App", "Do you really want to delete the app?");
                                if (msg_ret == MSG_DIALOG_RETURN_OK) {
                                    appfsDeleteFile(app->slug);
                                    char text_buffer[256];
                                    snprintf(text_buffer, sizeof(text_buffer), "/sd/apps/%s", app->slug);
                                    remove(text_buffer);
                                    snprintf(text_buffer, sizeof(text_buffer), "/int/apps/%s", app->slug);
                                    remove(text_buffer);
                                    return true;
                                }
                                render(buffer, theme, position, false, false, app);
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
            render(buffer, theme, position, false, false, app);
        }
    }
}
