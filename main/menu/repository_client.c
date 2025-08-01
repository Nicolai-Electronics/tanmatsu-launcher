#include "repository_client.h"
#include <stdio.h>
#include <string.h>
#include "bsp/input.h"
#include "cJSON.h"
#include "common/display.h"
#include "esp_log.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "http_download.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_codecs.h"
#include "pax_types.h"
#include "wifi_connection.h"

#define MAX_PROJECTS 32

extern bool wifi_stack_get_initialized(void);

static const char* TAG = "Repository client";

static pax_buf_t icons[MAX_PROJECTS] = {0};

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

// Repository

static char*  data_projects = NULL;
static size_t size_projects = 0;
static cJSON* json_projects = NULL;

static char*  data_information = NULL;
static size_t size_information = 0;
static cJSON* json_information = NULL;

static bool load_information(void) {
    if (data_information == NULL) {
        char url[128];
        sprintf(url, "https://apps.tanmatsu.cloud/v1/information");
        bool success = download_ram(url, (uint8_t**)&data_information, &size_information);
        if (!success) return false;
    }
    if (data_information == NULL) return false;
    json_information = cJSON_ParseWithLength(data_information, size_information);
    if (json_information == NULL) return false;
    return true;
}

static void free_information(void) {
    if (json_information != NULL) {
        cJSON_Delete(json_information);
        json_information = NULL;
    }
    if (data_information != NULL) {
        free(data_information);
        data_information = NULL;
        size_information = 0;
    }
}

static bool load_projects(void) {
    if (data_projects == NULL) {
        char url[128];
        sprintf(url, "https://apps.tanmatsu.cloud/v1/projects?amount=%u", MAX_PROJECTS);
        bool success = download_ram(url, (uint8_t**)&data_projects, &size_projects);
        if (!success) return false;
    }
    if (data_projects == NULL) return false;
    json_projects = cJSON_ParseWithLength(data_projects, size_projects);
    if (json_projects == NULL) return false;
    return true;
}

static void free_projects(void) {
    if (json_projects != NULL) {
        cJSON_Delete(json_projects);
        json_projects = NULL;
    }
    if (data_projects != NULL) {
        free(data_projects);
        data_projects = NULL;
        size_projects = 0;
    }
}

static void populate_project_list(menu_t* menu) {
    cJSON* entry_obj;
    int    i = 0;
    cJSON_ArrayForEach(entry_obj, json_projects) {
        cJSON* slug_obj    = cJSON_GetObjectItem(entry_obj, "slug");
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
        menu_insert_item_icon(menu, name_obj->valuestring, NULL, (void*)i, -1, get_icon(ICON_DOWNLOADING));

        void* buffer = heap_caps_calloc(1, ICON_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        if (buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for icon for entry %u", i);
            continue;
        }
        pax_buf_init(&icons[i], buffer, ICON_WIDTH, ICON_HEIGHT, ICON_COLOR_FORMAT);
#if defined(CONFIG_BSP_TARGET_KAMI)
        icons[i].palette      = palette;
        icons[i].palette_size = sizeof(palette) / sizeof(pax_col_t);
#endif
        /*if (!pax_insert_png_buf(&icons[i], icon_buf, icon_size, 0, 0, 0)) {
            ESP_LOGE(TAG, "Failed to decode icon for entry %u", i);
        }*/

        i++;
    }
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, bool partial, bool icons) {
    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_REPOSITORY), "Repository"}}), 1,
                                     ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}),
                                     2, ((gui_header_field_t[]){{NULL, "↑ / ↓ Navigate ⏎ Select"}}), 1);
    }
    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_repository_client(pax_buf_t* buffer, gui_theme_t* theme) {
    if (!wifi_stack_get_initialized()) {
        ESP_LOGE(TAG, "WiFi stack not initialized");
        return;
    }

    if (!wifi_connection_is_connected()) {
        if (wifi_connect_try_all() != ESP_OK) {
            ESP_LOGE(TAG, "Not connected to WiFi");
            return;
        }
    }

    bool success = load_information();
    if (!success) {
        ESP_LOGE(TAG, "Failed to load repository information");
        return;
    }

    success = load_projects();
    if (!success) {
        ESP_LOGE(TAG, "Failed to load projects");
        return;
    }

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    populate_project_list(&menu);

    render(buffer, theme, &menu, false, true);
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
                                render(buffer, theme, &menu, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                // execute_action(buffer, (menu_home_action_t)arg, theme);
                                render(buffer, theme, &menu, false, true);
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
            render(buffer, theme, &menu, true, true);
        }
    }

    free_projects();
    free_information();
}
