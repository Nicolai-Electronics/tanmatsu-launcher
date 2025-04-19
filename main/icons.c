#include "icons.h"
#include "esp_log.h"
#include "pax_codecs.h"

static char const TAG[] = "icons";

const char* icon_paths[] = {
    [ICON_ESC]           = "/int/icons/keyboard/esc.png",
    [ICON_F1]            = "/int/icons/keyboard/f1.png",
    [ICON_F2]            = "/int/icons/keyboard/f2.png",
    [ICON_F3]            = "/int/icons/keyboard/f3.png",
    [ICON_F4]            = "/int/icons/keyboard/f4.png",
    [ICON_F5]            = "/int/icons/keyboard/f5.png",
    [ICON_F6]            = "/int/icons/keyboard/f6.png",
    [ICON_EXTENSION]     = "/int/icons/menu/extension.png",
    [ICON_HOME]          = "/int/icons/menu/home.png",
    [ICON_APPS]          = "/int/icons/menu/apps.png",
    [ICON_REPOSITORY]    = "/int/icons/menu/repository.png",
    [ICON_TAG]           = "/int/icons/menu/tag.png",
    [ICON_DEV]           = "/int/icons/menu/dev.png",
    [ICON_SYSTEM_UPDATE] = "/int/icons/menu/system_update.png",
    [ICON_SETTINGS]      = "/int/icons/menu/settings.png",
    [ICON_WIFI]          = "/int/icons/menu/wifi.png",
    [ICON_INFO]          = "/int/icons/menu/info.png",
};

pax_buf_t icons[ICON_LAST];

void load_icons(void) {
    for (int i = 0; i < ICON_LAST; i++) {
        FILE* fd = fopen(icon_paths[i], "rb");
        if (fd == NULL) {
            ESP_LOGE(TAG, "Failed to open icon file %s", icon_paths[i]);
            continue;
        }
        if (!pax_decode_png_fd(&icons[i], fd, PAX_BUF_32_8888ARGB, 0)) {
            ESP_LOGE(TAG, "Failed to decode icon file %s", icon_paths[i]);
        }
        fclose(fd);
    }
}

pax_buf_t* get_icon(icon_t icon) {
    if (icon < 0 || icon >= ICON_LAST) {
        ESP_LOGE(TAG, "Invalid icon index %d", icon);
        return NULL;
    }
    return &icons[icon];
}
