#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Learn the report layout of a connected mouse from its report descriptor
///
/// @return true when the descriptor describes something usable
bool hid_mouse_connect(const uint8_t* report_descriptor, size_t length);

/// @brief Forget the connected mouse and release every button it was holding
void hid_mouse_disconnect(void);

/// @brief Handle a raw mouse report: decode it and inject the resulting BSP input events
void hid_mouse_handle_report(const uint8_t* data, int length);

#ifdef __cplusplus
}
#endif
