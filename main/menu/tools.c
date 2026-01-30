#include "tools.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "firmware_update.h"
#include "freertos/idf_additions.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/about.h"
#include "menu/menu_power_information.h"
#include "menu/message_dialog.h"
#include "menu/wifi.h"
#include "menu_brightness.h"
#include "menu_device_information.h"
#include "menu_filebrowser.h"
#include "menu_hardware_test.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "radio_update.h"
#include "settings_clock.h"
#include "settings_repository.h"

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
    ACTION_FIRMWARE_UPDATE,
    ACTION_RADIO_UPDATE,
    ACTION_HARDWARE_TEST,
} menu_home_action_t;

static void radio_update_v2(void) {
    char filename[260] = "";
    bool result =
        menu_filebrowser("/sd", (const char*[]){"trf"}, 1, filename, sizeof(filename), "Select radio firmware");
    if (result) {
        radio_install(filename);
    }
}

static void execute_action(pax_buf_t* fb, menu_home_action_t action, gui_theme_t* theme) {
    switch (action) {
        case ACTION_FIRMWARE_UPDATE:
            menu_firmware_update();
            break;
        case ACTION_RADIO_UPDATE:
            radio_update_v2();
            break;
        case ACTION_HARDWARE_TEST:
            menu_hardware_test();
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
                                     ((gui_element_icontext_t[]){{get_icon(ICON_EXTENSION), "Tools"}}), 1, FOOTER_LEFT,
                                     FOOTER_RIGHT);
    }
    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_tools(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_icon(&menu, "Firmware update", NULL, (void*)ACTION_FIRMWARE_UPDATE, -1,
                          get_icon(ICON_SYSTEM_UPDATE));
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
    menu_insert_item_icon(&menu, "Install radio firmware from SD card", NULL, (void*)ACTION_RADIO_UPDATE, -1,
                          get_icon(ICON_RELEASE_ALERT));
#endif
    menu_insert_item_icon(&menu, "Hardware test", NULL, (void*)ACTION_HARDWARE_TEST, -1, get_icon(ICON_DEV));

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
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(&menu);
                                render(&menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
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
