// USB HID mouse support: turns mouse buttons and the scroll wheel into BSP navigation events.
// The BSP has no pointer event type, so cursor movement is only logged.

#include <stddef.h>
#include "bsp/input.h"
#include "esp_log.h"
#include "hid_report.h"

static const char* TAG = "hid_mouse";

// Limit for how many navigation events a single wheel report may generate
#define SCROLL_STEP_LIMIT 8

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

void hid_mouse_handle_report(const uint8_t* data, int length) {
    hid_mouse_report_t report = hid_parse_mouse_report(data, length);

    static uint8_t prev_buttons = 0;
    static int     x_pos        = 0;
    static int     y_pos        = 0;

    const struct {
        uint8_t                    mask;
        bsp_input_navigation_key_t key;
    } buttons[] = {
        {0x01, BSP_INPUT_NAVIGATION_KEY_RETURN},
        {0x02, BSP_INPUT_NAVIGATION_KEY_ESC},
        {0x04, BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS},
    };

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
        bool was = prev_buttons & buttons[i].mask;
        bool now = report.buttons.val & buttons[i].mask;
        if (was != now) {
            inject_navigation(buttons[i].key, now);
        }
    }
    prev_buttons = report.buttons.val;

    // Wheel up scrolls up, wheel down scrolls down
    int steps = report.scroll;
    if (steps > SCROLL_STEP_LIMIT) {
        steps = SCROLL_STEP_LIMIT;
    }
    if (steps < -SCROLL_STEP_LIMIT) {
        steps = -SCROLL_STEP_LIMIT;
    }
    for (int i = 0; i < steps; i++) {
        inject_navigation_press(BSP_INPUT_NAVIGATION_KEY_UP);
    }
    for (int i = 0; i > steps; i--) {
        inject_navigation_press(BSP_INPUT_NAVIGATION_KEY_DOWN);
    }

    x_pos += report.x_displacement;
    y_pos += report.y_displacement;
    ESP_LOGD(TAG, "X: %06d Y: %06d |%c|%c|%c| scroll: %d tilt: %d", x_pos, y_pos, report.buttons.button1 ? 'o' : ' ',
             report.buttons.button3 ? 'o' : ' ', report.buttons.button2 ? 'o' : ' ', report.scroll, report.tilt);
}
