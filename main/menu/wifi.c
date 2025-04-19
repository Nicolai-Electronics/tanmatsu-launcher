#include "wifi.h"
#include "bsp/input.h"
#include "common/display.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
// #include "shapes/pax_misc.h"

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial) {
    if (!partial) {
        pax_background(buffer, theme->palette.color_background);
        // gui_render_header(buffer, theme, "WiFi networks");
        // gui_render_footer(buffer, theme, "ESC / ❌ Back", "↑ / ↓ Navigate ⏎ Select");
        gui_render_header_adv(buffer, theme, &((gui_icontext_t){get_icon(ICON_WIFI), "WiFi networks"}), 1);
        gui_render_footer_adv(buffer, theme,
                              ((gui_icontext_t[]){{get_icon(ICON_ESC), "/"},
                                                  {get_icon(ICON_F1), "Back"},
                                                  {get_icon(ICON_F2), "Scan"},
                                                  {get_icon(ICON_F3), "Add manually"}}),
                              4, "↑ / ↓ Navigate ⏎ Select");
    }
    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_wifi(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu, "WiFi networks", NULL);

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    render(buffer, theme, &menu, position, false);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                                menu_free(&menu);
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, position, true);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, position, true);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                render(buffer, theme, &menu, position, false);
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
        }
    }
}
