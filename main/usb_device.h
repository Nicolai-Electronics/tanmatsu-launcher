
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "badgelink.h"

typedef enum {
    USB_DEBUG    = 0,
    USB_DEVICE   = 1,
    USB_DISABLED = 2,
} usb_mode_t;

void usb_initialize(void);

void       usb_mode_set(usb_mode_t mode);
usb_mode_t usb_mode_get(void);

void usb_send_data(uint8_t const* data, size_t len);

// Adapter that translates the host-agnostic badgelink USB-mode enum into
// the launcher-internal usb_mode_t and forwards to usb_mode_set(). Intended
// to be registered via badgelink_set_usb_mode_callback() at startup.
void usb_mode_set_from_badgelink(badgelink_usb_mode_t mode);
