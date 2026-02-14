#include "lora_information.h"
#include <string.h>
#include "bsp/device.h"
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "esp_log.h"
#include "gui_style.h"
#include "icons.h"
#include "lora.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"

const char TAG[] = "LoRa information menu";

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

// Temporarily copied here, should come from LoRa driver instead
typedef enum {
    SX126X_ERROR_RC64K_CALIB_ERR = 0x0001,
    SX126X_ERROR_RC13M_CALIB_ERR = 0x0002,
    SX126X_ERROR_PLL_CALIB_ERR   = 0x0004,
    SX126X_ERROR_ADC_CALIB_ERR   = 0x0008,
    SX126X_ERROR_IMG_CALIB_ERR   = 0x0010,
    SX126X_XOSC_START_ERR        = 0x0020,
    SX126X_PLL_LOCK_ERR          = 0x0040,
    SX126X_PA_RAMP_ERR           = 0x0100,
} sx126x_error_t;

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

    render_base_screen_statusbar(buffer, theme, true, true, true,
                                 ((gui_element_icontext_t[]){{get_icon(ICON_INFO), "LoRa information"}}), 1,
                                 FOOTER_LEFT, FOOTER_RIGHT);

    char text_buffer[256];
    int  line = 0;

    lora_protocol_status_params_t status = {0};
    esp_err_t                     res    = lora_get_status(&status);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read LoRa radio status: %s", esp_err_to_name(res));
        snprintf(text_buffer, sizeof(text_buffer), "Failed to read LoRa radio status");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "%s", esp_err_to_name(res));
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
    } else {
        snprintf(text_buffer, sizeof(text_buffer), "Radio identification: %s", status.version_string);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Radio type:           %s",
                 status.chip_type == LORA_PROTOCOL_CHIP_SX1268 ? "SX1268 (410 - 493 MHz)" : "SX1262 (850 - 930 MHz)");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
    }

    lora_protocol_mode_t mode = LORA_PROTOCOL_MODE_UNKNOWN;
    res                       = lora_get_mode(&mode);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LoRa mode: %s", esp_err_to_name(res));
        snprintf(text_buffer, sizeof(text_buffer), "Failed to get LoRa mode");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "%s", esp_err_to_name(res));
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
    } else {
        snprintf(text_buffer, sizeof(text_buffer), "Current mode:         %s",
                 mode == LORA_PROTOCOL_MODE_STANDBY_RC     ? "Standby (RC)"
                 : mode == LORA_PROTOCOL_MODE_STANDBY_XOSC ? "Standby (XOSC)"
                 : mode == LORA_PROTOCOL_MODE_FS           ? "FS"
                 : mode == LORA_PROTOCOL_MODE_TX           ? "TX"
                 : mode == LORA_PROTOCOL_MODE_RX           ? "RX"
                                                           : "Unknown");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
    }

    lora_protocol_config_params_t config = {0};
    res                                  = lora_get_config(&config);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read LoRa radio config: %s", esp_err_to_name(res));
        snprintf(text_buffer, sizeof(text_buffer), "Failed to read LoRa radio config");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "%s", esp_err_to_name(res));
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
    } else {
        snprintf(text_buffer, sizeof(text_buffer), "Frequency:            %.3f MHz",
                 (float)(config.frequency) / 1000000.0);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Spreading factor:     SF%d", config.spreading_factor);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        float bandwidth_float = config.bandwidth;
        if (config.bandwidth == 7) bandwidth_float = 7.8;
        if (config.bandwidth == 10) bandwidth_float = 10.4;
        if (config.bandwidth == 15) bandwidth_float = 15.6;
        if (config.bandwidth == 20) bandwidth_float = 20.8;
        if (config.bandwidth == 31) bandwidth_float = 31.25;
        if (config.bandwidth == 41) bandwidth_float = 41.7;
        if (config.bandwidth == 62) bandwidth_float = 62.5;
        snprintf(text_buffer, sizeof(text_buffer), "Bandwidth:            %.1f kHz", bandwidth_float);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Coding rate:          4/%d", config.coding_rate);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Power:                %d dBm", config.power);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "CRC enabled:          %s", config.crc_enabled ? "Yes" : "No");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Invert IQ:            %s", config.invert_iq ? "Yes" : "No");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Low data rate opt. :  %s",
                 config.low_data_rate_optimization ? "Yes" : "No");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);

        if (status.errors & SX126X_ERROR_RC64K_CALIB_ERR)
            pax_draw_text(buffer, 0xFFFF0000, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), "RC64K calibration error");
        if (status.errors & SX126X_ERROR_RC13M_CALIB_ERR)
            pax_draw_text(buffer, 0xFFFF0000, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), "RC13M calibration error");
        if (status.errors & SX126X_ERROR_PLL_CALIB_ERR)
            pax_draw_text(buffer, 0xFFFF0000, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), "PLL calibration error");
        if (status.errors & SX126X_ERROR_ADC_CALIB_ERR)
            pax_draw_text(buffer, 0xFFFF0000, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), "ADC calibration error");
        if (status.errors & SX126X_ERROR_IMG_CALIB_ERR)
            pax_draw_text(buffer, 0xFFFF0000, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), "Image calibration error");
        if (status.errors & SX126X_XOSC_START_ERR)
            pax_draw_text(buffer, 0xFFFF0000, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), "XOSC start error");
        if (status.errors & SX126X_PLL_LOCK_ERR)
            pax_draw_text(buffer, 0xFFFF0000, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), "PLL lock error");
        if (status.errors & SX126X_PA_RAMP_ERR)
            pax_draw_text(buffer, 0xFFFF0000, TEXT_FONT, TEXT_SIZE, position.x0,
                          position.y0 + (TEXT_SIZE + 2) * (line++), "PA ramp error");
    }

    line++;

    int line2 = 0;

    for (size_t i = 0; i < num_packets; i++) {
        lora_protocol_lora_packet_t* packet = &packets[i];
        if (packet->length > 0) {
            memset(text_buffer, '\0', sizeof(text_buffer));
            for (size_t j = 0; j < packet->length && j < sizeof(packet->data); j++) {
                char byte_str[3];
                snprintf(byte_str, sizeof(byte_str), "%02X", packet->data[j]);
                strncat(text_buffer, byte_str, sizeof(text_buffer) - strlen(text_buffer) - 1);
            }
        } else {
            snprintf(text_buffer, sizeof(text_buffer), "(no data)");
        }

        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, (TEXT_SIZE / 2), position.x0,
                      position.y0 + ((TEXT_SIZE) + 2) * (line) + (TEXT_SIZE / 2) * (line2++), text_buffer);
    }

    display_blit_buffer(buffer);
}

void test_tx(void) {
    lora_protocol_lora_packet_t packet = {
        .length = 16,
        .data   = {0x42, 0x42, 0xDE, 0xAD, 0xBE, 0xEF, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42},
    };
    esp_err_t res = lora_send_packet(&packet);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send LoRa packet: %s", esp_err_to_name(res));
    } else {
        ESP_LOGI(TAG, "LoRa packet sent");
    }
}

void menu_lora_information(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    lora_protocol_lora_packet_t lora_packets[10] = {0};

    render(false, true, (sizeof(lora_packets) / sizeof(lora_protocol_lora_packet_t)) - 1, lora_packets);

    while (1) {
        bool received = false;
        if (lora_receive_packet(&lora_packets[sizeof(lora_packets) / sizeof(lora_protocol_lora_packet_t) - 1], 0) ==
            ESP_OK) {
            for (size_t i = 1; i < sizeof(lora_packets) / sizeof(lora_protocol_lora_packet_t); i++) {
                memcpy(&lora_packets[i - 1], &lora_packets[i], sizeof(lora_protocol_lora_packet_t));
            }
            printf("Received LoRa packet: length: %d, data: ",
                   lora_packets[sizeof(lora_packets) / sizeof(lora_protocol_lora_packet_t) - 1].length);
            for (size_t j = 0;
                 j < lora_packets[sizeof(lora_packets) / sizeof(lora_protocol_lora_packet_t) - 1].length &&
                 j < sizeof(lora_packets[sizeof(lora_packets) / sizeof(lora_protocol_lora_packet_t) - 1].data);
                 j++) {
                printf("%02X", lora_packets[sizeof(lora_packets) / sizeof(lora_protocol_lora_packet_t) - 1].data[j]);
            }
            printf("\r\n");
            received = true;
        }

        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, received ? 0 : pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_F6:
                                test_tx();
                                break;
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
