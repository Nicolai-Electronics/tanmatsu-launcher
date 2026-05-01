#include "settings.h"
#include "bsp/display.h"
#include "common/device.h"
#include "common/display.h"
#include "firmware_update.h"
#include "gui_menu.h"
#include "icons.h"
#include "menu/information.h"
#include "menu/menu_helpers.h"
#include "menu/owner.h"
#include "menu/tools.h"
#include "menu/wifi.h"
#include "menu_brightness.h"
#include "pax_types.h"
#include "settings_clock.h"
#include "settings_lora.h"
#include "settings_repository.h"
#include "settings_theme.h"

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

static bool on_action(void* action_arg, void* user_ctx) {
    (void)user_ctx;
    switch ((menu_home_action_t)action_arg) {
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
            ota_update_stable();
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
    return false;
}

void menu_settings(void) {
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
    menu_insert_item_icon(&menu, "Update firmware", NULL, (void*)ACTION_FIRMWARE_UPDATE, -1,
                          get_icon(ICON_SYSTEM_UPDATE));

    menu_run_grid(&menu, ((gui_element_icontext_t[]){{get_icon(ICON_SETTINGS), "Settings"}}), 1, MENU_FOOTER_BACK,
                  MENU_FOOTER_NAV_SELECT, on_action, NULL, true);
}
