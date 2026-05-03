#include "menu_repository_client_categories.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "bsp/input.h"
#include "cJSON.h"
#include "common/display.h"
#include "common/theme.h"
#include "device_settings.h"
#include "esp_log.h"
#include "filesystem_utils.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/menu_helpers.h"
#include "menu/message_dialog.h"
#include "menu_repository_client.h"
#include "message_dialog.h"
#include "nvs_settings_repository.h"
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

repository_json_data_t categories = {0};

static void populate_categories_list(menu_t* menu, cJSON* json_categories) {
    menu_insert_item(menu, "All categories", NULL, NULL, -1);

    cJSON* entry_obj;
    if (!cJSON_IsArray(json_categories)) {
        ESP_LOGE(TAG, "Result from server was not an array");
        return;
    }
    cJSON_ArrayForEach(entry_obj, json_categories) {
        if (!cJSON_IsString(entry_obj)) {
            ESP_LOGE(TAG, "Skipped item because it was not a string");
            continue;
        }
        char* category = entry_obj->valuestring;
        if (strlen(category) < 1) {
            continue;
        }

        // Temporary way for adjusting name due to repository api limitation
        char label[64] = "";
        snprintf(label, sizeof(label), "%s", category);
        label[0] = toupper(label[0]);
        for (size_t i = 0; i < strlen(label); i++) {
            if (label[i] == '_') {
                label[i] = ' ';
            }
        }

        menu_insert_item(menu, label, NULL, (void*)category, -1);
    }
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, const char* server, bool partial, bool icons) {
    pax_vec2_t position = menu_calc_position(buffer, theme);
    if (!partial || icons) {
        char server_info[160];
        snprintf(server_info, sizeof(server_info), "Server: %s", server);

        char* header_title = "Repository";

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
        {
            gui_element_icontext_t footer_left[5];
            int                    footer_left_count = 0;
            footer_left[footer_left_count++]         = (gui_element_icontext_t){get_icon(ICON_ESC), "/"};
            footer_left[footer_left_count++]         = (gui_element_icontext_t){get_icon(ICON_F1), "Back"};

            render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                         ((gui_element_icontext_t[]){{get_icon(ICON_STOREFRONT), header_title}}), 1,
                                         footer_left, footer_left_count,
                                         ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Load projects"}}), 1);
        }
#else
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_STOREFRONT), "Repository"}}), 1,
                                     FOOTER_LEFT, ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Load projects"}}), 1);
#endif
    }
    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_repository_client_categories(void) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

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

    busy_dialog(get_icon(ICON_STOREFRONT), "Repository", "Downloading list of categories...", true);

    char server[128] = {0};
    nvs_settings_get_repo_server(server, sizeof(server), DEFAULT_REPO_SERVER);
    bool success = load_categories(server, &categories);
    if (!success) {
        ESP_LOGE(TAG, "Failed to load projects");
        message_dialog(get_icon(ICON_STOREFRONT), "Repository: fatal error", "Failed to load categories from server",
                       "Quit");
        return;
    }

    busy_dialog(get_icon(ICON_STOREFRONT), "Repository", "Rendering list of categories...", true);

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    populate_categories_list(&menu, categories.json);

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
                                size_t pos = menu_get_position(&menu);
                                menu_repository_client(menu_get_callback_args(&menu, pos));
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
}
