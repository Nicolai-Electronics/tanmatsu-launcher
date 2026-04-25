#include "tools.h"
#include "bsp/display.h"
#include "common/display.h"
#include "firmware_update.h"
#include "gui_menu.h"
#include "icons.h"
#include "menu/menu_helpers.h"
#include "menu/message_dialog.h"
#include "menu_filebrowser.h"
#include "menu_hardware_test.h"
#include "pax_types.h"
#include "radio_ota.h"
#include "radio_update.h"

typedef enum {
    ACTION_NONE,
    ACTION_FIRMWARE_UPDATE_STABLE,
    ACTION_FIRMWARE_UPDATE_STAGING,
    ACTION_FIRMWARE_UPDATE_EXPERIMENTAL,
    ACTION_RADIO_UPDATE,
    ACTION_RADIO_OTA,
    ACTION_HARDWARE_TEST,
    ACTION_DOWNLOAD_ICONS,
} menu_home_action_t;

static void radio_update_v2(void) {
    char filename[260] = "";
    bool result =
        menu_filebrowser("/sd", (const char*[]){"trf"}, 1, filename, sizeof(filename), "Select radio firmware");
    if (result) {
        radio_install(filename);
    }
}

static bool on_action(void* action_arg, void* user_ctx) {
    (void)user_ctx;
    switch ((menu_home_action_t)action_arg) {
        case ACTION_FIRMWARE_UPDATE_STABLE:
            ota_update_stable();
            break;
        case ACTION_FIRMWARE_UPDATE_STAGING:
            ota_update_staging();
            break;
        case ACTION_FIRMWARE_UPDATE_EXPERIMENTAL:
            ota_update_experimental();
            break;
        case ACTION_RADIO_UPDATE:
            radio_update_v2();
            break;
        case ACTION_RADIO_OTA:
            radio_ota_update();
            break;
        case ACTION_HARDWARE_TEST:
            menu_hardware_test();
            break;
        case ACTION_DOWNLOAD_ICONS:
            download_icons(false);
            break;
        default:
            break;
    }
    return false;
}

void menu_tools(void) {
    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_icon(&menu, "Start firmware update", NULL, (void*)ACTION_FIRMWARE_UPDATE_STABLE, -1,
                          get_icon(ICON_SYSTEM_UPDATE));
    menu_insert_item_icon(&menu, "Start firmware update (staging)", NULL, (void*)ACTION_FIRMWARE_UPDATE_STAGING, -1,
                          get_icon(ICON_SYSTEM_UPDATE));
    menu_insert_item_icon(&menu, "Start firmware update (experimental)", NULL,
                          (void*)ACTION_FIRMWARE_UPDATE_EXPERIMENTAL, -1, get_icon(ICON_SYSTEM_UPDATE));
#if 0 && (defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL))
    menu_insert_item_icon(&menu, "Install radio fiFirmware updatermware from SD card", NULL, (void*)ACTION_RADIO_UPDATE, -1,
                          get_icon(ICON_RELEASE_ALERT));
#endif
    menu_insert_item_icon(&menu, "Hardware test", NULL, (void*)ACTION_HARDWARE_TEST, -1, get_icon(ICON_BUG_REPORT));
    menu_insert_item_icon(&menu, "Force icon update", NULL, (void*)ACTION_DOWNLOAD_ICONS, -1, get_icon(ICON_COLORS));
    menu_insert_item_icon(&menu, "Force radio update", NULL, (void*)ACTION_RADIO_OTA, -1, get_icon(ICON_SYSTEM_UPDATE));

    menu_run_list(&menu, ((gui_element_icontext_t[]){{get_icon(ICON_EXTENSION), "Tools"}}), 1, MENU_FOOTER_BACK,
                  MENU_FOOTER_NAV_SELECT, on_action, NULL, true);
}
