#include "apps.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "app_metadata_parser.h"
#include "appfs.h"
#include "bsp/input.h"
#include "common/display.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "projdefs.h"
// #include "shapes/pax_misc.h"

#define MAX_NUM_APPS 128

static void populate_menu(menu_t* menu, app_t** apps, size_t app_count) {
    for (size_t i = 0; i < app_count; i++) {
        if (apps[i] != NULL && apps[i]->slug != NULL && apps[i]->name != NULL) {
            menu_insert_item_icon(menu, apps[i]->name, NULL, (void*)apps[i], -1, apps[i]->icon);
        }
    }
}

extern bool wifi_stack_get_task_done(void);

static void execute_app(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, app_t* app) {
    render_base_screen_statusbar(buffer, theme, true, true, true,
                                 ((gui_header_field_t[]){{get_icon(ICON_APPS), "Apps"}}), 1, NULL, 0, NULL, 0);
    char message[64] = {0};
    snprintf(message, sizeof(message), "Starting %s...", app->name);
    pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, theme->footer.text_height,
                  position.x0, position.y0, message);
    display_blit_buffer(buffer);
    printf("Starting %s...\n", app->slug);
    if (app->appfs_fd != APPFS_INVALID_FD) {
        appfsBootSelect(app->appfs_fd, NULL);
        while (wifi_stack_get_task_done() == false) {
            printf("Waiting for wifi stack task to finish...\n");
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        esp_restart();
    } else {
        message_dialog(buffer, theme, "Error", "App not found", "OK");
    }
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_APPS), "Apps"}}), 1,
                                     ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}),
                                     2, ((gui_header_field_t[]){{NULL, "↑ / ↓ Navigate ⏎ Start app"}}), 1);
    }
    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_apps(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

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
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    render(buffer, theme, &menu, position, false, true);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
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
                            case BSP_INPUT_NAVIGATION_KEY_RETURN: {
                                void*  arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                app_t* app = (app_t*)arg;
                                execute_app(buffer, theme, position, app);
                                render(buffer, theme, &menu, position, false, false);
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
}
