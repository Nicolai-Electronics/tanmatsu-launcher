#include "bsp/input.h"
#include "common/display.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "wifi.h"
#include "wifi_connection.h"
#include "wifi_settings.h"
// #include "shapes/pax_misc.h"

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_WIFI), "Edit WiFi network"}}), 1,
                                     ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"},
                                                             {get_icon(ICON_F1), "Exit without saving"},
                                                             {get_icon(ICON_F2), "Save and exit"}}),
                                     3, ((gui_header_field_t[]){{NULL, "↑ / ↓ Navigate ⏎ Edit setting"}}), 1);
    }
    display_blit_buffer(buffer);
}

void menu_wifi_edit(pax_buf_t* buffer, gui_theme_t* theme, uint8_t index) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    render(buffer, theme, &menu, false, true);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                                menu_free(&menu);
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_F2:
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                render(buffer, theme, &menu, false, false);
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
        } else {
            render(buffer, theme, &menu, true, true);
        }
    }
}
