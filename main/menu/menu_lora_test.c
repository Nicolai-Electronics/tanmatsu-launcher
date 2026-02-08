#include "menu_lora_test.h"
#include <string.h>
#include "bsp/device.h"
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "gui_style.h"
#include "icons.h"
#include "lora.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    18
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{NULL, "🅱 Back"}}), 1
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#endif

static void render(bool partial, bool icons, size_t num_packets, lora_protocol_lora_packet_t* packets) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_INFO), "LoRa test"}}), 1, FOOTER_LEFT,
                                     FOOTER_RIGHT);
    }
    char text_buffer[256];
    int  line = 0;
    snprintf(text_buffer, sizeof(text_buffer), "XXXXX");
    pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                  position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
    display_blit_buffer(buffer);
}

void menu_lora_test(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    lora_protocol_lora_packet_t lora_packets[11] = {0};

    render(false, true, (sizeof(lora_packets) / sizeof(lora_protocol_lora_packet_t)) - 1, lora_packets);

    while (1) {
        bool received = false;
        if (lora_receive_packet(&lora_packets[sizeof(lora_packets) / sizeof(lora_protocol_lora_packet_t)], 0) ==
            pdTRUE) {
            for (size_t i = 1; i < sizeof(lora_packets) / sizeof(lora_protocol_lora_packet_t); i++) {
                memcpy(&lora_packets[i - 1], &lora_packets[i], sizeof(lora_protocol_lora_packet_t));
            }
            received = true;
        }

        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, received ? 0 : pdMS_TO_TICKS(500)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                                return;
                            default:
                                break;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }

        render(true, true, (sizeof(lora_packets) / sizeof(lora_protocol_lora_packet_t)) - 1, lora_packets);
    }
}
