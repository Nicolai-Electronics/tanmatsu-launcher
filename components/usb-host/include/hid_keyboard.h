#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize USB host + HID host for boot keyboards
esp_err_t hid_kbd_init(void);

// Feed one boot-keyboard report into the common BSP input translator.
// This is also used by keyboards transported over I2C, such as the Tab5 A164.
void hid_kbd_process_report(uint8_t modifier, const uint8_t keys[6]);

#ifdef __cplusplus
}
#endif
