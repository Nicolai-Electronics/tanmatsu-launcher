#include "settings.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "common/device.h"
#include "common/display.h"
#include "common/theme.h"
#include "firmware_update.h"
#include "freertos/idf_additions.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/about.h"
#include "menu/information.h"
#include "menu/menu_power_information.h"
#include "menu/message_dialog.h"
#include "menu/owner.h"
#include "menu/tools.h"
#include "menu/wifi.h"
#include "menu_brightness.h"
#include "menu_filebrowser.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "settings_clock.h"
#include "settings_lora.h"
#include "settings_repository.h"
#include "settings_theme.h"

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
    ACTION_OWNER,
    ACTION_BRIGHTNESS,
    ACTION_WIFI,
    ACTION_CLOCK,
    ACTION_REPOSITORY,
    ACTION_LORA,
    ACTION_FIRMWARE_UPDATE,
    ACTION_TOOLS,
    ACTION_INFO,
    ACTION_THEME,
} menu_home_action_t;

static void execute_action(menu_home_action_t action) {
    switch (action) {
        case ACTION_OWNER:
            menu_settings_owner();
            break;
        case ACTION_BRIGHTNESS:
            menu_settings_brightness();
            break;
        case ACTION_WIFI:
            menu_settings_wifi();
            break;
        case ACTION_CLOCK:
            menu_settings_clock();
            break;
        case ACTION_REPOSITORY:
            menu_settings_repository();
            break;
        case ACTION_LORA:
            menu_settings_lora();
            break;
        case ACTION_FIRMWARE_UPDATE:
            menu_firmware_update();
            break;
        case ACTION_TOOLS:
            menu_tools();
            break;
        case ACTION_INFO:
            menu_information();
            break;
        case ACTION_THEME:
            menu_settings_theme();
            break;
        default:
            break;
    }
}

static void render(menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_SETTINGS), "Settings"}}), 1,
                                     FOOTER_LEFT, FOOTER_RIGHT);
    }
    menu_render_grid(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_settings(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_icon(&menu, "Owner", NULL, (void*)ACTION_OWNER, -1, get_icon(ICON_BADGE));
    uint8_t dummy;
    if (bsp_display_get_backlight_brightness(&dummy) == ESP_OK) {
        menu_insert_item_icon(&menu, "Brightness", NULL, (void*)ACTION_BRIGHTNESS, -1, get_icon(ICON_BRIGHTNESS));
    }
    if (!display_is_epaper()) {
        menu_insert_item_icon(&menu, "Theme", NULL, (void*)ACTION_THEME, -1, get_icon(ICON_COLORS));
    }
    menu_insert_item_icon(&menu, "WiFi", NULL, (void*)ACTION_WIFI, -1, get_icon(ICON_WIFI));
    menu_insert_item_icon(&menu, "Clock", NULL, (void*)ACTION_CLOCK, -1, get_icon(ICON_CLOCK));
    menu_insert_item_icon(&menu, "Repository", NULL, (void*)ACTION_REPOSITORY, -1, get_icon(ICON_STOREFRONT));
    if (device_has_lora()) {
        menu_insert_item_icon(&menu, "LoRa radio", NULL, (void*)ACTION_LORA, -1, get_icon(ICON_CHAT));
    }
    menu_insert_item_icon(&menu, "Tools", NULL, (void*)ACTION_TOOLS, -1, get_icon(ICON_SETTINGS));
    menu_insert_item_icon(&menu, "Info", NULL, (void*)ACTION_INFO, -1, get_icon(ICON_INFO));

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
                                execute_action((menu_home_action_t)arg);
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
