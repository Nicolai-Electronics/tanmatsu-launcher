#include "about.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "chakrapetchmedium.h"
#include "common/display.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_style.h"
#include "icons.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"
// #include "shapes/pax_misc.h"

static void render(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position) {
    pax_background(buffer, theme->palette.color_background);
    // gui_render_header(buffer, theme, "About");
    // gui_render_footer(buffer, theme, "ESC / ❌ Back", "");
    gui_render_header_adv(buffer, theme, &((gui_icontext_t){get_icon(ICON_INFO), "About"}), 1);
    gui_render_footer_adv(buffer, theme, ((gui_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}),
                          2, "");

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

    render(buffer, theme, position);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
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
        }
    }
}
