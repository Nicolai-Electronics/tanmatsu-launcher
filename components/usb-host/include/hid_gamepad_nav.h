#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Quirks, the report descriptor parser and the decoder that works out which way a pad is being
// pushed live in the badgeteam/hid-host component, since the C64 emulator needs the same answer.
// Include its hid_gamepad.h for hid_gamepad_quirk_t and hid_gamepad_find_quirk(). What is left
// here is what a gamepad means to a launcher.

/// @brief Learn the report layout of a connected gamepad from its report descriptor
///
/// @return true when the descriptor describes something usable
bool hid_gamepad_connect(const uint8_t* report_descriptor, size_t length, uint16_t vid, uint16_t pid);

/// @brief Forget the connected gamepad and release every key it was holding
void hid_gamepad_disconnect(void);

/// @brief Handle a raw gamepad report: decode it and inject the resulting BSP input events
void hid_gamepad_handle_report(const uint8_t* data, int length);

#ifdef __cplusplus
}
#endif
