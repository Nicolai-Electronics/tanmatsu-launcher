// USB HID mouse support: turns mouse buttons, movement and the scroll wheel into BSP navigation
// events. The BSP has no pointer event type, so movement steps through a menu rather than moving
// a cursor around.

#include <stddef.h>
#include <string.h>
#include "bsp/input.h"
#include "esp_log.h"
#include "hid_layout.h"
#include "hid_mouse.h"

static const char* TAG = "hid_mouse";

// Mouse counts to travel before movement steps one place. A mouse reports somewhere between
// 800 and 1600 counts per inch, so this is roughly a quarter of an inch.
#define MOVE_COUNTS_PER_STEP 300

// Navigation keys the buttons map to, in the order the mouse reports them
static const bsp_input_navigation_key_t button_keys[] = {
    BSP_INPUT_NAVIGATION_KEY_RETURN,          // Left
    BSP_INPUT_NAVIGATION_KEY_ESC,             // Right
    BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS,  // Middle
};

#define BUTTON_KEY_COUNT (sizeof(button_keys) / sizeof(button_keys[0]))

static hid_layout_t layout;

// What the last report said, so only changes are sent on, and how far the mouse has travelled
// since it last stepped. All of it belongs to the mouse that is plugged in now.
static uint32_t prev_buttons;
static int      travelled_x;
static int      travelled_y;

// A plain wheel reports one count per detent, a wheel with a resolution multiplier reports
// eight or more. The largest count seen so far is taken as one detent, so the first turn of an
// unknown wheel moves one place and the wheel calibrates itself from there.
static int detent = 1;
static int scrolled;

static void inject_navigation(bsp_input_navigation_key_t key, bool state) {
    bsp_input_event_t event = {
        .type                      = INPUT_EVENT_TYPE_NAVIGATION,
        .args_navigation.key       = key,
        .args_navigation.modifiers = 0,
        .args_navigation.state     = state,
    };
    bsp_input_inject_event(&event);
}

static void inject_navigation_press(bsp_input_navigation_key_t key) {
    inject_navigation(key, true);
    inject_navigation(key, false);
}

/// @brief Add a movement to a travelled distance, starting over when it turns around
///
/// Starting over on a turn keeps a step in the other direction from having to undo everything
/// that was travelled one way first.
static int accumulate(int travelled, int movement) {
    return (travelled > 0) == (movement > 0) ? travelled + movement : movement;
}

/// @brief Step a navigation key once per whole unit of travel
static void step(int* travelled, int unit, bsp_input_navigation_key_t forward,
                 bsp_input_navigation_key_t backward) {
    while (*travelled >= unit) {
        *travelled -= unit;
        inject_navigation_press(forward);
    }
    while (*travelled <= -unit) {
        *travelled += unit;
        inject_navigation_press(backward);
    }
}

void hid_mouse_disconnect(void) {
    // A button that was held when the mouse went away is never released by a report, so let go of
    // it here. Leaving it down would have the launcher acting on a key nobody is pressing.
    for (uint16_t b = 0; b < BUTTON_KEY_COUNT; b++) {
        if (prev_buttons & (1u << b)) {
            inject_navigation(button_keys[b], false);
        }
    }

    prev_buttons = 0;
    travelled_x  = 0;
    travelled_y  = 0;
    detent       = 1;
    scrolled     = 0;

    memset(&layout, 0, sizeof(layout));
}

bool hid_mouse_connect(const uint8_t* report_descriptor, size_t length) {
    hid_mouse_disconnect();

    if (!hid_layout_parse(report_descriptor, length, &layout)) {
        ESP_LOGW(TAG, "Nothing usable in the report descriptor, ignoring this mouse");
        return false;
    }

    ESP_LOGI(TAG, "Mouse layout: report id %d, x %d, y %d, wheel %d, %d buttons at %d", layout.report_id,
             layout.x.present ? layout.x.bit_offset : -1, layout.y.present ? layout.y.bit_offset : -1,
             layout.wheel.present ? layout.wheel.bit_offset : -1, layout.button_count,
             layout.buttons.present ? layout.buttons.bit_offset : -1);

    return true;
}

void hid_mouse_handle_report(const uint8_t* data, int length) {
    if (!layout.valid || !hid_layout_strip_report_id(&layout, &data, &length)) {
        return;
    }

    uint32_t pressed = 0;
    for (uint16_t b = 0; b < layout.button_count && b < BUTTON_KEY_COUNT; b++) {
        if (hid_layout_read_button(data, length, &layout, b)) {
            pressed |= 1u << b;
        }
    }

    for (uint16_t b = 0; b < BUTTON_KEY_COUNT; b++) {
        bool was = prev_buttons & (1u << b);
        bool now = pressed & (1u << b);
        if (was != now) {
            inject_navigation(button_keys[b], now);
        }
    }
    prev_buttons = pressed;

    int scroll = layout.wheel.present ? hid_layout_read(data, length, &layout.wheel) : 0;
    int turned = scroll < 0 ? -scroll : scroll;
    if (turned > detent) {
        detent   = turned;
        scrolled = 0;
    }
    scrolled = accumulate(scrolled, scroll);
    step(&scrolled, detent, BSP_INPUT_NAVIGATION_KEY_UP, BSP_INPUT_NAVIGATION_KEY_DOWN);

    int moved_x = layout.x.present ? hid_layout_read(data, length, &layout.x) : 0;
    int moved_y = layout.y.present ? hid_layout_read(data, length, &layout.y) : 0;

    travelled_x = accumulate(travelled_x, moved_x);
    travelled_y = accumulate(travelled_y, moved_y);

    step(&travelled_x, MOVE_COUNTS_PER_STEP, BSP_INPUT_NAVIGATION_KEY_RIGHT, BSP_INPUT_NAVIGATION_KEY_LEFT);
    step(&travelled_y, MOVE_COUNTS_PER_STEP, BSP_INPUT_NAVIGATION_KEY_DOWN, BSP_INPUT_NAVIGATION_KEY_UP);

    ESP_LOGD(TAG, "buttons %02X, moved %d %d, scrolled %d of %d per detent", (unsigned)pressed, moved_x, moved_y,
             scroll, detent);
}
