#include "menu_repository_client_project.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/_intsup.h>
#include "app_management.h"
#include "app_metadata_parser.h"
#include "appfs.h"
#include "bsp/device.h"
#include "bsp/input.h"
#include "cJSON.h"
#include "common/display.h"
#include "device_settings.h"
#include "esp_log.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "nvs_settings.h"
#include "pax_fonts.h"
#include "pax_text.h"
#include "pax_types.h"
#include "repository_client.h"

static const char* TAG = "Repository client: project";

typedef enum {
    ACTION_INSTALL = 0,
    ACTION_INSTALL_SD,
} menu_repository_client_project_action_t;

static void render_project(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, cJSON* project) {

    cJSON* name_obj         = cJSON_GetObjectItem(project, "name");
    cJSON* description_obj  = cJSON_GetObjectItem(project, "description");
    cJSON* version_obj      = cJSON_GetObjectItem(project, "version");
    cJSON* author_obj       = cJSON_GetObjectItem(project, "author");
    cJSON* license_type_obj = cJSON_GetObjectItem(project, "license_type");

    float font_size = 16;

    char text_buffer[256];
    sprintf(text_buffer, "Name: %s", name_obj ? name_obj->valuestring : "Unknown");
    pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, font_size, position.x0,
                  position.y0 + font_size * 0, text_buffer);
    sprintf(text_buffer, "Description: %s", description_obj ? description_obj->valuestring : "Unknown");
    pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, font_size, position.x0,
                  position.y0 + font_size * 1, text_buffer);
    sprintf(text_buffer, "Version: %s", version_obj ? version_obj->valuestring : "Unknown");
    pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, font_size, position.x0,
                  position.y0 + font_size * 2, text_buffer);
    sprintf(text_buffer, "Author: %s", author_obj ? author_obj->valuestring : "Unknown");
    pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, font_size, position.x0,
                  position.y0 + font_size * 3, text_buffer);
    sprintf(text_buffer, "License: %s", license_type_obj ? license_type_obj->valuestring : "Unknown");
    pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, font_size, position.x0,
                  position.y0 + font_size * 4, text_buffer);
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, bool partial, bool icons, cJSON* project) {
    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    if (!partial || icons) {
        render_base_screen_statusbar(
            buffer, theme, !partial, !partial || icons, !partial,
            ((gui_element_icontext_t[]){{get_icon(ICON_STOREFRONT), "Repository"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
            ((gui_element_icontext_t[]){{NULL, "← / → | ⏎ Select"}}), 1);
    }

    // Description
    pax_vec2_t desc_position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    render_project(buffer, theme, desc_position, project);

    // Menu
    pax_vec2_t menu_position = {
        .x0 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding - 400,
        .y0 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding -
              128,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    gui_theme_t modified_theme                = *theme;
    modified_theme.menu.grid_horizontal_count = menu_get_length(menu);
    modified_theme.menu.grid_vertical_count   = 1;
    menu_render_grid(buffer, menu, menu_position, &modified_theme, partial);

    // Blit
    display_blit_buffer(buffer);
}

static void download_callback(size_t download_position, size_t file_size, const char* status_text) {
    if (file_size == 0) {
        ESP_LOGD(TAG, "Download callback called with file_size == 0");
        return;
    }
    uint8_t        percentage      = 100 * download_position / file_size;
    static uint8_t last_percentage = 0;
    if (percentage == last_percentage) {
        return;  // No change, no need to update
    }
    last_percentage = percentage;
    char text[64];
    snprintf(text, sizeof(text), "%s (%u%%)", status_text ? status_text : "Downloading", percentage);
    busy_dialog(get_icon(ICON_DOWNLOADING), "Downloading", text, true);
};

// Find the interpreter slug for the current device from project metadata.
// Returns NULL if the app is not a script type or has no interpreter.
// The returned string points into the cJSON tree — do not free.
static const char* find_interpreter_slug(cJSON* project) {
    cJSON* applications = cJSON_GetObjectItem(project, "application");
    if (applications == NULL || !cJSON_IsArray(applications)) {
        return NULL;
    }

    char device_name[32] = {0};
    bsp_device_get_name(device_name, sizeof(device_name));
    size_t device_name_len = strlen(device_name);

    cJSON* application = NULL;
    cJSON_ArrayForEach(application, applications) {
        cJSON* targets = cJSON_GetObjectItem(application, "targets");
        if (targets == NULL || !cJSON_IsArray(targets)) {
            continue;
        }
        cJSON* target = NULL;
        bool   matched = false;
        cJSON_ArrayForEach(target, targets) {
            if (target != NULL && cJSON_IsString(target) &&
                strlen(target->valuestring) == device_name_len &&
                strncasecmp(target->valuestring, device_name, device_name_len) == 0) {
                matched = true;
                break;
            }
        }
        if (matched) {
            cJSON* type_obj = cJSON_GetObjectItem(application, "type");
            if (type_obj != NULL && cJSON_IsString(type_obj) &&
                strcmp(type_obj->valuestring, "script") == 0) {
                cJSON* interp_obj = cJSON_GetObjectItem(application, "interpreter");
                if (interp_obj != NULL && cJSON_IsString(interp_obj)) {
                    return interp_obj->valuestring;
                }
            }
            return NULL;  // Found matching app but it's not a script
        }
    }
    return NULL;
}

// Check if interpreter is already available (in appfs or install dirs)
static bool interpreter_available(const char* interpreter_slug) {
    if (appfsExists(interpreter_slug)) {
        return true;
    }
    return app_mgmt_has_binary_in_install_dir(interpreter_slug);
}

// Prompt user to install the interpreter using the standard project install dialog
static void prompt_install_interpreter(pax_buf_t* buffer, gui_theme_t* theme, const char* interpreter_slug) {
    char msg[128];
    snprintf(msg, sizeof(msg), "This app requires interpreter '%s'.\nInstall it?", interpreter_slug);
    message_dialog_return_type_t result = adv_dialog_yes_no(get_icon(ICON_HELP), "Interpreter needed", msg);
    if (result != MSG_DIALOG_RETURN_OK) {
        return;
    }

    // Fetch interpreter project metadata from repo
    char server[128] = {0};
    nvs_settings_get_repo_server(server, sizeof(server), DEFAULT_REPO_SERVER);

    busy_dialog(get_icon(ICON_STOREFRONT), "Repository", "Loading interpreter info...", true);
    repository_json_data_t interp_data = {0};
    if (!load_project(server, &interp_data, interpreter_slug)) {
        message_dialog(get_icon(ICON_ERROR), "Repository",
                       "Interpreter not found in repository.\nYou may need to install it manually.", "OK");
        return;
    }

    // Build a wrapper object like the project list entries have: {"slug": "...", "project": {...}}
    cJSON* wrapper = cJSON_CreateObject();
    cJSON_AddStringToObject(wrapper, "slug", interpreter_slug);
    cJSON_AddItemToObject(wrapper, "project", cJSON_Duplicate(interp_data.json, true));
    free_repository_data_json(&interp_data);

    // Use the standard project install dialog
    menu_repository_client_project(buffer, theme, wrapper, false);
    cJSON_Delete(wrapper);
}

static bool execute_action(pax_buf_t* buffer, menu_repository_client_project_action_t action, gui_theme_t* theme,
                           cJSON* wrapper, bool is_plugin) {
    char server[128] = {0};
    nvs_settings_get_repo_server(server, sizeof(server), DEFAULT_REPO_SERVER);

    cJSON* slug_obj = cJSON_GetObjectItem(wrapper, "slug");
    cJSON* project  = cJSON_GetObjectItem(wrapper, "project");

    app_mgmt_location_t location;
    const char*         loc_text;
    switch (action) {
        case ACTION_INSTALL:
            location = is_plugin ? APP_MGMT_LOCATION_INTERNAL_PLUGINS : APP_MGMT_LOCATION_INTERNAL;
            loc_text = "internal memory";
            break;
        case ACTION_INSTALL_SD:
            location = is_plugin ? APP_MGMT_LOCATION_SD_PLUGINS : APP_MGMT_LOCATION_SD;
            loc_text = "SD card";
            break;
        default:
            return false;
    }

    const char* item_type = is_plugin ? "Plugin" : "App";

    char busy_msg[64];
    snprintf(busy_msg, sizeof(busy_msg), "Installing on %s...", loc_text);
    busy_dialog(get_icon(ICON_STOREFRONT), "Repository", busy_msg, true);

    esp_err_t res = app_mgmt_install(server, slug_obj->valuestring, location, download_callback);
    if (res != ESP_OK) {
        message_dialog(get_icon(ICON_ERROR), "Repository", "Installation failed", "OK");
        return false;
    }

    char success_msg[64];
    snprintf(success_msg, sizeof(success_msg), "%s successfully installed", item_type);
    message_dialog(get_icon(ICON_STOREFRONT), "Repository", success_msg, "OK");

    // Check if this is a script app that needs an interpreter (not applicable for plugins)
    if (!is_plugin && project != NULL) {
        const char* interpreter_slug = find_interpreter_slug(project);
        if (interpreter_slug != NULL && !interpreter_available(interpreter_slug)) {
            prompt_install_interpreter(buffer, theme, interpreter_slug);
        }
    }

    return true;
}

void menu_repository_client_project(pax_buf_t* buffer, gui_theme_t* theme, cJSON* wrapper, bool is_plugin) {
    busy_dialog(get_icon(ICON_STOREFRONT), "Repository", "Rendering project...", true);

    cJSON* project = cJSON_GetObjectItem(wrapper, "project");
    if (project == NULL) {
        ESP_LOGE(TAG, "Project object is NULL");
        return;
    }

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item(&menu, "Install on\nSD card", NULL, (void*)ACTION_INSTALL_SD, -1);
    menu_insert_item(&menu, "Install on\nInternal memory", NULL, (void*)ACTION_INSTALL, -1);

    render(buffer, theme, &menu, false, true, project);
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
                                menu_free(&menu);
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_LEFT:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, true, false, project);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, true, false, project);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                bool  installed =
                                    execute_action(buffer, (menu_repository_client_project_action_t)arg, theme, wrapper, is_plugin);
                                if (installed) {
                                    menu_free(&menu);
                                    return;
                                }
                                render(buffer, theme, &menu, false, true, project);
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
            render(buffer, theme, &menu, true, true, project);
        }
    }
}
