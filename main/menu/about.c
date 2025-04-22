#include "about.h"
#include "bsp/input.h"
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

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT  ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT NULL, 0
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#endif

static void render(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_INFO), "About"}}), 1, FOOTER_LEFT,
                                     FOOTER_RIGHT);
    }
    if (!partial) {
        pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                      position.y0 + 18 * 0, "Tanmatsu launcher firmware");
        pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                      position.y0 + 18 * 1, "Copyright Nicolai Electronics 2025");
        pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                      position.y0 + 18 * 2, "Library authors:");
        pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                      position.y0 + 18 * 3, " - Copyright (c) 2021-2023 Julian Scheffers");
        pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                      position.y0 + 18 * 4, " - Copyright (c) 2024 Orange-Murker");
        pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                      position.y0 + 18 * 5, " - Jeroen Domburg");
    }
    display_blit_buffer(buffer);
}

void menu_about(pax_buf_t* buffer, gui_theme_t* theme) {
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

    render(buffer, theme, position, false, true);
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
            render(buffer, theme, position, true, true);
        }
    }
}
