#include <stdbool.h>
#include "common/display.h"
#include "common/theme.h"
#include "device_information.h"
#include "gui_menu.h"
#include "icons.h"
#include "menu/menu_helpers.h"
#include "nvs_settings.h"
#include "pax_types.h"

static menu_t* g_theme_menu = NULL;

static void update_menu(menu_t* menu) {
    theme_setting_t theme = THEME_BLACK;
    nvs_settings_get_theme(&theme);
    if ((int)theme > menu_get_length(menu)) {
        theme = THEME_BLACK;
        nvs_settings_set_theme(theme);
    }
    for (size_t i = 0; i < menu_get_length(menu); i++) {
        if ((theme_setting_t)menu_get_callback_args(menu, i) == theme) {
            menu_set_value(menu, i, "X");
        } else {
            menu_set_value(menu, i, "");
        }
    }
}

static bool on_action(void* action_arg, void* user_ctx) {
    (void)user_ctx;
    theme_setting_t theme_setting = (theme_setting_t)action_arg;
    nvs_settings_set_theme(theme_setting);
    update_menu(g_theme_menu);
    theme_initialize();
    unload_icons();
    load_icons();
    return false;
}

void menu_settings_theme(void) {
    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_value(&menu, "Light", "", NULL, (void*)THEME_BLACK, -1);
    menu_insert_item_value(&menu, "Dark", "", NULL, (void*)THEME_WHITE, -1);
    menu_insert_item_value(&menu, "Red", "", NULL, (void*)THEME_RED, -1);
    menu_insert_item_value(&menu, "Blue", "", NULL, (void*)THEME_BLUE, -1);
    menu_insert_item_value(&menu, "Purple", "", NULL, (void*)THEME_PURPLE, -1);
    menu_insert_item_value(&menu, "Orange", "", NULL, (void*)THEME_ORANGE, -1);
    menu_insert_item_value(&menu, "Green", "", NULL, (void*)THEME_GREEN, -1);
    menu_insert_item_value(&menu, "Yellow", "", NULL, (void*)THEME_YELLOW, -1);

    device_identity_t device_identity = {0};
    read_device_identity(&device_identity);
    if (device_identity.color == DEVICE_VARIANT_COLOR_COMMODORE) {
        menu_insert_item_value(&menu, "Commodore blue", "", NULL, (void*)THEME_C_BLUE, -1);
        menu_insert_item_value(&menu, "Commodore red", "", NULL, (void*)THEME_C_RED, -1);
    }

    update_menu(&menu);
    g_theme_menu = &menu;

    menu_run_list(&menu, ((gui_element_icontext_t[]){{get_icon(ICON_COLORS), "Theme"}}), 1, MENU_FOOTER_BACK,
                  MENU_FOOTER_NAV_SELECT, on_action, NULL, false);

    g_theme_menu = NULL;
}
