// SPDX-License-Identifier: MIT
// Tanmatsu Plugin API Implementation
// Provides host functions that plugins can call.

#include "tanmatsu_plugin.h"
#include "plugin_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pax_gfx.h"
#include "pax_fonts.h"
#include "chakrapetchmedium.h"

#include "common/display.h"
#include "bsp/input.h"
#include "fastopen.h"

static const char* TAG = "plugin_api";

// Mutex for protecting plugin API registries during concurrent access
static SemaphoreHandle_t plugin_api_mutex = NULL;

// ============================================
// Status Widget Registry
// ============================================

#define MAX_STATUS_WIDGETS 8

typedef struct {
    bool active;
    plugin_status_widget_fn callback;
    void* user_data;
    plugin_context_t* owner;  // Track which plugin owns this registration
} status_widget_entry_t;

static status_widget_entry_t status_widgets[MAX_STATUS_WIDGETS] = {0};

// Logging API moved to badge-elf-api (asp/log.h)

// ============================================
// LED Claim Tracking (Tanmatsu-specific)
// ============================================
// Plugins can claim specific LEDs to prevent the system from overwriting them.
// The badge-elf-api LED functions write directly to BSP, so the system needs
// to check claims before setting power/WiFi indicator LEDs.

#define LED_COUNT 6

typedef struct {
    bool claimed;
    plugin_context_t* owner;
} led_claim_t;

static led_claim_t led_claims[LED_COUNT] = {0};

bool asp_plugin_led_claim(plugin_context_t* ctx, uint32_t index) {
    if (index >= LED_COUNT || ctx == NULL) return false;

    // Can only claim if unclaimed or already owned by this plugin
    if (!led_claims[index].claimed || led_claims[index].owner == ctx) {
        led_claims[index].claimed = true;
        led_claims[index].owner = ctx;
        ESP_LOGI(TAG, "Plugin %s claimed LED %lu",
                 ctx->plugin_slug ? ctx->plugin_slug : "unknown", (unsigned long)index);
        return true;
    }
    return false;
}

void asp_plugin_led_release(plugin_context_t* ctx, uint32_t index) {
    if (index >= LED_COUNT) return;

    // Can only release if owned by this plugin (or ctx is NULL for force release)
    if (led_claims[index].claimed &&
        (ctx == NULL || led_claims[index].owner == ctx)) {
        ESP_LOGI(TAG, "Released LED %lu", (unsigned long)index);
        led_claims[index].claimed = false;
        led_claims[index].owner = NULL;
    }
}

bool plugin_api_is_led_claimed(uint32_t index) {
    if (index >= LED_COUNT) return false;
    return led_claims[index].claimed;
}

// Display API moved to badge-elf-api (asp/display.h)
// Use asp_disp_write() and asp_disp_write_part() for display updates

// ============================================
// Status Bar Widget API Implementation
// ============================================

int asp_plugin_status_widget_register(plugin_context_t* ctx, plugin_status_widget_fn callback, void* user_data) {
    for (int i = 0; i < MAX_STATUS_WIDGETS; i++) {
        if (!status_widgets[i].active) {
            status_widgets[i].active = true;
            status_widgets[i].callback = callback;
            status_widgets[i].user_data = user_data;
            status_widgets[i].owner = ctx;
            ESP_LOGI(TAG, "Registered status widget %d", i);
            return i;
        }
    }
    ESP_LOGW(TAG, "No free status widget slots");
    return -1;
}

void asp_plugin_status_widget_unregister(int widget_id) {
    if (widget_id >= 0 && widget_id < MAX_STATUS_WIDGETS) {
        status_widgets[widget_id].active = false;
        status_widgets[widget_id].callback = NULL;
        status_widgets[widget_id].user_data = NULL;
        ESP_LOGI(TAG, "Unregistered status widget %d", widget_id);
    }
}

// ============================================
// Drawing Primitives - REMOVED
// Use PAX library functions directly (pax_draw_circle, pax_draw_rect, etc.)
// These are already exported via kbelf_lib_pax_gfx
// ============================================

// Called by render_base_screen_statusbar to render plugin status widgets
// Widgets draw right-to-left from x_right position
// Returns total width used by all widgets
int plugin_api_render_status_widgets(pax_buf_t* buffer, int x_right, int y, int height) {
    int total_width = 0;
    int current_x = x_right;

    for (int i = 0; i < MAX_STATUS_WIDGETS; i++) {
        if (status_widgets[i].active && status_widgets[i].callback) {
            int widget_width = status_widgets[i].callback(buffer, current_x, y, height,
                                                           status_widgets[i].user_data);
            if (widget_width > 0) {
                current_x -= widget_width;
                total_width += widget_width;
            }
        }
    }
    return total_width;
}

// ============================================
// Input Hook API Implementation
// ============================================

// Track registered hooks per plugin for cleanup
#define MAX_PLUGIN_INPUT_HOOKS 8

typedef struct {
    int bsp_hook_id;
    plugin_input_hook_fn callback;
    void* user_data;
    bool in_use;
    plugin_context_t* owner;  // Track which plugin owns this registration
} plugin_input_hook_entry_t;

static plugin_input_hook_entry_t plugin_input_hooks[MAX_PLUGIN_INPUT_HOOKS] = {0};

// Internal callback that wraps plugin hook to BSP hook
static bool plugin_input_hook_wrapper(bsp_input_event_t* bsp_event, void* user_data) {
    int hook_index = (int)(intptr_t)user_data;
    if (hook_index < 0 || hook_index >= MAX_PLUGIN_INPUT_HOOKS) {
        return false;
    }

    plugin_input_hook_entry_t* entry = &plugin_input_hooks[hook_index];
    if (!entry->in_use || !entry->callback) {
        return false;
    }

    // Convert BSP event to plugin event format
    plugin_input_event_t plugin_event = {0};
    plugin_event.type = bsp_event->type;

    switch (bsp_event->type) {
        case INPUT_EVENT_TYPE_NAVIGATION:
            plugin_event.key = bsp_event->args_navigation.key;
            plugin_event.state = bsp_event->args_navigation.state;
            plugin_event.modifiers = bsp_event->args_navigation.modifiers;
            break;
        case INPUT_EVENT_TYPE_KEYBOARD:
            plugin_event.key = (uint32_t)bsp_event->args_keyboard.ascii;
            plugin_event.state = true;
            plugin_event.modifiers = bsp_event->args_keyboard.modifiers;
            break;
        case INPUT_EVENT_TYPE_SCANCODE:
            plugin_event.key = bsp_event->args_scancode.scancode;
            plugin_event.state = !(bsp_event->args_scancode.scancode & BSP_INPUT_SCANCODE_RELEASE_MODIFIER);
            plugin_event.modifiers = 0;
            break;
        default:
            break;
    }

    return entry->callback(&plugin_event, entry->user_data);
}

int asp_plugin_input_hook_register(plugin_context_t* ctx, plugin_input_hook_fn callback, void* user_data) {
    if (!callback) {
        return -1;
    }

    // Find free slot
    int hook_index = -1;
    for (int i = 0; i < MAX_PLUGIN_INPUT_HOOKS; i++) {
        if (!plugin_input_hooks[i].in_use) {
            hook_index = i;
            break;
        }
    }

    if (hook_index < 0) {
        ESP_LOGW(TAG, "No free plugin input hook slots");
        return -1;
    }

    // Register with BSP, passing our index as user_data
    int bsp_id = bsp_input_hook_register(plugin_input_hook_wrapper, (void*)(intptr_t)hook_index);
    if (bsp_id < 0) {
        ESP_LOGW(TAG, "Failed to register BSP input hook");
        return -1;
    }

    plugin_input_hooks[hook_index].bsp_hook_id = bsp_id;
    plugin_input_hooks[hook_index].callback = callback;
    plugin_input_hooks[hook_index].user_data = user_data;
    plugin_input_hooks[hook_index].in_use = true;
    plugin_input_hooks[hook_index].owner = ctx;

    ESP_LOGI(TAG, "Registered plugin input hook %d (BSP hook %d)", hook_index, bsp_id);
    return hook_index;
}

void asp_plugin_input_hook_unregister(int hook_id) {
    if (hook_id < 0 || hook_id >= MAX_PLUGIN_INPUT_HOOKS) {
        return;
    }

    plugin_input_hook_entry_t* entry = &plugin_input_hooks[hook_id];
    if (!entry->in_use) {
        return;
    }

    bsp_input_hook_unregister(entry->bsp_hook_id);

    entry->bsp_hook_id = -1;
    entry->callback = NULL;
    entry->user_data = NULL;
    entry->in_use = false;

    ESP_LOGI(TAG, "Unregistered plugin input hook %d", hook_id);
}

bool asp_plugin_input_inject(plugin_input_event_t* event) {
    if (!event) {
        return false;
    }

    bsp_input_event_t bsp_event = {0};
    bsp_event.type = event->type;

    switch (event->type) {
        case INPUT_EVENT_TYPE_NAVIGATION:
            bsp_event.args_navigation.key = event->key;
            bsp_event.args_navigation.state = event->state;
            bsp_event.args_navigation.modifiers = event->modifiers;
            break;
        case INPUT_EVENT_TYPE_KEYBOARD:
            bsp_event.args_keyboard.ascii = (char)event->key;
            bsp_event.args_keyboard.utf8 = NULL;
            bsp_event.args_keyboard.modifiers = event->modifiers;
            break;
        case INPUT_EVENT_TYPE_SCANCODE:
            bsp_event.args_scancode.scancode = event->key;
            if (!event->state) {
                bsp_event.args_scancode.scancode |= BSP_INPUT_SCANCODE_RELEASE_MODIFIER;
            }
            break;
        default:
            return false;
    }

    return bsp_input_inject_event(&bsp_event) == ESP_OK;
}

// ============================================
// Input API Implementation
// ============================================

bool asp_plugin_input_poll(plugin_input_event_t* event, uint32_t timeout_ms) {
    QueueHandle_t input_queue = NULL;
    if (bsp_input_get_queue(&input_queue) != ESP_OK || input_queue == NULL) {
        return false;
    }

    bsp_input_event_t bsp_event;
    if (xQueueReceive(input_queue, &bsp_event, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        event->type = bsp_event.type;
        if (bsp_event.type == INPUT_EVENT_TYPE_NAVIGATION) {
            event->key = bsp_event.args_navigation.key;
            event->state = bsp_event.args_navigation.state;
            event->modifiers = 0;
        } else if (bsp_event.type == INPUT_EVENT_TYPE_KEYBOARD) {
            // Keyboard events provide ASCII character, not key code
            event->key = (uint32_t)bsp_event.args_keyboard.ascii;
            event->state = true;  // Keyboard events are character inputs
            event->modifiers = bsp_event.args_keyboard.modifiers;
        }
        return true;
    }
    return false;
}

bool asp_plugin_input_get_key_state(uint32_t key) {
    // TODO: Implement direct key state query
    return false;
}

// LED API moved to badge-elf-api (asp/led.h)

// ============================================
// Storage API Implementation (Sandboxed)
// ============================================

// Build sandboxed path - ensures all paths are within plugin directory
static bool build_sandboxed_path(plugin_context_t* ctx, const char* path, char* out, size_t out_len) {
    if (!ctx || !ctx->storage_base_path || !path || !out) {
        return false;
    }

    // Reject absolute paths and parent directory traversal
    if (path[0] == '/' || strstr(path, "..") != NULL) {
        ESP_LOGW(TAG, "Rejected unsafe path: %s", path);
        return false;
    }

    int written = snprintf(out, out_len, "%s/%s", ctx->storage_base_path, path);
    return written > 0 && (size_t)written < out_len;
}

plugin_file_t asp_plugin_storage_open(plugin_context_t* ctx, const char* path, const char* mode) {
    char full_path[256];
    if (!build_sandboxed_path(ctx, path, full_path, sizeof(full_path))) {
        return NULL;
    }

    FILE* f = fastopen(full_path, mode);
    if (!f) {
        ESP_LOGD(TAG, "Failed to open %s: %s", full_path, strerror(errno));
    }
    return (plugin_file_t)f;
}

size_t asp_plugin_storage_read(plugin_file_t file, void* buf, size_t size) {
    if (!file) return 0;
    return fread(buf, 1, size, (FILE*)file);
}

size_t asp_plugin_storage_write(plugin_file_t file, const void* buf, size_t size) {
    if (!file) return 0;
    return fwrite(buf, 1, size, (FILE*)file);
}

int asp_plugin_storage_seek(plugin_file_t file, long offset, int whence) {
    if (!file) return -1;
    return fseek((FILE*)file, offset, whence);
}

long asp_plugin_storage_tell(plugin_file_t file) {
    if (!file) return -1;
    return ftell((FILE*)file);
}

void asp_plugin_storage_close(plugin_file_t file) {
    if (file) {
        fastclose((FILE*)file);  // Must use fastclose to free DMA buffer
    }
}

bool asp_plugin_storage_exists(plugin_context_t* ctx, const char* path) {
    char full_path[256];
    if (!build_sandboxed_path(ctx, path, full_path, sizeof(full_path))) {
        return false;
    }

    struct stat st;
    return stat(full_path, &st) == 0;
}

bool asp_plugin_storage_mkdir(plugin_context_t* ctx, const char* path) {
    char full_path[256];
    if (!build_sandboxed_path(ctx, path, full_path, sizeof(full_path))) {
        return false;
    }

    return mkdir(full_path, 0755) == 0;
}

bool asp_plugin_storage_remove(plugin_context_t* ctx, const char* path) {
    char full_path[256];
    if (!build_sandboxed_path(ctx, path, full_path, sizeof(full_path))) {
        return false;
    }

    return remove(full_path) == 0;
}

// ============================================
// Memory API - REMOVED
// Plugins should use standard libc functions (malloc, calloc, realloc, free)
// These are already exported via kbelf_lib_c
// ============================================

// ============================================
// Timer/Delay API Implementation
// ============================================

void asp_plugin_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint32_t asp_plugin_get_tick_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

bool asp_plugin_should_stop(plugin_context_t* ctx) {
    if (ctx == NULL) return true;
    return ctx->stop_requested;
}

// ============================================
// Menu API Implementation
// ============================================

// TODO: Implement menu item registration
// This requires integration with the GUI menu system

int asp_plugin_menu_add_item(const char* label, pax_buf_t* icon,
                              plugin_menu_callback_t callback, void* arg) {
    ESP_LOGW(TAG, "asp_plugin_menu_add_item not yet implemented");
    return -1;
}

void asp_plugin_menu_remove_item(int item_id) {
    ESP_LOGW(TAG, "asp_plugin_menu_remove_item not yet implemented");
}

// ============================================
// Event API Implementation
// ============================================

#define MAX_EVENT_HANDLERS 16

typedef struct {
    bool active;
    uint32_t event_mask;
    plugin_event_handler_t handler;
    void* arg;
    plugin_context_t* owner;  // Track which plugin owns this registration
} event_handler_entry_t;

static event_handler_entry_t event_handlers[MAX_EVENT_HANDLERS] = {0};

int asp_plugin_event_register(plugin_context_t* ctx, uint32_t event_mask, plugin_event_handler_t handler, void* arg) {
    for (int i = 0; i < MAX_EVENT_HANDLERS; i++) {
        if (!event_handlers[i].active) {
            event_handlers[i].active = true;
            event_handlers[i].event_mask = event_mask;
            event_handlers[i].handler = handler;
            event_handlers[i].arg = arg;
            event_handlers[i].owner = ctx;
            ESP_LOGI(TAG, "Registered event handler %d for mask 0x%lx", i, (unsigned long)event_mask);
            return i;
        }
    }
    ESP_LOGW(TAG, "No free event handler slots");
    return -1;
}

void asp_plugin_event_unregister(int handler_id) {
    if (handler_id >= 0 && handler_id < MAX_EVENT_HANDLERS) {
        event_handlers[handler_id].active = false;
        event_handlers[handler_id].handler = NULL;
        ESP_LOGI(TAG, "Unregistered event handler %d", handler_id);
    }
}

// Called by plugin manager to dispatch events to registered handlers
int plugin_api_dispatch_event(uint32_t event_type, void* event_data) {
    int handled = 0;
    for (int i = 0; i < MAX_EVENT_HANDLERS; i++) {
        if (event_handlers[i].active &&
            (event_handlers[i].event_mask & event_type) &&
            event_handlers[i].handler) {
            int result = event_handlers[i].handler(event_type, event_data, event_handlers[i].arg);
            if (result > 0) handled++;
        }
    }
    return handled;
}

// Network API moved to badge-elf-api (asp/http.h)

// ============================================
// Settings API Implementation
// ============================================

bool asp_plugin_settings_get_string(plugin_context_t* ctx, const char* key,
                                     char* value, size_t max_len) {
    if (!ctx || !ctx->settings_namespace || !key || !value) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ctx->settings_namespace, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_get_str(handle, key, value, &max_len);
    nvs_close(handle);

    return err == ESP_OK;
}

bool asp_plugin_settings_set_string(plugin_context_t* ctx, const char* key,
                                     const char* value) {
    if (!ctx || !ctx->settings_namespace || !key || !value) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ctx->settings_namespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    return err == ESP_OK;
}

bool asp_plugin_settings_get_int(plugin_context_t* ctx, const char* key, int32_t* value) {
    if (!ctx || !ctx->settings_namespace || !key || !value) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ctx->settings_namespace, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_get_i32(handle, key, value);
    nvs_close(handle);

    return err == ESP_OK;
}

bool asp_plugin_settings_set_int(plugin_context_t* ctx, const char* key, int32_t value) {
    if (!ctx || !ctx->settings_namespace || !key) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ctx->settings_namespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_set_i32(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    return err == ESP_OK;
}

// ============================================
// Power Information API - Moved to badge-elf-api
// ============================================
// See: components/badgeteam__badge-elf-api/include/asp/power.h

// ============================================
// Dialog API Implementation
// ============================================

#include "common/theme.h"
#include "menu/message_dialog.h"
#include "icons.h"

plugin_dialog_result_t asp_plugin_show_info_dialog(
    const char* title,
    const char* message,
    uint32_t timeout_ms
) {
    if (!title || !message) {
        return PLUGIN_DIALOG_RESULT_CANCEL;
    }

    pax_buf_t* buffer = display_get_buffer();
    gui_theme_t* theme = get_theme();
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);

    // Draw dialog background and header
    render_base_screen_statusbar(buffer, theme, true, true, true,
        ((gui_element_icontext_t[]){{get_icon(ICON_SPEAKER), (char*)title}}), 1,
        ADV_DIALOG_FOOTER_OK, NULL, 0);

    // Draw message in content area
    int content_y = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding;
    int content_x = theme->menu.horizontal_margin + theme->menu.horizontal_padding;

    pax_draw_text(buffer, theme->palette.color_foreground,
                  theme->menu.text_font, 16, content_x, content_y, message);

    display_blit_buffer(buffer);

    // Wait for input or timeout
    TickType_t wait_ticks = timeout_ms > 0 ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY;
    TickType_t start_time = xTaskGetTickCount();

    while (1) {
        bsp_input_event_t event;
        TickType_t elapsed = xTaskGetTickCount() - start_time;
        TickType_t remaining = (elapsed < wait_ticks) ? (wait_ticks - elapsed) : 0;

        if (timeout_ms > 0 && remaining == 0) {
            return PLUGIN_DIALOG_RESULT_TIMEOUT;
        }

        TickType_t poll_time = (timeout_ms > 0 && remaining < pdMS_TO_TICKS(1000))
                               ? remaining : pdMS_TO_TICKS(1000);

        if (xQueueReceive(input_event_queue, &event, poll_time) == pdTRUE) {
            if (event.type == INPUT_EVENT_TYPE_NAVIGATION && event.args_navigation.state) {
                switch (event.args_navigation.key) {
                    case BSP_INPUT_NAVIGATION_KEY_ESC:
                    case BSP_INPUT_NAVIGATION_KEY_F1:
                    case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                        return PLUGIN_DIALOG_RESULT_OK;
                    default:
                        break;
                }
            }
        } else {
            // Refresh status bar on timeout (clock, battery, etc.)
            render_base_screen_statusbar(buffer, theme, false, true, false,
                ((gui_element_icontext_t[]){{get_icon(ICON_SPEAKER), (char*)title}}), 1,
                NULL, 0, NULL, 0);
            display_blit_buffer(buffer);
        }
    }
}

plugin_dialog_result_t asp_plugin_show_text_dialog(
    const char* title,
    const char** lines,
    size_t line_count,
    uint32_t timeout_ms
) {
    if (!title || !lines || line_count == 0) {
        return PLUGIN_DIALOG_RESULT_CANCEL;
    }

    pax_buf_t* buffer = display_get_buffer();
    gui_theme_t* theme = get_theme();
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);

    // Draw dialog
    render_base_screen_statusbar(buffer, theme, true, true, true,
        ((gui_element_icontext_t[]){{get_icon(ICON_SPEAKER), (char*)title}}), 1,
        ADV_DIALOG_FOOTER_OK, NULL, 0);

    // Draw lines in content area
    int content_y = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding;
    int content_x = theme->menu.horizontal_margin + theme->menu.horizontal_padding;
    int line_height = 20;

    // Limit to 10 lines max to avoid overflow
    size_t max_lines = (line_count > 10) ? 10 : line_count;

    for (size_t i = 0; i < max_lines; i++) {
        if (lines[i]) {
            pax_draw_text(buffer, theme->palette.color_foreground,
                          theme->menu.text_font, 16, content_x,
                          content_y + (i * line_height), lines[i]);
        }
    }

    display_blit_buffer(buffer);

    // Wait for input or timeout
    TickType_t wait_ticks = timeout_ms > 0 ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY;
    TickType_t start_time = xTaskGetTickCount();

    while (1) {
        bsp_input_event_t event;
        TickType_t elapsed = xTaskGetTickCount() - start_time;
        TickType_t remaining = (elapsed < wait_ticks) ? (wait_ticks - elapsed) : 0;

        if (timeout_ms > 0 && remaining == 0) {
            return PLUGIN_DIALOG_RESULT_TIMEOUT;
        }

        TickType_t poll_time = (timeout_ms > 0 && remaining < pdMS_TO_TICKS(1000))
                               ? remaining : pdMS_TO_TICKS(1000);

        if (xQueueReceive(input_event_queue, &event, poll_time) == pdTRUE) {
            if (event.type == INPUT_EVENT_TYPE_NAVIGATION && event.args_navigation.state) {
                switch (event.args_navigation.key) {
                    case BSP_INPUT_NAVIGATION_KEY_ESC:
                    case BSP_INPUT_NAVIGATION_KEY_F1:
                    case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                        return PLUGIN_DIALOG_RESULT_OK;
                    default:
                        break;
                }
            }
        } else {
            // Refresh status bar on timeout
            render_base_screen_statusbar(buffer, theme, false, true, false,
                ((gui_element_icontext_t[]){{get_icon(ICON_SPEAKER), (char*)title}}), 1,
                NULL, 0, NULL, 0);
            display_blit_buffer(buffer);
        }
    }
}

// ============================================
// Plugin API Initialization and Cleanup
// ============================================

void plugin_api_init(void) {
    if (plugin_api_mutex == NULL) {
        plugin_api_mutex = xSemaphoreCreateMutex();
        if (plugin_api_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create plugin API mutex");
        }
    }
}

void plugin_api_cleanup_for_plugin(plugin_context_t* ctx) {
    if (ctx == NULL) return;

    ESP_LOGI(TAG, "Cleaning up API registrations for plugin: %s",
             ctx->plugin_slug ? ctx->plugin_slug : "unknown");

    if (plugin_api_mutex) {
        xSemaphoreTake(plugin_api_mutex, portMAX_DELAY);
    }

    // Clear all status widgets owned by this plugin
    for (int i = 0; i < MAX_STATUS_WIDGETS; i++) {
        if (status_widgets[i].active && status_widgets[i].owner == ctx) {
            ESP_LOGI(TAG, "Auto-unregistering status widget %d", i);
            status_widgets[i].active = false;
            status_widgets[i].callback = NULL;
            status_widgets[i].user_data = NULL;
            status_widgets[i].owner = NULL;
        }
    }

    // Clear all input hooks owned by this plugin
    for (int i = 0; i < MAX_PLUGIN_INPUT_HOOKS; i++) {
        if (plugin_input_hooks[i].in_use && plugin_input_hooks[i].owner == ctx) {
            ESP_LOGI(TAG, "Auto-unregistering input hook %d", i);
            bsp_input_hook_unregister(plugin_input_hooks[i].bsp_hook_id);
            plugin_input_hooks[i].bsp_hook_id = -1;
            plugin_input_hooks[i].callback = NULL;
            plugin_input_hooks[i].user_data = NULL;
            plugin_input_hooks[i].in_use = false;
            plugin_input_hooks[i].owner = NULL;
        }
    }

    // Clear all event handlers owned by this plugin
    for (int i = 0; i < MAX_EVENT_HANDLERS; i++) {
        if (event_handlers[i].active && event_handlers[i].owner == ctx) {
            ESP_LOGI(TAG, "Auto-unregistering event handler %d", i);
            event_handlers[i].active = false;
            event_handlers[i].event_mask = 0;
            event_handlers[i].handler = NULL;
            event_handlers[i].arg = NULL;
            event_handlers[i].owner = NULL;
        }
    }

    // Release all LED claims owned by this plugin
    for (int i = 0; i < LED_COUNT; i++) {
        if (led_claims[i].claimed && led_claims[i].owner == ctx) {
            ESP_LOGI(TAG, "Auto-releasing LED claim %d", i);
            led_claims[i].claimed = false;
            led_claims[i].owner = NULL;
        }
    }

    if (plugin_api_mutex) {
        xSemaphoreGive(plugin_api_mutex);
    }
}
