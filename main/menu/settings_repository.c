#include "settings_repository.h"
#include <string.h>
#include "bsp/input.h"
#include "common/display.h"
#include "device_settings.h"
#include "freertos/idf_additions.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/textedit.h"
#include "message_dialog.h"
#include "pax_gfx.h"
#include "pax_types.h"

#define REPO_SERVER_MAX_LEN   128
#define REPO_BASE_URI_MAX_LEN 64

#define DEFAULT_REPO_SERVER   "https://apps.tanmatsu.cloud"
#define DEFAULT_REPO_BASE_URI "/v1"

typedef enum {
    ACTION_NONE,
    ACTION_SERVER,
    ACTION_BASE_URI,
    ACTION_RESET_DEFAULTS,
} menu_repo_settings_action_t;

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_REPOSITORY), "Repository settings"}}), 1,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},
                                                                 {get_icon(ICON_F1), "Back"}}),
                                     2, ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Edit setting"}}), 1);
    }

    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

static void menu_populate(menu_t* menu) {
    char server[REPO_SERVER_MAX_LEN]     = {0};
    char base_uri[REPO_BASE_URI_MAX_LEN] = {0};

    device_settings_get_repo_server(server, sizeof(server));
    device_settings_get_repo_base_uri(base_uri, sizeof(base_uri));

    size_t previous_position = menu_get_position(menu);
    while (menu_get_length(menu) > 0) {
        menu_remove_item(menu, 0);
    }

    menu_insert_item_value(menu, "Server", server, NULL, (void*)ACTION_SERVER, -1);
    menu_insert_item_value(menu, "Base URI", base_uri, NULL, (void*)ACTION_BASE_URI, -1);
    menu_insert_item(menu, "Reset to defaults", NULL, (void*)ACTION_RESET_DEFAULTS, -1);

    if (previous_position >= menu_get_length(menu)) {
        previous_position = menu_get_length(menu) - 1;
    }
    menu_set_position(menu, previous_position);
}

static void edit_server(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu) {
    char server[REPO_SERVER_MAX_LEN] = {0};
    bool accepted                    = false;

    device_settings_get_repo_server(server, sizeof(server));
    menu_textedit(buffer, theme, "Server", server, sizeof(server), false, &accepted);
    if (accepted) {
        device_settings_set_repo_server(server);
        menu_set_value(menu, 0, server);
    }
}

static void edit_base_uri(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu) {
    char base_uri[REPO_BASE_URI_MAX_LEN] = {0};
    bool accepted                        = false;

    device_settings_get_repo_base_uri(base_uri, sizeof(base_uri));
    menu_textedit(buffer, theme, "Base URI", base_uri, sizeof(base_uri), false, &accepted);
    if (accepted) {
        device_settings_set_repo_base_uri(base_uri);
        menu_set_value(menu, 1, base_uri);
    }
}

static void reset_defaults(menu_t* menu) {
    device_settings_set_repo_server(DEFAULT_REPO_SERVER);
    device_settings_set_repo_base_uri(DEFAULT_REPO_BASE_URI);
    menu_set_value(menu, 0, DEFAULT_REPO_SERVER);
    menu_set_value(menu, 1, DEFAULT_REPO_BASE_URI);
}

void menu_settings_repository(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_populate(&menu);

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
                                menu_repo_settings_action_t action =
                                    (menu_repo_settings_action_t)menu_get_callback_args(&menu, menu_get_position(&menu));
                                switch (action) {
                                    case ACTION_SERVER:
                                        edit_server(buffer, theme, &menu);
                                        break;
                                    case ACTION_BASE_URI:
                                        edit_base_uri(buffer, theme, &menu);
                                        break;
                                    case ACTION_RESET_DEFAULTS:
                                        reset_defaults(&menu);
                                        break;
                                    default:
                                        break;
                                }
                                render(buffer, theme, &menu, position, false, true);
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
            render(buffer, theme, &menu, position, true, true);
        }
    }
}
