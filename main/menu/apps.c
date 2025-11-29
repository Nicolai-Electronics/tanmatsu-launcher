#include "apps.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslimits.h>
#include "app_inspect.h"
#include "app_management.h"
#include "app_metadata_parser.h"
#include "appfs.h"
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

#define MAX_NUM_APPS 128

static void populate_menu(menu_t* menu, app_t** apps, size_t app_count) {
    for (size_t i = 0; i < app_count; i++) {
        if (apps[i] != NULL && apps[i]->slug != NULL && apps[i]->name != NULL) {
            menu_insert_item_icon(menu, apps[i]->name, NULL, (void*)apps[i], -1, apps[i]->icon);
        }
    }
}

extern bool wifi_stack_get_task_done(void);

void execute_app(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, app_t* app) {

    if (app == NULL) {
        message_dialog(get_icon(ICON_ERROR), "Error", "No app selected", "OK");
        return;
    }

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
            appfsBootSelect(app->executable_appfs_fd, NULL);
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

            // TODO: properly parse app entry for interpreter, for now just assume it's in AppFS
            appfs_handle_t interpreter_fd = find_appfs_handle_for_slug(app->executable_interpreter_slug);
            if (interpreter_fd == APPFS_INVALID_FD) {
                message_dialog(get_icon(ICON_ERROR), "Error", "Interpreter not found in AppFS", "OK");
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

            appfsBootSelect(app->executable_appfs_fd, path);
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

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT                                              \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},       \
                                {get_icon(ICON_F1), "Back"},     \
                                {get_icon(ICON_F2), "Details"},  \
                                {get_icon(ICON_F5), "Remove"}}), \
        4
#define FOOTER_LEFT_UPDATE                                       \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},       \
                                {get_icon(ICON_F1), "Back"},     \
                                {get_icon(ICON_F2), "Details"},  \
                                {get_icon(ICON_F5), "Remove"},   \
                                {get_icon(ICON_F6), "Update"}}), \
        5
#define FOOTER_RIGHT             ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Start app"}}), 1
#define FOOTER_RIGHT_INSTALL     ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Install app"}}), 1
#define FOOTER_RIGHT_UNAVAILABLE ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ (Unavailable)"}}), 1
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT              NULL, 0
#define FOOTER_LEFT_UPDATE       NULL, 0
#define FOOTER_RIGHT             ((gui_element_icontext_t[]){{NULL, "🅼 Info 🅴 Remove 🅰 Start"}}), 1
#define FOOTER_RIGHT_INSTALL     ((gui_element_icontext_t[]){{NULL, "🅼 Info 🅴 Remove 🅰 Install"}}), 1
#define FOOTER_RIGHT_UNAVAILABLE ((gui_element_icontext_t[]){{NULL, "🅼 Info 🅴 Remove 🅰 (Unavailable)"}}), 1
#else
#define FOOTER_LEFT                                                                                \
    ((gui_element_icontext_t[]){                                                                   \
        {get_icon(ICON_ESC), "/"}, {NULL, "F1 Back"}, {NULL, "F2 Details"}, {NULL, "F5 Remove"}}), \
        4
#define FOOTER_LEFT_UPDATE                                 \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, \
                                {NULL, "F1 Back"},         \
                                {NULL, "F2 Details"},      \
                                {NULL, "F5 Remove"},       \
                                {NULL, "F6 Update"}}),     \
        5
#define FOOTER_RIGHT             ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Start app"}}), 1
#define FOOTER_RIGHT_INSTALL     ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Install app"}}), 1
#define FOOTER_RIGHT_UNAVAILABLE ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ (Unavailable)"}}), 1
#endif

typedef enum {
    APP_MENU_FOOTER_TYPE_NORMAL = 0,
    APP_MENU_FOOTER_TYPE_INSTALL,
    APP_MENU_FOOTER_TYPE_UPDATE,
    APP_MENU_FOOTER_TYPE_UNAVAILABLE,
    APP_MENU_FOOTER_TYPE_COUNT,
} app_menu_footer_type_t;

static app_menu_footer_type_t previous_footer_type = APP_MENU_FOOTER_TYPE_COUNT;

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    void*  arg = menu_get_callback_args(menu, menu_get_position(menu));
    app_t* app = (app_t*)arg;

    app_menu_footer_type_t footer_type = APP_MENU_FOOTER_TYPE_NORMAL;
    if (app == NULL) {
        footer_type = APP_MENU_FOOTER_TYPE_UNAVAILABLE;
    } else {
        if (app->executable_type == EXECUTABLE_TYPE_APPFS) {
            if (app->executable_appfs_fd == APPFS_INVALID_FD) {
                if (app->executable_on_sd_available) {
                    footer_type = APP_MENU_FOOTER_TYPE_INSTALL;
                } else {
                    footer_type = APP_MENU_FOOTER_TYPE_UNAVAILABLE;
                }
            } else {
                // Check for updates
                if (app->executable_on_sd_available && app->executable_on_sd_revision > app->executable_revision) {
                    footer_type = APP_MENU_FOOTER_TYPE_UPDATE;
                }
            }
        }
    };

    if (previous_footer_type != footer_type) {
        previous_footer_type = footer_type;
        partial              = false;
    }

    if (!partial || icons) {
        switch (footer_type) {
            case APP_MENU_FOOTER_TYPE_NORMAL:
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_APPS), "Apps"}}), 1,
                                             FOOTER_LEFT, FOOTER_RIGHT);
                break;
            case APP_MENU_FOOTER_TYPE_INSTALL:
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_APPS), "Apps"}}), 1,
                                             FOOTER_LEFT, FOOTER_RIGHT_INSTALL);
                break;
            case APP_MENU_FOOTER_TYPE_UPDATE:
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_APPS), "Apps"}}), 1,
                                             FOOTER_LEFT_UPDATE, FOOTER_RIGHT);
                break;
            case APP_MENU_FOOTER_TYPE_UNAVAILABLE:
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_APPS), "Apps"}}), 1,
                                             FOOTER_LEFT, FOOTER_RIGHT_UNAVAILABLE);
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
                                    if (app->executable_type == EXECUTABLE_TYPE_APPFS &&
                                        app->executable_appfs_fd == APPFS_INVALID_FD &&
                                        app->executable_on_sd_available) {
                                        // Install from SD card
                                        busy_dialog(get_icon(ICON_APPS), "Installing", "Installing application...",
                                                    true);
                                        esp_err_t res = app_mgmt_install_from_file(app->slug, app->name,
                                                                                   app->executable_on_sd_revision,
                                                                                   app->executable_on_sd_filename);
                                        if (res == ESP_OK) {
                                            message_dialog(get_icon(ICON_INFO), "Success", "App installed successfully",
                                                           "OK");
                                        } else {
                                            message_dialog(get_icon(ICON_ERROR), "Failed", "Failed to install app",
                                                           "OK");
                                        }
                                        refresh = true;
                                    } else {
                                        execute_app(buffer, theme, position, app);
                                    }
                                    render(buffer, theme, &menu, position, false, false);
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
