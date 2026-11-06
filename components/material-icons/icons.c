#include "icons.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs_settings.h"
#include "pax_codecs.h"
#include "pax_gfx.h"
#include "sdkconfig.h"

static char const TAG[] = "icons";

#if defined(CONFIG_BSP_TARGET_KAMI)
#define ICON_WIDTH        16
#define ICON_HEIGHT       16
#define ICON_BUFFER_SIZE  (ICON_WIDTH * ICON_HEIGHT * 4)  // 4 bytes per pixel (ARGB8888)
#define ICON_COLOR_FORMAT PAX_BUF_32_8888ARGB
#else
#define ICON_WIDTH        32
#define ICON_HEIGHT       32
#define ICON_BUFFER_SIZE  (ICON_WIDTH * ICON_HEIGHT * 4)  // 4 bytes per pixel (ARGB8888)
#define ICON_COLOR_FORMAT PAX_BUF_32_8888ARGB
#endif

#define ICON_BASE_PATH "/int/icons"
#define ICON_EXT       ".png"

static const char* icon_paths[] = {
    // Keyboard keys (these are custom icons)
    [ICON_ESC] = "esc",
    [ICON_F1]  = "f1",
    [ICON_F2]  = "f2",
    [ICON_F3]  = "f3",
    [ICON_F4]  = "f4",
    [ICON_F5]  = "f5",
    [ICON_F6]  = "f6",

    // Battery
    [ICON_BATTERY_0]       = "battery_android_0",
    [ICON_BATTERY_1]       = "battery_android_1",
    [ICON_BATTERY_2]       = "battery_android_2",
    [ICON_BATTERY_3]       = "battery_android_3",
    [ICON_BATTERY_4]       = "battery_android_4",
    [ICON_BATTERY_5]       = "battery_android_5",
    [ICON_BATTERY_6]       = "battery_android_6",
    [ICON_BATTERY_FULL]    = "battery_android_full",
    [ICON_BATTERY_BOLT]    = "battery_android_bolt",
    [ICON_BATTERY_ALERT]   = "battery_android_alert",
    [ICON_BATTERY_UNKNOWN] = "battery_android_question",

    // WiFi
    [ICON_WIFI]         = "wifi",
    [ICON_WIFI_OFF]     = "signal_wifi_off",
    [ICON_WIFI_0_BAR]   = "signal_wifi_0_bar",
    [ICON_WIFI_1_BAR]   = "network_wifi_1_bar",
    [ICON_WIFI_2_BAR]   = "network_wifi_2_bar",
    [ICON_WIFI_3_BAR]   = "network_wifi_3_bar",
    [ICON_WIFI_4_BAR]   = "signal_wifi_4_bar",
    [ICON_WIFI_ERROR]   = "signal_wifi_off",
    [ICON_WIFI_UNKNOWN] = "signal_wifi_statusbar_not_connected",

    [ICON_EXTENSION]           = "extension",
    [ICON_HOME]                = "home",
    [ICON_APPS]                = "apps",
    [ICON_STOREFRONT]          = "storefront",
    [ICON_BADGE]               = "badge",
    [ICON_BUG_REPORT]          = "bug_report",
    [ICON_SYSTEM_UPDATE]       = "system_update_alt",
    [ICON_SETTINGS]            = "settings",
    [ICON_INFO]                = "info",
    [ICON_USB]                 = "usb",
    [ICON_SD_CARD]             = "sd_card",
    [ICON_SD_CARD_ALERT]       = "sd_card_alert",
    [ICON_HEADPHONES]          = "headphones",
    [ICON_VOLUME_UP]           = "volume_up",
    [ICON_VOLUME_DOWN]         = "volume_down",
    [ICON_SPEAKER]             = "speaker",
    [ICON_BLUETOOTH]           = "bluetooth",
    [ICON_BLUETOOTH_SEARCHING] = "bluetooth_searching",
    [ICON_BLUETOOTH_CONNECTED] = "bluetooth_connected",
    [ICON_BLUETOOTH_DISABLED]  = "bluetooth_disabled",
    [ICON_RELEASE_ALERT]       = "new_releases",
    [ICON_DOWNLOADING]         = "downloading",
    [ICON_HELP]                = "help",
    [ICON_CLOCK]               = "schedule",
    [ICON_LANGUAGE]            = "language",
    [ICON_GLOBE]               = "globe",
    [ICON_GLOBE_LOCATION]      = "globe_location_pin",
    [ICON_APP]                 = "exit_to_app",
    [ICON_ERROR]               = "error",
    [ICON_BRIGHTNESS]          = "brightness_5",
    [ICON_CHAT]                = "chat",
    [ICON_CONTACT]             = "person",
    [ICON_DATABASE]            = "storage",
    [ICON_FILE]                = "description",
    [ICON_FOLDER]              = "folder",
    [ICON_IMAGE]               = "image",
    [ICON_LOCATION_OFF]        = "location_off",
    [ICON_LOCATION_ON]         = "location_on",
    [ICON_MAIL]                = "mail",
    [ICON_MAP]                 = "map",
    [ICON_COLORS]              = "colors",
    [ICON_SEND]                = "send",
    [ICON_WORKSPACES]          = "workspaces",
    [ICON_MENU]                = "menu",
    [ICON_FAVORITE]            = "favorite",
};

pax_buf_t EXT_RAM_BSS_ATTR icons[ICON_LAST] = {0};
bool                       icons_missing    = false;

void get_icon_path(icon_t icon, char* out_path, size_t max_path_len) {
    const char* icon_path = icon_paths[icon];
    if (icon_path == NULL) {
        ESP_LOGE(TAG, "Icon path is NULL for %u", icon);
        memset(out_path, 0, max_path_len);
    } else if (icon_path[0] == '/') {
        // Absolute path, use as is
        snprintf(out_path, max_path_len, "%s", icon_path);
    } else {
        // Relative path, prepend base path
        snprintf(out_path, max_path_len, ICON_BASE_PATH "/%s" ICON_EXT, icon_path);
    }
}

const char* get_icon_name(icon_t icon) {
    if (icon < 0 || icon >= ICON_LAST) {
        return NULL;
    }
    const char* icon_path = icon_paths[icon];
    if (icon_path == NULL || icon_path[0] == '/') {
        return NULL;
    }
    return icon_path;
}

void load_icons(pax_col_t recolor_foreground) {
    theme_setting_t theme_setting = THEME_BLACK;
    nvs_settings_get_theme(&theme_setting);

    for (int i = 0; i < ICON_LAST; i++) {
        char path[512] = {0};
        get_icon_path(i, path, sizeof(path));
        FILE* fd = fopen(path, "rb");
        if (fd == NULL) {
            ESP_LOGE(TAG, "Failed to open icon file %s", path);
            icons_missing = true;
            continue;
        }
        void* buffer = heap_caps_calloc(1, ICON_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        if (buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for icon %s", path);
            fclose(fd);
            icons_missing = true;
            continue;
        }
        pax_buf_init(&icons[i], buffer, ICON_WIDTH, ICON_HEIGHT, ICON_COLOR_FORMAT);
        if (!pax_insert_png_fd(&icons[i], fd, 0, 0, 0)) {
            pax_buf_destroy(&icons[i]);
            free(buffer);
            memset(&icons[i], 0, sizeof(pax_buf_t));
            ESP_LOGE(TAG, "Failed to decode icon file %s", icon_paths[i]);
            icons_missing = true;
        }
        fclose(fd);

        if (theme_setting != THEME_BLACK) {
            if (i < ICON_F1 || i >= ICON_BATTERY_0) {
                for (size_t y = 0; y < ICON_HEIGHT; y++) {
                    for (size_t x = 0; x < ICON_WIDTH; x++) {
                        pax_col_t col       = pax_get_pixel(&icons[i], x, y);
                        col                ^= 0x00FFFFFF;  // Invert color
                        uint8_t brightness  = ((col & 0xFF) + (col >> 8 & 0xFF) + (col >> 16 & 0xFF)) / 3;
                        col                &= 0xFF000000;  // Remove colors
                        uint8_t r           = (((recolor_foreground >> 16) & 0xFF) * brightness) / 255;
                        uint8_t g           = (((recolor_foreground >> 8) & 0xFF) * brightness) / 255;
                        uint8_t b           = (((recolor_foreground >> 0) & 0xFF) * brightness) / 255;
                        col                |= (r << 16) | (g << 8) | b;
                        pax_set_pixel(&icons[i], col, x, y);
                    }
                }
            }
        }
    }
}

void unload_icons(void) {
    for (int i = 0; i < ICON_LAST; i++) {
        if (pax_buf_get_width(&icons[i]) == 0 || pax_buf_get_height(&icons[i]) == 0) {
            continue;  // Not loaded, skip
        }
        uint8_t* buffer = (uint8_t*)pax_buf_get_pixels(&icons[i]);
        pax_buf_destroy(&icons[i]);
        free(buffer);
        memset(&icons[i], 0, sizeof(pax_buf_t));
    }
}

pax_buf_t* get_icon(icon_t icon) {
    if (icon < 0 || icon >= ICON_LAST) {
        ESP_LOGE(TAG, "Invalid icon index %d", icon);
        return NULL;
    }
    return &icons[icon];
}

bool get_icons_missing(void) {
    return icons_missing;
}
