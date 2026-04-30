#include "usb_debug_listener.h"
#include "sdkconfig.h"

#ifdef CONFIG_IDF_TARGET_ESP32P4

#include <string.h>
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb_device.h"

static const char* TAG = "usb_dbg_listener";

#define LISTENER_RX_BUFFER_SIZE  256
#define LISTENER_TX_BUFFER_SIZE  64
#define LISTENER_TASK_STACK_SIZE 3072
#define LINE_BUFFER_SIZE         32

static const char TOKEN_BADGELINK[] = "BADGELINK";
static const char ACK_BADGELINK[]   = "OK switching to badgelink mode\r\n";

static void usb_debug_listener_task(void* pvParameters) {
    usb_serial_jtag_driver_config_t config = {
        .rx_buffer_size = LISTENER_RX_BUFFER_SIZE,
        .tx_buffer_size = LISTENER_TX_BUFFER_SIZE,
    };
    esp_err_t res = usb_serial_jtag_driver_install(&config);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB-serial/JTAG driver: %d", res);
        vTaskDelete(NULL);
        return;
    }

    char   line[LINE_BUFFER_SIZE];
    size_t line_len = 0;

    while (true) {
        uint8_t buf[32];
        int     n = usb_serial_jtag_read_bytes(buf, sizeof(buf), portMAX_DELAY);
        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                line[line_len] = '\0';
                if (strcmp(line, TOKEN_BADGELINK) == 0) {
                    usb_serial_jtag_write_bytes((const uint8_t*)ACK_BADGELINK, strlen(ACK_BADGELINK),
                                                pdMS_TO_TICKS(100));
                    ESP_LOGI(TAG, "Switching to BadgeLink USB mode");
                    usb_mode_set(USB_DEVICE);
                    // Peripheral is now off-bus; reads will block until the
                    // user switches back to USB_DEBUG via the home menu.
                }
                line_len = 0;
            } else if (line_len < sizeof(line) - 1) {
                line[line_len++] = c;
            } else {
                // Line exceeded buffer; drop it and resync on the next newline.
                line_len = 0;
            }
        }
    }
}

void usb_debug_listener_initialize(void) {
    xTaskCreate(usb_debug_listener_task, "usb_dbg_listener", LISTENER_TASK_STACK_SIZE, NULL, 5, NULL);
}

#else

void usb_debug_listener_initialize(void) {
    // USB-serial/JTAG listener only available on ESP32-P4.
}

#endif
