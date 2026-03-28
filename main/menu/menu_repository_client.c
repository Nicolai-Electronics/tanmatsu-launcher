#include "menu_repository_client.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "app_management.h"
#include "app_metadata_parser.h"
#include "bsp/device.h"
#include "bsp/input.h"
#include "cJSON.h"
#include "common/display.h"
#include "device_settings.h"
#include "esp_log.h"
#include "filesystem_utils.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "http_download.h"
#include "icons.h"
#include "mbedtls/base64.h"
#include "menu/message_dialog.h"
#include "menu_repository_client_project.h"
#include "pax_codecs.h"
#include "pax_text.h"
#include "pax_types.h"
#include "repository_client.h"
#include "wifi_connection.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Select"}}), 1
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | 🅱 Back 🅰 Select"}}), 1
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#endif

extern bool wifi_stack_get_initialized(void);

static const char* TAG = "Repository client";

repository_json_data_t projects = {0};

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

typedef enum {
    INSTALL_STATUS_NOT_INSTALLED = 0,
    INSTALL_STATUS_INSTALLED,
    INSTALL_STATUS_UPDATE_AVAILABLE,
} install_status_t;

typedef struct {
    const char*      name;
    const char*      slug;
    int              index;
    pax_buf_t*       icon;
    install_status_t     status;
    app_mgmt_location_t install_location;  // Where the app is installed (if applicable)
} project_sort_entry_t;

// Read the "version" field from an installed app's metadata.json.
// Returns a malloc'd string or NULL. Caller must free.
static char* get_installed_version(const char* base_path, const char* slug) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s/metadata.json", base_path, slug);

    FILE* fd = fopen(path, "r");
    if (fd == NULL) return NULL;

    fseek(fd, 0, SEEK_END);
    size_t fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    char* data = malloc(fsize + 1);
    if (data == NULL) {
        fclose(fd);
        return NULL;
    }
    fread(data, 1, fsize, fd);
    data[fsize] = '\0';
    fclose(fd);

    cJSON* root = cJSON_Parse(data);
    free(data);
    if (root == NULL) return NULL;

    char* version = NULL;
    cJSON* version_obj = cJSON_GetObjectItem(root, "version");
    if (version_obj != NULL && cJSON_IsString(version_obj)) {
        version = strdup(version_obj->valuestring);
    } else if (version_obj != NULL && cJSON_IsNumber(version_obj)) {
        size_t len = snprintf(NULL, 0, "%d", version_obj->valueint);
        version = malloc(len + 1);
        if (version != NULL) {
            snprintf(version, len + 1, "%d", version_obj->valueint);
        }
    }

    cJSON_Delete(root);
    return version;
}

// Check install status of an app by slug. Compares version strings.
// Sets location if installed.
static install_status_t check_install_status(const char* slug, const char* repo_version,
                                              app_mgmt_location_t* out_location) {
    const char* base_paths[]    = {"/sd/apps", "/int/apps"};  // SD priority
    app_mgmt_location_t locs[] = {APP_MGMT_LOCATION_SD, APP_MGMT_LOCATION_INTERNAL};

    for (int i = 0; i < 2; i++) {
        char check_path[256];
        snprintf(check_path, sizeof(check_path), "%s/%s", base_paths[i], slug);
        if (!fs_utils_exists(check_path)) continue;

        if (out_location != NULL) {
            *out_location = locs[i];
        }

        char* installed_version = get_installed_version(base_paths[i], slug);
        if (installed_version == NULL) {
            return INSTALL_STATUS_INSTALLED;  // Installed but can't read version
        }

        bool versions_match = (repo_version != NULL && strcmp(installed_version, repo_version) == 0);
        free(installed_version);

        return versions_match ? INSTALL_STATUS_INSTALLED : INSTALL_STATUS_UPDATE_AVAILABLE;
    }
    return INSTALL_STATUS_NOT_INSTALLED;
}

// Decode a base64-encoded PNG into a pax_buf_t. Returns NULL on failure.
static pax_buf_t* decode_base64_icon(const char* base64_data) {
    size_t b64_len = strlen(base64_data);
    size_t decoded_len = 0;

    // Get decoded size
    if (mbedtls_base64_decode(NULL, 0, &decoded_len, (const unsigned char*)base64_data, b64_len) != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        return NULL;
    }

    uint8_t* png_data = malloc(decoded_len);
    if (png_data == NULL) {
        return NULL;
    }

    size_t actual_len = 0;
    if (mbedtls_base64_decode(png_data, decoded_len, &actual_len, (const unsigned char*)base64_data, b64_len) != 0) {
        free(png_data);
        return NULL;
    }

    pax_buf_t* icon = calloc(1, sizeof(pax_buf_t));
    if (icon == NULL) {
        free(png_data);
        return NULL;
    }

    if (!pax_decode_png_buf(icon, png_data, actual_len, ICON_COLOR_FORMAT, 0)) {
        free(icon);
        free(png_data);
        return NULL;
    }

    free(png_data);
    return icon;
}

// Free all icons attached to menu items
static void free_menu_icons(menu_t* menu) {
    for (size_t i = 0; i < menu_get_length(menu); i++) {
        pax_buf_t* icon = menu_get_icon(menu, i);
        if (icon != NULL) {
            pax_buf_destroy(icon);
            free(icon);
        }
    }
}

// Parallel array of install status, indexed by menu position (after sorting).
// Allocated in populate_project_list, freed by caller.
static install_status_t*    project_statuses   = NULL;
static app_mgmt_location_t* project_locations  = NULL;
static int*                 project_indices    = NULL;
static int                  project_count      = 0;
static bool                 has_updates        = false;

static cJSON* get_project_by_index(cJSON* json_projects, int index) {
    return cJSON_GetArrayItem(json_projects, index);
}

static void free_project_info(void) {
    free(project_statuses);
    free(project_locations);
    free(project_indices);
    project_statuses  = NULL;
    project_locations = NULL;
    project_indices   = NULL;
    project_count     = 0;
    has_updates       = false;
}

static int compare_projects_by_name(const void* a, const void* b) {
    const project_sort_entry_t* ea = (const project_sort_entry_t*)a;
    const project_sort_entry_t* eb = (const project_sort_entry_t*)b;
    return strcasecmp(ea->name, eb->name);
}

static void populate_project_list(menu_t* menu, cJSON* json_projects) {
    // Count valid entries
    int    total = 0;
    cJSON* entry_obj;
    cJSON_ArrayForEach(entry_obj, json_projects) {
        cJSON* project_obj = cJSON_GetObjectItem(entry_obj, "project");
        if (project_obj != NULL && cJSON_GetObjectItem(project_obj, "name") != NULL) {
            total++;
        }
    }

    if (total == 0) return;

    // Collect entries for sorting
    project_sort_entry_t* sorted = malloc(sizeof(project_sort_entry_t) * total);
    if (sorted == NULL) return;

    int i = 0;
    int idx = 0;
    cJSON_ArrayForEach(entry_obj, json_projects) {
        cJSON* slug_obj = cJSON_GetObjectItem(entry_obj, "slug");
        if (slug_obj == NULL) {
            ESP_LOGE(TAG, "Slug object is NULL for entry %d", idx);
            idx++;
            continue;
        }
        cJSON* project_obj = cJSON_GetObjectItem(entry_obj, "project");
        if (project_obj == NULL) {
            ESP_LOGE(TAG, "Project object is NULL for entry %d", idx);
            idx++;
            continue;
        }
        cJSON* name_obj = cJSON_GetObjectItem(project_obj, "name");
        if (name_obj == NULL) {
            ESP_LOGE(TAG, "Name object is NULL for entry %d", idx);
            idx++;
            continue;
        }
        sorted[i].name  = name_obj->valuestring;
        sorted[i].slug  = slug_obj->valuestring;
        sorted[i].index = idx;
        sorted[i].icon  = NULL;

        // Check install status by comparing version strings
        sorted[i].install_location = APP_MGMT_LOCATION_INTERNAL;
        cJSON* version_obj = cJSON_GetObjectItem(project_obj, "version");
        char   repo_version_buf[32] = {0};
        const char* repo_version    = NULL;
        if (version_obj != NULL && cJSON_IsString(version_obj)) {
            repo_version = version_obj->valuestring;
        } else if (version_obj != NULL && cJSON_IsNumber(version_obj)) {
            snprintf(repo_version_buf, sizeof(repo_version_buf), "%d", version_obj->valueint);
            repo_version = repo_version_buf;
        }
        sorted[i].status = check_install_status(slug_obj->valuestring, repo_version,
                                                 &sorted[i].install_location);

        // Decode optional base64 icon
        cJSON* icon_obj = cJSON_GetObjectItem(entry_obj, "icon");
        if (icon_obj != NULL && cJSON_IsString(icon_obj)) {
            sorted[i].icon = decode_base64_icon(icon_obj->valuestring);
        }

        i++;
        idx++;
    }

    qsort(sorted, i, sizeof(project_sort_entry_t), compare_projects_by_name);

    // Save status info in parallel arrays (indexed by menu position after sort)
    free_project_info();
    project_statuses  = malloc(sizeof(install_status_t) * i);
    project_locations = malloc(sizeof(app_mgmt_location_t) * i);
    project_indices   = malloc(sizeof(int) * i);
    project_count     = i;
    has_updates       = false;

    for (int j = 0; j < i; j++) {
        const char* prefix;
        switch (sorted[j].status) {
            case INSTALL_STATUS_UPDATE_AVAILABLE:
                prefix      = "[U]";
                has_updates = true;
                break;
            case INSTALL_STATUS_INSTALLED:
                prefix = "[I]";
                break;
            default:
                prefix = "   ";
                break;
        }
        char label[128];
        snprintf(label, sizeof(label), "%s %s", prefix, sorted[j].name);
        menu_insert_item_icon(menu, label, NULL, (void*)sorted[j].index, -1, sorted[j].icon);

        if (project_statuses)  project_statuses[j]  = sorted[j].status;
        if (project_locations) project_locations[j] = sorted[j].install_location;
        if (project_indices)   project_indices[j]   = sorted[j].index;
    }

    free(sorted);
}

static void download_callback(size_t download_position, size_t file_size, const char* status_text) {
    if (file_size == 0) return;
    uint8_t        percentage      = 100 * download_position / file_size;
    static uint8_t last_percentage = 0;
    if (percentage == last_percentage) return;
    last_percentage = percentage;
    char text[64];
    snprintf(text, sizeof(text), "%s (%u%%)", status_text ? status_text : "Downloading", percentage);
    busy_dialog(get_icon(ICON_DOWNLOADING), "Downloading", text, true);
}

static install_status_t previous_render_status = INSTALL_STATUS_NOT_INSTALLED;

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, const char* server, bool partial, bool icons) {
    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    // Check if footer needs redrawing due to status change of selected item
    install_status_t current_status = INSTALL_STATUS_NOT_INSTALLED;
    size_t           pos            = menu_get_position(menu);
    if (pos < (size_t)project_count && project_statuses != NULL) {
        current_status = project_statuses[pos];
    }
    if (current_status != previous_render_status) {
        previous_render_status = current_status;
        partial = false;
    }

    if (!partial || icons) {
        // Determine action text based on selected item's install status
        const char* action_text;
        switch (current_status) {
            case INSTALL_STATUS_UPDATE_AVAILABLE:
                action_text = "⏎ Update";
                break;
            case INSTALL_STATUS_INSTALLED:
                action_text = "⏎ Re-Install";
                break;
            default:
                action_text = "⏎ Install";
                break;
        }

        char footer_right_text[64];
        snprintf(footer_right_text, sizeof(footer_right_text), "↑ / ↓ | %s", action_text);

        char server_info[160];
        snprintf(server_info, sizeof(server_info), "Server: %s", server);
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
        {
            bool is_installed = (current_status == INSTALL_STATUS_INSTALLED ||
                                 current_status == INSTALL_STATUS_UPDATE_AVAILABLE);
            gui_element_icontext_t footer_left[4];
            int                    footer_left_count = 0;
            footer_left[footer_left_count++] = (gui_element_icontext_t){get_icon(ICON_ESC), "/"};
            footer_left[footer_left_count++] = (gui_element_icontext_t){get_icon(ICON_F1), "Back"};
            if (is_installed) {
                footer_left[footer_left_count++] = (gui_element_icontext_t){get_icon(ICON_F5), "Remove"};
            }
            if (has_updates) {
                footer_left[footer_left_count++] = (gui_element_icontext_t){get_icon(ICON_F6), "Update all"};
            }
            render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                         ((gui_element_icontext_t[]){{get_icon(ICON_STOREFRONT), "Repository"}}), 1,
                                         footer_left, footer_left_count,
                                         ((gui_element_icontext_t[]){{NULL, footer_right_text}}), 1);
        }
#else
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_STOREFRONT), "Repository"}}), 1,
                                     FOOTER_LEFT, ((gui_element_icontext_t[]){{NULL, footer_right_text}}), 1);
#endif
    }
    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_repository_client(pax_buf_t* buffer, gui_theme_t* theme) {
    busy_dialog(get_icon(ICON_STOREFRONT), "Repository", "Connecting to WiFi...", true);

    if (!wifi_stack_get_initialized()) {
        ESP_LOGE(TAG, "WiFi stack not initialized");
        message_dialog(get_icon(ICON_STOREFRONT), "Repository: fatal error", "WiFi stack not initialized", "Quit");
        return;
    }

    if (!wifi_connection_is_connected()) {
        if (wifi_connect_try_all() != ESP_OK) {
            ESP_LOGE(TAG, "Not connected to WiFi");
            message_dialog(get_icon(ICON_STOREFRONT), "Repository: fatal error", "Failed to connect to WiFi network",
                           "Quit");
            return;
        }
    }

    busy_dialog(get_icon(ICON_STOREFRONT), "Repository", "Downloading list of projects...", true);

    char server[128] = {0};
    device_settings_get_repo_server(server, sizeof(server));
    bool success = load_projects(server, &projects, NULL);
    if (!success) {
        ESP_LOGE(TAG, "Failed to load projects");
        message_dialog(get_icon(ICON_STOREFRONT), "Repository: fatal error", "Failed to load projects from server",
                       "Quit");
        return;
    }

    busy_dialog(get_icon(ICON_STOREFRONT), "Repository", "Rendering list of projects...", true);

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    populate_project_list(&menu, projects.json);

    render(buffer, theme, &menu, server, false, true);
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
                                free_project_info();
                                free_menu_icons(&menu);
                                menu_free(&menu);
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, server, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, server, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                size_t pos = menu_get_position(&menu);
                                if (pos < (size_t)project_count && project_statuses != NULL &&
                                    project_statuses[pos] != INSTALL_STATUS_NOT_INSTALLED) {
                                    // Update or re-install: confirm, use existing location
                                    const char* action_name =
                                        (project_statuses[pos] == INSTALL_STATUS_UPDATE_AVAILABLE)
                                            ? "Update" : "Re-install";
                                    char confirm_msg[128];
                                    cJSON* wrapper = get_project_by_index(projects.json, project_indices[pos]);
                                    cJSON* slug_obj = wrapper ? cJSON_GetObjectItem(wrapper, "slug") : NULL;
                                    snprintf(confirm_msg, sizeof(confirm_msg), "%s this app?", action_name);
                                    message_dialog_return_type_t msg_ret = adv_dialog_yes_no(
                                        get_icon(ICON_HELP), action_name, confirm_msg);
                                    if (msg_ret == MSG_DIALOG_RETURN_OK && slug_obj != NULL) {
                                        busy_dialog(get_icon(ICON_STOREFRONT), "Repository",
                                                    "Installing...", true);
                                        app_mgmt_install(server, slug_obj->valuestring,
                                                         project_locations[pos], download_callback);
                                    }
                                    // Rebuild menu to refresh status markers
                                    free_menu_icons(&menu);
                                    menu_free(&menu);
                                    menu_initialize(&menu);
                                    size_t saved_pos = pos;
                                    populate_project_list(&menu, projects.json);
                                    if (saved_pos >= menu_get_length(&menu) && menu_get_length(&menu) > 0) {
                                        saved_pos = menu_get_length(&menu) - 1;
                                    }
                                    menu_set_position(&menu, saved_pos);
                                } else {
                                    // Not installed: open project detail with location picker
                                    void*  arg     = menu_get_callback_args(&menu, pos);
                                    cJSON* wrapper = get_project_by_index(projects.json, (int)(arg));
                                    if (wrapper == NULL) {
                                        ESP_LOGE(TAG, "Wrapper object is NULL");
                                        break;
                                    }
                                    menu_repository_client_project(buffer, theme, wrapper);
                                    // Rebuild menu to refresh status markers (app may have been installed)
                                    free_menu_icons(&menu);
                                    menu_free(&menu);
                                    menu_initialize(&menu);
                                    populate_project_list(&menu, projects.json);
                                    if (pos >= menu_get_length(&menu) && menu_get_length(&menu) > 0) {
                                        pos = menu_get_length(&menu) - 1;
                                    }
                                    menu_set_position(&menu, pos);
                                }
                                render(buffer, theme, &menu, server, false, true);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_F5: {
                                size_t pos = menu_get_position(&menu);
                                if (pos >= (size_t)project_count || project_statuses == NULL) break;
                                if (project_statuses[pos] == INSTALL_STATUS_NOT_INSTALLED) break;

                                cJSON* wrapper  = get_project_by_index(projects.json, project_indices[pos]);
                                cJSON* slug_obj = wrapper ? cJSON_GetObjectItem(wrapper, "slug") : NULL;
                                if (slug_obj == NULL) break;

                                message_dialog_return_type_t msg_ret = adv_dialog_yes_no(
                                    get_icon(ICON_HELP), "Delete App",
                                    "Do you really want to delete the app?");
                                if (msg_ret == MSG_DIALOG_RETURN_OK) {
                                    app_mgmt_uninstall(slug_obj->valuestring, APP_MGMT_LOCATION_INTERNAL);
                                    app_mgmt_uninstall(slug_obj->valuestring, APP_MGMT_LOCATION_SD);

                                    // Rebuild menu to refresh status markers
                                    free_menu_icons(&menu);
                                    menu_free(&menu);
                                    menu_initialize(&menu);
                                    populate_project_list(&menu, projects.json);
                                    if (pos >= menu_get_length(&menu) && menu_get_length(&menu) > 0) {
                                        pos = menu_get_length(&menu) - 1;
                                    }
                                    menu_set_position(&menu, pos);
                                }
                                render(buffer, theme, &menu, server, false, true);
                                break;
                            }
                            case BSP_INPUT_NAVIGATION_KEY_F6: {
                                if (!has_updates) break;
                                message_dialog_return_type_t msg_ret = adv_dialog_yes_no(
                                    get_icon(ICON_HELP), "Update all",
                                    "Update all apps with available updates?");
                                if (msg_ret == MSG_DIALOG_RETURN_OK) {
                                    int updated = 0;
                                    int failed  = 0;
                                    int total   = 0;
                                    for (int j = 0; j < project_count; j++) {
                                        if (project_statuses[j] != INSTALL_STATUS_UPDATE_AVAILABLE) continue;
                                        total++;
                                        cJSON* wrapper  = get_project_by_index(projects.json, project_indices[j]);
                                        cJSON* slug_obj = wrapper ? cJSON_GetObjectItem(wrapper, "slug") : NULL;
                                        if (slug_obj == NULL) { failed++; continue; }
                                        char msg[64];
                                        snprintf(msg, sizeof(msg), "Updating %s (%d/%d)...",
                                                 slug_obj->valuestring, updated + failed + 1, total);
                                        busy_dialog(get_icon(ICON_STOREFRONT), "Updating", msg, true);
                                        esp_err_t res = app_mgmt_install(server, slug_obj->valuestring,
                                                                          project_locations[j], download_callback);
                                        if (res == ESP_OK) {
                                            updated++;
                                        } else {
                                            failed++;
                                        }
                                    }
                                    if (failed > 0) {
                                        char summary[64];
                                        snprintf(summary, sizeof(summary),
                                                 "Updated %d/%d apps. %d failed.", updated, total, failed);
                                        message_dialog(get_icon(ICON_ERROR), "Update all", summary, "OK");
                                    }
                                    // Rebuild menu to refresh status markers
                                    size_t saved_pos = menu_get_position(&menu);
                                    free_menu_icons(&menu);
                                    menu_free(&menu);
                                    menu_initialize(&menu);
                                    populate_project_list(&menu, projects.json);
                                    if (saved_pos >= menu_get_length(&menu) && menu_get_length(&menu) > 0) {
                                        saved_pos = menu_get_length(&menu) - 1;
                                    }
                                    menu_set_position(&menu, saved_pos);
                                }
                                render(buffer, theme, &menu, server, false, true);
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
            render(buffer, theme, &menu, server, true, true);
        }
    }

    free_repository_data_json(&projects);
}
