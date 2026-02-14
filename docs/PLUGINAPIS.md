# Tanmatsu Plugin API Reference

**API Version:** 2.0.0
**Last Updated:** 2026-02-14

This document describes all APIs available to Tanmatsu plugins. Plugins are dynamically loaded ELF shared libraries that run on the ESP32-P4 processor via the BadgeELF loader. All plugin-callable functions use the `asp_*` naming convention for BadgeELF compatibility.

## Table of Contents

- [Overview](#overview)
- [Plugin Registration](#plugin-registration)
- [Logging API](#logging-api)
- [Display API](#display-api)
- [Status Bar Widget API](#status-bar-widget-api)
- [Input API](#input-api)
- [Input Hook API](#input-hook-api)
- [RGB LED API](#rgb-led-api)
- [Storage API](#storage-api)
- [Memory API](#memory-api)
- [Timer API](#timer-api)
- [Menu API](#menu-api)
- [Event API](#event-api)
- [Network API](#network-api)
- [Settings API](#settings-api)
- [I2C Bus API](#i2c-bus-api)
- [Dialog API](#dialog-api)
- [Data Types](#data-types)

---

## Overview

Tanmatsu plugins can extend the launcher with new menu items, background services, or system event hooks.

### Plugin Types

| Type | Description |
|------|-------------|
| `PLUGIN_TYPE_MENU` (0) | Adds items to the launcher menu |
| `PLUGIN_TYPE_SERVICE` (1) | Background service running in its own FreeRTOS task |
| `PLUGIN_TYPE_HOOK` (2) | Receives system events (app launch, WiFi, power, etc.) |

### API Version

Current API version: `2.0.0`

```c
#define TANMATSU_PLUGIN_API_VERSION_MAJOR 2
#define TANMATSU_PLUGIN_API_VERSION_MINOR 0
#define TANMATSU_PLUGIN_API_VERSION_PATCH 0
#define TANMATSU_PLUGIN_API_VERSION \
    ((TANMATSU_PLUGIN_API_VERSION_MAJOR << 16) | \
     (TANMATSU_PLUGIN_API_VERSION_MINOR << 8) | \
     TANMATSU_PLUGIN_API_VERSION_PATCH)
```

Plugins must specify the API version they were built against. The host will reject plugins with incompatible API versions.

---

## Plugin Registration

### TANMATSU_PLUGIN_REGISTER(entry)

Macro to register a plugin with the host system. Must be called once at file scope. Places the registration structure in the `.plugin_info` ELF section.

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
| `menu_render` | Menu only | Render custom menu content. Receives `plugin_context_t*` and `pax_buf_t*` |
| `menu_select` | Menu only | Handle menu item selection. Return `true` to stay in plugin, `false` to return to menu |
| `service_run` | Service only | Main service loop (runs in dedicated FreeRTOS task) |
| `hook_event` | Hook only | Handle system events. Return 0 if not handled, positive if handled |

### plugin_registration_t

The registration structure written by the `TANMATSU_PLUGIN_REGISTER` macro:

```c
#define TANMATSU_PLUGIN_MAGIC 0x544D5350  // "TMSP"

typedef struct {
    uint32_t magic;             // Must be TANMATSU_PLUGIN_MAGIC
    uint32_t struct_size;       // sizeof(plugin_registration_t)
    plugin_entry_t entry;       // Plugin entry points
} plugin_registration_t;
```

---

## Logging API

### asp_log_info(tag, fmt, ...)

Log an informational message.

**Parameters:**
- `tag`: Log tag (typically plugin slug)
- `fmt`: printf-style format string
- `...`: Format arguments

**Example:**
```c
asp_log_info("my-plugin", "Initialized with value %d", value);
```

### asp_log_warn(tag, fmt, ...)

Log a warning message.

### asp_log_error(tag, fmt, ...)

Log an error message.

---

## Display API

The display API is provided by badge-elf-api. Include `<asp/display.h>` to use these functions.

### asp_disp_get_pax_buf()

Get the PAX graphics buffer for drawing operations.

**Returns:** `pax_buf_t*` - Pointer to the display buffer

### asp_disp_write()

Write the full display buffer to the screen.

### asp_disp_write_part()

Write a partial region of the display buffer to the screen.

### Drawing Primitives

Use PAX library functions directly for drawing (`pax_draw_circle`, `pax_draw_rect`, `pax_draw_text`, etc.). These are already exported via `kbelf_lib_pax_gfx`.

---

## Status Bar Widget API

Register callbacks to render custom widgets in the launcher status bar. Widgets are drawn right-to-left from the right edge of the header.

**Maximum widgets:** 8 across all plugins

### asp_plugin_status_widget_register(ctx, callback, user_data)

Register a status bar widget.

**Parameters:**
- `ctx`: `plugin_context_t*` - Plugin context
- `callback`: `plugin_status_widget_fn` - Render callback
- `user_data`: Arbitrary pointer passed to callback

**Returns:** Widget ID (>= 0) on success, -1 on error

### asp_plugin_status_widget_unregister(widget_id)

Remove a previously registered status bar widget.

**Parameters:**
- `widget_id`: ID returned by `asp_plugin_status_widget_register`

### plugin_status_widget_fn

```c
typedef int (*plugin_status_widget_fn)(pax_buf_t* buffer, int x_right, int y,
                                       int height, void* user_data);
```

The callback receives:
- `buffer`: Display buffer to draw into
- `x_right`: Rightmost X position available (draw to the LEFT of this)
- `y`: Y position of the status bar
- `height`: Height of the status bar area
- `user_data`: User-provided context

**Returns:** Width in pixels used by this widget. The next widget will be drawn to the left of this one.

---

## Input API

### asp_plugin_input_poll(event, timeout_ms)

Poll for an input event with timeout.

**Parameters:**
- `event`: `plugin_input_event_t*` - Pointer to receive the event
- `timeout_ms`: Maximum time to wait in milliseconds

**Returns:** `true` if an event was received, `false` on timeout

### asp_plugin_input_get_key_state(key)

Get the current state of a specific key.

**Parameters:**
- `key`: Key code (from BSP input definitions)

**Returns:** `true` if key is pressed, `false` otherwise

**Note:** Not yet implemented; currently always returns `false`.

### Input Event Types

| Constant | Value | Description |
|----------|-------|-------------|
| `PLUGIN_INPUT_EVENT_TYPE_NONE` | 0 | No event |
| `PLUGIN_INPUT_EVENT_TYPE_NAVIGATION` | 1 | D-pad / navigation keys |
| `PLUGIN_INPUT_EVENT_TYPE_KEYBOARD` | 2 | Keyboard character input |
| `PLUGIN_INPUT_EVENT_TYPE_ACTION` | 3 | Action buttons |
| `PLUGIN_INPUT_EVENT_TYPE_SCANCODE` | 4 | Raw scancodes |

---

## Input Hook API

Input hooks intercept input events before they reach the application. Hooks are called in registration order for every input event. If any hook returns `true`, the event is consumed and not queued for the application.

**Maximum hooks:** 8 across all plugins

### asp_plugin_input_hook_register(ctx, callback, user_data)

Register an input hook.

**Parameters:**
- `ctx`: `plugin_context_t*` - Plugin context
- `callback`: `plugin_input_hook_fn` - Hook callback
- `user_data`: Arbitrary pointer passed to callback

**Returns:** Hook ID (>= 0) on success, -1 on error

### asp_plugin_input_hook_unregister(hook_id)

Unregister a previously registered input hook.

**Parameters:**
- `hook_id`: ID returned by `asp_plugin_input_hook_register`

### asp_plugin_input_inject(event)

Inject a synthetic input event into the input queue. This bypasses hooks and directly queues the event.

**Parameters:**
- `event`: `plugin_input_event_t*` - Event to inject

**Returns:** `true` on success, `false` on error

### plugin_input_hook_fn

```c
typedef bool (*plugin_input_hook_fn)(plugin_input_event_t* event, void* user_data);
```

Return `true` to consume the event (prevent application from seeing it), `false` to pass it through.

---

## RGB LED API

Control the 6 RGB LEDs on the device. LEDs 0-1 are normally used by the system for WiFi and power indicators. Plugins must claim LEDs before using them to prevent conflicts.

```c
#define PLUGIN_LED_COUNT 6
```

### Brightness and Mode

#### asp_led_set_brightness(percentage)

Set overall LED brightness.

**Parameters:**
- `percentage`: 0-100

**Returns:** `true` on success

#### asp_led_get_brightness(out_percentage)

Get overall LED brightness.

**Parameters:**
- `out_percentage`: `uint8_t*` - Pointer to receive brightness value (0-100)

**Returns:** `true` on success

#### asp_led_set_mode(automatic)

Set LED mode.

**Parameters:**
- `automatic`: `true` = system control, `false` = manual/plugin control

Must set to `false` before controlling LEDs directly.

**Returns:** `true` on success

#### asp_led_get_mode(out_automatic)

Get current LED mode.

**Parameters:**
- `out_automatic`: `bool*` - Pointer to receive mode

**Returns:** `true` on success

### Pixel Control

All `set_pixel` functions stage the color but do not update the hardware. Call `asp_led_send()` after setting pixels.

#### asp_led_set_pixel(index, color)

Set LED color using packed RGB.

**Parameters:**
- `index`: 0-5
- `color`: `0xRRGGBB` format

**Returns:** `true` on success

#### asp_led_set_pixel_rgb(index, red, green, blue)

Set LED color using separate RGB components.

**Parameters:**
- `index`: 0-5
- `red`, `green`, `blue`: 0-255

**Returns:** `true` on success

#### asp_led_set_pixel_hsv(index, hue, saturation, value)

Set LED color using HSV.

**Parameters:**
- `index`: 0-5
- `hue`: 0-65535 (maps to 0-360 degrees)
- `saturation`: 0-255
- `value`: 0-255

**Returns:** `true` on success

#### asp_led_send()

Send staged LED data to hardware. Call after setting pixels.

**Returns:** `true` on success

#### asp_led_clear()

Set all LEDs to black and send to hardware.

**Returns:** `true` on success

### LED Claiming

Plugins must claim LEDs to prevent the system and other plugins from overwriting them. Claims are automatically released when the plugin unloads.

#### asp_plugin_led_claim(ctx, index)

Claim an LED for plugin use.

**Parameters:**
- `ctx`: `plugin_context_t*` - Plugin context
- `index`: LED index (0-5)

**Returns:** `true` if claim succeeded, `false` if already claimed by another plugin

#### asp_plugin_led_release(ctx, index)

Release an LED claim.

**Parameters:**
- `ctx`: `plugin_context_t*` - Plugin context
- `index`: LED index (0-5)

---

## Storage API

All storage operations are sandboxed to the plugin's directory. Absolute paths and `..` traversal are rejected.

### asp_plugin_storage_open(ctx, path, mode)

Open a file for reading or writing.

**Parameters:**
- `ctx`: `plugin_context_t*` - Plugin context
- `path`: Relative path within plugin directory
- `mode`: File mode (`"r"`, `"w"`, `"a"`, `"rb"`, `"wb"`, `"ab"`)

**Returns:** `plugin_file_t` on success, `NULL` on failure

### asp_plugin_storage_read(file, buf, size)

Read bytes from an open file.

**Parameters:**
- `file`: File handle from `asp_plugin_storage_open`
- `buf`: Buffer to read into
- `size`: Maximum bytes to read

**Returns:** Number of bytes read

### asp_plugin_storage_write(file, buf, size)

Write bytes to an open file.

**Parameters:**
- `file`: File handle
- `buf`: Buffer containing data to write
- `size`: Number of bytes to write

**Returns:** Number of bytes written

### asp_plugin_storage_seek(file, offset, whence)

Seek to a position in the file.

**Parameters:**
- `file`: File handle
- `offset`: Byte offset
- `whence`: `0` (SET), `1` (CUR), or `2` (END)

**Returns:** 0 on success, -1 on error

### asp_plugin_storage_tell(file)

Get current position in file.

**Returns:** Current byte offset, or -1 on error

### asp_plugin_storage_close(file)

Close an open file.

### asp_plugin_storage_exists(ctx, path)

Check if a file or directory exists.

**Returns:** `true` if exists, `false` otherwise

### asp_plugin_storage_mkdir(ctx, path)

Create a directory.

**Returns:** `true` on success, `false` on failure

### asp_plugin_storage_remove(ctx, path)

Delete a file or empty directory.

**Returns:** `true` on success, `false` on failure

---

## Memory API

Use standard C library functions (`malloc`, `calloc`, `realloc`, `free`). These are exported to plugins via `kbelf_lib_c`.

---

## Timer API

### asp_plugin_delay_ms(ms)

Sleep for the specified number of milliseconds. Uses FreeRTOS `vTaskDelay` internally.

**Parameters:**
- `ms`: Milliseconds to sleep

### asp_plugin_get_tick_ms()

Get the current system tick count in milliseconds.

**Returns:** Milliseconds since boot

### asp_plugin_should_stop(ctx)

Check if a stop has been requested. Service plugins should poll this regularly and exit their `service_run` function when it returns `true`.

**Parameters:**
- `ctx`: `plugin_context_t*` - Plugin context

**Returns:** `true` if the plugin should stop, `false` otherwise

---

## Menu API

For `PLUGIN_TYPE_MENU` plugins.

**Note:** These functions are declared but not yet implemented. They currently return -1 / do nothing.

### asp_plugin_menu_add_item(label, icon, callback, arg)

Add an item to the launcher menu.

**Parameters:**
- `label`: Display text for the menu item
- `icon`: `pax_buf_t*` - Optional icon, or `NULL`
- `callback`: `plugin_menu_callback_t` - Function called when item is selected
- `arg`: Arbitrary pointer passed to callback

**Returns:** Item ID (>= 0) on success, -1 on error

### asp_plugin_menu_remove_item(item_id)

Remove a previously added menu item.

**Parameters:**
- `item_id`: ID returned by `asp_plugin_menu_add_item`

---

## Event API

For `PLUGIN_TYPE_HOOK` plugins. Maximum 16 event handlers across all plugins.

### Event Types

| Constant | Value | Description |
|----------|-------|-------------|
| `PLUGIN_EVENT_APP_LAUNCH` | 0x0001 | An app is being launched |
| `PLUGIN_EVENT_APP_EXIT` | 0x0002 | An app has exited |
| `PLUGIN_EVENT_WIFI_CONNECTED` | 0x0003 | WiFi connection established |
| `PLUGIN_EVENT_WIFI_DISCONNECTED` | 0x0004 | WiFi connection lost |
| `PLUGIN_EVENT_SD_INSERTED` | 0x0005 | SD card inserted |
| `PLUGIN_EVENT_SD_REMOVED` | 0x0006 | SD card removed |
| `PLUGIN_EVENT_POWER_LOW` | 0x0007 | Battery low warning |
| `PLUGIN_EVENT_USB_CONNECTED` | 0x0008 | USB cable connected |
| `PLUGIN_EVENT_USB_DISCONNECTED` | 0x0009 | USB cable disconnected |

### asp_plugin_event_register(ctx, event_mask, handler, arg)

Register a handler for system events.

**Parameters:**
- `ctx`: `plugin_context_t*` - Plugin context
- `event_mask`: Bitmask of events to receive (OR together event types)
- `handler`: `plugin_event_handler_t` callback
- `arg`: Arbitrary pointer passed to handler

**Returns:** Handler ID (>= 0) on success, -1 on error

### asp_plugin_event_unregister(handler_id)

Remove an event handler.

**Parameters:**
- `handler_id`: ID returned by `asp_plugin_event_register`

### plugin_event_handler_t

```c
typedef int (*plugin_event_handler_t)(uint32_t event, void* data, void* arg);
```

Return 0 if not handled, positive if handled.

---

## Network API

### asp_net_is_connected()

Check if network is available.

**Returns:** `true` if connected, `false` otherwise

### asp_http_get(url, response, max_len)

Perform an HTTP GET request.

**Parameters:**
- `url`: Full URL to fetch
- `response`: Buffer to receive response body
- `max_len`: Maximum bytes to store

**Returns:** HTTP status code (200, 404, etc.) or -1 on error

### asp_http_post(url, body, response, max_len)

Perform an HTTP POST request.

**Parameters:**
- `url`: Full URL
- `body`: Request body (null-terminated string)
- `response`: Buffer to receive response
- `max_len`: Maximum response bytes

**Returns:** HTTP status code or -1 on error

---

## Settings API

Persistent key-value storage for plugin configuration, backed by NVS (Non-Volatile Storage). Settings are automatically namespaced per plugin using the plugin slug.

### asp_plugin_settings_get_string(ctx, key, value, max_len)

Read a string setting.

**Parameters:**
- `ctx`: `plugin_context_t*` - Plugin context
- `key`: Setting key
- `value`: Buffer to receive value
- `max_len`: Buffer size

**Returns:** `true` on success, `false` if not found

### asp_plugin_settings_set_string(ctx, key, value)

Write a string setting.

**Returns:** `true` on success

### asp_plugin_settings_get_int(ctx, key, value)

Read an integer setting.

**Parameters:**
- `ctx`: `plugin_context_t*` - Plugin context
- `key`: Setting key
- `value`: `int32_t*` - Pointer to receive value

**Returns:** `true` on success, `false` if not found

### asp_plugin_settings_set_int(ctx, key, value)

Write an integer setting.

**Parameters:**
- `ctx`: `plugin_context_t*` - Plugin context
- `key`: Setting key
- `value`: `int32_t` - Value to store

**Returns:** `true` on success

---

## I2C Bus API

Access I2C peripherals connected to the device. Maximum 8 I2C device handles across all plugins. Devices are automatically closed when the plugin unloads.

### asp_i2c_open(ctx, bus, address, speed_hz)

Open an I2C device on the specified bus.

**Parameters:**
- `ctx`: `plugin_context_t*` - Plugin context
- `bus`: `0` = primary (internal), `1` = external (QWIIC/SAO)
- `address`: 7-bit I2C device address
- `speed_hz`: Clock speed (e.g. `100000` for 100 kHz, `400000` for 400 kHz)

**Returns:** `asp_i2c_device_t` handle on success, `NULL` on failure

### asp_i2c_close(device)

Close an I2C device and release resources.

**Parameters:**
- `device`: Handle from `asp_i2c_open`

### asp_i2c_write(device, data, len)

Write data to an I2C device.

**Parameters:**
- `device`: Device handle
- `data`: `const uint8_t*` - Data to write
- `len`: Number of bytes

**Returns:** `true` on success

### asp_i2c_read(device, data, len)

Read data from an I2C device.

**Parameters:**
- `device`: Device handle
- `data`: `uint8_t*` - Buffer to read into
- `len`: Number of bytes to read

**Returns:** `true` on success

### asp_i2c_write_read(device, write_data, write_len, read_data, read_len)

Write then read in a single I2C transaction (repeated start). Useful for register reads.

**Parameters:**
- `device`: Device handle
- `write_data`: `const uint8_t*` - Data to write (e.g. register address)
- `write_len`: Number of bytes to write
- `read_data`: `uint8_t*` - Buffer for read data
- `read_len`: Number of bytes to read

**Returns:** `true` on success

### asp_i2c_probe(bus, address)

Probe for the presence of an I2C device.

**Parameters:**
- `bus`: `0` = primary, `1` = external
- `address`: 7-bit I2C address to probe

**Returns:** `true` if a device responds at that address

---

## Dialog API

Show modal dialogs that block until the user dismisses them or a timeout expires.

### asp_plugin_show_info_dialog(title, message, timeout_ms)

Show an information dialog with a title and single-line message.

**Parameters:**
- `title`: Dialog title
- `message`: Message text
- `timeout_ms`: Timeout in milliseconds, or `0` for no timeout

**Returns:** `plugin_dialog_result_t`

### asp_plugin_show_text_dialog(title, lines, line_count, timeout_ms)

Show a multi-line text dialog.

**Parameters:**
- `title`: Dialog title
- `lines`: `const char**` - Array of strings to display (max 10 lines)
- `line_count`: Number of lines
- `timeout_ms`: Timeout in milliseconds, or `0` for no timeout

**Returns:** `plugin_dialog_result_t`

### plugin_dialog_result_t

```c
typedef enum {
    PLUGIN_DIALOG_RESULT_OK = 0,      // User dismissed the dialog
    PLUGIN_DIALOG_RESULT_CANCEL = 1,  // Dialog was cancelled
    PLUGIN_DIALOG_RESULT_TIMEOUT = 2, // Dialog timed out
} plugin_dialog_result_t;
```

---

## Data Types

### plugin_info_t

Plugin metadata structure:

```c
typedef struct {
    const char* name;           // Display name (e.g., "My Plugin")
    const char* slug;           // Unique identifier (e.g., "my-plugin")
    const char* version;        // Semantic version (e.g., "1.0.0")
    const char* author;         // Author name
    const char* description;    // Short description
    uint32_t api_version;       // API version (use TANMATSU_PLUGIN_API_VERSION)
    plugin_type_t type;         // PLUGIN_TYPE_MENU/SERVICE/HOOK
    uint32_t flags;             // Reserved for future use
} plugin_info_t;
```

### plugin_input_event_t

Input event structure:

```c
typedef struct {
    uint32_t type;          // Event type (PLUGIN_INPUT_EVENT_TYPE_*)
    uint32_t key;           // Key code (navigation key or ASCII character)
    bool state;             // true = pressed, false = released
    uint32_t modifiers;     // Modifier keys state
} plugin_input_event_t;
```

### plugin_icontext_t

Icon and text pair for UI elements:

```c
typedef struct {
    pax_buf_t* icon;    // Pointer to 32x32 ARGB icon buffer (or NULL)
    char* text;         // Optional text label (or NULL)
} plugin_icontext_t;
```

### plugin_file_t

Opaque file handle for storage operations:

```c
typedef void* plugin_file_t;
```

### asp_i2c_device_t

Opaque I2C device handle:

```c
typedef void* asp_i2c_device_t;
```

---

## Resource Limits

| Resource | Maximum | Scope |
|----------|---------|-------|
| Status widgets | 8 | All plugins combined |
| Input hooks | 8 | All plugins combined |
| Event handlers | 16 | All plugins combined |
| I2C devices | 8 | All plugins combined |
| RGB LEDs | 6 | Device total |
| Text dialog lines | 10 | Per dialog |

All registrations (widgets, hooks, event handlers, LED claims, I2C devices) are automatically cleaned up when a plugin is unloaded.

---

## Thread Safety

- **Display API**: Only call from the main task or during render callbacks
- **Storage API**: Thread-safe, can be called from any task
- **Memory API**: Thread-safe (standard libc)
- **Timer API**: Thread-safe
- **Settings API**: Thread-safe
- **I2C API**: Thread-safe (uses bus claim/release internally)
- **Status Bar API**: Only call during init or from render callbacks
- **LED API**: Thread-safe

Service plugins run in their own FreeRTOS task. Use appropriate synchronization when sharing data with callbacks.

---

## Example: Minimal Service Plugin

```c
#include "tanmatsu_plugin.h"

static const plugin_info_t info = {
    .name = "Hello World",
    .slug = "hello-world",
    .version = "1.0.0",
    .author = "Developer",
    .description = "A minimal example plugin",
    .api_version = TANMATSU_PLUGIN_API_VERSION,
    .type = PLUGIN_TYPE_SERVICE,
    .flags = 0,
};

static const plugin_info_t* get_info(void) {
    return &info;
}

static int plugin_init(plugin_context_t* ctx) {
    asp_log_info("hello", "Hello from plugin!");
    return 0;
}

static void plugin_cleanup(plugin_context_t* ctx) {
    asp_log_info("hello", "Goodbye from plugin!");
}

static void plugin_run(plugin_context_t* ctx) {
    while (!asp_plugin_should_stop(ctx)) {
        asp_log_info("hello", "Still running...");
        asp_plugin_delay_ms(5000);
    }
}

static const plugin_entry_t entry = {
    .get_info = get_info,
    .init = plugin_init,
    .cleanup = plugin_cleanup,
    .service_run = plugin_run,
};

TANMATSU_PLUGIN_REGISTER(entry);
```
