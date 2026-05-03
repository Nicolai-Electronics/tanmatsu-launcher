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
#include "menu/menu_helpers.h"
#include "menu/message_dialog.h"
#include "nvs_settings.h"
#include "pax_fonts.h"
#include "pax_text.h"
#include "pax_types.h"
#include "repository_client.h"
#include "sdcard.h"
#include "shapes/pax_lines.h"

static const char* TAG = "Repository client: project";

typedef enum {
    ACTION_INSTALL = 0,
    ACTION_INSTALL_SD,
} menu_repository_client_project_action_t;

typedef struct {
    bool external_only;
    bool external_preferred;
    bool internal_only;
    bool internal_preferred;
    bool sd_present;
    bool internal_disabled;
    bool sd_disabled;
    bool default_internal;
    bool sd_required_warning;
} install_constraints_t;

static bool get_bool_field(cJSON* obj, const char* name) {
    cJSON* item = cJSON_GetObjectItem(obj, name);
    if (item == NULL || !cJSON_IsBool(item)) {
        return false;
    }
    return cJSON_IsTrue(item);
}

// Locate the application[] entry whose targets[] contains the current device name.
// Per metadata.json schema, per-target properties (type, interpreter, external_only,
// external_preferred, internal_only, internal_preferred, ...) live inside this entry,
// not at the top level. Returns NULL if no matching entry is found.
static cJSON* find_application_for_device(cJSON* project) {
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
        cJSON_ArrayForEach(target, targets) {
            if (target != NULL && cJSON_IsString(target) && strlen(target->valuestring) == device_name_len &&
                strncasecmp(target->valuestring, device_name, device_name_len) == 0) {
                return application;
            }
        }
    }
    return NULL;
}

static void resolve_constraints(cJSON* project, install_constraints_t* out) {
    cJSON* application      = find_application_for_device(project);
    cJSON* source           = application != NULL ? application : project;
    out->external_only      = get_bool_field(source, "external_only");
    out->external_preferred = get_bool_field(source, "external_preferred");
    out->internal_only      = get_bool_field(source, "internal_only");
    out->internal_preferred = get_bool_field(source, "internal_preferred");
    out->sd_present         = (sd_status() == SD_STATUS_OK);

    // If both *_only flags are set the metadata is contradictory; let internal_only win
    // because the device always has internal storage but may not have an SD card inserted.
    out->internal_disabled = out->external_only && !out->internal_only;
    out->sd_disabled       = out->internal_only || !out->sd_present;

    bool prefer_internal;
    if (out->internal_preferred && out->external_preferred) {
        prefer_internal = true;
    } else if (out->internal_preferred) {
        prefer_internal = true;
    } else if (out->external_preferred) {
        prefer_internal = false;
    } else {
        prefer_internal = true;
    }
    if (out->internal_disabled) {
        prefer_internal = false;
    } else if (out->sd_disabled) {
        prefer_internal = true;
    }
    out->default_internal = prefer_internal;

    out->sd_required_warning = out->external_only && !out->sd_present;
}

static void render_project(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, cJSON* project,
                           const install_constraints_t* constraints) {

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

    if (constraints != NULL && constraints->sd_required_warning) {
        pax_draw_text(buffer, 0xFFFF0000, theme->menu.text_font, font_size, position.x0, position.y0 + font_size * 5,
                      "SD card is required for installation");
    }
}

// Draw a red "X" across one cell of the install location grid (slot 0 = SD, slot 1 = Internal).
// Geometry must mirror menu_render_grid in components/gui/gui_menu_render.c.
// Uses pax_simple_line (orientation-aware) — pax_draw_thick_line skips the buffer orientation
// transform, which puts it in the wrong place on rotated displays like the Tanmatsu.
static void draw_disabled_overlay(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t menu_position, int slot) {
    int   entry_count_x = 2;
    int   entry_count_y = 1;
    float entry_width =
        ((menu_position.x1 - menu_position.x0) - (theme->menu.horizontal_margin * (entry_count_x + 1))) / entry_count_x;
    float entry_height =
        ((menu_position.y1 - menu_position.y0) - (theme->menu.vertical_margin * (entry_count_y + 1))) / entry_count_y;

    float x = menu_position.x0 + theme->menu.horizontal_margin + (slot * (entry_width + theme->menu.horizontal_margin));
    float y = menu_position.y0 + theme->menu.vertical_margin;

    float     inset = 6.0f;
    pax_col_t color = 0xFFFF0000;  // bright red

    float ax0 = x + inset;
    float ay0 = y + inset;
    float ax1 = x + entry_width - inset;
    float ay1 = y + entry_height - inset;
    float bx0 = x + entry_width - inset;
    float by0 = y + inset;
    float bx1 = x + inset;
    float by1 = y + entry_height - inset;

    // Fake thick lines with parallel offsets perpendicular to the diagonals.
    for (int o = -2; o <= 2; o++) {
        pax_simple_line(buffer, color, ax0 + o, ay0 - o, ax1 + o, ay1 - o);
        pax_simple_line(buffer, color, bx0 + o, by0 + o, bx1 + o, by1 + o);
    }
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, bool partial, bool icons, cJSON* project,
                   const install_constraints_t* constraints) {
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    if (!partial || icons) {
        render_base_screen_statusbar(
            buffer, theme, !partial, !partial || icons, !partial,
            ((gui_element_icontext_t[]){{get_icon(ICON_STOREFRONT), "Repository"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
            ((gui_element_icontext_t[]){{NULL, "← / → | ⏎ Select"}}), 1);
    }

    // Description
    pax_vec2_t desc_position = menu_calc_position(buffer, theme);

    render_project(buffer, theme, desc_position, project, constraints);

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

    if (constraints != NULL) {
        if (constraints->sd_disabled) {
            draw_disabled_overlay(buffer, theme, menu_position, 0);
        }
        if (constraints->internal_disabled) {
            draw_disabled_overlay(buffer, theme, menu_position, 1);
        }
    }

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
    progress_dialog(get_icon(ICON_DOWNLOADING), "Downloading", text, percentage, true);
};

// Find the interpreter slug for the current device from project metadata.
// Returns NULL if the app is not a script type or has no interpreter.
// The returned string points into the cJSON tree — do not free.
static const char* find_interpreter_slug(cJSON* project) {
    cJSON* application = find_application_for_device(project);
    if (application == NULL) {
        return NULL;
    }
    cJSON* type_obj = cJSON_GetObjectItem(application, "type");
    if (type_obj == NULL || !cJSON_IsString(type_obj) || strcmp(type_obj->valuestring, "script") != 0) {
        return NULL;
    }
    cJSON* interp_obj = cJSON_GetObjectItem(application, "interpreter");
    if (interp_obj == NULL || !cJSON_IsString(interp_obj)) {
        return NULL;
    }
    return interp_obj->valuestring;
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
                           cJSON* wrapper, bool is_plugin, const install_constraints_t* constraints) {
    char server[128] = {0};
    nvs_settings_get_repo_server(server, sizeof(server), DEFAULT_REPO_SERVER);

    cJSON* slug_obj = cJSON_GetObjectItem(wrapper, "slug");
    cJSON* project  = cJSON_GetObjectItem(wrapper, "project");

    app_mgmt_location_t location;
    const char*         loc_text;
    switch (action) {
        case ACTION_INSTALL:
            if (constraints != NULL && constraints->internal_disabled) {
                message_dialog(get_icon(ICON_ERROR), "Repository", "This app must be installed on the SD card.", "OK");
                return false;
            }
            location = is_plugin ? APP_MGMT_LOCATION_INTERNAL_PLUGINS : APP_MGMT_LOCATION_INTERNAL;
            loc_text = "internal memory";
            break;
        case ACTION_INSTALL_SD:
            if (constraints != NULL && constraints->sd_disabled) {
                const char* msg = constraints->internal_only ? "This app must be installed on internal memory."
                                                             : "Insert an SD card to install here.";
                message_dialog(get_icon(ICON_ERROR), "Repository", msg, "OK");
                return false;
            }
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
        const char* err_msg = "Installation failed";
        if (res == ESP_ERR_NO_MEM) {
            err_msg = "Installation failed: AppFS full";
        }
        message_dialog(get_icon(ICON_ERROR), "Repository", err_msg, "OK");
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

    install_constraints_t constraints = {0};
    resolve_constraints(project, &constraints);

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item(&menu, "Install on\nSD card", NULL, (void*)ACTION_INSTALL_SD, -1);
    menu_insert_item(&menu, "Install on\nInternal memory", NULL, (void*)ACTION_INSTALL, -1);
    menu_set_position(&menu, constraints.default_internal ? 1 : 0);

    render(buffer, theme, &menu, false, true, project, &constraints);
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
                                render(buffer, theme, &menu, true, false, project, &constraints);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, true, false, project, &constraints);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                void* arg       = menu_get_callback_args(&menu, menu_get_position(&menu));
                                bool  installed = execute_action(buffer, (menu_repository_client_project_action_t)arg,
                                                                 theme, wrapper, is_plugin, &constraints);
                                if (installed) {
                                    menu_free(&menu);
                                    return;
                                }
                                render(buffer, theme, &menu, false, true, project, &constraints);
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
            render(buffer, theme, &menu, true, true, project, &constraints);
        }
    }
}
