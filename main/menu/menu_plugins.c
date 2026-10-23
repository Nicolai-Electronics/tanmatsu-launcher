// SPDX-License-Identifier: MIT
// Plugin management menu

#include "menu_plugins.h"
#include <stdio.h>
#include <string.h>
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "esp_log.h"
#include "fastopen.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "gui_element_footer.h"
#include "gui_menu.h"
#include "icons.h"
#include "menu/menu_helpers.h"
#include "menu/message_dialog.h"
#include "pax_codecs.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "plugin_manager.h"
#include "wifi_connection.h"

extern bool wifi_stack_get_initialized(void);

static const char* TAG = "menu_plugins";

typedef enum {
    ACTION_NONE = 0,
    ACTION_BACK,
    ACTION_PLUGIN_TOGGLE,
} plugin_menu_action_t;

static pax_buf_t* load_plugin_icon(const char* plugin_path) {
    char icon_path[512];
    snprintf(icon_path, sizeof(icon_path), "%s/icon32.png", plugin_path);
    FILE* fd = fastopen(icon_path, "rb");
    if (fd == NULL) return NULL;
    pax_buf_t* icon = calloc(1, sizeof(pax_buf_t));
    if (icon != NULL) {
        if (!pax_decode_png_fd(icon, fd, PAX_BUF_32_8888ARGB, 0)) {
            free(icon);
            icon = NULL;
        }
    }
    fastclose(fd);
    return icon;
}

static void free_menu_icons(menu_t* menu) {
    pax_buf_t* default_icon = get_icon(ICON_EXTENSION);
    for (size_t i = 0; i < menu_get_length(menu); i++) {
        pax_buf_t* icon = menu_get_icon(menu, i);
        if (icon != NULL && icon != default_icon) {
            pax_buf_destroy(icon);
            free(icon);
        }
    }
}

// What F2 (or Return) does to the selected plugin: a menu plugin runs, the
// others load or unload.
static const char* select_label(plugin_discovery_info_t* plugins, size_t plugin_count, menu_t* menu) {
    if (plugin_count == 0) return "";
    size_t idx = (size_t)(uintptr_t)menu_get_callback_args(menu, menu_get_position(menu));
    if (idx >= plugin_count) return "";
    if (plugins[idx].type == PLUGIN_TYPE_MENU) return "Run";
    return plugins[idx].is_loaded ? "Unload" : "Load";
}

static void render_footer(pax_buf_t* buffer, gui_theme_t* theme, bool has_plugins, const char* select) {
    gui_element_icontext_t footer_left[] = {
        {get_icon(ICON_F1), "Back"},
        {get_icon(ICON_F2), has_plugins ? (char*)select : ""},
    };
    gui_element_icontext_t footer_right[] = {
        {get_icon(ICON_F3), has_plugins ? "Auto[A]" : ""},
    };
    gui_footer_draw(buffer, theme, footer_left, has_plugins ? 2 : 1, footer_right, has_plugins ? 1 : 0);
}

// Run a menu plugin: its menu_select gets the screen and the input queue,
// and is called again for as long as it returns true. One that asks for the
// "network" permission gets WiFi first (if that fails it runs anyway, and
// can tell with asp_net_is_connected). Loaded for the run and unloaded after,
// unless it was loaded already.
static void run_menu_plugin(plugin_discovery_info_t* plugin) {
    bool              was_loaded = plugin->is_loaded;
    plugin_context_t* ctx = was_loaded ? plugin_manager_get_by_slug(plugin->slug) : plugin_manager_load(plugin->path);
    if (ctx == NULL) {
        message_dialog(get_icon(ICON_ERROR), "Error", "Failed to load plugin", "OK");
        return;
    }

    if (ctx->registration == NULL || ctx->registration->entry.menu_select == NULL) {
        message_dialog(get_icon(ICON_ERROR), "Error", "This plugin has nothing to run", "OK");
    } else {
        if (plugin->network && wifi_stack_get_initialized() && !wifi_connection_is_connected()) {
            busy_dialog(get_icon(ICON_EXTENSION), plugin->name, "Connecting to WiFi...", true);
            if (wifi_connect_try_all() != ESP_OK) {
                ESP_LOGW(TAG, "Plugin %s: could not connect to WiFi", plugin->slug);
            }
        }
        ESP_LOGI(TAG, "Running menu plugin %s", plugin->slug);
        while (ctx->registration->entry.menu_select(ctx)) {
        }
    }

    if (!was_loaded && !plugin_manager_unload(ctx)) {
        ESP_LOGW(TAG, "Failed to unload plugin %s", plugin->slug);
    }
}

void menu_plugins(void) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    ESP_LOGI(TAG, "Entering plugins menu");

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    bool   exit           = false;
    bool   refresh        = true;
    size_t saved_position = 0;  // Remember menu position across refreshes

    while (!exit) {
        // Discover available plugins
        plugin_discovery_info_t* plugins      = NULL;
        size_t                   plugin_count = plugin_manager_discover(&plugins);

        menu_t menu = {0};
        menu_initialize(&menu);

        // Add discovered plugins to menu
        for (size_t i = 0; i < plugin_count; i++) {
            char        label[80];
            const char* type_str = "";
            switch (plugins[i].type) {
                case PLUGIN_TYPE_MENU:
                    type_str = "[Menu]";
                    break;
                case PLUGIN_TYPE_SERVICE:
                    type_str = "[Svc]";
                    break;
                case PLUGIN_TYPE_HOOK:
                    type_str = "[Hook]";
                    break;
            }

            // Status indicators:
            // [*] = loaded, [A] = auto-start enabled
            const char* loaded_indicator = plugins[i].is_loaded ? "[*] " : "[ ] ";
            bool        autostart        = plugin_manager_get_autostart(plugins[i].slug);
            const char* auto_indicator   = autostart ? " [A]" : "";

            snprintf(label, sizeof(label), "%s%s %s%s", loaded_indicator, plugins[i].name, type_str, auto_indicator);

            pax_buf_t* icon = load_plugin_icon(plugins[i].path);
            menu_insert_item_icon(&menu, label, NULL, (void*)(uintptr_t)i, -1, icon ? icon : get_icon(ICON_EXTENSION));
        }

        // Restore saved position (clamped to valid range)
        if (saved_position > 0 && plugin_count > 0) {
            if (saved_position >= plugin_count) {
                saved_position = plugin_count - 1;
            }
            menu_set_position(&menu, saved_position);
        }

        // Calculate menu position
        pax_vec2_t position = menu_calc_position(buffer, theme);

        // Initial render
        render_base_screen_statusbar(buffer, theme, true, true, false,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_EXTENSION), "Plugins"}}), 1, NULL, 0,
                                     NULL, 0);

        if (plugin_count == 0) {
            pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, theme->menu.text_height,
                          position.x0, position.y0,
                          "No plugins found.\nPlace plugins in /sd/plugins/ or /int/plugins/");
        } else {
            menu_render(buffer, &menu, position, theme, false);
        }

        render_footer(buffer, theme, plugin_count > 0, select_label(plugins, plugin_count, &menu));
        display_blit_buffer(buffer);

        refresh = false;

        while (!refresh && !exit) {
            bsp_input_event_t event;
            // Use shorter timeout when services are running for status bar updates
            TickType_t timeout = plugin_manager_has_running_services() ? pdMS_TO_TICKS(200) : pdMS_TO_TICKS(1000);
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
                                render_base_screen_statusbar(
                                    buffer, theme, true, true, false,
                                    ((gui_element_icontext_t[]){{get_icon(ICON_EXTENSION), "Plugins"}}), 1, NULL, 0,
                                    NULL, 0);
                                menu_render(buffer, &menu, position, theme, false);
                                render_footer(buffer, theme, true, select_label(plugins, plugin_count, &menu));
                                display_blit_buffer(buffer);
                            }
                            break;

                        case BSP_INPUT_NAVIGATION_KEY_DOWN:
                            if (plugin_count > 0) {
                                menu_navigate_next(&menu);
                                render_base_screen_statusbar(
                                    buffer, theme, true, true, false,
                                    ((gui_element_icontext_t[]){{get_icon(ICON_EXTENSION), "Plugins"}}), 1, NULL, 0,
                                    NULL, 0);
                                menu_render(buffer, &menu, position, theme, false);
                                render_footer(buffer, theme, true, select_label(plugins, plugin_count, &menu));
                                display_blit_buffer(buffer);
                            }
                            break;

                        case BSP_INPUT_NAVIGATION_KEY_RETURN:
                        case BSP_INPUT_NAVIGATION_KEY_F2: {
                            if (plugin_count > 0) {
                                size_t idx = (size_t)(uintptr_t)menu_get_callback_args(&menu, menu_get_position(&menu));

                                if (idx < plugin_count) {
                                    plugin_discovery_info_t* plugin = &plugins[idx];

                                    if (plugin->type == PLUGIN_TYPE_MENU) {
                                        run_menu_plugin(plugin);
                                    } else if (plugin->is_loaded) {
                                        // Unload plugin
                                        plugin_context_t* ctx = plugin_manager_get_by_slug(plugin->slug);
                                        if (ctx) {
                                            if (!plugin_manager_unload(ctx)) {
                                                message_dialog(get_icon(ICON_ERROR), "Error", "Failed to unload plugin",
                                                               "OK");
                                            }
                                        }
                                    } else {
                                        // Load plugin
                                        plugin_context_t* ctx = plugin_manager_load(plugin->path);
                                        if (ctx) {
                                            // Start service if it's a service plugin
                                            if (plugin->type == PLUGIN_TYPE_SERVICE) {
                                                plugin_manager_start_service(ctx);
                                            }
                                        } else {
                                            message_dialog(get_icon(ICON_ERROR), "Error", "Failed to load plugin",
                                                           "OK");
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
                                size_t idx = (size_t)(uintptr_t)menu_get_callback_args(&menu, menu_get_position(&menu));

                                if (idx < plugin_count) {
                                    plugin_discovery_info_t* plugin  = &plugins[idx];
                                    bool                     current = plugin_manager_get_autostart(plugin->slug);
                                    ESP_LOGI(TAG, "Plugin %s: current autostart=%d, setting to %d", plugin->slug,
                                             current, !current);
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

        free_menu_icons(&menu);
        menu_free(&menu);
        plugin_manager_free_discovery(plugins, plugin_count);
    }

    ESP_LOGI(TAG, "Exiting plugins menu");
}
