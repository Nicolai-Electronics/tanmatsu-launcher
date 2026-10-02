#include "tools.h"
#include <stdio.h>
#include "bsp/display.h"
#include "bsp/storage.h"
#include "common/display.h"
#include "esp_err.h"
#include "esp_log.h"
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

static const char* TAG = "tools";

typedef enum {
    ACTION_NONE,
    ACTION_FIRMWARE_UPDATE_STABLE,
    ACTION_FIRMWARE_UPDATE_STAGING,
    ACTION_FIRMWARE_UPDATE_EXPERIMENTAL,
    ACTION_RADIO_UPDATE,
    ACTION_RADIO_OTA,
    ACTION_HARDWARE_TEST,
    ACTION_DOWNLOAD_ICONS,
    ACTION_FORMAT_SD_CARD,
} menu_home_action_t;

static void radio_update_v2(void) {
    char filename[260] = "";
    bool result =
        menu_filebrowser("/sd", (const char*[]){"trf"}, 1, filename, sizeof(filename), "Select radio firmware");
    if (result) {
        radio_install(filename);
    }
}

static esp_err_t format_sd_card(void) {
    bsp_storage_status_t status = bsp_storage_get_status(BSP_STORAGE_TYPE_SDCARD);
    if (status == BSP_STORAGE_STATUS_MOUNTED) {
        ESP_LOGI(TAG, "SD card is mounted, using bsp_storage_format to format");
        return bsp_storage_format(BSP_STORAGE_TYPE_SDCARD);
    } else if (status == BSP_STORAGE_STATUS_ERROR) {
        ESP_LOGI(TAG, "SD card failed to mount, mounting with auto format enabled");
        return bsp_storage_mount_advanced(BSP_STORAGE_TYPE_SDCARD, "/sd", 10, true);
    } else {
        ESP_LOGE(TAG, "No SD card available");
        return ESP_ERR_INVALID_STATE;
    }
}

static void format_sd_card_ui(void) {
    bsp_storage_status_t status = bsp_storage_get_status(BSP_STORAGE_TYPE_SDCARD);
    if (status == BSP_STORAGE_STATUS_MOUNTED || status == BSP_STORAGE_STATUS_ERROR) {
        if (adv_dialog_yes_no(get_icon(ICON_SD_CARD_ALERT), "Format SD card",
                              "Formatting the SD card will erase all of its contents. This cannot be undone.") !=
            MSG_DIALOG_RETURN_OK) {
            return;
        }
        busy_dialog(get_icon(ICON_SD_CARD), "Format SD card", "Formatting SD card, please wait...", true);
        esp_err_t res = format_sd_card();
        if (res == ESP_OK) {
            adv_dialog_ok(get_icon(ICON_SD_CARD), "Format SD card", "The SD card has been formatted successfully.");
        } else {
            char message[128];
            snprintf(message, sizeof(message), "Failed to format the SD card: %s", esp_err_to_name(res));
            adv_dialog_ok(get_icon(ICON_ERROR), "Format SD card", message);
        }
    } else {
        adv_dialog_ok(get_icon(ICON_SD_CARD_ALERT), "Format SD card", "No SD card is inserted.");
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
        case ACTION_FORMAT_SD_CARD:
            format_sd_card_ui();
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
    menu_insert_item_icon(&menu, "Hardware tests", NULL, (void*)ACTION_HARDWARE_TEST, -1, get_icon(ICON_BUG_REPORT));
    menu_insert_item_icon(&menu, "Format SD card", NULL, (void*)ACTION_FORMAT_SD_CARD, -1, get_icon(ICON_SD_CARD));
    /*
    menu_insert_item_icon(&menu, "Force icon update (only needed when icons are missing)", NULL,
                          (void*)ACTION_DOWNLOAD_ICONS, -1, get_icon(ICON_COLORS));
    menu_insert_item_icon(&menu, "Force radio update (only needed when radio firmware is mismatched)", NULL,
                          (void*)ACTION_RADIO_OTA, -1, get_icon(ICON_SYSTEM_UPDATE));
    */

    menu_run_list(&menu, ((gui_element_icontext_t[]){{get_icon(ICON_EXTENSION), "Tools"}}), 1, MENU_FOOTER_BACK,
                  MENU_FOOTER_NAV_SELECT, on_action, NULL, true);
}
