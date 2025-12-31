// SPDX-License-Identifier: MIT
// Plugin management menu

#include "menu_plugins.h"
#include <string.h>
#include <stdio.h>
#include "plugin_manager.h"
#include "gui_menu.h"
#include "gui_element_footer.h"
#include "icons.h"
#include "bsp/input.h"
#include "common/display.h"
#include "menu/message_dialog.h"
#include "esp_log.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char* TAG = "menu_plugins";

typedef enum {
    ACTION_NONE = 0,
    ACTION_BACK,
    ACTION_PLUGIN_TOGGLE,
} plugin_menu_action_t;

static void render_footer(pax_buf_t* buffer, gui_theme_t* theme, bool has_plugins) {
    gui_element_icontext_t footer_left[] = {
        {get_icon(ICON_F1), "Back"},
        {get_icon(ICON_F2), has_plugins ? "Load" : ""},
    };
    gui_element_icontext_t footer_right[] = {
        {get_icon(ICON_F3), has_plugins ? "Auto[A]" : ""},
    };
    gui_footer_draw(buffer, theme,
                    footer_left, has_plugins ? 2 : 1,
                    footer_right, has_plugins ? 1 : 0);
}

void menu_plugins(pax_buf_t* buffer, gui_theme_t* theme) {
    ESP_LOGI(TAG, "Entering plugins menu");

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    bool exit = false;
    bool refresh = true;
    size_t saved_position = 0;  // Remember menu position across refreshes

    while (!exit) {
        // Discover available plugins
        plugin_discovery_info_t* plugins = NULL;
        size_t plugin_count = plugin_manager_discover(&plugins);

        menu_t menu = {0};
        menu_initialize(&menu);

        // Add discovered plugins to menu
        for (size_t i = 0; i < plugin_count; i++) {
            char label[80];
            const char* type_str = "";
            switch (plugins[i].type) {
                case PLUGIN_TYPE_MENU: type_str = "[Menu]"; break;
                case PLUGIN_TYPE_SERVICE: type_str = "[Svc]"; break;
                case PLUGIN_TYPE_HOOK: type_str = "[Hook]"; break;
            }

            // Status indicators:
            // [*] = loaded, [A] = auto-start enabled
            const char* loaded_indicator = plugins[i].is_loaded ? "[*] " : "[ ] ";
            bool autostart = plugin_manager_get_autostart(plugins[i].slug);
            const char* auto_indicator = autostart ? " [A]" : "";

            snprintf(label, sizeof(label), "%s%s %s%s",
                     loaded_indicator,
                     plugins[i].name,
                     type_str,
                     auto_indicator);

            menu_insert_item_icon(&menu, label, NULL,
                                  (void*)(uintptr_t)i, -1,
                                  get_icon(ICON_EXTENSION));
        }

        // Restore saved position (clamped to valid range)
        if (saved_position > 0 && plugin_count > 0) {
            if (saved_position >= plugin_count) {
                saved_position = plugin_count - 1;
            }
            menu_set_position(&menu, saved_position);
        }

        // Calculate menu position
        int header_height = theme->header.height + (theme->header.vertical_margin * 2);
        int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

        pax_vec2_t position = {
            .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
            .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
            .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin -
                  theme->menu.horizontal_padding,
            .y1 = pax_buf_get_height(buffer) - footer_height -
                  theme->menu.vertical_margin - theme->menu.vertical_padding,
        };

        // Initial render
        render_base_screen_statusbar(buffer, theme, true, true, false,
            ((gui_element_icontext_t[]){{get_icon(ICON_EXTENSION), "Plugins"}}), 1,
            NULL, 0, NULL, 0);

        if (plugin_count == 0) {
            pax_draw_text(buffer, theme->palette.color_foreground,
                         theme->menu.text_font, theme->menu.text_height,
                         position.x0, position.y0,
                         "No plugins found.\nPlace plugins in /sd/plugins/ or /int/plugins/");
        } else {
            menu_render(buffer, &menu, position, theme, false);
        }

        render_footer(buffer, theme, plugin_count > 0);
        display_blit_buffer(buffer);

        refresh = false;

        while (!refresh && !exit) {
            bsp_input_event_t event;
            // Use shorter timeout when services are running for status bar updates
            TickType_t timeout = plugin_manager_has_running_services() ?
                                  pdMS_TO_TICKS(200) : pdMS_TO_TICKS(1000);
            if (xQueueReceive(input_event_queue, &event, timeout) == pdTRUE) {
                if (event.type == INPUT_EVENT_TYPE_NAVIGATION && event.args_navigation.state) {
                    switch (event.args_navigation.key) {
                        case BSP_INPUT_NAVIGATION_KEY_ESC:
                        case BSP_INPUT_NAVIGATION_KEY_F1:
                            exit = true;
                            break;

                        case BSP_INPUT_NAVIGATION_KEY_UP:
                            if (plugin_count > 0) {
                                menu_navigate_previous(&menu);
                                render_base_screen_statusbar(buffer, theme, true, true, false,
                                    ((gui_element_icontext_t[]){{get_icon(ICON_EXTENSION), "Plugins"}}), 1,
                                    NULL, 0, NULL, 0);
                                menu_render(buffer, &menu, position, theme, false);
                                render_footer(buffer, theme, true);
                                display_blit_buffer(buffer);
                            }
                            break;

                        case BSP_INPUT_NAVIGATION_KEY_DOWN:
                            if (plugin_count > 0) {
                                menu_navigate_next(&menu);
                                render_base_screen_statusbar(buffer, theme, true, true, false,
                                    ((gui_element_icontext_t[]){{get_icon(ICON_EXTENSION), "Plugins"}}), 1,
                                    NULL, 0, NULL, 0);
                                menu_render(buffer, &menu, position, theme, false);
                                render_footer(buffer, theme, true);
                                display_blit_buffer(buffer);
                            }
                            break;

                        case BSP_INPUT_NAVIGATION_KEY_RETURN:
                        case BSP_INPUT_NAVIGATION_KEY_F2: {
                            if (plugin_count > 0) {
                                size_t idx = (size_t)(uintptr_t)menu_get_callback_args(
                                    &menu, menu_get_position(&menu));

                                if (idx < plugin_count) {
                                    plugin_discovery_info_t* plugin = &plugins[idx];

                                    if (plugin->is_loaded) {
                                        // Unload plugin
                                        plugin_context_t* ctx =
                                            plugin_manager_get_by_slug(plugin->slug);
                                        if (ctx) {
                                            if (!plugin_manager_unload(ctx)) {
                                                message_dialog(get_icon(ICON_ERROR),
                                                               "Error", "Failed to unload plugin", "OK");
                                            }
                                        }
                                    } else {
                                        // Load plugin
                                        plugin_context_t* ctx =
                                            plugin_manager_load(plugin->path);
                                        if (ctx) {
                                            // Start service if it's a service plugin
                                            if (plugin->type == PLUGIN_TYPE_SERVICE) {
                                                plugin_manager_start_service(ctx);
                                            }
                                        } else {
                                            message_dialog(get_icon(ICON_ERROR),
                                                           "Error", "Failed to load plugin", "OK");
                                        }
                                    }
                                    refresh = true;
                                }
                            }
                            break;
                        }

                        case BSP_INPUT_NAVIGATION_KEY_F3: {
                            // Toggle auto-start for selected plugin
                            ESP_LOGI(TAG, "F3 pressed - toggling autostart");
                            if (plugin_count > 0) {
                                size_t idx = (size_t)(uintptr_t)menu_get_callback_args(
                                    &menu, menu_get_position(&menu));

                                if (idx < plugin_count) {
                                    plugin_discovery_info_t* plugin = &plugins[idx];
                                    bool current = plugin_manager_get_autostart(plugin->slug);
                                    ESP_LOGI(TAG, "Plugin %s: current autostart=%d, setting to %d",
                                             plugin->slug, current, !current);
                                    bool success = plugin_manager_set_autostart(plugin->slug, !current);
                                    ESP_LOGI(TAG, "Set autostart result: %s", success ? "OK" : "FAILED");
                                    refresh = true;
                                }
                            }
                            break;
                        }

                        default:
                            break;
                    }
                }
            }
        }

        // Save position before freeing menu
        saved_position = menu_get_position(&menu);

        menu_free(&menu);
        plugin_manager_free_discovery(plugins, plugin_count);
    }

    ESP_LOGI(TAG, "Exiting plugins menu");
}
