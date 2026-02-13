#include "icons.h"
#include "bsp/power.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "fastopen.h"
#include "filesystem_utils.h"
#include "http_download.h"
#include "menu/message_dialog.h"
#include "pax_codecs.h"
#include "pax_gfx.h"
#include "wifi_connection.h"

static char const TAG[] = "icons";

#if defined(CONFIG_BSP_TARGET_KAMI)
#define ICON_WIDTH        32
#define ICON_HEIGHT       32
#define ICON_BUFFER_SIZE  (ICON_WIDTH * ICON_HEIGHT * 4)  // 32x32 pixels, 2 bits per pixel
#define ICON_COLOR_FORMAT PAX_BUF_2_PAL
#else
#define ICON_WIDTH        32
#define ICON_HEIGHT       32
#define ICON_BUFFER_SIZE  (ICON_WIDTH * ICON_HEIGHT * 4)  // 32x32 pixels, 4 bytes per pixel (ARGB8888)
#define ICON_COLOR_FORMAT PAX_BUF_32_8888ARGB
#endif

#define ICON_BASE_PATH "/int/icons"
#define ICON_EXT       ".png"

#if defined(CONFIG_BSP_TARGET_KAMI)
char icon_suffix[64] = "_f_r_black_16";
#else
char icon_suffix[64] = "_f_r_black_32";
#endif

#if defined(CONFIG_BSP_TARGET_KAMI)
static pax_col_t palette[] = {0xffffffff, 0xff000000, 0xffff0000};  // white, black, red
#endif

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

void load_icons(void) {
    for (int i = 0; i < ICON_LAST; i++) {
        char path[512] = {0};
        get_icon_path(i, path, sizeof(path));
        FILE* fd = fastopen(path, "rb");
        if (fd == NULL) {
            ESP_LOGE(TAG, "Failed to open icon file %s", path);
            icons_missing = true;
            continue;
        }
        void* buffer = heap_caps_calloc(1, ICON_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        if (buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for icon %s", path);
            fastclose(fd);
            icons_missing = true;
            continue;
        }
        pax_buf_init(&icons[i], buffer, ICON_WIDTH, ICON_HEIGHT, ICON_COLOR_FORMAT);
#if defined(CONFIG_BSP_TARGET_KAMI)
        icons[i].palette      = palette;
        icons[i].palette_size = sizeof(palette) / sizeof(pax_col_t);
#endif
        if (!pax_insert_png_fd(&icons[i], fd, 0, 0, 0)) {
            pax_buf_destroy(&icons[i]);
            free(buffer);
            memset(&icons[i], 0, sizeof(pax_buf_t));
            ESP_LOGE(TAG, "Failed to decode icon file %s", icon_paths[i]);
            icons_missing = true;
        }
        fastclose(fd);
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

extern bool wifi_stack_get_initialized(void);

static void download_callback(size_t download_position, size_t file_size, const char* status_text) {
    uint8_t        percentage      = 100 * download_position / file_size;
    static uint8_t last_percentage = 0;
    if (percentage == last_percentage) {
        return;  // No change, no need to update
    }
    last_percentage = percentage;
    char text[512];
    sprintf(text, "%s (%u%%)", status_text, percentage);
    busy_dialog(get_icon(ICON_DOWNLOADING), "Icon downloader", text, true);
};

void print_commands(void) {
    for (int i = 0; i < ICON_LAST; i++) {
        if (icon_paths[i] == NULL || icon_paths[i][0] == '/') {
            // Absolute path, skip
            continue;
        }
        char path[512] = {0};
        get_icon_path(i, path, sizeof(path));
        char url[512] = {0};
        if (i > ICON_F6) {
            snprintf(url, sizeof(url), "https://ota.tanmatsu.cloud/icons/%s%s" ICON_EXT, icon_paths[i], icon_suffix);
        } else {
            // For keyboard icons, use a different naming scheme
            snprintf(url, sizeof(url), "https://ota.tanmatsu.cloud/icons/%s" ICON_EXT, icon_paths[i]);
        }
        printf("wget -O '%s' '%s'\n", path, url);
    }
}

esp_err_t download_icons(bool delete_old_files) {
    print_commands();
    const char title[] = "Icon downloader";
    pax_buf_t* icon    = get_icon(ICON_DOWNLOADING);

    busy_dialog(icon, title, "Connecting to WiFi...", true);
    if (!wifi_stack_get_initialized()) {
        ESP_LOGE(TAG, "WiFi stack not initialized");
        message_dialog(icon, title, "WiFi stack not initialized", "Quit");
        return ESP_FAIL;
    }
    if (!wifi_connection_is_connected()) {
        if (wifi_connect_try_all() != ESP_OK) {
            ESP_LOGE(TAG, "Not connected to WiFi");
            message_dialog(icon, title, "Failed to connect to WiFi network", "Quit");
            return ESP_FAIL;
        }
    }

    if (delete_old_files) {
        busy_dialog(icon, title, "Removing old icon files...", true);
        fs_utils_remove("/int/icons");
        if (mkdir("/int/icons", 0777) != 0) {
            message_dialog(icon, title, "Failed to create icons directory", "Quit");
            return ESP_FAIL;
        }
    }

    for (int i = 0; i < ICON_LAST; i++) {
        if (icon_paths[i] == NULL || icon_paths[i][0] == '/') {
            // Absolute path, skip
            continue;
        }
        char path[512] = {0};
        get_icon_path(i, path, sizeof(path));
        remove(path);  // Remove currently present icon if it exists

        char url[512] = {0};
        if (i > ICON_F6) {
            snprintf(url, sizeof(url), "https://ota.tanmatsu.cloud/icons/%s%s" ICON_EXT, icon_paths[i], icon_suffix);
        } else {
            // For keyboard icons, use a different naming scheme
            snprintf(url, sizeof(url), "https://ota.tanmatsu.cloud/icons/%s" ICON_EXT, icon_paths[i]);
        }
        ESP_LOGI(TAG, "Downloading icon from '%s' and saving to '%s'...", url, path);

        char status[512] = {0};
        snprintf(status, sizeof(status), "Downloading icon '%s'...", icon_paths[i]);
        bool success = download_file(url, path, download_callback, status);

        if (!success) {
            snprintf(status, sizeof(status), "Failed to download icon '%s'", icon_paths[i]);
            ESP_LOGE(TAG, "%s", status);
            busy_dialog(icon, title, status, true);
        }
    }

    busy_dialog(NULL, "Icon download successful",
                "The icons have been downloaded successfully.\r\nDevice will restart now.", true);
    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}
