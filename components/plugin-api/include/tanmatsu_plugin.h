// SPDX-License-Identifier: MIT
// Tanmatsu Plugin API Header
// This file defines the interface for Tanmatsu launcher plugins.
// Function names use the asp_* naming convention for BadgeELF compatibility.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "asp/err.h"
#include "asp/input_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================
// Plugin API Version
// ============================================

#define TANMATSU_PLUGIN_API_VERSION_MAJOR 3
#define TANMATSU_PLUGIN_API_VERSION_MINOR 0
#define TANMATSU_PLUGIN_API_VERSION_PATCH 0
#define TANMATSU_PLUGIN_API_VERSION \
    ((TANMATSU_PLUGIN_API_VERSION_MAJOR << 16) | \
     (TANMATSU_PLUGIN_API_VERSION_MINOR << 8) | \
     TANMATSU_PLUGIN_API_VERSION_PATCH)

// ============================================
// Plugin Types and States
// ============================================

// Plugin types
typedef enum {
    PLUGIN_TYPE_MENU = 0,      // Adds items to launcher menu
    PLUGIN_TYPE_SERVICE = 1,   // Background service task
    PLUGIN_TYPE_HOOK = 2,      // Registers input event hooks from init()
} plugin_type_t;

// Plugin state
typedef enum {
    PLUGIN_STATE_UNLOADED = 0,
    PLUGIN_STATE_LOADED,
    PLUGIN_STATE_INITIALIZED,
    PLUGIN_STATE_RUNNING,
    PLUGIN_STATE_STOPPED,
    PLUGIN_STATE_ERROR,
} plugin_state_t;

// ============================================
// Forward Declarations
// ============================================

// Opaque plugin context (defined in plugin_context.h)
typedef struct plugin_context plugin_context_t;

// PAX graphics buffer (from pax_gfx.h)
typedef struct pax_buf pax_buf_t;

// ============================================
// Plugin Metadata Structure
// ============================================

typedef struct {
    const char* name;           // Plugin display name
    const char* slug;           // Unique identifier (lowercase, no spaces)
    const char* version;        // Semantic version string (e.g., "1.0.0")
    const char* author;         // Author name
    const char* description;    // Short description
    uint32_t api_version;       // Required API version
    plugin_type_t type;         // Plugin type
    uint32_t flags;             // Plugin capability flags
} plugin_info_t;

// ============================================
// Icon-Text Structure (for status bar widgets)
// ============================================

typedef struct {
    pax_buf_t* icon;    // Pointer to 32x32 ARGB icon buffer (or NULL)
    char* text;         // Optional text label (or NULL)
} plugin_icontext_t;

// ============================================
// Plugin Entry Points
// ============================================

typedef struct {
    // Required: Get plugin information
    const plugin_info_t* (*get_info)(void);

    // Required: Initialize plugin (called once at load)
    // Return 0 on success, non-zero on failure
    int (*init)(plugin_context_t* ctx);

    // Required: Cleanup plugin (called before unload)
    void (*cleanup)(plugin_context_t* ctx);

    // Optional for PLUGIN_TYPE_MENU: Render menu item
    void (*menu_render)(plugin_context_t* ctx, pax_buf_t* buffer);

    // Optional for PLUGIN_TYPE_MENU: Handle menu selection
    // Return true to stay in plugin, false to return to menu
    bool (*menu_select)(plugin_context_t* ctx);

    // Optional for PLUGIN_TYPE_SERVICE: Service main loop
    // This runs in its own FreeRTOS task
    void (*service_run)(plugin_context_t* ctx);
} plugin_entry_t;

// ============================================
// Plugin Registration
// ============================================

// Magic value for plugin registration validation
#define TANMATSU_PLUGIN_MAGIC 0x544D5350  // "TMSP"

// Plugin registration structure (placed in .plugin_info section)
typedef struct {
    uint32_t magic;             // Must be TANMATSU_PLUGIN_MAGIC
    uint32_t struct_size;       // sizeof(plugin_registration_t)
    plugin_entry_t entry;       // Plugin entry points
} plugin_registration_t;

// Macro for plugin registration
// Usage: TANMATSU_PLUGIN_REGISTER(my_entry_struct);
#define TANMATSU_PLUGIN_REGISTER(entry_struct) \
    __attribute__((section(".plugin_info"), used)) \
    const plugin_registration_t _plugin_registration = { \
        .magic = TANMATSU_PLUGIN_MAGIC, \
        .struct_size = sizeof(plugin_registration_t), \
        .entry = entry_struct, \
    }

// ============================================
// Host API: Logging
// ============================================

void asp_log_info(const char* tag, const char* fmt, ...);
void asp_log_warn(const char* tag, const char* fmt, ...);
void asp_log_error(const char* tag, const char* fmt, ...);

// ============================================
// Host API: Display
// ============================================
// Display API is provided by badge-elf-api. Use:
//   asp_disp_get_pax_buf() - Get PAX buffer for drawing
//   asp_disp_write()       - Write full display buffer to screen
//   asp_disp_write_part()  - Write partial region to screen
// See: #include <asp/display.h>

// ============================================
// Host API: Status Bar Widgets
// ============================================

// Status widget callback type
// Called with:
//   buffer: display buffer to draw to
//   x_right: rightmost X position available (draw to the LEFT of this)
//   y: Y position of the status bar
//   height: height of the status bar area
//   user_data: user-provided context
// Returns: width used by this widget (next widget will be drawn to the left)
typedef int (*plugin_status_widget_fn)(pax_buf_t* buffer, int x_right, int y, int height, void* user_data);

// Register a status widget to appear in header bar
// Returns: widget_id (>=0) on success, -1 on error
int asp_plugin_status_widget_register(plugin_context_t* ctx, plugin_status_widget_fn callback, void* user_data);

// Unregister a status widget
void asp_plugin_status_widget_unregister(int widget_id);

// ============================================
// Host API: Drawing Primitives
// Note: Use PAX library functions directly (pax_draw_circle, pax_draw_rect, etc.)
// These are already exported via kbelf_lib_pax_gfx
// ============================================

// ============================================
// Host API: Input Hooks
// ============================================
// Plugins observe and inject events through the unified input event queue.
// All event types — keyboard, navigation, scancode, and action — flow through
// the same pipeline. There is no separate "lifecycle event" subsystem; system
// signals (SD card insert/remove, WiFi state, USB connect/disconnect, etc.)
// are dispatched as INPUT_EVENT_TYPE_ACTION events with a subtype identifying
// the source. See asp/input.h and asp/input_types.h for the event structure.

// Launcher-extended action subtypes. These augment asp_input_action_type_t
// (defined in asp/input_types.h) with values the launcher synthesizes for
// system-level state changes that don't originate from the BSP. Values are
// placed above the BSP's enumerator range to avoid collision.
#define ASP_INPUT_ACTION_TYPE_LAUNCHER_BASE      0x100
#define ASP_INPUT_ACTION_TYPE_WIFI_CONNECTED     (ASP_INPUT_ACTION_TYPE_LAUNCHER_BASE + 0)
#define ASP_INPUT_ACTION_TYPE_WIFI_DISCONNECTED  (ASP_INPUT_ACTION_TYPE_LAUNCHER_BASE + 1)
#define ASP_INPUT_ACTION_TYPE_USB_CONNECTED      (ASP_INPUT_ACTION_TYPE_LAUNCHER_BASE + 2)
#define ASP_INPUT_ACTION_TYPE_USB_DISCONNECTED   (ASP_INPUT_ACTION_TYPE_LAUNCHER_BASE + 3)
#define ASP_INPUT_ACTION_TYPE_APP_LAUNCH         (ASP_INPUT_ACTION_TYPE_LAUNCHER_BASE + 4)
#define ASP_INPUT_ACTION_TYPE_APP_EXIT           (ASP_INPUT_ACTION_TYPE_LAUNCHER_BASE + 5)
#define ASP_INPUT_ACTION_TYPE_POWER_LOW          (ASP_INPUT_ACTION_TYPE_LAUNCHER_BASE + 6)

// Input hook callback type
// Called for every input event before it reaches the application.
// Return true if the event was consumed (should not be forwarded).
// Return false to pass the event through to subsequent hooks and the queue.
typedef bool (*plugin_input_hook_fn)(asp_input_event_t* event, void* user_data);

// Register an input hook.
// Hooks are called in registration order for every input event.
// If any hook returns true, the event is consumed and not queued.
// Returns: hook_id (>=0) on success, -1 on error
int asp_plugin_input_hook_register(plugin_context_t* ctx, plugin_input_hook_fn callback, void* user_data);

// Unregister an input hook
void asp_plugin_input_hook_unregister(int hook_id);

// Inject a synthetic input event into the input queue.
// The injected event flows through the registered hook chain just like a real
// event, so other plugins (and the global handler) can observe or consume it.
// Returns: true on success, false on error
bool asp_plugin_input_inject(asp_input_event_t* event);

// Note: to receive events without registering a hook, plugins can poll the
// shared input queue using asp_input_poll() from <asp/input.h>. To query the
// instantaneous state of an action or navigation key, use asp_input_get_action()
// or asp_input_get_nav() from the same header.

// ============================================
// Host API: RGB LEDs
// ============================================

// Get the number of RGB LEDs available on the device
uint32_t asp_led_get_count(void);

// Set overall LED brightness (0-100%)
asp_err_t asp_led_set_brightness(uint8_t percentage);

// Get overall LED brightness (0-100%)
asp_err_t asp_led_get_brightness(uint8_t* out_percentage);

// Set LED mode (true = automatic/system control, false = manual/plugin control)
// Must set to false (manual) before controlling LEDs directly
asp_err_t asp_led_set_mode(bool automatic);

// Get current LED mode
asp_err_t asp_led_get_mode(bool* out_automatic);

// Set a single LED pixel color using 0xRRGGBB format
// Does not update hardware until asp_led_send() is called
asp_err_t asp_led_set_pixel(uint32_t index, uint32_t color);

// Set a single LED pixel color using RGB components
// Does not update hardware until asp_led_send() is called
asp_err_t asp_led_set_pixel_rgb(uint32_t index, uint8_t red, uint8_t green, uint8_t blue);

// Set a single LED pixel color using HSV
// hue: 0-65535 (maps to 0-360 degrees)
// saturation: 0-255
// value: 0-255
// Does not update hardware until asp_led_send() is called
asp_err_t asp_led_set_pixel_hsv(uint32_t index, uint16_t hue, uint8_t saturation, uint8_t value);

// Send LED data to hardware (call after setting pixels)
asp_err_t asp_led_send(void);

// Clear all LEDs (sets all to black and sends to hardware)
asp_err_t asp_led_clear(void);

// Claim an LED for plugin use
// Plugins MUST claim an LED before using it. This prevents conflicts
// between multiple plugins and the system (which controls LEDs 0-1 for
// WiFi and power indicators). Claimed LEDs won't be overwritten by
// the system. Claims are automatically released when the plugin unloads.
// Returns: true if claim succeeded, false if already claimed by another plugin
bool asp_plugin_led_claim(plugin_context_t* ctx, uint32_t index);

// Release an LED claim (allows system or other plugins to use it)
void asp_plugin_led_release(plugin_context_t* ctx, uint32_t index);

// ============================================
// Host API: Storage (Sandboxed to plugin directory)
// ============================================

typedef void* plugin_file_t;

// Open file (relative to plugin directory)
// Mode: "r", "w", "a", "rb", "wb", "ab"
plugin_file_t asp_plugin_storage_open(plugin_context_t* ctx, const char* path, const char* mode);

// Read bytes from file
size_t asp_plugin_storage_read(plugin_file_t file, void* buf, size_t size);

// Write bytes to file
size_t asp_plugin_storage_write(plugin_file_t file, const void* buf, size_t size);

// Seek in file (whence: 0=SET, 1=CUR, 2=END)
int asp_plugin_storage_seek(plugin_file_t file, long offset, int whence);

// Get current position
long asp_plugin_storage_tell(plugin_file_t file);

// Close file
void asp_plugin_storage_close(plugin_file_t file);

// Check if file/directory exists (relative to plugin directory)
bool asp_plugin_storage_exists(plugin_context_t* ctx, const char* path);

// Create directory (relative to plugin directory)
bool asp_plugin_storage_mkdir(plugin_context_t* ctx, const char* path);

// Delete file or empty directory (relative to plugin directory)
bool asp_plugin_storage_remove(plugin_context_t* ctx, const char* path);

// ============================================
// Host API: Memory
// Note: Use standard libc functions (malloc, calloc, realloc, free)
// These are already exported via kbelf_lib_c
// ============================================

// ============================================
// Host API: Timer/Delay
// ============================================

// Sleep for specified milliseconds
void asp_plugin_delay_ms(uint32_t ms);

// Get current system tick in milliseconds
uint32_t asp_plugin_get_tick_ms(void);

// Check if stop has been requested (for service plugins)
// Service plugins should check this regularly and exit when true
bool asp_plugin_should_stop(plugin_context_t* ctx);

// ============================================
// Host API: Menu Integration (for PLUGIN_TYPE_MENU)
// ============================================

typedef void (*plugin_menu_callback_t)(void* arg);

// Add item to launcher menu
// Returns: item_id (>=0) on success, -1 on error
int asp_plugin_menu_add_item(const char* label, pax_buf_t* icon,
                              plugin_menu_callback_t callback, void* arg);

// Remove menu item
void asp_plugin_menu_remove_item(int item_id);

// ============================================
// Host API: Networking
// ============================================

// Check if network is available.
// On success, writes the connection state to *out_connected and returns ASP_OK.
asp_err_t asp_net_is_connected(bool* out_connected);

// Perform HTTP GET request.
// On success, writes the HTTP status code to *out_status_code and returns ASP_OK.
asp_err_t asp_http_get(const char* url, char* response, size_t max_len, int* out_status_code);

// Perform HTTP POST request.
// On success, writes the HTTP status code to *out_status_code and returns ASP_OK.
asp_err_t asp_http_post(const char* url, const char* body, char* response, size_t max_len, int* out_status_code);

// ============================================
// Host API: Settings Storage
// ============================================

// Get string setting (settings are namespaced per plugin)
bool asp_plugin_settings_get_string(plugin_context_t* ctx, const char* key,
                                     char* value, size_t max_len);

// Set string setting
bool asp_plugin_settings_set_string(plugin_context_t* ctx, const char* key,
                                     const char* value);

// Get integer setting
bool asp_plugin_settings_get_int(plugin_context_t* ctx, const char* key, int32_t* value);

// Set integer setting
bool asp_plugin_settings_set_int(plugin_context_t* ctx, const char* key, int32_t value);

// ============================================
// Host API: Power Information
// ============================================
// Power API has moved to badge-elf-api: #include <asp/power.h>

// ============================================
// Host API: Dialog System
// ============================================

// Dialog result type
typedef enum {
    PLUGIN_DIALOG_RESULT_OK = 0,
    PLUGIN_DIALOG_RESULT_CANCEL = 1,
    PLUGIN_DIALOG_RESULT_TIMEOUT = 2,
} plugin_dialog_result_t;

// Show an information dialog with title and message
// Blocks until user dismisses (ESC/F1) or timeout (0 = no timeout)
// Returns: dialog result indicating how it was closed
plugin_dialog_result_t asp_plugin_show_info_dialog(
    const char* title,
    const char* message,
    uint32_t timeout_ms
);

// Show a multi-line text dialog
// lines: array of strings to display
// line_count: number of lines
// Returns: dialog result
plugin_dialog_result_t asp_plugin_show_text_dialog(
    const char* title,
    const char** lines,
    size_t line_count,
    uint32_t timeout_ms
);

#ifdef __cplusplus
}
#endif
