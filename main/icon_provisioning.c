#include "icon_provisioning.h"
#include "bsp/power.h"
#include "esp_log.h"
#include "fastopen.h"
#include "filesystem_utils.h"
#include "http_download.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "wifi_connection.h"

static char const TAG[] = "icon_provisioning";

#define ICON_EXT ".png"

#if defined(CONFIG_BSP_TARGET_KAMI)
static char const icon_suffix[] = "_f_r_black_16";
#else
static char const icon_suffix[] = "_f_r_black_32";
#endif

extern bool wifi_stack_get_initialized(void);

static void download_callback(size_t download_position, size_t file_size, const char* status_text) {
    if (file_size == 0) {
        ESP_LOGD(TAG, "Download callback called with file_size == 0");
        return;
    }
    uint8_t percentage = 100 * download_position / file_size;
    ESP_LOGI(TAG, "%s (%u%%)", status_text, percentage);
};

static void build_icon_url(icon_t icon, const char* name, char* url, size_t url_len) {
    if (icon > ICON_F6) {
        snprintf(url, url_len, "https://ota.tanmatsu.cloud/icons/%s%s" ICON_EXT, name, icon_suffix);
    } else {
        // For keyboard icons, use a different naming scheme
        snprintf(url, url_len, "https://ota.tanmatsu.cloud/icons/%s" ICON_EXT, name);
    }
}

static void print_commands(void) {
    for (int i = 0; i < ICON_LAST; i++) {
        const char* name = get_icon_name(i);
        if (name == NULL) {
            continue;
        }
        char path[512] = {0};
        get_icon_path(i, path, sizeof(path));
        char url[512] = {0};
        build_icon_url(i, name, url, sizeof(url));
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

    http_session_t session = http_session_begin("https://ota.tanmatsu.cloud/icons/");
    if (session == NULL) {
        message_dialog(icon, title, "Failed to create HTTP session", "Quit");
        return ESP_FAIL;
    }

    for (int i = 0; i < ICON_LAST; i++) {
        const char* name = get_icon_name(i);
        if (name == NULL) {
            continue;
        }
        char path[512] = {0};
        get_icon_path(i, path, sizeof(path));
        remove(path);  // Remove currently present icon if it exists

        char url[512] = {0};
        build_icon_url(i, name, url, sizeof(url));
        ESP_LOGI(TAG, "Downloading icon from '%s' and saving to '%s'...", url, path);

        char status[512] = {0};
        snprintf(status, sizeof(status), "Downloading icon '%s' (%u of %u)...", name, i + 1, ICON_LAST);
        progress_dialog(get_icon(ICON_DOWNLOADING), "Icon downloader", status, ((i * 100) / ICON_LAST), true);
        http_session_set_callback(session, download_callback, status);
        bool success = http_session_download_file(session, url, path);

        if (!success) {
            snprintf(status, sizeof(status), "Failed to download icon '%s'", name);
            ESP_LOGE(TAG, "%s", status);
            busy_dialog(icon, title, status, true);
        }
    }

    http_session_end(session);

    busy_dialog(NULL, "Icon download successful",
                "The icons have been downloaded successfully.\r\nDevice will restart now.", true);
    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}
