// USB HID gamepad support: turns buttons, hat switch and the left stick into BSP navigation events.

#include <stddef.h>
#include "bsp/input.h"
#include "esp_log.h"
#include "hid_report.h"

static const char* TAG = "hid_gamepad";

// Distance from the center (0x80) before an analog stick counts as a direction
#define STICK_DEADZONE 48

static void inject_navigation(bsp_input_navigation_key_t key, bool state) {
    bsp_input_event_t event = {
        .type                      = INPUT_EVENT_TYPE_NAVIGATION,
        .args_navigation.key       = key,
        .args_navigation.modifiers = 0,
        .args_navigation.state     = state,
    };
    bsp_input_inject_event(&event);
}

void hid_gamepad_handle_report(const uint8_t* data, int length) {
    if (length < 10) {
        ESP_LOGD(TAG, "Ignoring %d byte report", length);
        return;
    }

    hid_gamepad_report_t report = hid_parse_gamepad_report(data, length);

    // The left stick doubles as a d-pad
    bool left  = report.buttons.left || report.lx < (0x80 - STICK_DEADZONE);
    bool right = report.buttons.right || report.lx > (0x80 + STICK_DEADZONE);
    bool up    = report.buttons.up || report.ly < (0x80 - STICK_DEADZONE);
    bool down  = report.buttons.down || report.ly > (0x80 + STICK_DEADZONE);

    const struct {
        bool                       state;
        bsp_input_navigation_key_t key;
    } keys[] = {
        {left, BSP_INPUT_NAVIGATION_KEY_LEFT},
        {right, BSP_INPUT_NAVIGATION_KEY_RIGHT},
        {up, BSP_INPUT_NAVIGATION_KEY_UP},
        {down, BSP_INPUT_NAVIGATION_KEY_DOWN},
        {report.buttons.a, BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A},
        {report.buttons.b, BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B},
        {report.buttons.x, BSP_INPUT_NAVIGATION_KEY_GAMEPAD_X},
        {report.buttons.y, BSP_INPUT_NAVIGATION_KEY_GAMEPAD_Y},
        {report.buttons.start, BSP_INPUT_NAVIGATION_KEY_START},
        {report.buttons.select, BSP_INPUT_NAVIGATION_KEY_SELECT},
        {report.buttons.home, BSP_INPUT_NAVIGATION_KEY_HOME},
        {report.buttons.l1, BSP_INPUT_NAVIGATION_KEY_PGUP},
        {report.buttons.r1, BSP_INPUT_NAVIGATION_KEY_PGDN},
        {report.buttons.l3 || report.buttons.r3, BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS},
    };

    // Only send events on state changes, gamepads report their full state continuously
    static uint32_t prev_state = 0;
    uint32_t        state      = 0;

    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (keys[i].state) {
            state |= (1 << i);
        }
        bool was = prev_state & (1 << i);
        if (was != keys[i].state) {
            inject_navigation(keys[i].key, keys[i].state);
            ESP_LOGD(TAG, "Navigation key %d = %d", keys[i].key, keys[i].state);
        }
    }

    prev_state = state;
}
