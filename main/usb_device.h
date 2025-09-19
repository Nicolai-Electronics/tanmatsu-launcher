
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    USB_DEBUG    = 0,
    USB_DEVICE   = 1,
    USB_DISABLED = 2,
} usb_mode_t;

void usb_initialize(void);

void       usb_mode_set(usb_mode_t mode);
usb_mode_t usb_mode_get(void);

void usb_send_data(uint8_t const* data, size_t len);
