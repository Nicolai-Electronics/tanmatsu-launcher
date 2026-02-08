#include "chat.h"
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "freertos/idf_additions.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "messages.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Select"}}), 1
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | 🅱 Back 🅰 Select"}}), 1
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#endif

typedef enum {
    ACTION_NONE,
    ACTION_FIXME,
    ACTION_MESSAGES
} menu_home_action_t;

static void execute_action(pax_buf_t* fb, menu_home_action_t action, gui_theme_t* theme) {
    switch (action) {
        case ACTION_MESSAGES:
            menu_messages();
            break;
        case ACTION_FIXME:
        default:
            break;
    }
}

static void render(menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_SETTINGS), "Chat"}}), 1, FOOTER_LEFT,
                                     FOOTER_RIGHT);
    }
    menu_render_grid(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_chat(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_icon(&menu, "Contacts", NULL, (void*)ACTION_FIXME, -1, get_icon(ICON_BADGE));
    menu_insert_item_icon(&menu, "Channels", NULL, (void*)ACTION_MESSAGES, -1, get_icon(ICON_APPS));
    menu_insert_item_icon(&menu, "Map", NULL, (void*)ACTION_FIXME, -1, get_icon(ICON_GLOBE_LOCATION));
    menu_insert_item_icon(&menu, "Configuration", NULL, (void*)ACTION_FIXME, -1, get_icon(ICON_SETTINGS));
    menu_insert_item_icon(&menu, "Tools", NULL, (void*)ACTION_FIXME, -1, get_icon(ICON_EXTENSION));

    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    render(&menu, position, false, true);
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
                            case BSP_INPUT_NAVIGATION_KEY_HOME:
                                menu_free(&menu);
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_LEFT:
                                menu_navigate_previous(&menu);
                                render(&menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                                menu_navigate_next(&menu);
                                render(&menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous_row(&menu, theme);
                                render(&menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next_row(&menu, theme);
                                render(&menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                execute_action(buffer, (menu_home_action_t)arg, theme);
                                render(&menu, position, false, true);
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
            render(&menu, position, true, true);
        }
    }
}
