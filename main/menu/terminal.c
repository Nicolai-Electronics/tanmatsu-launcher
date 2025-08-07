#include "terminal.h"
#include <string.h>
#include <sys/_intsup.h>
#include <time.h>
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/power.h"
#include "common/display.h"
#include "console.h"
#include "driver/uart.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "gui_element_footer.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_types.h"

static char const TAG[] = "terminal";

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_F5), "Settings"}, {get_icon(ICON_F6), "USB mode"}}), 2
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ / ← / → Navigate ⏎ Select"}}), 1
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "🅰 Select"}}), 1
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#endif

#define TERMINAL_UART 0

#define BUFFER_SIZE 4096

static QueueHandle_t uart_queue;
static uint8_t       read_buffer[BUFFER_SIZE] = {0};

static void install_uart_driver(void) {
    if (uart_is_driver_installed(TERMINAL_UART)) return;
    ESP_ERROR_CHECK(uart_driver_install(TERMINAL_UART, BUFFER_SIZE, BUFFER_SIZE, 10, &uart_queue, 0));
    uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(TERMINAL_UART, &uart_config));

    uart_set_pin(TERMINAL_UART, 53, 54, -1, -1);
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_HOME), "Home"}}), 1, FOOTER_LEFT,
                                     FOOTER_RIGHT);
    }
    display_blit_buffer(buffer);
}

void console_write_cb(char* str, size_t len) {
    // uart_write_bytes(TERMINAL_UART, str, len);
}

void menu_terminal(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    struct cons_insts_s console_instance;

    struct cons_config_s con_conf = {
        .font = pax_font_sky_mono, .font_size_mult = 1, .paxbuf = display_get_buffer(), .output_cb = console_write_cb};

    console_init(&console_instance, &con_conf);
    display_blit_buffer(buffer);

    install_uart_driver();

    console_put(&console_instance, '@');
    display_blit_buffer(buffer);

    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
                                } else {
                                    uart_driver_delete(TERMINAL_UART);
                                    return;
                                }
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F2:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_BOOTLOADER);
                                }
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F3:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_APPLICATION);
                                }
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                                uart_write_bytes(TERMINAL_UART, "\n", 1);
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                }
                case INPUT_EVENT_TYPE_ACTION:
                    switch (event.args_action.type) {
                        default:
                            break;
                    }
                    break;
                default:
                    break;
                case INPUT_EVENT_TYPE_KEYBOARD:
                    uart_write_bytes(TERMINAL_UART, event.args_keyboard.utf8, strlen(event.args_keyboard.utf8));
                    break;
            }
        }

        uart_event_t uart_event;
        while (xQueueReceive(uart_queue, (void*)&uart_event, 0) == pdTRUE) {
            bzero(read_buffer, sizeof(read_buffer));
            switch (uart_event.type) {
                case UART_DATA: {
                    int read = uart_read_bytes(TERMINAL_UART, read_buffer, uart_event.size, 0);
                    if (read > 0) {
                        for (int pos = 0; pos < read; pos++) {
                            console_put(&console_instance, read_buffer[pos]);
                            putc(read_buffer[pos], stdout);
                        }
                        display_blit_buffer(buffer);
                    }
                    break;
                }
                // Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow");
                    uart_flush_input(TERMINAL_UART);
                    xQueueReset(uart_queue);
                    break;
                // Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full");
                    uart_flush_input(TERMINAL_UART);
                    xQueueReset(uart_queue);
                    break;
                // Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break");
                    break;
                // Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error");
                    break;
                // Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error");
                    break;
                default:
                    ESP_LOGI(TAG, "uart uart_event type: %d", event.type);
                    break;
            }
        }

        int read = uart_read_bytes(TERMINAL_UART, read_buffer, uart_event.size, 0);
        if (read > 0) {
            for (int pos = 0; pos < read; pos++) {
                console_put(&console_instance, read_buffer[pos]);
                putc(read_buffer[pos], stdout);
            }
            display_blit_buffer(buffer);
        }
    }
}
