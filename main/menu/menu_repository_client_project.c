#include "menu_repository_client_project.h"
#include <stdio.h>
#include <string.h>
#include <sys/_intsup.h>
#include "app_management.h"
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
#include "pax_fonts.h"
#include "pax_text.h"
#include "pax_types.h"
#include "repository_client.h"
#include "wifi_connection.h"

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
    pax_draw_text(buffer, theme->palette.color_foreground, pax_font_sky_mono, font_size, position.x0,
                  position.y0 + font_size * 0, text_buffer);
    sprintf(text_buffer, "Description: %s", description_obj ? description_obj->valuestring : "Unknown");
    pax_draw_text(buffer, theme->palette.color_foreground, pax_font_sky_mono, font_size, position.x0,
                  position.y0 + font_size * 1, text_buffer);
    sprintf(text_buffer, "Version: %s", version_obj ? version_obj->valuestring : "Unknown");
    pax_draw_text(buffer, theme->palette.color_foreground, pax_font_sky_mono, font_size, position.x0,
                  position.y0 + font_size * 2, text_buffer);
    sprintf(text_buffer, "Author: %s", author_obj ? author_obj->valuestring : "Unknown");
    pax_draw_text(buffer, theme->palette.color_foreground, pax_font_sky_mono, font_size, position.x0,
                  position.y0 + font_size * 3, text_buffer);
    sprintf(text_buffer, "License: %s", license_type_obj ? license_type_obj->valuestring : "Unknown");
    pax_draw_text(buffer, theme->palette.color_foreground, pax_font_sky_mono, font_size, position.x0,
                  position.y0 + font_size * 4, text_buffer);
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, bool partial, bool icons, cJSON* project) {
    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_REPOSITORY), "Repository"}}), 1,
                                     ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}),
                                     2, ((gui_header_field_t[]){{NULL, "← / → Navigate ⏎ Select"}}), 1);
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

static void render_dialog(pax_buf_t* buffer, gui_theme_t* theme, const char* message) {
    render_base_screen_statusbar(buffer, theme, true, true, true,
                                 ((gui_header_field_t[]){{get_icon(ICON_REPOSITORY), "Repository"}}), 1, NULL, 0, NULL,
                                 0);
    pax_center_text(buffer, 0xFF000000, theme->menu.text_font, 24, pax_buf_get_width(buffer) / 2.0f,
                    (pax_buf_get_height(buffer) - 24) / 2.0f, message);
    display_blit_buffer(buffer);
}

static void execute_action(pax_buf_t* buffer, menu_repository_client_project_action_t action, gui_theme_t* theme,
                           cJSON* wrapper) {

    cJSON* slug_obj = cJSON_GetObjectItem(wrapper, "slug");
    switch (action) {
        case ACTION_INSTALL: {
            render_dialog(buffer, theme, "Installing on internal memory...");
            if (app_mgmt_install("https://apps.tanmatsu.cloud", slug_obj->valuestring, APP_MGMT_LOCATION_INTERNAL) !=
                ESP_OK) {
                render_dialog(buffer, theme, "Installation failed!");
            } else {
                render_dialog(buffer, theme, "Installation successful!");
            }
            break;
        }
        case ACTION_INSTALL_SD: {
            render_dialog(buffer, theme, "Installing on SD card...");
            if (app_mgmt_install("https://apps.tanmatsu.cloud", slug_obj->valuestring, APP_MGMT_LOCATION_SD) !=
                ESP_OK) {
                render_dialog(buffer, theme, "Installation failed!");
            } else {
                render_dialog(buffer, theme, "Installation successful!");
            }
            break;
        }
        default:
            break;
    }
}

void menu_repository_client_project(pax_buf_t* buffer, gui_theme_t* theme, cJSON* wrapper) {
    render_dialog(buffer, theme, "Rendering project...");

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
                                execute_action(buffer, (menu_repository_client_project_action_t)arg, theme, wrapper);
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
