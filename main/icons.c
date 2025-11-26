#include "icons.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "pax_codecs.h"

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

#if defined(CONFIG_BSP_TARGET_KAMI)
static pax_col_t palette[] = {0xffffffff, 0xff000000, 0xffff0000};  // white, black, red
#endif

const char* icon_paths[] = {
    [ICON_ESC]                 = "/int/icons/keyboard/esc.png",
    [ICON_F1]                  = "/int/icons/keyboard/f1.png",
    [ICON_F2]                  = "/int/icons/keyboard/f2.png",
    [ICON_F3]                  = "/int/icons/keyboard/f3.png",
    [ICON_F4]                  = "/int/icons/keyboard/f4.png",
    [ICON_F5]                  = "/int/icons/keyboard/f5.png",
    [ICON_F6]                  = "/int/icons/keyboard/f6.png",
    [ICON_EXTENSION]           = "/int/icons/menu/extension.png",
    [ICON_HOME]                = "/int/icons/menu/home.png",
    [ICON_APPS]                = "/int/icons/menu/apps.png",
    [ICON_REPOSITORY]          = "/int/icons/menu/repository.png",
    [ICON_TAG]                 = "/int/icons/menu/tag.png",
    [ICON_DEV]                 = "/int/icons/menu/dev.png",
    [ICON_SYSTEM_UPDATE]       = "/int/icons/menu/system_update.png",
    [ICON_SETTINGS]            = "/int/icons/menu/settings.png",
    [ICON_INFO]                = "/int/icons/menu/info.png",
    [ICON_BATTERY_0]           = "/int/icons/menu/battery_0.png",
    [ICON_BATTERY_1]           = "/int/icons/menu/battery_1.png",
    [ICON_BATTERY_2]           = "/int/icons/menu/battery_2.png",
    [ICON_BATTERY_3]           = "/int/icons/menu/battery_3.png",
    [ICON_BATTERY_4]           = "/int/icons/menu/battery_4.png",
    [ICON_BATTERY_5]           = "/int/icons/menu/battery_5.png",
    [ICON_BATTERY_6]           = "/int/icons/menu/battery_6.png",
    [ICON_BATTERY_7]           = "/int/icons/menu/battery_7.png",
    [ICON_BATTERY_CHARGING]    = "/int/icons/menu/battery_charging.png",
    [ICON_BATTERY_ERROR]       = "/int/icons/menu/battery_error.png",
    [ICON_BATTERY_UNKNOWN]     = "/int/icons/menu/battery_unknown.png",
    [ICON_BATTERY_PLUS]        = "/int/icons/menu/battery_plus.png",
    [ICON_WIFI]                = "/int/icons/menu/wifi.png",
    [ICON_WIFI_OFF]            = "/int/icons/menu/wifi_off.png",
    [ICON_WIFI_0]              = "/int/icons/menu/wifi_0.png",
    [ICON_WIFI_1]              = "/int/icons/menu/wifi_1.png",
    [ICON_WIFI_2]              = "/int/icons/menu/wifi_2.png",
    [ICON_WIFI_3]              = "/int/icons/menu/wifi_3.png",
    [ICON_WIFI_4]              = "/int/icons/menu/wifi_4.png",
    [ICON_WIFI_ERROR]          = "/int/icons/menu/wifi_error.png",
    [ICON_WIFI_UNKNOWN]        = "/int/icons/menu/wifi_unknown.png",
    [ICON_USB]                 = "/int/icons/menu/usb.png",
    [ICON_SD]                  = "/int/icons/menu/sd.png",
    [ICON_SD_ERROR]            = "/int/icons/menu/sd_error.png",
    [ICON_HEADPHONES]          = "/int/icons/menu/headphones.png",
    [ICON_VOLUME_UP]           = "/int/icons/menu/volume_up.png",
    [ICON_VOLUME_DOWN]         = "/int/icons/menu/volume_down.png",
    [ICON_LOUDSPEAKER]         = "/int/icons/menu/loudspeaker.png",
    [ICON_BLUETOOTH]           = "/int/icons/menu/bluetooth.png",
    [ICON_BLUETOOTH_SEARCHING] = "/int/icons/menu/bluetooth_searching.png",
    [ICON_BLUETOOTH_CONNECTED] = "/int/icons/menu/bluetooth_connected.png",
    [ICON_BLUETOOTH_DISABLED]  = "/int/icons/menu/bluetooth_disabled.png",
    [ICON_BLUETOOTH_SETTINGS]  = "/int/icons/menu/bluetooth_settings.png",
    [ICON_RELEASE_ALERT]       = "/int/icons/menu/release_alert.png",
    [ICON_DOWNLOADING]         = "/int/icons/menu/downloading.png",
    [ICON_HELP]                = "/int/icons/menu/help.png",
    [ICON_DEVICE_INFO]         = "/int/icons/menu/device_info.png",
    [ICON_CLOCK]               = "/int/icons/menu/clock.png",
    [ICON_LANGUAGE]            = "/int/icons/menu/language.png",
    [ICON_GLOBE]               = "/int/icons/menu/globe.png",
    [ICON_GLOBE_LOCATION]      = "/int/icons/menu/globe_location.png",
    [ICON_APP]                 = "/int/icons/menu/app.png",
    [ICON_ERROR]               = "/int/icons/menu/error.png",
    [ICON_BRIGHTNESS]          = "/int/icons/menu/brightness.png",  // Missing!
};

pax_buf_t icons[ICON_LAST] = {0};

void load_icons(void) {
#if !defined(CONFIG_BSP_TARGET_MCH2022)
    for (int i = 0; i < ICON_LAST; i++) {
        FILE* fd = fopen(icon_paths[i], "rb");
        if (fd == NULL) {
            ESP_LOGE(TAG, "Failed to open icon file %s", icon_paths[i]);
            continue;
        }
        void* buffer = heap_caps_calloc(1, ICON_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        if (buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for icon %s", icon_paths[i]);
            fclose(fd);
            continue;
        }
        pax_buf_init(&icons[i], buffer, ICON_WIDTH, ICON_HEIGHT, ICON_COLOR_FORMAT);
#if defined(CONFIG_BSP_TARGET_KAMI)
        icons[i].palette      = palette;
        icons[i].palette_size = sizeof(palette) / sizeof(pax_col_t);
#endif
        if (!pax_insert_png_fd(&icons[i], fd, 0, 0, 0)) {
            ESP_LOGE(TAG, "Failed to decode icon file %s", icon_paths[i]);
        }
        fclose(fd);
    }
#endif
}

pax_buf_t* get_icon(icon_t icon) {
    if (icon < 0 || icon >= ICON_LAST) {
        ESP_LOGE(TAG, "Invalid icon index %d", icon);
        return NULL;
    }
    return &icons[icon];
}
