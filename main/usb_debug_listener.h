#pragma once

// Background task that listens on the ESP32-P4 USB-serial/JTAG peripheral
// (the interface visible when USB is in USB_DEBUG / flash-monitor mode) and
// reacts to simple LF- or CRLF-terminated command lines.
//
// Supported commands:
//   BADGELINK    Switch USB to BadgeLink (USB_DEVICE) mode.
void usb_debug_listener_initialize(void);
