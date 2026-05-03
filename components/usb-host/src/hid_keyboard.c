#include <string.h>
#include "bsp/input.h"
#include "bsp/power.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"
#include "usb/usb_host.h"

static const char* TAG = "hid_kbd";

/* --- HID → BSP scancode mapping --- */

static const bsp_input_scancode_t hid_to_bsp_scancode[256] = {
    [0x00] = BSP_INPUT_SCANCODE_NONE,
    [0x01] = BSP_INPUT_SCANCODE_NONE,
    [0x02] = BSP_INPUT_SCANCODE_NONE,
    [0x03] = BSP_INPUT_SCANCODE_NONE,

    [0x04] = BSP_INPUT_SCANCODE_A,
    [0x05] = BSP_INPUT_SCANCODE_B,
    [0x06] = BSP_INPUT_SCANCODE_C,
    [0x07] = BSP_INPUT_SCANCODE_D,
    [0x08] = BSP_INPUT_SCANCODE_E,
    [0x09] = BSP_INPUT_SCANCODE_F,
    [0x0A] = BSP_INPUT_SCANCODE_G,
    [0x0B] = BSP_INPUT_SCANCODE_H,
    [0x0C] = BSP_INPUT_SCANCODE_I,
    [0x0D] = BSP_INPUT_SCANCODE_J,
    [0x0E] = BSP_INPUT_SCANCODE_K,
    [0x0F] = BSP_INPUT_SCANCODE_L,
    [0x10] = BSP_INPUT_SCANCODE_M,
    [0x11] = BSP_INPUT_SCANCODE_N,
    [0x12] = BSP_INPUT_SCANCODE_O,
    [0x13] = BSP_INPUT_SCANCODE_P,
    [0x14] = BSP_INPUT_SCANCODE_Q,
    [0x15] = BSP_INPUT_SCANCODE_R,
    [0x16] = BSP_INPUT_SCANCODE_S,
    [0x17] = BSP_INPUT_SCANCODE_T,
    [0x18] = BSP_INPUT_SCANCODE_U,
    [0x19] = BSP_INPUT_SCANCODE_V,
    [0x1A] = BSP_INPUT_SCANCODE_W,
    [0x1B] = BSP_INPUT_SCANCODE_X,
    [0x1C] = BSP_INPUT_SCANCODE_Y,
    [0x1D] = BSP_INPUT_SCANCODE_Z,

    [0x1E] = BSP_INPUT_SCANCODE_1,
    [0x1F] = BSP_INPUT_SCANCODE_2,
    [0x20] = BSP_INPUT_SCANCODE_3,
    [0x21] = BSP_INPUT_SCANCODE_4,
    [0x22] = BSP_INPUT_SCANCODE_5,
    [0x23] = BSP_INPUT_SCANCODE_6,
    [0x24] = BSP_INPUT_SCANCODE_7,
    [0x25] = BSP_INPUT_SCANCODE_8,
    [0x26] = BSP_INPUT_SCANCODE_9,
    [0x27] = BSP_INPUT_SCANCODE_0,

    [0x28] = BSP_INPUT_SCANCODE_ENTER,
    [0x29] = BSP_INPUT_SCANCODE_ESC,
    [0x2A] = BSP_INPUT_SCANCODE_BACKSPACE,
    [0x2B] = BSP_INPUT_SCANCODE_TAB,
    [0x2C] = BSP_INPUT_SCANCODE_SPACE,
    [0x2D] = BSP_INPUT_SCANCODE_MINUS,
    [0x2E] = BSP_INPUT_SCANCODE_EQUAL,
    [0x2F] = BSP_INPUT_SCANCODE_LEFTBRACE,
    [0x30] = BSP_INPUT_SCANCODE_RIGHTBRACE,
    [0x31] = BSP_INPUT_SCANCODE_BACKSLASH,
    [0x33] = BSP_INPUT_SCANCODE_SEMICOLON,
    [0x34] = BSP_INPUT_SCANCODE_APOSTROPHE,
    [0x35] = BSP_INPUT_SCANCODE_GRAVE,
    [0x36] = BSP_INPUT_SCANCODE_COMMA,
    [0x37] = BSP_INPUT_SCANCODE_DOT,
    [0x38] = BSP_INPUT_SCANCODE_SLASH,

    [0x39] = BSP_INPUT_SCANCODE_CAPSLOCK,

    [0x3A] = BSP_INPUT_SCANCODE_F1,
    [0x3B] = BSP_INPUT_SCANCODE_F2,
    [0x3C] = BSP_INPUT_SCANCODE_F3,
    [0x3D] = BSP_INPUT_SCANCODE_F4,
    [0x3E] = BSP_INPUT_SCANCODE_F5,
    [0x3F] = BSP_INPUT_SCANCODE_F6,
    [0x40] = BSP_INPUT_SCANCODE_F7,
    [0x41] = BSP_INPUT_SCANCODE_F8,
    [0x42] = BSP_INPUT_SCANCODE_F9,
    [0x43] = BSP_INPUT_SCANCODE_F10,
    [0x44] = BSP_INPUT_SCANCODE_F11,
    [0x45] = BSP_INPUT_SCANCODE_F12,

    // Arrow keys and extended keys
    [0x4F] = BSP_INPUT_SCANCODE_ESCAPED_GREY_RIGHT,
    [0x50] = BSP_INPUT_SCANCODE_ESCAPED_GREY_LEFT,
    [0x51] = BSP_INPUT_SCANCODE_ESCAPED_GREY_DOWN,
    [0x52] = BSP_INPUT_SCANCODE_ESCAPED_GREY_UP,

    [0x49] = BSP_INPUT_SCANCODE_ESCAPED_GREY_PGUP,
    [0x4E] = BSP_INPUT_SCANCODE_ESCAPED_GREY_PGDN,
    [0x4A] = BSP_INPUT_SCANCODE_ESCAPED_GREY_HOME,
    [0x4D] = BSP_INPUT_SCANCODE_ESCAPED_GREY_END,
    [0x4C] = BSP_INPUT_SCANCODE_ESCAPED_GREY_INSERT,
    [0x4B] = BSP_INPUT_SCANCODE_ESCAPED_GREY_DEL,

    /* Numeric keypad */
    [0x53] = BSP_INPUT_SCANCODE_NUMLOCK,          // Num Lock
    [0x54] = BSP_INPUT_SCANCODE_SLASH,            // Keypad / should be another constant
    [0x55] = BSP_INPUT_SCANCODE_KPASTERISK,       // Keypad *
    [0x56] = BSP_INPUT_SCANCODE_KPMINUS,          // Keypad -
    [0x57] = BSP_INPUT_SCANCODE_KPPLUS,           // Keypad +
    [0x58] = BSP_INPUT_SCANCODE_ESCAPED_KPENTER,  // Keypad Enter

    [0x59] = BSP_INPUT_SCANCODE_KP1,
    [0x5A] = BSP_INPUT_SCANCODE_KP2,
    [0x5B] = BSP_INPUT_SCANCODE_KP3,
    [0x5C] = BSP_INPUT_SCANCODE_KP4,
    [0x5D] = BSP_INPUT_SCANCODE_KP5,
    [0x5E] = BSP_INPUT_SCANCODE_KP6,
    [0x5F] = BSP_INPUT_SCANCODE_KP7,
    [0x60] = BSP_INPUT_SCANCODE_KP8,
    [0x61] = BSP_INPUT_SCANCODE_KP9,
    [0x62] = BSP_INPUT_SCANCODE_KP0,

    [0x63] = BSP_INPUT_SCANCODE_KPDOT,  // Keypad .
    // Modifiers handled separately
};

/* Modifiers from byte 0 of boot report */
static void inject_modifier_changes(uint8_t prev, uint8_t curr) {
    const struct {
        uint8_t              mask;
        bsp_input_scancode_t sc;
    } mods[] = {
        {0x01, BSP_INPUT_SCANCODE_LEFTCTRL},      {0x02, BSP_INPUT_SCANCODE_LEFTSHIFT},
        {0x04, BSP_INPUT_SCANCODE_LEFTALT},       {0x08, BSP_INPUT_SCANCODE_ESCAPED_LEFTMETA},
        {0x10, BSP_INPUT_SCANCODE_ESCAPED_RCTRL}, {0x20, BSP_INPUT_SCANCODE_RIGHTSHIFT},
        {0x40, BSP_INPUT_SCANCODE_ESCAPED_RALT},  {0x80, BSP_INPUT_SCANCODE_ESCAPED_RIGHTMETA},
    };

    for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
        bool was = prev & mods[i].mask;
        bool now = curr & mods[i].mask;
        if (was == now) {
            continue;
        }

        bsp_input_event_t bsp_input_event;
        bsp_input_event.type                   = INPUT_EVENT_TYPE_SCANCODE;
        bsp_input_event.args_scancode.scancode = mods[i].sc | (now ? 0x00 : 0x80);
        ESP_LOGI(TAG, "inject_modifier_changes, scancode= %x", bsp_input_event.args_scancode.scancode);
        bsp_input_inject_event(&bsp_input_event);
    }
}

QueueHandle_t app_event_queue = NULL;

/**
 * @brief APP event queue
 *
 * This event is used for delivering the HID Host event from callback to a task.
 */
typedef struct {
    hid_host_device_handle_t handle;
    hid_host_driver_event_t  event;
    void*                    arg;
} app_event_queue_t;

/**
 * @brief HID Protocol string names
 */
static const char* hid_proto_name_str[] = {"NONE", "KEYBOARD", "MOUSE"};

/**
 * @brief Key buffer scan code search.
 *
 * @param[in] src       Pointer to source buffer where to search
 * @param[in] key       Key scancode to search
 * @param[in] length    Size of the source buffer
 */
static inline bool key_found(const uint8_t* const src, uint8_t key, unsigned int length) {
    for (unsigned int i = 0; i < length; i++) {
        if (src[i] == key) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Makes new line depending on report output protocol type
 *
 * @param[in] proto Current protocol to output
 */
static void hid_print_new_device_report_header(hid_protocol_t proto) {
    static hid_protocol_t prev_proto_output = -1;

    if (prev_proto_output != proto) {
        prev_proto_output = proto;
        if (proto == HID_PROTOCOL_MOUSE) {
            ESP_LOGI(TAG, "Mouse");
        } else if (proto == HID_PROTOCOL_KEYBOARD) {
            ESP_LOGI(TAG, "Keyboard");
        } else {
            ESP_LOGI(TAG, "Generic");
        }
    }
}

static void inject_navigation_event(uint8_t hid_scancode, bool state) {
    bsp_input_navigation_key_t key = BSP_INPUT_NAVIGATION_KEY_NONE;

    switch (hid_scancode) {
        case 0x29:
            key = BSP_INPUT_NAVIGATION_KEY_ESC;
            break;
        case 0x50:
            key = BSP_INPUT_NAVIGATION_KEY_LEFT;
            break;
        case 0x4f:
            key = BSP_INPUT_NAVIGATION_KEY_RIGHT;
            break;
        case 0x52:
            key = BSP_INPUT_NAVIGATION_KEY_UP;
            break;
        case 0x51:
            key = BSP_INPUT_NAVIGATION_KEY_DOWN;
            break;
        case 0x4a:
            key = BSP_INPUT_NAVIGATION_KEY_HOME;
            break;
        case 0x4d:
            key = BSP_INPUT_NAVIGATION_KEY_END;
            break;
        case 0x4b:
            key = BSP_INPUT_NAVIGATION_KEY_PGUP;
            break;
        case 0x4e:
            key = BSP_INPUT_NAVIGATION_KEY_PGDN;
            break;
        case 0x5d:
            key = BSP_INPUT_NAVIGATION_KEY_MENU;
            break;
        // case 0x: key = BSP_INPUT_NAVIGATION_KEY_START; break;
        // case 0x: key = BSP_INPUT_NAVIGATION_KEY_SELECT; break;
        case 0x28:
            key = BSP_INPUT_NAVIGATION_KEY_RETURN;
            break;
        case 0xe3:
            key = BSP_INPUT_NAVIGATION_KEY_SUPER;
            break;
        case 0x09:
            key = BSP_INPUT_NAVIGATION_KEY_TAB;
            break;
        case 0x08:
            key = BSP_INPUT_NAVIGATION_KEY_BACKSPACE;
            break;
        case 0x20:
            key = BSP_INPUT_NAVIGATION_KEY_SPACE_M;
            break;
        case 0x70:
            key = BSP_INPUT_NAVIGATION_KEY_F1;
            break;
        case 0x71:
            key = BSP_INPUT_NAVIGATION_KEY_F2;
            break;
        case 0x72:
            key = BSP_INPUT_NAVIGATION_KEY_F3;
            break;
        case 0x73:
            key = BSP_INPUT_NAVIGATION_KEY_F4;
            break;
        case 0x74:
            key = BSP_INPUT_NAVIGATION_KEY_F5;
            break;
        case 0x75:
            key = BSP_INPUT_NAVIGATION_KEY_F6;
            break;
        case 0x76:
            key = BSP_INPUT_NAVIGATION_KEY_F7;
            break;
        case 0x77:
            key = BSP_INPUT_NAVIGATION_KEY_F8;
            break;
        case 0x78:
            key = BSP_INPUT_NAVIGATION_KEY_F9;
            break;
        case 0x79:
            key = BSP_INPUT_NAVIGATION_KEY_F10;
            break;
        case 0x7A:
            key = BSP_INPUT_NAVIGATION_KEY_F11;
            break;
        case 0x7B:
            key = BSP_INPUT_NAVIGATION_KEY_F12;
            break;
        // case 0x: key = BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A; break;
        // case 0x: key = BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B; break;
        // case 0x: key = BSP_INPUT_NAVIGATION_KEY_GAMEPAD_X; break;
        // case 0x: key = BSP_INPUT_NAVIGATION_KEY_GAMEPAD_Y; break;
        // case 0x: key = BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS; break;
        case 0xAF:
            key = BSP_INPUT_NAVIGATION_KEY_VOLUME_UP;
            break;
        case 0xAE:
            key = BSP_INPUT_NAVIGATION_KEY_VOLUME_DOWN;
            break;
        default:
            break;
    }

    if (key != BSP_INPUT_NAVIGATION_KEY_NONE) {
        bsp_input_event_t event = {
            .type                      = INPUT_EVENT_TYPE_NAVIGATION,
            .args_navigation.key       = key,
            .args_navigation.modifiers = 0,
            .args_navigation.state     = state,
        };
        bsp_input_inject_event(&event);
        ESP_LOGI(TAG, "Navigation %02x = %02x", hid_scancode, key);
    } else {
        ESP_LOGI(TAG, "Navigation %02x = unmapped", hid_scancode);
    }
}

/**
 * @brief USB HID Host Keyboard Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_keyboard_report_callback(const uint8_t* const data, const int length) {
    hid_keyboard_input_report_boot_t* kb_report = (hid_keyboard_input_report_boot_t*)data;

    if (length < sizeof(hid_keyboard_input_report_boot_t)) {
        return;
    }

    static uint8_t prev_modifier                   = 0;
    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = {0};

    uint8_t modifier = kb_report->modifier.val;
    inject_modifier_changes(prev_modifier, modifier);
    prev_modifier = modifier;

    bsp_input_event_t bsp_input_event;

    for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {
        // key has been released verification
        if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED && !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
            bsp_input_scancode_t sc                = hid_to_bsp_scancode[prev_keys[i]];
            bsp_input_event.type                   = INPUT_EVENT_TYPE_SCANCODE;
            bsp_input_event.args_scancode.scancode = sc | 0x80;
            bsp_input_inject_event(&bsp_input_event);
            inject_navigation_event(prev_keys[i], false);

            ESP_LOGI(TAG, "released, scancode= %x", bsp_input_event.args_scancode.scancode);
        }

        // key has been pressed verification
        if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
            !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
            bsp_input_scancode_t sc                = hid_to_bsp_scancode[kb_report->key[i]];
            bsp_input_event.type                   = INPUT_EVENT_TYPE_SCANCODE;
            bsp_input_event.args_scancode.scancode = sc;
            bsp_input_inject_event(&bsp_input_event);
            inject_navigation_event(kb_report->key[i], true);
            ESP_LOGI(TAG, "pressed, scancode= %x", bsp_input_event.args_scancode.scancode);
        }
    }
    memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

/**
 * @brief USB HID Host Mouse Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_mouse_report_callback(const uint8_t* const data, const int length) {
    hid_mouse_input_report_boot_t* mouse_report = (hid_mouse_input_report_boot_t*)data;

    if (length < sizeof(hid_mouse_input_report_boot_t)) {
        return;
    }

    static int x_pos = 0;
    static int y_pos = 0;

    // Calculate absolute position from displacement
    x_pos += mouse_report->x_displacement;
    y_pos += mouse_report->y_displacement;

    hid_print_new_device_report_header(HID_PROTOCOL_MOUSE);

    ESP_LOGI(TAG, "X: %06d\tY: %06d\t|%c|%c|", x_pos, y_pos, (mouse_report->buttons.button1 ? 'o' : ' '),
             (mouse_report->buttons.button2 ? 'o' : ' '));
}

/**
 * @brief USB HID Host Generic Interface report callback handler
 *
 * 'generic' means anything else than mouse or keyboard
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_generic_report_callback(const uint8_t* const data, const int length) {
    hid_print_new_device_report_header(HID_PROTOCOL_NONE);
    for (int i = 0; i < length; i++) {
        // printf("%02X", data[i]);
    }
    // putchar('\r');
}

/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host interface event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle, const hid_host_interface_event_t event,
                                 void* arg) {
    uint8_t               data[64]    = {0};
    size_t                data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
            ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle, data, 64, &data_length));

            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                    hid_host_keyboard_report_callback(data, data_length);
                } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
                    hid_host_mouse_report_callback(data, data_length);
                }
            } else {
                hid_host_generic_report_callback(data, data_length);
            }

            break;
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED", hid_proto_name_str[dev_params.proto]);
            ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
            break;
        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR", hid_proto_name_str[dev_params.proto]);
            break;
        default:
            ESP_LOGW(TAG, "HID Device, protocol '%s' Unhandled event: %d (possibly suspend/resume)",
                     hid_proto_name_str[dev_params.proto], event);
            break;
    }
}

/**
 * @brief USB HID Host Device event
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host Device event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_device_event(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event, void* arg) {
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
        case HID_HOST_DRIVER_EVENT_CONNECTED:
            ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED", hid_proto_name_str[dev_params.proto]);

            const hid_host_device_config_t dev_config = {.callback = hid_host_interface_callback, .callback_arg = NULL};

            if (dev_params.proto != HID_PROTOCOL_NONE) {
                ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
                if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                    ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
                    if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                        ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
                    }
                }
                ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Start USB Host install and handle common USB host library events while app pin not low
 *
 * @param[in] arg  Not used
 */
static void usb_lib_task(void* arg) {
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags     = ESP_INTR_FLAG_LOWMED,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);

    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        // In this example, there is only one client registered
        // So, once we deregister the client, this call must succeed with ESP_OK
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }

    ESP_LOGI(TAG, "USB shutdown");
    // Clean up USB Host
    vTaskDelay(10);  // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

static void hid_event_task(void* arg) {
    app_event_queue_t evt_queue;

    while (1) {
        if (xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
            hid_host_device_event(evt_queue.handle, evt_queue.event, evt_queue.arg);
        }
    }
}

/**
 * @brief HID Host Device callback
 *
 * Puts new HID Device event to the queue
 *
 * @param[in] hid_device_handle HID Device handle
 * @param[in] event             HID Device event
 * @param[in] arg               Not used
 */
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event,
                              void* arg) {
    const app_event_queue_t evt_queue = {// HID Host Device related info
                                         .handle = hid_device_handle,
                                         .event  = event,
                                         .arg    = arg};

    if (app_event_queue) {
        xQueueSend(app_event_queue, &evt_queue, 0);
    }
}

/* --- Public API --- */

esp_err_t hid_kbd_init(void) {
    bsp_power_set_usb_host_boost_enabled(true);

    /*
     * Create usb_lib_task to:
     * - initialize USB Host library
     * - Handle USB Host events while APP pin in in HIGH state
     */
    BaseType_t task_created =
        xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096, xTaskGetCurrentTaskHandle(), 2, NULL, 0);
    if (task_created != pdTRUE) {
        return ESP_FAIL;
    }

    // Wait for notification from usb_lib_task to proceed
    ulTaskNotifyTake(false, 1000);

    /*
     * HID host driver configuration
     * - create background task for handling low level event inside the HID driver
     * - provide the device callback to get new HID Device connection event
     */
    const hid_host_driver_config_t hid_host_driver_config = {.create_background_task = true,
                                                             .task_priority          = 5,
                                                             .stack_size             = 4096,
                                                             .core_id                = 0,
                                                             .callback               = hid_host_device_callback,
                                                             .callback_arg           = NULL};

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

    // Create queue
    app_event_queue = xQueueCreate(32, sizeof(app_event_queue_t));

    ESP_LOGI(TAG, "Waiting for HID Device to be connected");

    task_created = xTaskCreatePinnedToCore(hid_event_task, "hid_event", 4096, xTaskGetCurrentTaskHandle(), 2, NULL, 0);
    if (task_created != pdTRUE) {
        return ESP_FAIL;
    }

    return ESP_OK;
}
