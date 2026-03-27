
#include "app_inspect.h"
#include <string.h>
#include "app_management.h"
#include "appfs.h"
#include "bsp/input.h"
#include "common/display.h"
#include "gui_element_icontext.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/apps.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define FOOTER_LEFT_CACHE_MOVE                                           \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},               \
                                {get_icon(ICON_F1), "Back"},             \
                                {get_icon(ICON_F2), "Start"},            \
                                {get_icon(ICON_F3), "Move"},             \
                                {get_icon(ICON_F4), "Cache"},            \
                                {get_icon(ICON_F5), "Delete App"}}),     \
        6
#define FOOTER_LEFT_UNCACHE_MOVE                                         \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},               \
                                {get_icon(ICON_F1), "Back"},             \
                                {get_icon(ICON_F2), "Start"},            \
                                {get_icon(ICON_F3), "Move"},             \
                                {get_icon(ICON_F4), "Uncache"},          \
                                {get_icon(ICON_F5), "Delete App"}}),     \
        6
#define FOOTER_LEFT_CACHE                                                \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},               \
                                {get_icon(ICON_F1), "Back"},             \
                                {get_icon(ICON_F2), "Start"},            \
                                {get_icon(ICON_F4), "Cache"},            \
                                {get_icon(ICON_F5), "Delete App"}}),     \
        5
#define FOOTER_LEFT_UNCACHE                                              \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},               \
                                {get_icon(ICON_F1), "Back"},             \
                                {get_icon(ICON_F2), "Start"},            \
                                {get_icon(ICON_F4), "Uncache"},          \
                                {get_icon(ICON_F5), "Delete App"}}),     \
        5
#define FOOTER_LEFT_PLAIN                                                \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},               \
                                {get_icon(ICON_F1), "Back"},             \
                                {get_icon(ICON_F2), "Start"},            \
                                {get_icon(ICON_F5), "Delete App"}}),     \
        4
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    18
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT_CACHE_MOVE   ((gui_element_icontext_t[]){{NULL, "\xf0\x9f\x85\xb1 Back"}}), 1
#define FOOTER_LEFT_UNCACHE_MOVE ((gui_element_icontext_t[]){{NULL, "\xf0\x9f\x85\xb1 Back"}}), 1
#define FOOTER_LEFT_CACHE        ((gui_element_icontext_t[]){{NULL, "\xf0\x9f\x85\xb1 Back"}}), 1
#define FOOTER_LEFT_UNCACHE      ((gui_element_icontext_t[]){{NULL, "\xf0\x9f\x85\xb1 Back"}}), 1
#define FOOTER_LEFT_PLAIN        ((gui_element_icontext_t[]){{NULL, "\xf0\x9f\x85\xb1 Back"}}), 1
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#else
#define FOOTER_LEFT_CACHE_MOVE   NULL, 0
#define FOOTER_LEFT_UNCACHE_MOVE NULL, 0
#define FOOTER_LEFT_CACHE        NULL, 0
#define FOOTER_LEFT_UNCACHE      NULL, 0
#define FOOTER_LEFT_PLAIN        NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#endif
#define FOOTER_RIGHT NULL, 0

static bool app_is_on_sd(app_t* app) {
    return app->path != NULL && strncmp(app->path, "/sd", 3) == 0;
}

static bool app_is_on_internal(app_t* app) {
    return app->path != NULL && strncmp(app->path, "/int", 4) == 0;
}

static bool app_has_install_dir(app_t* app) {
    return app_is_on_sd(app) || app_is_on_internal(app);
}

static bool check_move_possible(app_t* app) {
    if (!app_has_install_dir(app)) {
        return false;
    }
    app_mgmt_location_t from = app_is_on_sd(app) ? APP_MGMT_LOCATION_SD : APP_MGMT_LOCATION_INTERNAL;
    app_mgmt_location_t to   = app_is_on_sd(app) ? APP_MGMT_LOCATION_INTERNAL : APP_MGMT_LOCATION_SD;
    return app_mgmt_can_move(app->slug, from, to);
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, bool partial, bool icons, app_t* app,
                   bool can_move) {

    char text_buffer[256];
    int  line = 0;

    // Determine which footer to show based on cache state and move capability
    bool in_appfs    = (app->executable_type == EXECUTABLE_TYPE_APPFS &&
                        app->executable_appfs_fd != APPFS_INVALID_FD);
    bool can_uncache = app_mgmt_can_uncache(app->slug);

    if (!partial || icons) {
        snprintf(text_buffer, sizeof(text_buffer), "App Info: %s", app->name);
        if (in_appfs && can_uncache) {
            if (can_move) {
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_INFO), text_buffer}}), 1,
                                             FOOTER_LEFT_UNCACHE_MOVE, FOOTER_RIGHT);
            } else {
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_INFO), text_buffer}}), 1,
                                             FOOTER_LEFT_UNCACHE, FOOTER_RIGHT);
            }
        } else if (!in_appfs && app->executable_type == EXECUTABLE_TYPE_APPFS) {
            if (can_move) {
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_INFO), text_buffer}}), 1,
                                             FOOTER_LEFT_CACHE_MOVE, FOOTER_RIGHT);
            } else {
                render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                             ((gui_element_icontext_t[]){{get_icon(ICON_INFO), text_buffer}}), 1,
                                             FOOTER_LEFT_CACHE, FOOTER_RIGHT);
            }
        } else {
            render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                         ((gui_element_icontext_t[]){{get_icon(ICON_INFO), text_buffer}}), 1,
                                         FOOTER_LEFT_PLAIN, FOOTER_RIGHT);
        }
    }

    if (!partial) {

        if (app->name) {
            snprintf(text_buffer, sizeof(text_buffer), "Name: %s", app->name);
            pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        }

        if (app->description) {
            snprintf(text_buffer, sizeof(text_buffer), "Description: %s", app->description);
            pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        }

        if (app->author) {
            snprintf(text_buffer, sizeof(text_buffer), "Author: %s", app->author);
            pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        }

        if (app->version) {
            snprintf(text_buffer, sizeof(text_buffer), "Version: %s", app->version);
            pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        }

        if (app->slug) {
            snprintf(text_buffer, sizeof(text_buffer), "Slug: %s", app->slug);
            pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        }

        // Install location
        if (app_is_on_sd(app)) {
            snprintf(text_buffer, sizeof(text_buffer), "Location: SD Card");
        } else if (app_is_on_internal(app)) {
            snprintf(text_buffer, sizeof(text_buffer), "Location: Internal");
        } else {
            snprintf(text_buffer, sizeof(text_buffer), "Location: AppFS only");
        }
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);

        // AppFS free space info
        size_t free_kb  = appfsGetFreeMem() / 1024;
        size_t total_kb = appfsGetTotalMem() / 1024;
        snprintf(text_buffer, sizeof(text_buffer), "AppFS: %u / %u KB free", free_kb, total_kb);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
    }

    display_blit_buffer(buffer);
}

bool menu_app_inspect(pax_buf_t* buffer, gui_theme_t* theme, app_t* app) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    bool can_move = check_move_possible(app);

    render(buffer, theme, position, false, false, app, can_move);

    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                                return false;
                            case BSP_INPUT_NAVIGATION_KEY_F2:
                                execute_app(buffer, theme, position, app);
                                render(buffer, theme, position, false, false, app, can_move);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F3: {
                                if (!app_has_install_dir(app)) {
                                    break;
                                }
                                app_mgmt_location_t from, to;
                                const char*         to_name;
                                if (app_is_on_sd(app)) {
                                    from    = APP_MGMT_LOCATION_SD;
                                    to      = APP_MGMT_LOCATION_INTERNAL;
                                    to_name = "Internal storage";
                                } else {
                                    from    = APP_MGMT_LOCATION_INTERNAL;
                                    to      = APP_MGMT_LOCATION_SD;
                                    to_name = "SD Card";
                                }
                                char confirm_msg[128];
                                snprintf(confirm_msg, sizeof(confirm_msg),
                                         "Move app to %s?", to_name);
                                message_dialog_return_type_t msg_ret = adv_dialog_yes_no(
                                    get_icon(ICON_HELP), "Move App", confirm_msg);
                                if (msg_ret == MSG_DIALOG_RETURN_OK) {
                                    busy_dialog(get_icon(ICON_APPS), "Moving",
                                                "Moving app...", true);
                                    esp_err_t res = app_mgmt_move(app->slug, from, to);
                                    if (res == ESP_OK) {
                                        message_dialog(get_icon(ICON_INFO), "Success",
                                                       "App moved successfully", "OK");
                                    } else if (res == ESP_ERR_NO_MEM) {
                                        char err_msg[128];
                                        snprintf(err_msg, sizeof(err_msg),
                                                 "Not enough space on %s", to_name);
                                        message_dialog(get_icon(ICON_ERROR), "No space",
                                                       err_msg, "OK");
                                    } else {
                                        message_dialog(get_icon(ICON_ERROR), "Failed",
                                                       "Failed to move app", "OK");
                                    }
                                    return true;  // Trigger app list refresh
                                }
                                render(buffer, theme, position, false, false, app, can_move);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_F4: {
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
                                        return true;  // Trigger app list refresh
                                    }
                                } else if (app->executable_type == EXECUTABLE_TYPE_APPFS &&
                                           app->executable_appfs_fd == APPFS_INVALID_FD) {
                                    // Not cached → cache
                                    char* firmware_path = app_mgmt_find_firmware_path(app->slug);
                                    if (firmware_path == NULL) {
                                        message_dialog(get_icon(ICON_ERROR), "Error",
                                                       "App binary not found", "OK");
                                    } else {
                                        busy_dialog(get_icon(ICON_APPS), "Caching",
                                                    "Copying app to AppFS...", true);
                                        esp_err_t res = app_mgmt_cache_to_appfs(
                                            app->slug, app->name, app->executable_revision, firmware_path);
                                        free(firmware_path);
                                        if (res == ESP_ERR_NO_MEM) {
                                            message_dialog(get_icon(ICON_ERROR), "No space",
                                                           "Not enough space in AppFS.\n"
                                                           "Remove cached apps (F4).", "OK");
                                        } else if (res == ESP_OK) {
                                            message_dialog(get_icon(ICON_INFO), "Success",
                                                           "App cached in AppFS", "OK");
                                        } else {
                                            message_dialog(get_icon(ICON_ERROR), "Failed",
                                                           "Failed to cache app", "OK");
                                        }
                                    }
                                    return true;  // Trigger app list refresh
                                }
                                render(buffer, theme, position, false, false, app, can_move);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_F5: {
                                message_dialog_return_type_t msg_ret = adv_dialog_yes_no(
                                    get_icon(ICON_HELP), "Delete App", "Do you really want to delete the app?");
                                if (msg_ret == MSG_DIALOG_RETURN_OK) {
                                    esp_err_t res_int = app_mgmt_uninstall(app->slug, APP_MGMT_LOCATION_INTERNAL);
                                    esp_err_t res_sd  = app_mgmt_uninstall(app->slug, APP_MGMT_LOCATION_SD);
                                    if (res_int == ESP_OK || res_sd == ESP_OK) {
                                        message_dialog(get_icon(ICON_INFO), "Success", "App removed successfully",
                                                       "OK");
                                    } else {
                                        message_dialog(get_icon(ICON_ERROR), "Failed", "Failed to remove app", "OK");
                                    }
                                    return true;
                                }
                                render(buffer, theme, position, false, false, app, can_move);
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
            render(buffer, theme, position, false, false, app, can_move);
        }
    }
}
