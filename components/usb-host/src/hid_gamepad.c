// USB HID gamepad support: turns directions and buttons into BSP navigation events.
//
// Working out which way a pad is being pushed is the badgeteam/hid-host component's job, since
// the C64 emulator needs exactly the same answer. What is left here is what those directions and
// buttons mean to a launcher, which is nobody else's business.

#include "hid_gamepad_nav.h"
#include <string.h>
#include "bsp/input.h"
#include "esp_log.h"
#include "hid_gamepad.h"

static const char* TAG = "hid_gamepad";

// Buttons of a DualShock 3, in the order it reports them
static const bsp_input_navigation_key_t dualshock3_button_map[] = {
    BSP_INPUT_NAVIGATION_KEY_SELECT,          // Select
    BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS,  // Left stick
    BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS,  // Right stick
    BSP_INPUT_NAVIGATION_KEY_START,           // Start
    BSP_INPUT_NAVIGATION_KEY_NONE,            // D-pad up
    BSP_INPUT_NAVIGATION_KEY_NONE,            // D-pad right
    BSP_INPUT_NAVIGATION_KEY_NONE,            // D-pad down
    BSP_INPUT_NAVIGATION_KEY_NONE,            // D-pad left
    BSP_INPUT_NAVIGATION_KEY_NONE,            // L2
    BSP_INPUT_NAVIGATION_KEY_NONE,            // R2
    BSP_INPUT_NAVIGATION_KEY_PGUP,            // L1
    BSP_INPUT_NAVIGATION_KEY_PGDN,            // R1
    BSP_INPUT_NAVIGATION_KEY_GAMEPAD_Y,       // Triangle
    BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B,       // Circle, cancels
    BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A,       // Cross, confirms
    BSP_INPUT_NAVIGATION_KEY_GAMEPAD_X,       // Square
    BSP_INPUT_NAVIGATION_KEY_HOME,            // PS button
};

/// A gamepad whose buttons we know by name, rather than by the order it happens to report them
typedef struct {
    uint16_t                          vid;
    uint16_t                          pid;
    const bsp_input_navigation_key_t* map;  ///< Navigation key per button, indexed the way the report holds them
    size_t                            length;
} hid_gamepad_button_map_t;

static const hid_gamepad_button_map_t button_maps[] = {
    {
        .vid    = 0x054c,
        .pid    = 0x0268,
        .map    = dualshock3_button_map,
        .length = sizeof(dualshock3_button_map) / sizeof(dualshock3_button_map[0]),
    },
};

// Navigation keys the buttons map to when a gamepad has no button map of its own. A descriptor
// names its buttons, so these go by that name rather than by where the button sits in the report.
static const bsp_input_navigation_key_t button_keys[] = {
    BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A,  // Button 1
    BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B,  // Button 2
    BSP_INPUT_NAVIGATION_KEY_GAMEPAD_X,  // Button 3
    BSP_INPUT_NAVIGATION_KEY_GAMEPAD_Y,  // Button 4
    BSP_INPUT_NAVIGATION_KEY_START,      // Button 5
    BSP_INPUT_NAVIGATION_KEY_SELECT,     // Button 6
};

#define BUTTON_KEY_COUNT (sizeof(button_keys) / sizeof(button_keys[0]))

// Buttons of one gamepad that can hold a navigation key, the rest is ignored
#define MAX_BUTTON_KEYS 20

// Everything a report turns into: the four directions followed by the buttons
#define NAVIGATION_KEY_COUNT (4 + MAX_BUTTON_KEYS)

static hid_gamepad_t                     gamepad;
static const bsp_input_navigation_key_t* button_map;
static size_t                            button_map_length;

// What the last report said, so only changes are sent on. Which navigation key each bit stands
// for is kept alongside it, since a gamepad that unplugs while a key is held has to release it.
static uint32_t                   prev_state;
static bsp_input_navigation_key_t prev_keys[NAVIGATION_KEY_COUNT];

static void inject_navigation(bsp_input_navigation_key_t key, bool state) {
    bsp_input_event_t event = {
        .type                      = INPUT_EVENT_TYPE_NAVIGATION,
        .args_navigation.key       = key,
        .args_navigation.modifiers = 0,
        .args_navigation.state     = state,
    };
    bsp_input_inject_event(&event);
}

void hid_gamepad_disconnect(void) {
    // A key that was held when the gamepad went away is never released by a report, so let go of
    // it here. Leaving it down would have the launcher scrolling on by itself.
    for (size_t i = 0; i < NAVIGATION_KEY_COUNT; i++) {
        if (prev_state & ((uint32_t)1 << i)) {
            inject_navigation(prev_keys[i], false);
        }
    }
    prev_state = 0;

    hid_gamepad_close(&gamepad);
    button_map        = NULL;
    button_map_length = 0;
}

bool hid_gamepad_connect(const uint8_t* report_descriptor, size_t length, uint16_t vid, uint16_t pid) {
    hid_gamepad_disconnect();

    if (!hid_gamepad_open(&gamepad, report_descriptor, length, vid, pid)) {
        return false;
    }

    for (size_t i = 0; i < sizeof(button_maps) / sizeof(button_maps[0]); i++) {
        if (button_maps[i].vid == vid && button_maps[i].pid == pid) {
            button_map        = button_maps[i].map;
            button_map_length = button_maps[i].length;
            ESP_LOGI(TAG, "Using the button map of this gamepad rather than report order");
            break;
        }
    }

    return true;
}

void hid_gamepad_handle_report(const uint8_t* data, int length) {
    hid_gamepad_state_t report;
    if (!hid_gamepad_decode(&gamepad, data, length, &report)) {
        return;
    }

    bsp_input_navigation_key_t keys[NAVIGATION_KEY_COUNT] = {
        BSP_INPUT_NAVIGATION_KEY_LEFT,
        BSP_INPUT_NAVIGATION_KEY_RIGHT,
        BSP_INPUT_NAVIGATION_KEY_UP,
        BSP_INPUT_NAVIGATION_KEY_DOWN,
    };
    bool states[NAVIGATION_KEY_COUNT] = {
        report.left,
        report.right,
        report.up,
        report.down,
    };

    size_t next_key = 4;
    for (uint16_t b = 0; b < report.button_count && b < HID_GAMEPAD_MAX_BUTTONS; b++) {
        bsp_input_navigation_key_t key;
        bool                       pressed;

        if (button_map != NULL) {
            // This gamepad is known, so its buttons go where they belong
            key     = b < button_map_length ? button_map[b] : BSP_INPUT_NAVIGATION_KEY_NONE;
            pressed = (report.buttons & ((uint32_t)1 << b)) != 0;
        } else {
            // Nothing is known beyond the descriptor, so take the buttons by the name it gave them
            key     = b < BUTTON_KEY_COUNT ? button_keys[b] : BSP_INPUT_NAVIGATION_KEY_NONE;
            pressed = (report.usage_buttons & ((uint32_t)1 << b)) != 0;
        }

        if (key == BSP_INPUT_NAVIGATION_KEY_NONE || next_key >= NAVIGATION_KEY_COUNT) {
            continue;
        }

        keys[next_key]   = key;
        states[next_key] = pressed;
        next_key++;
    }

    // Only send events on state changes, gamepads report their full state continuously
    uint32_t state = 0;

    for (size_t i = 0; i < NAVIGATION_KEY_COUNT; i++) {
        if (states[i]) {
            state |= (uint32_t)1 << i;
        }
        bool was = (prev_state & ((uint32_t)1 << i)) != 0;
        if (was != states[i]) {
            inject_navigation(keys[i], states[i]);
            ESP_LOGD(TAG, "Navigation key %d = %d", keys[i], states[i]);
        }
        prev_keys[i] = keys[i];
    }

    prev_state = state;
}
