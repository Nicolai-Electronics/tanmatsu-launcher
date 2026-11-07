
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "badgelink.h"

void usb_initialize(void);

void usb_send_data(uint8_t const* data, size_t len);

// Adapter that translates the host-agnostic badgelink USB-mode enum into
// the BSP usb mode and forwards it to bsp_usb_device_mode_set(). Intended
// to be registered via badgelink_set_usb_mode_callback() at startup.
void usb_mode_set_from_badgelink(badgelink_usb_mode_t mode);

// Suspend ESP log output, e.g. while BadgeLink is tunneled over the debug UART
// and raw protocol bytes would otherwise be corrupted by log lines.
void usb_suspend_log_output(void);

// Resume ESP log output after switching USB back to debug mode. Log output
// may have been suppressed while BadgeLink was tunneled over the debug UART.
void usb_resume_log_output(void);
