#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Parsed mouse report, normalized across boot and report protocol layouts
typedef struct {
    union {
        struct {
            uint8_t button1  : 1;  // Left
            uint8_t button2  : 1;  // Right
            uint8_t button3  : 1;  // Middle
            uint8_t reserved : 5;
        };
        uint8_t val;
    } buttons;
    int16_t x_displacement;
    int16_t y_displacement;
    int8_t  scroll;  // Vertical wheel
    int8_t  tilt;    // Horizontal wheel
} hid_mouse_report_t;

/// @brief Parsed gamepad report
typedef struct {
    uint8_t report_id;

    union {
        struct {
            uint32_t a : 1;
            uint32_t b : 1;
            uint32_t x : 1;
            uint32_t y : 1;

            uint32_t select : 1;
            uint32_t start  : 1;

            uint32_t l1 : 1;
            uint32_t r1 : 1;
            uint32_t l2 : 1;
            uint32_t r2 : 1;
            uint32_t l3 : 1;
            uint32_t r3 : 1;

            uint32_t home : 1;

            uint32_t l4 : 1;
            uint32_t r4 : 1;

            uint32_t up    : 1;
            uint32_t down  : 1;
            uint32_t left  : 1;
            uint32_t right : 1;

            uint32_t _reserved : 13;
        };
        uint32_t val;
    } buttons;

    uint8_t lx, ly;  // Left stick, 0x80 is centered
    uint8_t rx, ry;  // Right stick, 0x80 is centered
    uint8_t lt, rt;  // Analog triggers
} hid_gamepad_report_t;

/// @brief Parse a raw mouse input report
hid_mouse_report_t hid_parse_mouse_report(const uint8_t* data, int length);

/// @brief Parse a raw gamepad input report, requires at least 10 bytes
hid_gamepad_report_t hid_parse_gamepad_report(const uint8_t* data, int length);

/// @brief Handle a raw mouse report: parse it and inject the resulting BSP input events
void hid_mouse_handle_report(const uint8_t* data, int length);

/// @brief Handle a raw gamepad report: parse it and inject the resulting BSP input events
void hid_gamepad_handle_report(const uint8_t* data, int length);

#ifdef __cplusplus
}
#endif
