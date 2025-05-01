#include <stdio.h>
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pocketpy.h"
#include "portmacro.h"
#include "sdkconfig.h"

#ifdef CONFIG_IDF_TARGET_ESP32P4
#include "driver/usb_serial_jtag.h"
#include "hal/usb_serial_jtag_ll.h"
#include "projdefs.h"

static const char* TAG = "python";

#define REPL_BUFFER_SIZE       (1024)
#define PYTHON_TASK_STACK_SIZE (4096)

char     input_buffer[64] = {0};
uint32_t input_offset     = 0;
uint32_t input_position   = 0;

char     line_buffer[REPL_BUFFER_SIZE] = {0};
uint32_t line_position                 = 0;

static void python_exec(char* command) {
    py_StackRef p0 = py_peek(0);
    if (!py_exec((char*)command, "<stdin>", SINGLE_MODE, NULL)) {
        py_printexc();
        py_clearexc(p0);
    }
}

static void output_terminal_clear_character(void) {
    const char buffer[] = " \b";
    usb_serial_jtag_write_bytes(buffer, sizeof(buffer), 500 / portTICK_PERIOD_MS);
    usb_serial_jtag_ll_txfifo_flush();
}

static void output_terminal_write(char* buffer, size_t length) {
    usb_serial_jtag_write_bytes(buffer, length, 500 / portTICK_PERIOD_MS);
    usb_serial_jtag_ll_txfifo_flush();
}

static char python_getchar() {
    if (input_position > input_offset) {
        char result = input_buffer[input_offset];
        input_offset++;
        if (input_offset >= input_position) {
            input_offset   = 0;
            input_position = 0;
        }
        return result;
    }
    input_position = usb_serial_jtag_read_bytes(input_buffer, sizeof(input_buffer), portMAX_DELAY);
    output_terminal_write(input_buffer, input_position);
    if (input_position > 0) {
        input_offset = 1;
        return input_buffer[0];
    }
    return '\0';
}

static int python_repl(char* buf, int max_size) {
    buf[0] = '\0';  // reset first char because we check '@' at the beginning

    int  size      = 0;
    bool multiline = false;
    output_terminal_write(">>> ", 4);

    while (true) {
        char c = python_getchar();
        if (c == '\r') {
            c = '\n';
        }

        if (c == '\n') {
            char last = '\0';
            if (size > 0) last = buf[size - 1];
            if (multiline) {
                if (last == '\n') {
                    break;  // 2 consecutive newlines to end multiline input
                } else {
                    output_terminal_write("\r\n... ", 6);
                }
            } else {
                if (last == ':' || last == '(' || last == '[' || last == '{' || buf[0] == '@') {
                    output_terminal_write("\r\n... ", 6);
                    multiline = true;
                } else {
                    break;
                }
            }
        } else if (c == '\b') {
            if (size > 0) {
                output_terminal_clear_character();
                buf[size] = '\0';
                size--;
            } else {
                output_terminal_write(" ", 1);
            }
            continue;
        }

        if (size == max_size - 1) {
            buf[size] = '\0';
            output_terminal_write("\r\n", 2);
            return size;
        }

        buf[size++] = c;
    }

    buf[size] = '\0';
    output_terminal_write("\r\n", 2);
    return size;
}

static void python_task(void* pvParameters) {
    // Configure the USB serial driver
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .rx_buffer_size = REPL_BUFFER_SIZE,
        .tx_buffer_size = REPL_BUFFER_SIZE,
    };

    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));

    // Initialize the Python runtime
    py_initialize();

    while (true) {
        int size = python_repl(line_buffer, sizeof(line_buffer));
        assert(size < sizeof(line_buffer));
        if (size >= 0) {
            python_exec(line_buffer);
        }
    }
}

esp_err_t python_initialize(void) {
    xTaskCreate(python_task, TAG, PYTHON_TASK_STACK_SIZE, NULL, 10, NULL);
    return ESP_OK;
}

// For future use: this is how you run a script
/*
char noodle[] = "for i in range(10):\r\n  print(\"This is PocketPy running a loop i={}\".format(i))\r\n";
if (!py_exec(noodle, "fakefilename.py", EXEC_MODE, NULL)) {
    py_printexc();
}
*/
#endif