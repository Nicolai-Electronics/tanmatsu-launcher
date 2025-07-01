// SPDX-FileCopyrightText: 2019 Ha Thach (tinyusb.org)
// SPDX-License-Identifier: MIT
// SPDX-FileContributor: 2022-2024 Espressif Systems (Shanghai) CO LTD
// SPDX-FileContributor: 2025 Nicolai Electronics

#include "usb_device.h"
#include "sdkconfig.h"
#ifdef CONFIG_IDF_TARGET_ESP32P4

#include <stdlib.h>
#include <string.h>
#include "badgelink.h"
#include "bsp/device.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/usb_serial_jtag_ll.h"
#include "tinyusb.h"

static const char* TAG = "USB device";

static usb_mode_t current_mode = USB_DEBUG;

// Interface counter
enum interface_count {
    ITF_NUM_VENDOR = 0,
    ITF_COUNT
};

// USB Endpoint numbers
enum usb_endpoints {
    // Available USB Endpoints: 5 IN/OUT EPs and 1 IN EP
    EP_EMPTY = 0,
    EPNUM_VENDOR,
};

// TinyUSB descriptors

#define TUSB_DESCRIPTOR_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_VENDOR * TUD_VENDOR_DESC_LEN)

/**
 * @brief String descriptor
 */

#define USB_STRING_LENGTH 32
static char usb_vendor[USB_STRING_LENGTH];
static char usb_product[USB_STRING_LENGTH];
static char usb_serial[USB_STRING_LENGTH];

static const char* s_str_desc[5] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    usb_vendor,            // 1: Manufacturer
    usb_product,           // 2: Product
    "Control interface",   // 3: Control interface
    usb_serial,            // 4: Serials, should use chip ID
};

enum {
    STRING_DESC = 0,
    STRING_DESC_MANUFACTURER,
    STRING_DESC_PRODUCT,
    STRING_DESC_VENDOR,
    STRING_DESC_SERIAL
};

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0210,  // Supported USB standard (2.1)
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,    // Endpoint 0 packet size
    .idVendor           = 0x16D0,                    // MCH2022 badge vendor identifier
    .idProduct          = 0x0F9A,                    // MCH2022 badge product identifier
    .bcdDevice          = 0x0100,                    // Protocol version
    .iManufacturer      = STRING_DESC_MANUFACTURER,  // Index of manufacturer name string
    .iProduct           = STRING_DESC_PRODUCT,       // Index of product name string
    .iSerialNumber      = STRING_DESC_SERIAL,        // Index of serial number string
    .bNumConfigurations = 0x01                       // Number of configurations supported
};

const tusb_desc_device_qualifier_t desc_qualifier = {.bLength            = sizeof(tusb_desc_device_qualifier_t),
                                                     .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
                                                     .bcdUSB             = 0x0200,
                                                     .bDeviceClass       = 0,
                                                     .bDeviceSubClass    = 0,
                                                     .bDeviceProtocol    = 0,
                                                     .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
                                                     .bNumConfigurations = 0x01,
                                                     .bReserved          = 0};

/**
 * @brief Configuration descriptor
 *
 * This is a simple configuration descriptor that defines 1 configuration and a MIDI interface
 */
static const uint8_t s_cfg_desc[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, 0, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, STRING_DESC_VENDOR, EPNUM_VENDOR, (0x80 | EPNUM_VENDOR), 32),
};

#if (TUD_OPT_HIGH_SPEED)
/**
 * @brief High Speed configuration descriptor
 *
 * This is a simple configuration descriptor that defines 1 configuration and a MIDI interface
 */
static const uint8_t s_hs_cfg_desc[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, 0, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, STRING_DESC_VENDOR, EPNUM_VENDOR, (0x80 | EPNUM_VENDOR), 512),
};
#endif  // TUD_OPT_HIGH_SPEED

//--------------------------------------------------------------------+
// BOS Descriptor
//--------------------------------------------------------------------+

/* Microsoft OS 2.0 registry property descriptor
Per MS requirements https://msdn.microsoft.com/en-us/library/windows/hardware/hh450799(v=vs.85).aspx
device should create DeviceInterfaceGUIDs. It can be done by driver and
in case of real PnP solution device should expose MS "Microsoft OS 2.0
registry property descriptor". Such descriptor can insert any record
into Windows registry per device/configuration/interface. In our case it
will insert "DeviceInterfaceGUIDs" multistring property.

GUID is freshly generated and should be OK to use.

https://developers.google.com/web/fundamentals/native-hardware/build-for-webusb/
(Section Microsoft OS compatibility descriptors)
*/

enum {
    VENDOR_REQUEST_WEBUSB    = 1,
    VENDOR_REQUEST_MICROSOFT = 2,
};

#define BOS_TOTAL_LEN (TUD_BOS_DESC_LEN + TUD_BOS_WEBUSB_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

#define MS_OS_20_DESC_LEN 162

// BOS Descriptor is required for webUSB
uint8_t const desc_bos[] = {
    TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 2),                                      // total length, number of device caps
    TUD_BOS_WEBUSB_DESCRIPTOR(VENDOR_REQUEST_WEBUSB, 1),                       // Vendor Code, iLandingPage
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT),  // Microsoft OS 2.0 descriptor
};

uint8_t const* tud_descriptor_bos_cb(void) {
    return desc_bos;
}

uint8_t const desc_ms_os_20[] = {
    // Set header: length, type, windows version, total length
    U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000),
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

    // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
    U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // sub-compatible

    // MS OS 2.0 Registry property descriptor: length, type
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY), U16_TO_U8S_LE(0x0007),
    U16_TO_U8S_LE(
        0x002A),  // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00, 'r',
    0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
    U16_TO_U8S_LE(0x0050),  // wPropertyDataLength
                            // bPropertyData: {70394F16-EDAF-47D5-92C8-7CB51107A235}.
    '{', 0x00, '7', 0x00, '0', 0x00, '3', 0x00, '9', 0x00, '4', 0x00, 'F', 0x00, '1', 0x00, '6', 0x00, '-', 0x00, 'E',
    0x00, 'D', 0x00, 'A', 0x00, 'F', 0x00, '-', 0x00, '4', 0x00, '7', 0x00, 'D', 0x00, '5', 0x00, '-', 0x00, '9', 0x00,
    '2', 0x00, 'C', 0x00, '8', 0x00, '-', 0x00, '7', 0x00, 'C', 0x00, 'B', 0x00, '5', 0x00, '1', 0x00, '1', 0x00, '0',
    0x00, '7', 0x00, 'A', 0x00, '2', 0x00, '3', 0x00, '5', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "Incorrect size");

#define URL "webusb.tanmatsu.cloud"

const tusb_desc_webusb_url_t desc_url = {.bLength         = 3 + sizeof(URL) - 1,
                                         .bDescriptorType = 3,  // WEBUSB URL type
                                         .bScheme         = 1,  // 0: http, 1: https
                                         .url             = URL};

void usb_mode_set(usb_mode_t mode) {
    const usb_serial_jtag_pull_override_vals_t override_disable_usb = {
        .dm_pd = true, .dm_pu = false, .dp_pd = true, .dp_pu = false};
    const usb_serial_jtag_pull_override_vals_t override_enable_usb = {
        .dm_pd = false, .dm_pu = false, .dp_pd = false, .dp_pu = true};

    // Drop off the bus by removing the pull-up on USB DP
    usb_serial_jtag_ll_phy_enable_pull_override(&override_disable_usb);

    // Select USB mode by swapping and un-swapping the two PHYs
    switch (mode) {
        case USB_DEVICE:
            vTaskDelay(pdMS_TO_TICKS(500));  // Wait for disconnect before switching to device
            usb_serial_jtag_ll_phy_select(1);
            break;
        case USB_DEBUG:
        default:
            usb_serial_jtag_ll_phy_select(0);
            vTaskDelay(pdMS_TO_TICKS(500));  // Wait for disconnect after switching to debug
            break;
    }

    // Put the device back onto the bus by re-enabling the pull-up on USB DP
    usb_serial_jtag_ll_phy_enable_pull_override(&override_enable_usb);
    usb_serial_jtag_ll_phy_disable_pull_override();
    current_mode = mode;
}

usb_mode_t usb_mode_get(void) {
    return current_mode;
}

void usb_initialize(void) {
    ESP_LOGI(TAG, "USB initialization");

    // usb_mode_set(USB_DEVICE);

    bsp_device_get_manufacturer(usb_vendor, USB_STRING_LENGTH);
    bsp_device_get_name(usb_product, USB_STRING_LENGTH);

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BASE);
    snprintf(usb_serial, USB_STRING_LENGTH, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
             mac[5]);

    tinyusb_config_t const tusb_cfg = {
        .device_descriptor       = &desc_device,
        .string_descriptor       = s_str_desc,
        .string_descriptor_count = sizeof(s_str_desc) / sizeof(s_str_desc[0]),
        .external_phy            = false,
#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor =
            s_cfg_desc,  // HID configuration descriptor for full-speed and high-speed are the same
        .hs_configuration_descriptor = s_hs_cfg_desc,
        .qualifier_descriptor        = &desc_qualifier,
#else
        .configuration_descriptor = s_cfg_desc,
#endif  // TUD_OPT_HIGH_SPEED
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    ESP_LOGI(TAG, "USB initialization DONE");

    // vTaskDelay(pdMS_TO_TICKS(10000));
    // usb_serial_jtag_ll_phy_select(0);
}

uint16_t webusb_esp32_status                      = 0x0000;
bool     webusb_esp32_reset_requested             = false;
bool     webusb_esp32_reset_mode                  = false;
bool     webusb_esp32_baudrate_override_requested = false;
uint32_t webusb_esp32_baudrate_override_value     = 0;
bool     webusb_esp32_mode_change_requested       = false;
uint8_t  webusb_esp32_mode_change_target          = 0;

#define FW_VERSION 1

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    // nothing to with DATA & ACK stage
    if (stage != CONTROL_STAGE_SETUP) return true;

    switch (request->bmRequestType_bit.type) {
        case TUSB_REQ_TYPE_VENDOR:
            switch (request->bRequest) {
                case VENDOR_REQUEST_WEBUSB:
                    // match vendor request in BOS descriptor
                    // Get landing page url
                    return tud_control_xfer(rhport, request, (void*)(uintptr_t)&desc_url, desc_url.bLength);

                case VENDOR_REQUEST_MICROSOFT:
                    if (request->wIndex == 7) {
                        // Get Microsoft OS 2.0 compatible descriptor
                        uint16_t total_len;
                        memcpy(&total_len, desc_ms_os_20 + 8, 2);

                        return tud_control_xfer(rhport, request, (void*)(uintptr_t)desc_ms_os_20, total_len);
                    } else {
                        return false;
                    }

                default:
                    break;
            }
            break;

        case TUSB_REQ_TYPE_CLASS:
            if (request->bRequest == 0x22) {  // Set status
                if (request->wIndex == ITF_NUM_VENDOR) {
                    webusb_esp32_status = request->wValue;
                    return tud_control_status(rhport, request);
                }
            }
            if (request->bRequest == 0x23) {  // Reset ESP32
                if (request->wIndex == ITF_NUM_VENDOR) {
                    webusb_esp32_reset_requested = true;
                    webusb_esp32_reset_mode      = request->wValue;
                    return tud_control_status(rhport, request);
                }
            }
            if (request->bRequest == 0x24) {  // Set baudrate
                if (request->wIndex == ITF_NUM_VENDOR) {
                    webusb_esp32_baudrate_override_requested = true;
                    webusb_esp32_baudrate_override_value     = (request->wValue) * 100;
                    return tud_control_status(rhport, request);
                }
            }
            if (request->bRequest == 0x25) {  // Set mode
                if (request->wIndex == ITF_NUM_VENDOR) {
                    webusb_esp32_mode_change_requested = true;
                    webusb_esp32_mode_change_target    = request->wValue & 0xFF;
                    return tud_control_status(rhport, request);
                }
            }
            if (request->bRequest == 0x26) {  // Get mode
                if (request->wIndex == ITF_NUM_VENDOR) {
                    return tud_control_xfer(rhport, request, (void*)&webusb_esp32_mode_change_target, 1);
                }
            }
            if (request->bRequest == 0x27) {  // Get firmware version
                uint8_t version = FW_VERSION;
                return tud_control_xfer(rhport, request, (void*)&version, 1);
            }
            break;

        default:
            break;
    }

    // stall unknown request
    return false;
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize) {
    (void)itf;

    badgelink_rxdata_cb(buffer, bufsize);
    // tud_vendor_write(buffer, bufsize);
    // tud_vendor_write_flush();

// if using RX buffered is enabled, we need to flush the buffer to make room for new data
#if CFG_TUD_VENDOR_RX_BUFSIZE > 0
    tud_vendor_read_flush();
#endif
}

void usb_send_data(uint8_t const* data, size_t len) {
    while (len) {
        uint32_t max = tud_vendor_write(data, len);
        tud_vendor_write_flush();
        data += max;
        len  -= max;
    }
}

#else
#include "esp_log.h"
static const char* TAG = "USB device stub";
void               usb_initialize(void) {
    ESP_LOGD(TAG, "USB initialization skipped, no USB peripheral");
}

void usb_send_data(uint8_t const* data, size_t len) {
}

usb_mode_t usb_mode_get(void) {
    return USB_DEVICE;
}

void usb_mode_set(usb_mode_t mode) {
}

#endif
