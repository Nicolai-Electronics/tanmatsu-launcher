#include "information.h"
#include "common/display.h"
#include "common/theme.h"
#include "gui_menu.h"
#include "icons.h"
#include "menu/about.h"
#include "menu/menu_helpers.h"
#include "menu/menu_power_information.h"
#include "menu_device_information.h"
#include "pax_types.h"

typedef enum {
    ACTION_NONE,
    ACTION_DEVICE_INFO,
    ACTION_ABOUT,
    ACTION_POWER_INFORMATION,
} menu_home_action_t;

static bool on_action(void* action_arg, void* user_ctx) {
    (void)user_ctx;
    pax_buf_t*   fb    = display_get_buffer();
    gui_theme_t* theme = get_theme();
    switch ((menu_home_action_t)action_arg) {
        case ACTION_DEVICE_INFO:
            menu_device_information(fb, theme);
            break;
        case ACTION_ABOUT:
            menu_about();
            break;
        case ACTION_POWER_INFORMATION:
            menu_power_information();
            break;
        default:
            break;
    }
    return false;
}

void menu_information(void) {
    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_icon(&menu, "Device information", NULL, (void*)ACTION_DEVICE_INFO, -1, get_icon(ICON_INFO));
    menu_insert_item_icon(&menu, "Power information", NULL, (void*)ACTION_POWER_INFORMATION, -1,
                          get_icon(ICON_BATTERY_UNKNOWN));
    menu_insert_item_icon(&menu, "About", NULL, (void*)ACTION_ABOUT, -1, get_icon(ICON_INFO));

    menu_run_list(&menu, ((gui_element_icontext_t[]){{get_icon(ICON_INFO), "Information"}}), 1, MENU_FOOTER_BACK,
                  MENU_FOOTER_NAV_SELECT, on_action, NULL, true);
}
