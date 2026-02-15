// SPDX-License-Identifier: MIT
// Tanmatsu Plugin API Header
// This file defines the interface for Tanmatsu launcher plugins.
// Function names use the asp_* naming convention for BadgeELF compatibility.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "asp/err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================
// Plugin API Version
// ============================================

#define TANMATSU_PLUGIN_API_VERSION_MAJOR 2
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
    PLUGIN_TYPE_HOOK = 2,      // Event handler hooks
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

    // Optional for PLUGIN_TYPE_HOOK: Event handler
    // Return 0 if not handled, positive if handled
    int (*hook_event)(plugin_context_t* ctx, uint32_t event_type, void* event_data);
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
// Host API: Input
// ============================================

// Input event types (matches bsp_input_event_type_t)
#define PLUGIN_INPUT_EVENT_TYPE_NONE       0
#define PLUGIN_INPUT_EVENT_TYPE_NAVIGATION 1
#define PLUGIN_INPUT_EVENT_TYPE_KEYBOARD   2
#define PLUGIN_INPUT_EVENT_TYPE_ACTION     3
#define PLUGIN_INPUT_EVENT_TYPE_SCANCODE   4

// Input event structure
typedef struct {
    uint32_t type;          // Event type (PLUGIN_INPUT_EVENT_TYPE_*)
    uint32_t key;           // Key code
    bool state;             // true = pressed, false = released
    uint32_t modifiers;     // Modifier keys state
} plugin_input_event_t;

// Poll for input event with timeout
// Returns true if event received, false on timeout
bool asp_plugin_input_poll(plugin_input_event_t* event, uint32_t timeout_ms);

// Get current state of specific key
bool asp_plugin_input_get_key_state(uint32_t key);

// ============================================
// Host API: Input Hooks
// ============================================

// Input hook callback type
// Called for every input event before it reaches the application
// Return true if the event was consumed (should not be processed by application)
// Return false to pass the event through to normal processing
typedef bool (*plugin_input_hook_fn)(plugin_input_event_t* event, void* user_data);

// Register an input hook
// Hooks are called in registration order for every input event
// If any hook returns true, the event is consumed and not queued
// Returns: hook_id (>=0) on success, -1 on error
int asp_plugin_input_hook_register(plugin_context_t* ctx, plugin_input_hook_fn callback, void* user_data);

// Unregister an input hook
void asp_plugin_input_hook_unregister(int hook_id);

// Inject a synthetic input event into the input queue
// This bypasses hooks and directly queues the event
// Returns: true on success, false on error
bool asp_plugin_input_inject(plugin_input_event_t* event);

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
// Host API: Event System (for PLUGIN_TYPE_HOOK)
// ============================================

// Event types
#define PLUGIN_EVENT_APP_LAUNCH         0x0001
#define PLUGIN_EVENT_APP_EXIT           0x0002
#define PLUGIN_EVENT_WIFI_CONNECTED     0x0003
#define PLUGIN_EVENT_WIFI_DISCONNECTED  0x0004
#define PLUGIN_EVENT_SD_INSERTED        0x0005
#define PLUGIN_EVENT_SD_REMOVED         0x0006
#define PLUGIN_EVENT_POWER_LOW          0x0007
#define PLUGIN_EVENT_USB_CONNECTED      0x0008
#define PLUGIN_EVENT_USB_DISCONNECTED   0x0009

typedef int (*plugin_event_handler_t)(uint32_t event, void* data, void* arg);

// Register event handler for specified event mask
// Returns: handler_id (>=0) on success, -1 on error
int asp_plugin_event_register(plugin_context_t* ctx, uint32_t event_mask, plugin_event_handler_t handler, void* arg);

// Unregister event handler
void asp_plugin_event_unregister(int handler_id);

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
// Host API: I2C Bus Access
// ============================================

// Opaque I2C device handle
typedef void* asp_i2c_device_t;

// Open an I2C device on the specified bus.
// bus: 0 = primary (internal), 1 = external (QWIIC/SAO)
// address: 7-bit I2C device address
// speed_hz: clock speed (e.g. 100000 for 100kHz, 400000 for 400kHz)
// On success, writes device handle to *out_i2c_device_handle and returns ASP_OK.
// Devices are auto-closed when the plugin unloads.
asp_err_t asp_i2c_open(plugin_context_t* ctx, asp_i2c_device_t* out_i2c_device_handle, uint8_t bus, uint16_t address, uint32_t speed_hz);

// Close an I2C device and release resources.
asp_err_t asp_i2c_close(asp_i2c_device_t device);

// Write data to an I2C device.
asp_err_t asp_i2c_write(asp_i2c_device_t device, const uint8_t* data, size_t len);

// Read data from an I2C device.
asp_err_t asp_i2c_read(asp_i2c_device_t device, uint8_t* data, size_t len);

// Write then read in a single I2C transaction (repeated start).
asp_err_t asp_i2c_write_read(asp_i2c_device_t device,
                         const uint8_t* write_data, size_t write_len,
                         uint8_t* read_data, size_t read_len);

// Probe for an I2C device on the specified bus.
// Returns ASP_OK if a device responds, ASP_ERR_NOT_FOUND otherwise.
asp_err_t asp_i2c_probe(uint8_t bus, uint16_t address);

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
