#include "settings_lora.h"
#include <string.h>
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "device_settings.h"
#include "freertos/idf_additions.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/textedit.h"
#include "message_dialog.h"
#include "pax_gfx.h"
#include "pax_types.h"

#define REPO_SERVER_MAX_LEN     128
#define REPO_BASE_URI_MAX_LEN   64
#define HTTP_USER_AGENT_MAX_LEN 128

#define DEFAULT_REPO_SERVER   "https://apps.tanmatsu.cloud"
#define DEFAULT_REPO_BASE_URI "/v1"

typedef enum {
    ACTION_NONE,
    ACTION_SERVER,
    ACTION_BASE_URI,
    ACTION_USER_AGENT,
} menu_repo_settings_action_t;

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(
            buffer, theme, !partial, !partial || icons, !partial,
            ((gui_element_icontext_t[]){{get_icon(ICON_CHAT), "LoRa settings"}}), 1,
            ((gui_element_icontext_t[]){
                {get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}, {get_icon(ICON_F4), "Reset defaults"}}),
            3, ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Edit setting"}}), 1);
    }

    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_settings_lora(void) {
    pax_buf_t*    buffer            = display_get_buffer();
    gui_theme_t*  theme             = get_theme();
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);

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
                            case BSP_INPUT_NAVIGATION_KEY_F4:
                                render(buffer, theme, &menu, position, false, true);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                menu_repo_settings_action_t action =
                                    (menu_repo_settings_action_t)menu_get_callback_args(&menu,
                                                                                        menu_get_position(&menu));
                            }
                                render(buffer, theme, &menu, position, false, true);
                                break;
                            default:
                                break;
                        }
                        default:
                            break;
                    }
                } break;
            }
        } else {
            render(buffer, theme, &menu, position, true, true);
        }
    }
}