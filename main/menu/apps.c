#include "apps.h"
#include <esp_log.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslimits.h>
#include <time.h>
#include "app_inspect.h"
#include "app_management.h"
#include "app_metadata_parser.h"
#include "appfs.h"
#include "device_settings.h"
#include "fastopen.h"
#if CONFIG_IDF_TARGET_ESP32P4
#include "badge_elf.h"
#endif
#include "bsp/input.h"
#include "common/display.h"
#include "filesystem_utils.h"
#include "freertos/idf_additions.h"
#include "gui_element_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/app_inspect.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "sdkconfig.h"
#include "usb_device.h"

static const char* TAG = "apps";

#define MAX_NUM_APPS 128

static void populate_menu(menu_t* menu, app_t** apps, size_t app_count) {
    for (size_t i = 0; i < app_count; i++) {
        if (apps[i] != NULL && apps[i]->slug != NULL && apps[i]->name != NULL) {
            char label[128];
            bool in_appfs = (apps[i]->executable_type == EXECUTABLE_TYPE_APPFS &&
                             apps[i]->executable_appfs_fd != APPFS_INVALID_FD);
            snprintf(label, sizeof(label), "%s %s", in_appfs ? "[C]" : "   ", apps[i]->name);
            menu_insert_item_icon(menu, label, NULL, (void*)apps[i], -1, apps[i]->icon);
        }
    }
}

extern bool wifi_stack_get_task_done(void);

// Helper: ensure interpreter is loaded into AppFS for script apps.
// Returns the appfs handle on success, APPFS_INVALID_FD on failure.
static appfs_handle_t ensure_interpreter_in_appfs(const char* interpreter_slug) {
    appfs_handle_t fd = find_appfs_handle_for_slug(interpreter_slug);
    if (fd != APPFS_INVALID_FD) {
        return fd;
    }

    // Interpreter not in appfs — try to find its binary in install dirs and load it
    const char* base_paths[] = {"/int/apps", "/sd/apps"};
    for (int i = 0; i < 2; i++) {
        uint32_t revision  = 0;
        char*    exec_path = NULL;
        if (get_executable_revision(base_paths[i], interpreter_slug, &revision, &exec_path)) {
            if (exec_path != NULL && fs_utils_exists(exec_path)) {
                busy_dialog(get_icon(ICON_APPS), "Loading", "Loading interpreter to AppFS...", true);
                esp_err_t res = app_mgmt_ensure_in_appfs(interpreter_slug, interpreter_slug, revision, exec_path);
                if (res == ESP_ERR_NO_MEM) {
                    uint8_t auto_cleanup = 0;
                    device_settings_get_appfs_auto_cleanup(&auto_cleanup);
                    if (auto_cleanup) {
                        busy_dialog(get_icon(ICON_APPS), "Loading", "Freeing up space...", true);
                        FILE* f = fastopen(exec_path, "rb");
                        if (f != NULL) {
                            size_t file_size    = fs_utils_get_file_size(f);
                            fastclose(f);
                            size_t rounded_size = (file_size + (SPI_FLASH_MMU_PAGE_SIZE - 1)) &
                                                  (~(SPI_FLASH_MMU_PAGE_SIZE - 1));
                            app_mgmt_appfs_evict_lru(rounded_size);
                            busy_dialog(get_icon(ICON_APPS), "Loading", "Loading interpreter to AppFS...", true);
                            res = app_mgmt_ensure_in_appfs(interpreter_slug, interpreter_slug, revision, exec_path);
                        }
                    }
                    if (res == ESP_ERR_NO_MEM) {
                        message_dialog(get_icon(ICON_ERROR), "No space",
                                       "Not enough space in AppFS for interpreter.\nRemove cached apps (F4).", "OK");
                    }
                }
                free(exec_path);
                if (res == ESP_OK) {
                    return find_appfs_handle_for_slug(interpreter_slug);
                }
                return APPFS_INVALID_FD;
            }
            free(exec_path);
        }
    }

    message_dialog(get_icon(ICON_ERROR), "Error", "Interpreter binary not found", "OK");
    return APPFS_INVALID_FD;
}

void execute_app(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, app_t* app) {

    if (app == NULL) {
        message_dialog(get_icon(ICON_ERROR), "Error", "No app selected", "OK");
        return;
    }

    // Record last-used time for LRU eviction
    device_settings_set_app_last_used(app->slug, (uint32_t)time(NULL));

    render_base_screen_statusbar(buffer, theme, true, true, true,
                                 ((gui_element_icontext_t[]){{get_icon(ICON_APPS), "Apps"}}), 1, NULL, 0, NULL, 0);
    char message[64] = {0};
    snprintf(message, sizeof(message), "Starting %s...", app->name);
    pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, theme->footer.text_height,
                  position.x0, position.y0, message);
    display_blit_buffer(buffer);
    printf("Starting %s (from %s)...\n", app->slug, app->path);

    switch (app->executable_type) {
        case EXECUTABLE_TYPE_APPFS:
            if (app->executable_appfs_fd == APPFS_INVALID_FD) {
                message_dialog(get_icon(ICON_ERROR), "Error", "App not found in AppFS", "OK");
                return;
            }
            {
                char app_path[256];
                snprintf(app_path, sizeof(app_path), "%s/%s", app->path, app->slug);
                appfsBootSelect(app->executable_appfs_fd, app_path);
            }
            while (wifi_stack_get_task_done() == false) {
                printf("Waiting for wifi stack task to finish...\n");
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            usb_mode_set(USB_DEBUG);
            esp_restart();
            break;
        case EXECUTABLE_TYPE_ELF: {
#if CONFIG_IDF_TARGET_ESP32P4
            size_t req = snprintf(NULL, 0, "%s/%s/%s", app->path, app->slug, app->executable_filename);
            if (req > PATH_MAX) {
                message_dialog(get_icon(ICON_ERROR), "Error", "Applet path is too long", "OK");
            } else {
                char* path = malloc(req + 1);
                if (path == NULL) {
                    printf("Out of memory\r\n");
                    return;
                }
                snprintf(path, req + 1, "%s/%s/%s", app->path, app->slug, app->executable_filename);
                if (!fs_utils_exists(path)) {
                    message_dialog(get_icon(ICON_ERROR), "Error", "Applet not found", "OK");
                } else if (!badge_elf_start(path)) {
                    message_dialog(get_icon(ICON_ERROR), "Error", "Failed to start app", "OK");
                }
                free(path);
            }
#else
            message_dialog(get_icon(ICON_ERROR), "Error", "Applets are not supported on this platform", "OK");
#endif
            break;
        }
        case EXECUTABLE_TYPE_SCRIPT: {
            if (app->executable_interpreter_slug == NULL) {
                message_dialog(get_icon(ICON_ERROR), "Error", "No interpreter specified for script", "OK");
                return;
            }

            if (app->executable_filename == NULL) {
                ESP_LOGE(TAG, "No executable specified for script app %s", app->slug);
                return;
            }

            // Ensure interpreter is in AppFS (load on-demand if needed)
            appfs_handle_t interpreter_fd = ensure_interpreter_in_appfs(app->executable_interpreter_slug);
            if (interpreter_fd == APPFS_INVALID_FD) {
                return;
            }

            size_t req = snprintf(NULL, 0, "%s/%s/%s", app->path, app->slug, app->executable_filename);
            if (req > PATH_MAX) {
                message_dialog(get_icon(ICON_ERROR), "Error", "Script path is too long", "OK");
                return;
            }

            char* path = malloc(req + 1);
            if (path == NULL) {
                printf("Out of memory (failed to alloc %u bytes)\r\n", req + 1);
                return;
            }
            snprintf(path, req + 1, "%s/%s/%s", app->path, app->slug, app->executable_filename);
            if (!fs_utils_exists(path)) {
                message_dialog(get_icon(ICON_ERROR), "Error", "Script file not found", "OK");
                free(path);
                return;
            }

            ESP_LOGI(TAG, "Launching script: interpreter=%s path=%s", app->executable_interpreter_slug, path);
            appfsBootSelect(interpreter_fd, path);
            while (wifi_stack_get_task_done() == false) {
                printf("Waiting for wifi stack task to finish...\n");
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            usb_mode_set(USB_DEBUG);
            esp_restart();
            free(path);  // Not reached
            break;
        }
        default:
            message_dialog(get_icon(ICON_ERROR), "Error", "Unknown executable type", "OK");
            break;
    }
}

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define FOOTER_LEFT_CACHE                                          \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},         \
                                {get_icon(ICON_F1), "Back"},       \
                                {get_icon(ICON_F2), "Details"},    \
                                {get_icon(ICON_F4), "Cache"},      \
                                {get_icon(ICON_F5), "Remove"}}),   \
        5
#define FOOTER_LEFT_UNCACHE                                        \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},         \
                                {get_icon(ICON_F1), "Back"},       \
                                {get_icon(ICON_F2), "Details"},    \
                                {get_icon(ICON_F4), "Uncache"},    \
                                {get_icon(ICON_F5), "Remove"}}),   \
        5
#define FOOTER_LEFT_UNCACHE_UPDATE                                 \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},         \
                                {get_icon(ICON_F1), "Back"},       \
                                {get_icon(ICON_F2), "Details"},    \
                                {get_icon(ICON_F4), "Uncache"},    \
                                {get_icon(ICON_F5), "Remove"},     \
                                {get_icon(ICON_F6), "Update"}}),   \
        6
#define FOOTER_LEFT_PLAIN                                          \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},         \
                                {get_icon(ICON_F1), "Back"},       \
                                {get_icon(ICON_F2), "Details"},    \
                                {get_icon(ICON_F5), "Remove"}}),   \
        4
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT_CACHE          NULL, 0
#define FOOTER_LEFT_UNCACHE        NULL, 0
#define FOOTER_LEFT_UNCACHE_UPDATE NULL, 0
#define FOOTER_LEFT_PLAIN          NULL, 0
#else
#define FOOTER_LEFT_CACHE                                          \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},         \
                                {NULL, "F1 Back"},                 \
                                {NULL, "F2 Details"},              \
                                {NULL, "F4 Cache"},                \
                                {NULL, "F5 Remove"}}),             \
        5
#define FOOTER_LEFT_UNCACHE                                        \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},         \
                                {NULL, "F1 Back"},                 \
                                {NULL, "F2 Details"},              \
                                {NULL, "F4 Uncache"},              \
                                {NULL, "F5 Remove"}}),             \
        5
#define FOOTER_LEFT_UNCACHE_UPDATE                                 \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},         \
                                {NULL, "F1 Back"},                 \
                                {NULL, "F2 Details"},              \
                                {NULL, "F4 Uncache"},              \
                                {NULL, "F5 Remove"},               \
                                {NULL, "F6 Update"}}),             \
        6
#define FOOTER_LEFT_PLAIN                                          \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},         \
                                {NULL, "F1 Back"},                 \
                                {NULL, "F2 Details"},              \
                                {NULL, "F5 Remove"}}),             \
        4
#endif

typedef enum {
    APP_MENU_FOOTER_TYPE_NORMAL_CACHED = 0,  // In appfs, has install dir → F4 "Uncache"
    APP_MENU_FOOTER_TYPE_NORMAL_UNCACHED,    // Not in appfs, has install dir → F4 "Cache"
    APP_MENU_FOOTER_TYPE_NORMAL_PLAIN,       // In appfs, no install dir (dev/legacy) → no F4
    APP_MENU_FOOTER_TYPE_UPDATE_CACHED,      // In appfs, update available → F4 "Uncache" + F6 "Update"
    APP_MENU_FOOTER_TYPE_INSTALL,            // Binary on disk, not in appfs → F4 "Cache"
    APP_MENU_FOOTER_TYPE_UNAVAILABLE,        // Nothing available → no F4
    APP_MENU_FOOTER_TYPE_COUNT,
} app_menu_footer_type_t;

static app_menu_footer_type_t previous_footer_type = APP_MENU_FOOTER_TYPE_COUNT;

// Helper to find firmware path for an app (checks SD then internal install dir)
// Returns malloc'd string or NULL. Caller must free.
static char* find_firmware_path(app_t* app) {
    if (app->executable_on_sd_available && app->executable_on_sd_filename != NULL) {
        if (fs_utils_exists(app->executable_on_sd_filename)) {
            return strdup(app->executable_on_sd_filename);
        }
    }
    // Check internal install dir
    const char* base_paths[] = {"/int/apps", "/sd/apps"};
    for (int i = 0; i < 2; i++) {
        uint32_t revision  = 0;
        char*    exec_path = NULL;
        if (get_executable_revision(base_paths[i], app->slug, &revision, &exec_path)) {
            if (exec_path != NULL && fs_utils_exists(exec_path)) {
                return exec_path;
            }
            free(exec_path);
        }
    }
    return NULL;
}

// Helper to load an app into AppFS on-demand, with auto-cleanup support.
// Returns ESP_OK on success, error otherwise. Shows dialogs.
static esp_err_t load_app_to_appfs(app_t* app, const char* firmware_path) {
    busy_dialog(get_icon(ICON_APPS), "Loading", "Copying app to AppFS...", true);
    esp_err_t res = app_mgmt_ensure_in_appfs(app->slug, app->name, app->executable_revision, firmware_path);
    if (res == ESP_ERR_NO_MEM) {
        uint8_t auto_cleanup = 0;
        device_settings_get_appfs_auto_cleanup(&auto_cleanup);
        if (auto_cleanup) {
            busy_dialog(get_icon(ICON_APPS), "Loading", "Freeing up space...", true);
            // Calculate needed size
            FILE* fd = fastopen(firmware_path, "rb");
            if (fd != NULL) {
                size_t file_size    = fs_utils_get_file_size(fd);
                fastclose(fd);
                size_t rounded_size = (file_size + (SPI_FLASH_MMU_PAGE_SIZE - 1)) & (~(SPI_FLASH_MMU_PAGE_SIZE - 1));
                app_mgmt_appfs_evict_lru(rounded_size);
                busy_dialog(get_icon(ICON_APPS), "Loading", "Copying app to AppFS...", true);
                res = app_mgmt_ensure_in_appfs(app->slug, app->name, app->executable_revision, firmware_path);
            }
        }
        if (res == ESP_ERR_NO_MEM) {
            message_dialog(get_icon(ICON_ERROR), "No space",
                           "Not enough space in AppFS.\nRemove cached apps (F4) to free space.", "OK");
        }
    }
    if (res != ESP_OK && res != ESP_ERR_NO_MEM) {
        message_dialog(get_icon(ICON_ERROR), "Failed", "Failed to load app to AppFS", "OK");
    }
    return res;
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    void*  arg = menu_get_callback_args(menu, menu_get_position(menu));
    app_t* app = (app_t*)arg;

    app_menu_footer_type_t footer_type = APP_MENU_FOOTER_TYPE_NORMAL_UNCACHED;
    if (app == NULL) {
        footer_type = APP_MENU_FOOTER_TYPE_UNAVAILABLE;
    } else {
        if (app->executable_type == EXECUTABLE_TYPE_APPFS) {
            bool can_uncache = app_mgmt_can_uncache(app->slug);
            if (app->executable_appfs_fd == APPFS_INVALID_FD) {
                if (app->executable_on_sd_available || app_mgmt_has_binary_in_install_dir(app->slug)) {
                    footer_type = APP_MENU_FOOTER_TYPE_INSTALL;
                } else {
                    footer_type = APP_MENU_FOOTER_TYPE_UNAVAILABLE;
                }
            } else {
                if (app->executable_on_sd_available && app->executable_on_sd_revision > app->executable_revision) {
                    footer_type = APP_MENU_FOOTER_TYPE_UPDATE_CACHED;
                } else if (can_uncache) {
                    footer_type = APP_MENU_FOOTER_TYPE_NORMAL_CACHED;
                } else {
                    // App only exists in appfs (dev/legacy), no cache toggle available
                    footer_type = APP_MENU_FOOTER_TYPE_NORMAL_PLAIN;
                }
            }
        }
    };

    if (previous_footer_type != footer_type) {
        previous_footer_type = footer_type;
        partial              = false;
    }

    if (!partial || icons) {
        // Build dynamic footer right with AppFS free space
        char footer_right_text[80];
        size_t free_kb  = appfsGetFreeMem() / 1024;
        size_t total_kb = appfsGetTotalMem() / 1024;

        const char* action_text;
        switch (footer_type) {
            case APP_MENU_FOOTER_TYPE_INSTALL:
                action_text = "⏎ Cache app";
                break;
            case APP_MENU_FOOTER_TYPE_UNAVAILABLE:
                action_text = "⏎ (Unavailable)";
                break;
            default:
                action_text = "⏎ Start app";
                break;
        }
        snprintf(footer_right_text, sizeof(footer_right_text), "AppFS: %u/%uKB | %s", free_kb, total_kb, action_text);
        gui_element_icontext_t footer_right[] = {{NULL, footer_right_text}};

        switch (footer_type) {
            case APP_MENU_FOOTER_TYPE_NORMAL_CACHED:
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_APPS), "Apps"}}), 1,
                                             FOOTER_LEFT_UNCACHE, footer_right, 1);
                break;
            case APP_MENU_FOOTER_TYPE_NORMAL_UNCACHED:
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_APPS), "Apps"}}), 1,
                                             FOOTER_LEFT_CACHE, footer_right, 1);
                break;
            case APP_MENU_FOOTER_TYPE_NORMAL_PLAIN:
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_APPS), "Apps"}}), 1,
                                             FOOTER_LEFT_PLAIN, footer_right, 1);
                break;
            case APP_MENU_FOOTER_TYPE_INSTALL:
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_APPS), "Apps"}}), 1,
                                             FOOTER_LEFT_CACHE, footer_right, 1);
                break;
            case APP_MENU_FOOTER_TYPE_UPDATE_CACHED:
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_APPS), "Apps"}}), 1,
                                             FOOTER_LEFT_UNCACHE_UPDATE, footer_right, 1);
                break;
            case APP_MENU_FOOTER_TYPE_UNAVAILABLE:
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_APPS), "Apps"}}), 1,
                                             FOOTER_LEFT_PLAIN, footer_right, 1);
                break;
            default:
                break;
        }
    }
    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_apps(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    bool exit = false;
    while (!exit) {

        app_t* apps[MAX_NUM_APPS] = {0};
        size_t number_of_apps     = create_list_of_apps(apps, MAX_NUM_APPS);

        menu_t menu = {0};
        menu_initialize(&menu);
        populate_menu(&menu, apps, number_of_apps);

        int header_height = theme->header.height + (theme->header.vertical_margin * 2);
        int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

        pax_vec2_t position = {
            .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
            .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
            .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
            .y1 =
                pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
        };

        render(buffer, theme, &menu, position, false, true);
        bool refresh = false;
        while (!refresh) {
            bsp_input_event_t event;
            if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
                switch (event.type) {
                    case INPUT_EVENT_TYPE_NAVIGATION: {
                        if (event.args_navigation.state) {
                            switch (event.args_navigation.key) {
                                case BSP_INPUT_NAVIGATION_KEY_ESC:
                                case BSP_INPUT_NAVIGATION_KEY_F1:
                                case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                                case BSP_INPUT_NAVIGATION_KEY_HOME:
                                    menu_free(&menu);
                                    free_list_of_apps(apps, MAX_NUM_APPS);
                                    return;
                                case BSP_INPUT_NAVIGATION_KEY_UP:
                                    menu_navigate_previous(&menu);
                                    render(buffer, theme, &menu, position, true, false);
                                    break;
                                case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                    menu_navigate_next(&menu);
                                    render(buffer, theme, &menu, position, true, false);
                                    break;
                                case BSP_INPUT_NAVIGATION_KEY_RETURN:
                                case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                                case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                    void*  arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                    app_t* app = (app_t*)arg;
                                    // On-demand AppFS loading for appfs-type apps
                                    if (app->executable_type == EXECUTABLE_TYPE_APPFS &&
                                        app->executable_appfs_fd == APPFS_INVALID_FD) {
                                        char* firmware_path = find_firmware_path(app);
                                        if (firmware_path == NULL) {
                                            message_dialog(get_icon(ICON_ERROR), "Error",
                                                           "App binary not found", "OK");
                                            render(buffer, theme, &menu, position, false, false);
                                            break;
                                        }
                                        esp_err_t res = load_app_to_appfs(app, firmware_path);
                                        free(firmware_path);
                                        if (res != ESP_OK) {
                                            refresh = true;
                                            break;
                                        }
                                        app->executable_appfs_fd = find_appfs_handle_for_slug(app->slug);
                                    }
                                    execute_app(buffer, theme, position, app);
                                    render(buffer, theme, &menu, position, false, false);
                                    break;
                                }
                                case BSP_INPUT_NAVIGATION_KEY_F4: {
                                    void*  arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                    app_t* app = (app_t*)arg;
                                    if (app->executable_appfs_fd != APPFS_INVALID_FD &&
                                        app_mgmt_can_uncache(app->slug)) {
                                        // Cached → uncache
                                        message_dialog_return_type_t msg_ret = adv_dialog_yes_no(
                                            get_icon(ICON_HELP), "Remove from cache",
                                            "Remove app binary from AppFS cache?");
                                        if (msg_ret == MSG_DIALOG_RETURN_OK) {
                                            busy_dialog(get_icon(ICON_APPS), "Removing",
                                                        "Removing from AppFS...", true);
                                            esp_err_t res = app_mgmt_remove_from_appfs(app->slug);
                                            if (res == ESP_OK) {
                                                message_dialog(get_icon(ICON_INFO), "Success",
                                                               "Removed from AppFS", "OK");
                                            } else {
                                                message_dialog(get_icon(ICON_ERROR), "Failed",
                                                               "Failed to remove from AppFS", "OK");
                                            }
                                            refresh = true;
                                        } else {
                                            render(buffer, theme, &menu, position, false, true);
                                        }
                                    } else if (app->executable_type == EXECUTABLE_TYPE_APPFS &&
                                               app->executable_appfs_fd == APPFS_INVALID_FD) {
                                        // Not cached → cache (pre-cache without launching)
                                        char* firmware_path = find_firmware_path(app);
                                        if (firmware_path == NULL) {
                                            message_dialog(get_icon(ICON_ERROR), "Error",
                                                           "App binary not found", "OK");
                                        } else {
                                            esp_err_t res = load_app_to_appfs(app, firmware_path);
                                            free(firmware_path);
                                            if (res == ESP_OK) {
                                                message_dialog(get_icon(ICON_INFO), "Success",
                                                               "App cached in AppFS", "OK");
                                            }
                                        }
                                        refresh = true;
                                    }
                                    break;
                                }
                                case BSP_INPUT_NAVIGATION_KEY_MENU:
                                case BSP_INPUT_NAVIGATION_KEY_F2: {
                                    void*  arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                    app_t* app = (app_t*)arg;
                                    if (menu_app_inspect(buffer, theme, app)) {
                                        refresh = true;
                                        break;
                                    }
                                    render(buffer, theme, &menu, position, false, true);
                                    break;
                                }
                                case BSP_INPUT_NAVIGATION_KEY_SELECT:
                                case BSP_INPUT_NAVIGATION_KEY_F5:
                                case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_Y: {
                                    void*  arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                    app_t* app = (app_t*)arg;
                                    message_dialog_return_type_t msg_ret = adv_dialog_yes_no(
                                        get_icon(ICON_HELP), "Delete App", "Do you really want to delete the app?");
                                    if (msg_ret == MSG_DIALOG_RETURN_OK) {
                                        esp_err_t res_int = app_mgmt_uninstall(app->slug, APP_MGMT_LOCATION_INTERNAL);
                                        esp_err_t res_sd  = app_mgmt_uninstall(app->slug, APP_MGMT_LOCATION_SD);
                                        if (res_int == ESP_OK || res_sd == ESP_OK) {
                                            message_dialog(get_icon(ICON_INFO), "Success", "App removed successfully",
                                                           "OK");
                                        } else {
                                            message_dialog(get_icon(ICON_ERROR), "Failed", "Failed to remove app",
                                                           "OK");
                                        }
                                        refresh = true;
                                    } else {
                                        render(buffer, theme, &menu, position, false, true);
                                    }
                                    break;
                                }
                                case BSP_INPUT_NAVIGATION_KEY_F6: {
                                    void*  arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                    app_t* app = (app_t*)arg;
                                    if (app->executable_type == EXECUTABLE_TYPE_APPFS &&
                                        app->executable_on_sd_available &&
                                        app->executable_revision != app->executable_on_sd_revision) {
                                        // Install from SD card
                                        busy_dialog(get_icon(ICON_APPS), "Updating", "Updating application...", true);
                                        esp_err_t res = app_mgmt_install_from_file(app->slug, app->name,
                                                                                   app->executable_on_sd_revision,
                                                                                   app->executable_on_sd_filename);
                                        if (res == ESP_OK) {
                                            message_dialog(get_icon(ICON_INFO), "Success", "App updated successfully",
                                                           "OK");
                                        } else {
                                            message_dialog(get_icon(ICON_ERROR), "Failed", "Failed to update app",
                                                           "OK");
                                        }
                                    } else {
                                        message_dialog(get_icon(ICON_ERROR), "Failed", "Nothing to update", "OK");
                                    }
                                    refresh = true;
                                    break;
                                }
                                default:
                                    break;
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
            } else {
                render(buffer, theme, &menu, position, true, true);
            }
        }
        menu_free(&menu);
        free_list_of_apps(apps, MAX_NUM_APPS);
    }
}
