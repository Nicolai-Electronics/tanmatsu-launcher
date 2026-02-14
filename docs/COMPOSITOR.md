# Tanmatsu Launcher Display Compositor Research

This document captures research into the tanmatsu-launcher display architecture and a proposal for implementing double-buffered compositing.

## Current Architecture

### Single Shared Framebuffer

The launcher uses a **single shared framebuffer** - all subsystems write to the same buffer.

**Location:** `tanmatsu-launcher/main/common/display.c`

```c
static pax_buf_t fb = {0};  // Single static framebuffer

pax_buf_t* display_get_buffer(void) {
    return &fb;
}

void display_blit_buffer(pax_buf_t* fb) {
    size_t display_h_res = 0, display_v_res = 0;
    ESP_ERROR_CHECK(bsp_display_get_parameters(&display_h_res, &display_v_res, NULL, NULL));
    ESP_ERROR_CHECK(bsp_display_blit(0, 0, display_h_res, display_v_res, pax_buf_get_pixels(fb)));
}
```

### How Subsystems Write to Screen

Every component follows the same pattern:
1. Get the shared buffer: `pax_buf_t* buffer = display_get_buffer();`
2. Draw using PAX graphics functions: `pax_background()`, `pax_draw_text()`, `pax_draw_rect()`, etc.
3. Blit to hardware: `display_blit_buffer(buffer);`

### Layered Rendering (same buffer)

- Background -> Header (status bar) -> Content area -> Footer
- All layers write sequentially to the shared buffer
- Components can overwrite each other's changes

### display_blit_buffer() Call Sites

The blit function is **not** called from a central location. Each subsystem calls it directly:

| Location | # of calls |
|----------|------------|
| `main.c` | 10 |
| `menu/terminal.c` | 5 |
| `menu/textedit.c` | 5 |
| `menu/wifi_edit.c` | 3 |
| `menu/firmware_update.c` | 4 |
| `menu/radio_update.c` | 3 |
| `plugin_api.c` | 4 |
| Various other menus | 1-2 each |

### Screen Buffer Memory

**Tanmatsu Display:** 800x480 pixels

| Format | Bits/pixel | Calculation | Memory |
|--------|------------|-------------|--------|
| RGB565 | 16-bit | 800 x 480 x 2 | **768 KB** |
| RGB888 | 24-bit | 800 x 480 x 3 | **1.125 MB** |
| 2-bit palette (KAMI) | 2-bit | 800 x 480 / 4 | **96 KB** |

Tanmatsu uses **RGB888 (BGR888)**, so the buffer is approximately **1.125 MB**.

### Buffer Allocation Location

The framebuffer is allocated in **PSRAM**.

1. **PAX uses standard `malloc()`** (`pax-graphics/core/src/pax_gfx.c:116`):
   ```c
   mem = malloc(pax_buf_calc_size_dynamic(width, height, type));
   ```

2. **SPIRAM is enabled** in sdkconfig (`sdkconfigs/tanmatsu`):
   ```
   CONFIG_SPIRAM=y
   CONFIG_SPIRAM_SPEED_200M=y
   CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=65536
   ```

3. The 1.125 MB buffer is too large for internal SRAM (~533 KB available), so it must be in PSRAM.

### Plugin Display Access

Plugins currently get direct access to the main framebuffer via the badge-elf-api:

**File:** `esp32-component-badge-elf-api/src/display.c`

```c
uint8_t*   asp_disp_fb      = NULL;  // Raw framebuffer pointer
pax_buf_t* asp_disp_pax_buf = NULL;  // PAX buffer pointer

asp_err_t asp_disp_get_pax_buf(pax_buf_t** buf_out) {
    *buf_out = asp_disp_pax_buf;
    return ASP_OK;
}
```

These pointers are set in `display_init()`:
```c
asp_disp_fb      = pax_buf_get_pixels_rw(&fb);
asp_disp_pax_buf = &fb;
```

---

## Proposed Compositor Architecture

### Goal

Implement double-buffered compositing where:
- The launcher renders to its own buffer
- Plugins render to a separate, smaller buffer
- A central compositor combines them non-destructively
- Neither buffer is overwritten during compositing

### Architecture Diagram

```
+---------------------------------------------------------------------------------+
|                              Compositor                                          |
|                                                                                  |
|  +-------------------+     +-------------------+     +-------------------+       |
|  | Launcher Buffer   |     | Dialog Window 0   |     | Dialog Window N   |       |
|  |    (800x480)      |     |   (variable size) |     |   (variable size) |       |
|  |     1.125 MB      |     |    Plugin A       | ... |    Plugin B       |       |
|  +---------+---------+     +---------+---------+     +---------+---------+       |
|            |                         |                         |                 |
|            +------------+------------+------------+------------+                 |
|                         v                                                        |
|                +----------------+                                                |
|                | Compose Buffer |  (or direct to display)                        |
|                |   (800x480)    |                                                |
|                +--------+-------+                                                |
|                         v                                                        |
|                  bsp_display_blit()                                              |
+---------------------------------------------------------------------------------+

+---------------------------------------------------------------------------------+
|                         Input Event Router                                       |
|                                                                                  |
|    Keyboard Events (BSP)                                                         |
|            |                                                                     |
|            v                                                                     |
|    +---------------+      +------------------+                                   |
|    | Input Router  |----->| ALT+TAB combo?   |-----> Window Switcher             |
|    +---------------+      +--------+---------+       (cycle/overlay)             |
|                                    | no                                          |
|                                    v                                             |
|                           +------------------+                                   |
|                           | Active Window?   |                                   |
|                           +--------+---------+                                   |
|                                    |                                             |
|                 +------------------+------------------+                          |
|                 v                                     v                          |
|    +------------------------+           +------------------------+              |
|    | Topmost Dialog Window  |           | Launcher (no dialogs)  |              |
|    | (plugin callback)      |           | (launcher event loop)  |              |
|    +------------------------+           +------------------------+              |
+---------------------------------------------------------------------------------+
```

### Required Changes

#### 1. compositor.c - Window and Buffer Management

```c
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MAX_DIALOG_WINDOWS 8

typedef uint32_t window_handle_t;
#define INVALID_WINDOW_HANDLE 0

typedef struct {
    window_handle_t handle;           // Unique handle for this window
    bool active;                      // Window slot in use
    pax_buf_t buffer;                 // Window's private framebuffer
    int x, y;                         // Position on screen
    int width, height;                // Window dimensions
    bool dirty;                       // Needs re-compositing
    plugin_context_t* owner;          // Plugin that owns this window (NULL = launcher)
    char title[64];                   // Window title for chrome
    bool modal;                       // If true, blocks input to windows below
} dialog_window_t;

typedef struct {
    // Buffers
    pax_buf_t launcher_fb;            // Launcher's private buffer (always exists)
    pax_buf_t compose_fb;             // Final composition target

    // Window stack (index 0 = bottom, higher = on top)
    dialog_window_t windows[MAX_DIALOG_WINDOWS];
    int window_count;                 // Number of active windows
    window_handle_t next_handle;      // Counter for generating unique handles

    // State
    bool launcher_dirty;
    SemaphoreHandle_t mutex;          // Protects all compositor state
} compositor_state_t;

static compositor_state_t compositor = {0};
```

#### 2. Compositor Function

```c
void compositor_composite(void) {
    xSemaphoreTake(compositor.mutex, portMAX_DELAY);

    // Start with launcher as base layer
    pax_draw_image(&compositor.compose_fb, &compositor.launcher_fb, 0, 0);

    // Composite each dialog window in z-order (bottom to top)
    for (int i = 0; i < compositor.window_count; i++) {
        dialog_window_t* win = &compositor.windows[i];
        if (!win->active) continue;

        // Draw window shadow (offset, semi-transparent black)
        draw_window_shadow(&compositor.compose_fb, win->x + 4, win->y + 4,
                          win->width, win->height);

        // Draw window chrome (border, title bar)
        draw_window_chrome(&compositor.compose_fb, win->x, win->y,
                          win->width, win->height, win->title,
                          i == compositor.window_count - 1);  // highlight if topmost

        // Composite window content
        int content_y = win->y + TITLE_BAR_HEIGHT;
        pax_draw_image(&compositor.compose_fb, &win->buffer, win->x, content_y);

        win->dirty = false;
    }

    compositor.launcher_dirty = false;
    xSemaphoreGive(compositor.mutex);

    // Blit to hardware (outside mutex to avoid blocking)
    bsp_display_blit(0, 0, 800, 480, pax_buf_get_pixels(&compositor.compose_fb));
}
```

#### 3. Memory Requirements

| Buffer | Size (RGB888) | Location |
|--------|---------------|----------|
| Launcher | 1.125 MB | PSRAM |
| Compose | 1.125 MB | PSRAM |
| Dialog window (typical 400x300) | ~360 KB each | PSRAM |
| Dialog window (small 300x200) | ~180 KB each | PSRAM |
| **Base total** | **2.25 MB** | PSRAM |
| **Per dialog (typical)** | **+360 KB** | PSRAM |
| **Max (8 dialogs)** | **~5 MB** | PSRAM (32 MB available) |

Note: Dialog windows are dynamically allocated/freed, so typical memory usage will be much lower than the maximum.

#### 4. API Changes

**For launcher:**
```c
// Buffer access
pax_buf_t* compositor_get_launcher_buffer(void);

// Compositing
void compositor_set_launcher_dirty(void);
void compositor_composite(void);              // Trigger immediate composite
```

**For plugins** (badge-elf-api / tanmatsu_plugin.h):
```c
// Window lifecycle
window_handle_t asp_window_create(uint16_t width, uint16_t height, const char* title);
void asp_window_destroy(window_handle_t handle);
void asp_window_set_position(window_handle_t handle, int x, int y);
void asp_window_set_modal(window_handle_t handle, bool modal);

// Drawing
pax_buf_t* asp_window_get_buffer(window_handle_t handle);
void asp_window_set_dirty(window_handle_t handle);

// Legacy compatibility (opens a default window)
asp_err_t asp_disp_get_plugin_buf(pax_buf_t** buf_out);  // Creates window if needed
```

**Internal compositor API:**
```c
// Called by plugin_manager during plugin cleanup
void compositor_destroy_plugin_windows(plugin_context_t* owner);

// Query state
window_handle_t compositor_get_topmost_window(void);
bool compositor_has_modal_window(void);
```

#### 5. Compositor Trigger Options

**Option A - Explicit calls** (simplest):
- Launcher calls `display_composite()` after rendering
- Plugin calls `asp_disp_request_composite()`

**Option B - Timer-based** (smoother):
- A FreeRTOS task runs at 30-60 Hz
- Checks dirty flags, composites only when needed
- Better for animated content

**Option C - Vsync-driven**:
- Use tearing effect (TE) interrupt
- Composite on vertical blank for tear-free updates

### Implementation Considerations

1. **Thread safety**: Need mutex around compositor state and dirty flags since launcher and plugin run in different tasks

2. **Partial updates**: For efficiency, track dirty rectangles instead of always compositing full screen

3. **Window chrome**: The compositor should draw the plugin window border/shadow, not the plugin itself

4. **Z-order**: Windows are stored in a stack; topmost window receives input

5. **Alpha blending**: PAX's `pax_draw_image()` supports alpha - could have semi-transparent plugin backgrounds

6. **Backward compatibility**: Existing plugin API (`asp_disp_get_pax_buf()`) creates a default window automatically

---

## Input Event Routing

### Central Input Handler

All keyboard/input events flow through a central router that forwards them to the appropriate recipient based on the window stack state.

```c
typedef enum {
    INPUT_TARGET_LAUNCHER,
    INPUT_TARGET_WINDOW,
} input_target_t;

typedef struct {
    input_target_t target;
    window_handle_t window_handle;    // Valid when target == INPUT_TARGET_WINDOW
    plugin_context_t* plugin;         // Plugin that owns the target window
} input_routing_t;

// Determine where input should go
input_routing_t compositor_get_input_target(void) {
    xSemaphoreTake(compositor.mutex, portMAX_DELAY);
    input_routing_t result = { .target = INPUT_TARGET_LAUNCHER };

    // Find topmost window
    for (int i = compositor.window_count - 1; i >= 0; i--) {
        dialog_window_t* win = &compositor.windows[i];
        if (win->active) {
            result.target = INPUT_TARGET_WINDOW;
            result.window_handle = win->handle;
            result.plugin = win->owner;
            break;
        }
    }

    xSemaphoreGive(compositor.mutex);
    return result;
}
```

### Input Router Task

```c
// Callback type for plugins to receive input events
typedef void (*plugin_input_callback_t)(plugin_context_t* ctx, bsp_input_event_t* event);

// Registered per-plugin input handler
static plugin_input_callback_t plugin_input_handlers[MAX_PLUGINS] = {0};

void input_router_task(void* arg) {
    bsp_input_event_t event;

    while (1) {
        // Wait for input event from BSP
        if (bsp_input_get_event(&event, portMAX_DELAY) == ESP_OK) {
            input_routing_t target = compositor_get_input_target();

            if (target.target == INPUT_TARGET_LAUNCHER) {
                // Send to launcher's event queue
                xQueueSend(launcher_input_queue, &event, 0);
            } else {
                // Send to plugin's registered handler
                if (target.plugin && plugin_input_handlers[target.plugin->id]) {
                    plugin_input_handlers[target.plugin->id](target.plugin, &event);
                }
            }
        }
    }
}
```

### Plugin Input API

```c
// Plugin registers to receive input when its window is active
asp_err_t asp_input_register_handler(plugin_input_callback_t callback);

// Plugin can also poll (blocks until event or timeout)
asp_err_t asp_input_get_event(bsp_input_event_t* event_out, uint32_t timeout_ms);

// Check if this plugin's window is currently receiving input
bool asp_input_is_active(void);
```

### Modal Windows

Modal windows block input to all windows below them:

```c
input_routing_t compositor_get_input_target(void) {
    // ... find topmost window ...

    // If topmost window is modal, only it receives input
    // If not modal, input could potentially go to windows below (future: click-through)

    // For now: topmost window always gets input, modal just prevents
    // the user from interacting with launcher until dialog is closed
}
```

### Launcher Integration

The launcher's main loop changes from directly polling BSP to receiving from a queue:

```c
// Old (direct BSP polling):
bsp_input_event_t event;
if (bsp_input_get_event(&event, pdMS_TO_TICKS(100)) == ESP_OK) {
    handle_input(&event);
}

// New (receive from router):
bsp_input_event_t event;
if (xQueueReceive(launcher_input_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
    handle_input(&event);
}
```

---

## Window Lifecycle Management

### Window Creation

```c
window_handle_t compositor_create_window(plugin_context_t* owner,
                                         uint16_t width, uint16_t height,
                                         const char* title) {
    xSemaphoreTake(compositor.mutex, portMAX_DELAY);

    // Find free slot
    dialog_window_t* win = NULL;
    for (int i = 0; i < MAX_DIALOG_WINDOWS; i++) {
        if (!compositor.windows[i].active) {
            win = &compositor.windows[i];
            break;
        }
    }

    if (!win) {
        xSemaphoreGive(compositor.mutex);
        return INVALID_WINDOW_HANDLE;  // No free slots
    }

    // Allocate buffer
    pax_buf_init(&win->buffer, NULL, width, height, PAX_BUF_24_888RGB);

    // Initialize window
    win->handle = ++compositor.next_handle;
    win->active = true;
    win->width = width;
    win->height = height;
    win->x = (800 - width) / 2;   // Center by default
    win->y = (480 - height) / 2;
    win->dirty = true;
    win->owner = owner;
    win->modal = false;
    strncpy(win->title, title ? title : "", sizeof(win->title) - 1);

    compositor.window_count++;

    xSemaphoreGive(compositor.mutex);
    return win->handle;
}
```

### Window Destruction

```c
void compositor_destroy_window(window_handle_t handle) {
    xSemaphoreTake(compositor.mutex, portMAX_DELAY);

    for (int i = 0; i < MAX_DIALOG_WINDOWS; i++) {
        dialog_window_t* win = &compositor.windows[i];
        if (win->active && win->handle == handle) {
            // Free the PAX buffer
            pax_buf_destroy(&win->buffer);

            // Clear the slot
            memset(win, 0, sizeof(dialog_window_t));
            compositor.window_count--;

            // Compact the array (maintain z-order)
            for (int j = i; j < MAX_DIALOG_WINDOWS - 1; j++) {
                compositor.windows[j] = compositor.windows[j + 1];
            }
            memset(&compositor.windows[MAX_DIALOG_WINDOWS - 1], 0,
                   sizeof(dialog_window_t));

            break;
        }
    }

    xSemaphoreGive(compositor.mutex);

    // Trigger re-composite to show what's underneath
    compositor_composite();
}
```

### Plugin Cleanup (Critical for Plugin Manager)

When a plugin exits (normally or crashes), all its windows must be cleaned up:

```c
// Called by plugin_manager when plugin is being unloaded
void compositor_destroy_plugin_windows(plugin_context_t* owner) {
    xSemaphoreTake(compositor.mutex, portMAX_DELAY);

    // Collect handles to destroy (can't modify array while iterating)
    window_handle_t to_destroy[MAX_DIALOG_WINDOWS];
    int destroy_count = 0;

    for (int i = 0; i < MAX_DIALOG_WINDOWS; i++) {
        dialog_window_t* win = &compositor.windows[i];
        if (win->active && win->owner == owner) {
            to_destroy[destroy_count++] = win->handle;
        }
    }

    xSemaphoreGive(compositor.mutex);

    // Destroy each window (this re-acquires mutex internally)
    for (int i = 0; i < destroy_count; i++) {
        ESP_LOGW(TAG, "Force-closing orphaned window %lu from plugin %s",
                 (unsigned long)to_destroy[i],
                 owner->plugin_slug ? owner->plugin_slug : "unknown");
        compositor_destroy_window(to_destroy[i]);
    }
}
```

### Plugin Manager Integration

```c
// In plugin_manager.c, when unloading a plugin:

void plugin_unload(plugin_context_t* ctx) {
    // ... existing cleanup ...

    // Clean up any dialog windows the plugin left open
    compositor_destroy_plugin_windows(ctx);

    // ... rest of cleanup ...
}
```

### Window Handle Tracking Per Plugin

For plugins that want to track their own windows:

```c
// In plugin_context_t (or plugin-side tracking)
typedef struct {
    window_handle_t windows[MAX_WINDOWS_PER_PLUGIN];
    int window_count;
} plugin_window_tracker_t;

// Plugin helper to track windows
void plugin_track_window(plugin_context_t* ctx, window_handle_t handle);
void plugin_untrack_window(plugin_context_t* ctx, window_handle_t handle);
```

---

## Window Z-Order Management

### Bringing Windows to Front

```c
void compositor_bring_to_front(window_handle_t handle) {
    xSemaphoreTake(compositor.mutex, portMAX_DELAY);

    int found_index = -1;
    dialog_window_t found_window;

    // Find the window
    for (int i = 0; i < MAX_DIALOG_WINDOWS; i++) {
        if (compositor.windows[i].active &&
            compositor.windows[i].handle == handle) {
            found_index = i;
            found_window = compositor.windows[i];
            break;
        }
    }

    if (found_index >= 0 && found_index < compositor.window_count - 1) {
        // Shift windows down
        for (int i = found_index; i < compositor.window_count - 1; i++) {
            compositor.windows[i] = compositor.windows[i + 1];
        }
        // Place at top
        compositor.windows[compositor.window_count - 1] = found_window;
    }

    xSemaphoreGive(compositor.mutex);
}
```

---

## Window Switcher (ALT+TAB)

A keyboard shortcut allows users to cycle between active dialog windows, similar to ALT+TAB on desktop operating systems.

### Key Binding

**Default:** `ALT + TAB` (subject to change based on hardware capabilities)

Alternative candidates:
- `Fn + TAB`
- `META + TAB`
- Dedicated hardware button + navigation keys

### Switcher Modes

#### Mode 1: Simple Cycling (Minimal Implementation)

Each press of the key combination cycles to the next window in the stack:

```c
// Key combination detection (in input_router.c)
#define WINDOW_SWITCH_MODIFIER  BSP_INPUT_KEY_ALT   // Subject to change
#define WINDOW_SWITCH_KEY       BSP_INPUT_KEY_TAB

static bool is_modifier_held = false;

void input_router_handle_event(bsp_input_event_t* event) {
    // Track modifier state
    if (event->key == WINDOW_SWITCH_MODIFIER) {
        is_modifier_held = (event->type == BSP_INPUT_EVENT_KEY_DOWN);
    }

    // Check for window switch combo
    if (is_modifier_held &&
        event->key == WINDOW_SWITCH_KEY &&
        event->type == BSP_INPUT_EVENT_KEY_DOWN) {

        compositor_cycle_next_window();
        return;  // Consume the event
    }

    // ... normal event routing ...
}

void compositor_cycle_next_window(void) {
    xSemaphoreTake(compositor.mutex, portMAX_DELAY);

    if (compositor.window_count < 2) {
        // Nothing to cycle (0 or 1 window)
        xSemaphoreGive(compositor.mutex);
        return;
    }

    // Move bottom window to top (rotate the stack)
    dialog_window_t bottom = compositor.windows[0];
    for (int i = 0; i < compositor.window_count - 1; i++) {
        compositor.windows[i] = compositor.windows[i + 1];
    }
    compositor.windows[compositor.window_count - 1] = bottom;

    xSemaphoreGive(compositor.mutex);

    // Redraw with new z-order
    compositor_composite();
}
```

#### Mode 2: Visual Switcher Overlay (Enhanced UX)

Holding the modifier shows a visual picker; releasing confirms selection:

```c
typedef struct {
    bool active;                      // Switcher overlay is visible
    int selected_index;               // Currently highlighted window
    pax_buf_t overlay_buffer;         // Rendered overlay (thumbnails + titles)
} window_switcher_state_t;

static window_switcher_state_t switcher = {0};

void compositor_begin_window_switch(void) {
    xSemaphoreTake(compositor.mutex, portMAX_DELAY);

    if (compositor.window_count < 2) {
        xSemaphoreGive(compositor.mutex);
        return;
    }

    switcher.active = true;
    switcher.selected_index = compositor.window_count - 2;  // Start at second-from-top

    xSemaphoreGive(compositor.mutex);

    compositor_render_switcher_overlay();
    compositor_composite();  // Will include overlay
}

void compositor_switch_next(void) {
    if (!switcher.active) return;

    xSemaphoreTake(compositor.mutex, portMAX_DELAY);
    switcher.selected_index--;
    if (switcher.selected_index < 0) {
        switcher.selected_index = compositor.window_count - 1;
    }
    xSemaphoreGive(compositor.mutex);

    compositor_render_switcher_overlay();
    compositor_composite();
}

void compositor_switch_prev(void) {
    if (!switcher.active) return;

    xSemaphoreTake(compositor.mutex, portMAX_DELAY);
    switcher.selected_index++;
    if (switcher.selected_index >= compositor.window_count) {
        switcher.selected_index = 0;
    }
    xSemaphoreGive(compositor.mutex);

    compositor_render_switcher_overlay();
    compositor_composite();
}

void compositor_confirm_window_switch(void) {
    if (!switcher.active) return;

    xSemaphoreTake(compositor.mutex, portMAX_DELAY);

    // Bring selected window to front
    if (switcher.selected_index >= 0 &&
        switcher.selected_index < compositor.window_count) {
        window_handle_t handle = compositor.windows[switcher.selected_index].handle;
        xSemaphoreGive(compositor.mutex);
        compositor_bring_to_front(handle);
    } else {
        xSemaphoreGive(compositor.mutex);
    }

    switcher.active = false;
    compositor_composite();
}

void compositor_cancel_window_switch(void) {
    switcher.active = false;
    compositor_composite();
}
```

### Switcher Overlay Rendering

```c
void compositor_render_switcher_overlay(void) {
    // Semi-transparent dark background
    pax_background(&switcher.overlay_buffer, 0x80000000);

    int card_width = 150;
    int card_height = 100;
    int padding = 20;
    int total_width = compositor.window_count * (card_width + padding) - padding;
    int start_x = (800 - total_width) / 2;
    int y = (480 - card_height) / 2;

    for (int i = 0; i < compositor.window_count; i++) {
        dialog_window_t* win = &compositor.windows[i];
        int x = start_x + i * (card_width + padding);

        // Card background
        pax_col_t bg_color = (i == switcher.selected_index) ? 0xFF4488FF : 0xFF333333;
        pax_simple_rect(&switcher.overlay_buffer, bg_color, x, y, card_width, card_height);

        // Window thumbnail (scaled down)
        pax_draw_image_sized(&switcher.overlay_buffer, &win->buffer,
                            x + 5, y + 5, card_width - 10, card_height - 30);

        // Window title
        pax_draw_text(&switcher.overlay_buffer, 0xFFFFFFFF, pax_font_sky_mono,
                     12, x + 5, y + card_height - 20, win->title);

        // Selection indicator
        if (i == switcher.selected_index) {
            pax_outline_rect(&switcher.overlay_buffer, 0xFFFFFFFF, x - 2, y - 2,
                            card_width + 4, card_height + 4);
        }
    }
}
```

### Compositor Integration

Update the composite function to include the switcher overlay:

```c
void compositor_composite(void) {
    xSemaphoreTake(compositor.mutex, portMAX_DELAY);

    // ... existing compositing (launcher + windows) ...

    // Draw switcher overlay on top if active
    if (switcher.active) {
        pax_draw_image(&compositor.compose_fb, &switcher.overlay_buffer, 0, 0);
    }

    xSemaphoreGive(compositor.mutex);
    bsp_display_blit(0, 0, 800, 480, pax_buf_get_pixels(&compositor.compose_fb));
}
```

### Input Handling During Switcher

While the switcher is active, input is handled specially:

```c
void input_router_handle_event(bsp_input_event_t* event) {
    // Track modifier state
    if (event->key == WINDOW_SWITCH_MODIFIER) {
        if (event->type == BSP_INPUT_EVENT_KEY_DOWN) {
            is_modifier_held = true;
        } else if (event->type == BSP_INPUT_EVENT_KEY_UP) {
            is_modifier_held = false;
            if (switcher.active) {
                // Modifier released - confirm selection
                compositor_confirm_window_switch();
            }
        }
    }

    // While switcher is active, intercept navigation
    if (switcher.active) {
        if (event->type == BSP_INPUT_EVENT_KEY_DOWN) {
            switch (event->key) {
                case WINDOW_SWITCH_KEY:  // TAB
                case BSP_INPUT_KEY_RIGHT:
                    compositor_switch_next();
                    break;
                case BSP_INPUT_KEY_LEFT:
                    compositor_switch_prev();
                    break;
                case BSP_INPUT_KEY_ESCAPE:
                    compositor_cancel_window_switch();
                    break;
                case BSP_INPUT_KEY_ENTER:
                    compositor_confirm_window_switch();
                    break;
            }
        }
        return;  // Consume all events while switcher is active
    }

    // Initiate switcher
    if (is_modifier_held &&
        event->key == WINDOW_SWITCH_KEY &&
        event->type == BSP_INPUT_EVENT_KEY_DOWN) {
        compositor_begin_window_switch();
        return;
    }

    // ... normal event routing ...
}
```

### Configuration

```c
// In a config header or NVS settings
typedef struct {
    uint8_t switch_modifier;      // Key code for modifier (ALT, Fn, etc.)
    uint8_t switch_key;           // Key code for switch (TAB, etc.)
    bool use_visual_switcher;     // true = overlay, false = simple cycling
} window_switcher_config_t;

// Default configuration
static window_switcher_config_t switcher_config = {
    .switch_modifier = BSP_INPUT_KEY_ALT,
    .switch_key = BSP_INPUT_KEY_TAB,
    .use_visual_switcher = true,
};
```

### Implementation Phases

#### Phase 1: Core Compositor (Minimum Viable)

1. Create `compositor.c` / `compositor.h` with:
   - Launcher buffer + compose buffer allocation
   - Single window support (simplified version of multi-window)
   - Basic `compositor_composite()` function

2. Modify `display.c`:
   - `display_get_buffer()` returns launcher buffer
   - `display_blit_buffer()` calls `compositor_composite()`

3. Add legacy compatibility:
   - `asp_disp_get_pax_buf()` creates a default window and returns its buffer

#### Phase 2: Input Routing

1. Create `input_router.c` / `input_router.h`
2. Add input queue for launcher
3. Modify launcher menus to receive from queue instead of BSP directly
4. Add plugin input callback registration

#### Phase 3: Multi-Window Support

1. Extend compositor to handle window array
2. Implement z-order management
3. Add window chrome rendering (title bar, borders, shadows)

#### Phase 4: Lifecycle Management

1. Add `compositor_destroy_plugin_windows()`
2. Integrate with plugin_manager unload path
3. Add window handle tracking per plugin

#### Phase 5: Window Switcher

1. Add simple cycling (ALT+TAB cycles through windows)
2. Add visual switcher overlay with thumbnails
3. Make key binding configurable

---

## PAX Graphics Compositing Support

PAX has built-in support for compositing via `pax_draw_image()`:

```c
// From pax-graphics/core/include/shapes/pax_misc.h
void pax_draw_image(pax_buf_t *buf, pax_buf_t const *image, float x, float y);
void pax_draw_image_sized(pax_buf_t *buf, pax_buf_t const *image, float x, float y, float width, float height);
```

These functions support alpha blending, making them suitable for compositing plugin windows with transparency effects.

---

## Key Files Reference

### Existing Files (to modify)

- `tanmatsu-launcher/main/common/display.c` - Current display management (simplify to use compositor)
- `tanmatsu-launcher/main/common/display.h` - Display API header
- `esp32-component-badge-elf-api/src/display.c` - Plugin display API (add window functions)
- `tanmatsu-launcher/main/plugin_manager.c` - Add window cleanup on plugin unload
- `tanmatsu-launcher/main/main.c` - Modify input handling to use router

### New Files (to create)

- `tanmatsu-launcher/main/common/compositor.c` - Compositor implementation
- `tanmatsu-launcher/main/common/compositor.h` - Compositor API
- `tanmatsu-launcher/main/common/input_router.c` - Input event routing
- `tanmatsu-launcher/main/common/input_router.h` - Input router API
- `tanmatsu-launcher/main/common/window_chrome.c` - Window decoration rendering
- `tanmatsu-launcher/main/common/window_chrome.h` - Window chrome API
- `tanmatsu-launcher/main/common/window_switcher.c` - ALT+TAB window switching
- `tanmatsu-launcher/main/common/window_switcher.h` - Window switcher API

### Reference Files

- `pax-graphics/core/src/pax_gfx.c` - PAX buffer initialization
- `pax-graphics/core/include/shapes/pax_misc.h` - `pax_draw_image()` for compositing
- `tanmatsu-launcher/sdkconfigs/tanmatsu` - ESP-IDF configuration (SPIRAM settings)
- `tanmatsu-launcher/components/plugin-api/include/tanmatsu_plugin.h` - Plugin API definitions
