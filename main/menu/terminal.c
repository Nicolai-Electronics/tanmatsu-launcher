#include "terminal.h"
#include <string.h>
#include <time.h>
#include "bsp/display.h"
#include "bsp/input.h"
#include "common/display.h"
#include "console.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "gui_footer.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_types.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT  ((gui_header_field_t[]){{get_icon(ICON_F5), "Settings"}, {get_icon(ICON_F6), "USB mode"}}), 2
#define FOOTER_RIGHT ((gui_header_field_t[]){{NULL, "↑ / ↓ / ← / → Navigate ⏎ Select"}}), 1
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT ((gui_header_field_t[]){{NULL, "🅰 Select"}}), 1
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#endif

static void render(pax_buf_t* buffer, gui_theme_t* theme, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_HOME), "Home"}}), 1, FOOTER_LEFT,
                                     FOOTER_RIGHT);
    }
    display_blit_buffer(buffer);
}

void my_console_output(char* str, size_t len) {
    /*for (size_t i = 0; i < len; i++) {
        printf("%c", str[i]);
    }
    printf("\r\n");*/
}

void menu_terminal(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    struct cons_insts_s console_instance;

    struct cons_config_s con_conf = {
        .font = pax_font_sky_mono, .font_size_mult = 1, .paxbuf = display_get_buffer(), .output_cb = my_console_output};

    console_init(&console_instance, &con_conf);
    display_blit_buffer(buffer);

    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                                console_put(&console_instance, '\n');
                                display_blit_buffer(buffer);
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
                    console_put(&console_instance, event.args_keyboard.ascii);
                    display_blit_buffer(buffer);
                    break;
            }
        }
    }
}
