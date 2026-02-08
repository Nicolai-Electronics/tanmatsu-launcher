#include "menu_repository_client.h"
#include <stdio.h>
#include <string.h>
#include "bsp/input.h"
#include "cJSON.h"
#include "common/display.h"
#include "device_settings.h"
#include "esp_log.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "http_download.h"
#include "icons.h"
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

static void populate_project_list(menu_t* menu, cJSON* json_projects) {
    cJSON* entry_obj;
    int    i = 0;
    cJSON_ArrayForEach(entry_obj, json_projects) {
        cJSON* slug_obj = cJSON_GetObjectItem(entry_obj, "slug");
        if (slug_obj == NULL) {
            ESP_LOGE(TAG, "Slug object is NULL for entry %d", i);
            continue;
        }
        cJSON* project_obj = cJSON_GetObjectItem(entry_obj, "project");
        if (project_obj == NULL) {
            ESP_LOGE(TAG, "Project object is NULL for entry %d", i);
            continue;
        }
        cJSON* name_obj = cJSON_GetObjectItem(project_obj, "name");
        if (name_obj == NULL) {
            ESP_LOGE(TAG, "Name object is NULL for entry %d", i);
            continue;
        }
        printf("Project [%s]: %s\r\n", slug_obj->valuestring, name_obj->valuestring);
        menu_insert_item(menu, name_obj->valuestring, NULL, (void*)i, -1);
        i++;
    }
}

static cJSON* get_project_by_index(cJSON* json_projects, int index) {
    return cJSON_GetArrayItem(json_projects, index);
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, const char* server, bool partial, bool icons) {
    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    if (!partial || icons) {
        char server_info[160];
        snprintf(server_info, sizeof(server_info), "Server: %s", server);
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_STOREFRONT), "Repository"}}), 1,
                                     ((gui_element_icontext_t[]){
                                         {get_icon(ICON_ESC), "/"},
                                         {get_icon(ICON_F1), "Back    "},
                                         {get_icon(ICON_GLOBE), server_info},
                                     }),
                                     3, ((gui_element_icontext_t[]){{NULL, "  ↑ / ↓ | ⏎ Select"}}), 1);
#else
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_STOREFRONT), "Repository"}}), 1,
                                     FOOTER_LEFT, FOOTER_RIGHT);
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
                                void*  arg     = menu_get_callback_args(&menu, menu_get_position(&menu));
                                cJSON* wrapper = get_project_by_index(projects.json, (int)(arg));
                                if (wrapper == NULL) {
                                    ESP_LOGE(TAG, "Wrapper object is NULL");
                                    break;
                                }
                                menu_repository_client_project(buffer, theme, wrapper);
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
