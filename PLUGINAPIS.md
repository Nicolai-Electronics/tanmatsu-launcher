# Tanmatsu Plugin API Reference

**Version:** 1.0.0
**Last Updated:** 2025-01-20

This document describes all APIs available to Tanmatsu plugins. Keep this file updated as APIs evolve.

## Table of Contents

- [Overview](#overview)
- [Plugin Registration](#plugin-registration)
- [Logging API](#logging-api)
- [Display API](#display-api)
- [Status Bar API](#status-bar-api)
- [Input API](#input-api)
- [Storage API](#storage-api)
- [Memory API](#memory-api)
- [Timer API](#timer-api)
- [Menu API](#menu-api)
- [Event API](#event-api)
- [Network API](#network-api)
- [Settings API](#settings-api)
- [Data Types](#data-types)

---

## Overview

Tanmatsu plugins are dynamically loaded ELF shared libraries that run on the ESP32-P4 processor. Plugins can extend the launcher with new menu items, background services, or system event hooks.

### Plugin Types

| Type | Description |
|------|-------------|
| `PLUGIN_TYPE_MENU` | Adds items to the launcher menu |
| `PLUGIN_TYPE_SERVICE` | Background service running in its own FreeRTOS task |
| `PLUGIN_TYPE_HOOK` | Receives system events (app launch, WiFi, power, etc.) |

### API Version

Current API version: `TANMATSU_PLUGIN_API_VERSION = 1`

Plugins must specify the API version they were built against. The host will reject plugins with incompatible API versions.

---

## Plugin Registration

### TANMATSU_PLUGIN_REGISTER(entry)

Macro to register a plugin with the host system. Must be called once at file scope.

**Parameters:**
- `entry`: `plugin_entry_t` struct containing function pointers

**Example:**
```c
static const plugin_entry_t entry = {
    .get_info = get_info,
    .init = plugin_init,
    .cleanup = plugin_cleanup,
};

TANMATSU_PLUGIN_REGISTER(entry);
```

### plugin_entry_t

Structure containing plugin callbacks:

| Function | Required | Description |
|----------|----------|-------------|
| `get_info` | Yes | Returns `const plugin_info_t*` with plugin metadata |
| `init` | Yes | Called once at load time. Return 0 on success, non-zero on failure |
| `cleanup` | Yes | Called before unload. Clean up resources |
| `menu_render` | Menu only | Render custom menu content |
| `menu_select` | Menu only | Handle menu item selection |
| `service_run` | Service only | Main service loop (runs in dedicated task) |
| `hook_event` | Hook only | Handle system events |

---

## Logging API

### plugin_log_info(tag, fmt, ...)

Log an informational message.

**Parameters:**
- `tag`: Log tag (typically plugin slug)
- `fmt`: printf-style format string
- `...`: Format arguments

**Example:**
```c
plugin_log_info("my-plugin", "Initialized with value %d", value);
```

### plugin_log_warn(tag, fmt, ...)

Log a warning message.

### plugin_log_error(tag, fmt, ...)

Log an error message.

---

## Display API

### plugin_display_get_buffer()

Get the current display buffer for drawing operations.

**Returns:** `pax_buf_t*` - Pointer to the display buffer

**Note:** The returned buffer is shared with the launcher. Draw only in appropriate contexts (menu render callbacks, etc.).

### plugin_display_flush()

Flush the entire display buffer to the screen.

### plugin_display_flush_region(x, y, w, h)

Flush a rectangular region of the display.

**Parameters:**
- `x`: X coordinate of top-left corner
- `y`: Y coordinate of top-left corner
- `w`: Width in pixels
- `h`: Height in pixels

---

## Status Bar API

APIs for adding widgets to the launcher status bar.

### plugin_status_widget_register(callback, user_data)

Register a callback to render a status bar widget.

**Parameters:**
- `callback`: `plugin_status_widget_fn` - Function called during status bar render
- `user_data`: Arbitrary pointer passed to callback

**Returns:** Widget ID (>= 0) on success, -1 on error

**Maximum widgets:** 8 across all plugins

### plugin_status_widget_unregister(widget_id)

Remove a previously registered status bar widget.

**Parameters:**
- `widget_id`: ID returned by `plugin_status_widget_register`

### plugin_status_draw_rect(x, y, w, h, color)

Draw a filled rectangle in the status bar area.

**Parameters:**
- `x`, `y`: Position in pixels
- `w`, `h`: Size in pixels
- `color`: 32-bit ARGB color (0xAARRGGBB)

### plugin_status_draw_circle(x, y, radius, color)

Draw a filled circle in the status bar area.

**Parameters:**
- `x`, `y`: Center position
- `radius`: Circle radius in pixels
- `color`: 32-bit ARGB color

### plugin_status_get_draw_x()

Get the X position where plugin widgets should draw.

**Returns:** X coordinate in pixels

### plugin_status_get_draw_y()

Get the Y position (top of header area).

**Returns:** Y coordinate in pixels

---

## Input API

### plugin_input_poll(event, timeout_ms)

Poll for an input event with timeout.

**Parameters:**
- `event`: Pointer to `plugin_input_event_t` to receive the event
- `timeout_ms`: Maximum time to wait in milliseconds

**Returns:** `true` if an event was received, `false` on timeout

### plugin_input_get_key_state(key)

Get the current state of a specific key.

**Parameters:**
- `key`: Key code (from BSP input definitions)

**Returns:** `true` if key is pressed, `false` otherwise

---

## Storage API

All storage operations are sandboxed to the plugin's directory.

### plugin_storage_open(ctx, path, mode)

Open a file for reading or writing.

**Parameters:**
- `ctx`: Plugin context (from init/service callbacks)
- `path`: Relative path within plugin directory
- `mode`: File mode ("r", "w", "a", "rb", "wb", etc.)

**Returns:** `plugin_file_t*` on success, `NULL` on failure

### plugin_storage_read(file, buf, size)

Read bytes from an open file.

**Parameters:**
- `file`: File handle from `plugin_storage_open`
- `buf`: Buffer to read into
- `size`: Maximum bytes to read

**Returns:** Number of bytes read, or -1 on error

### plugin_storage_write(file, buf, size)

Write bytes to an open file.

**Parameters:**
- `file`: File handle
- `buf`: Buffer containing data to write
- `size`: Number of bytes to write

**Returns:** Number of bytes written, or -1 on error

### plugin_storage_seek(file, offset, whence)

Seek to a position in the file.

**Parameters:**
- `file`: File handle
- `offset`: Byte offset
- `whence`: `SEEK_SET`, `SEEK_CUR`, or `SEEK_END`

**Returns:** 0 on success, -1 on error

### plugin_storage_tell(file)

Get current position in file.

**Returns:** Current byte offset, or -1 on error

### plugin_storage_close(file)

Close an open file.

### plugin_storage_exists(ctx, path)

Check if a file or directory exists.

**Returns:** `true` if exists, `false` otherwise

### plugin_storage_mkdir(ctx, path)

Create a directory.

**Returns:** `true` on success, `false` on failure

### plugin_storage_remove(ctx, path)

Delete a file or empty directory.

**Returns:** `true` on success, `false` on failure

---

## Memory API

### plugin_malloc(size)

Allocate memory from PSRAM.

**Parameters:**
- `size`: Bytes to allocate

**Returns:** Pointer to allocated memory, or `NULL` on failure

### plugin_calloc(nmemb, size)

Allocate zeroed memory.

**Parameters:**
- `nmemb`: Number of elements
- `size`: Size of each element

**Returns:** Pointer to allocated memory, or `NULL` on failure

### plugin_realloc(ptr, size)

Reallocate memory block.

**Parameters:**
- `ptr`: Existing allocation (or `NULL`)
- `size`: New size

**Returns:** Pointer to reallocated memory, or `NULL` on failure

### plugin_free(ptr)

Free allocated memory.

**Parameters:**
- `ptr`: Pointer to memory (may be `NULL`)

---

## Timer API

### plugin_delay_ms(ms)

Sleep for the specified number of milliseconds.

**Parameters:**
- `ms`: Milliseconds to sleep

**Note:** Uses FreeRTOS vTaskDelay internally. Minimum delay is one tick period.

### plugin_get_tick_ms()

Get the current system tick count in milliseconds.

**Returns:** Milliseconds since boot

---

## Menu API

For `PLUGIN_TYPE_MENU` plugins.

### plugin_menu_add_item(label, icon, callback, arg)

Add an item to the launcher menu.

**Parameters:**
- `label`: Display text for the menu item
- `icon`: Optional 32x32 ARGB icon buffer, or `NULL`
- `callback`: Function called when item is selected
- `arg`: Arbitrary pointer passed to callback

**Returns:** Item ID (>= 0) on success, -1 on error

### plugin_menu_remove_item(item_id)

Remove a previously added menu item.

**Parameters:**
- `item_id`: ID returned by `plugin_menu_add_item`

---

## Event API

For `PLUGIN_TYPE_HOOK` plugins.

### Event Types

| Event | Description |
|-------|-------------|
| `PLUGIN_EVENT_APP_LAUNCH` | An app is being launched |
| `PLUGIN_EVENT_APP_EXIT` | An app has exited |
| `PLUGIN_EVENT_WIFI_CONNECTED` | WiFi connection established |
| `PLUGIN_EVENT_WIFI_DISCONNECTED` | WiFi connection lost |
| `PLUGIN_EVENT_SD_INSERTED` | SD card inserted |
| `PLUGIN_EVENT_SD_REMOVED` | SD card removed |
| `PLUGIN_EVENT_POWER_LOW` | Battery low warning |
| `PLUGIN_EVENT_USB_CONNECTED` | USB cable connected |
| `PLUGIN_EVENT_USB_DISCONNECTED` | USB cable disconnected |

### plugin_event_register(event_mask, handler, arg)

Register a handler for system events.

**Parameters:**
- `event_mask`: Bitmask of events to receive (OR together event types)
- `handler`: Callback function
- `arg`: Arbitrary pointer passed to handler

**Returns:** Handler ID (>= 0) on success, -1 on error

### plugin_event_unregister(handler_id)

Remove an event handler.

---

## Network API

### plugin_net_is_connected()

Check if network is available.

**Returns:** `true` if connected, `false` otherwise

### plugin_http_get(url, response, max_len)

Perform an HTTP GET request.

**Parameters:**
- `url`: Full URL to fetch
- `response`: Buffer to receive response body
- `max_len`: Maximum bytes to store

**Returns:** HTTP status code (200, 404, etc.) or -1 on error

### plugin_http_post(url, body, response, max_len)

Perform an HTTP POST request.

**Parameters:**
- `url`: Full URL
- `body`: Request body (null-terminated string)
- `response`: Buffer to receive response
- `max_len`: Maximum response bytes

**Returns:** HTTP status code or -1 on error

---

## Settings API

Persistent key-value storage for plugin configuration.

### plugin_settings_get_string(ctx, key, value, max_len)

Read a string setting.

**Parameters:**
- `ctx`: Plugin context
- `key`: Setting key
- `value`: Buffer to receive value
- `max_len`: Buffer size

**Returns:** `true` on success, `false` if not found

### plugin_settings_set_string(ctx, key, value)

Write a string setting.

**Returns:** `true` on success

### plugin_settings_get_int(ctx, key, value)

Read an integer setting.

**Parameters:**
- `ctx`: Plugin context
- `key`: Setting key
- `value`: Pointer to receive value

**Returns:** `true` on success, `false` if not found

### plugin_settings_set_int(ctx, key, value)

Write an integer setting.

**Returns:** `true` on success

---

## Data Types

### plugin_info_t

Plugin metadata structure:

```c
typedef struct {
    const char* name;        // Display name (e.g., "My Plugin")
    const char* slug;        // Unique identifier (e.g., "my-plugin")
    const char* version;     // Semantic version (e.g., "1.0.0")
    const char* author;      // Author name
    const char* description; // Short description
    uint32_t api_version;    // API version (use TANMATSU_PLUGIN_API_VERSION)
    plugin_type_t type;      // PLUGIN_TYPE_MENU/SERVICE/HOOK
    uint32_t flags;          // Reserved for future use
} plugin_info_t;
```

### plugin_input_event_t

Input event structure:

```c
typedef struct {
    uint32_t type;       // Event type
    uint32_t key;        // Key code
    bool state;          // true = pressed, false = released
    uint32_t modifiers;  // Modifier keys (shift, ctrl, etc.)
} plugin_input_event_t;
```

### gui_element_icontext_t

Icon and text pair for UI elements:

```c
typedef struct {
    pax_buf_t* icon;  // 32x32 ARGB icon (or NULL)
    char* text;       // Label text (or NULL)
} gui_element_icontext_t;
```

### plugin_status_widget_fn

Status widget callback type:

```c
typedef gui_element_icontext_t (*plugin_status_widget_fn)(void* user_data);
```

---

## Error Handling

Most API functions return error indicators:
- Pointer-returning functions return `NULL` on error
- Integer-returning functions return -1 on error
- Boolean functions return `false` on error

Always check return values and handle errors gracefully.

---

## Thread Safety

- **Display API**: Only call from the main task or during render callbacks
- **Storage API**: Thread-safe, can be called from any task
- **Memory API**: Thread-safe
- **Timer API**: Thread-safe
- **Settings API**: Thread-safe
- **Status Bar API**: Only call during init or from render callbacks

Service plugins run in their own FreeRTOS task. Use appropriate synchronization when sharing data with callbacks.

---

## Example: Minimal Plugin

```c
#include "tanmatsu_plugin.h"

static const plugin_info_t info = {
    .name = "Hello World",
    .slug = "hello-world",
    .version = "1.0.0",
    .author = "Developer",
    .description = "A minimal example plugin",
    .api_version = TANMATSU_PLUGIN_API_VERSION,
    .type = PLUGIN_TYPE_MENU,
    .flags = 0,
};

static const plugin_info_t* get_info(void) {
    return &info;
}

static int plugin_init(plugin_context_t* ctx) {
    plugin_log_info("hello", "Hello from plugin!");
    return 0;
}

static void plugin_cleanup(plugin_context_t* ctx) {
    plugin_log_info("hello", "Goodbye from plugin!");
}

static const plugin_entry_t entry = {
    .get_info = get_info,
    .init = plugin_init,
    .cleanup = plugin_cleanup,
};

TANMATSU_PLUGIN_REGISTER(entry);
```

---

## Changelog

### Version 1.0.0 (Initial Release)
- Core plugin loading and lifecycle
- Logging, display, input APIs
- Storage with sandboxing
- Status bar widget system
- Memory management
- Timer functions
- Menu integration
- Event hooks
- Network helpers
- Settings persistence
