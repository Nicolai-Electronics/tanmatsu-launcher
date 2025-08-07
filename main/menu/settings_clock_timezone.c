#include <string.h>
#include "bsp/input.h"
#include "common/display.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "gui_element_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "timezone.h"
// #include "shapes/pax_misc.h"

static const char* TAG = "timezone settings";

static void populate_menu_from_timezones(menu_t* menu) {
    char current_timezone[TIMEZONE_NAME_LEN] = {0};
    timezone_nvs_get("system", "timezone", (char*)current_timezone, sizeof(current_timezone));
    const timezone_t* current_timezone_ptr = NULL;
    timezone_get_name((char*)current_timezone, &current_timezone_ptr);

    for (uint32_t i = 0; i < timezone_get_amount(); i++) {
        const timezone_t* timezone = timezone_get_index(i);
        const char*       name     = timezone->name;
        menu_insert_item(menu, name, NULL, (void*)i, -1);
        if (current_timezone_ptr == timezone) {
            menu_set_position(menu, i);
        }
    }
}

static void set_timezone(uint32_t index) {
    const timezone_t* timezone = timezone_get_index(index);
    if (timezone == NULL) {
        ESP_LOGE(TAG, "Failed to find timezone");
        return;
    }

    timezone_nvs_set("system", "timezone", timezone->name);
    timezone_nvs_set_tzstring("system", "tz", timezone->tz);
    timezone_apply_timezone(timezone);
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(
            buffer, theme, !partial, !partial || icons, !partial,
            ((gui_element_icontext_t[]){{get_icon(ICON_GLOBE_LOCATION), "Timezone"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
            ((gui_element_icontext_t[]){{NULL, "↑ / ↓ Navigate ⏎ Set timezone"}}), 1);
    }
    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_clock_timezone(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    populate_menu_from_timezones(&menu);

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    render(buffer, theme, &menu, position, false, true);
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
                                menu_free(&menu);
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                void*    arg   = menu_get_callback_args(&menu, menu_get_position(&menu));
                                uint32_t index = (uint32_t)arg;
                                set_timezone(index);
                                menu_free(&menu);
                                return;
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
            render(buffer, theme, &menu, position, true, true);
        }
    }
}
