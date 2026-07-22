#include "menu_device_information.h"
#include <string.h>
#include "bsp/device.h"
#include "bsp/input.h"
#include "common/display.h"
#include "device_information.h"
#include "esp_app_desc.h"
#include "gui_element_footer.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/menu_helpers.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"
#include "radio_system_protocol_client.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#include "bsp/tanmatsu.h"
#include "tanmatsu_coprocessor.h"
#endif

#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    18

static void render(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_INFO), "Device information"}}), 1,
                                     FOOTER_LEFT, FOOTER_RIGHT);
    }
    char text_buffer[256];
    int  line = 0;
    if (!partial) {
        const esp_app_desc_t*               app_description   = esp_app_get_description();
        radio_system_protocol_information_t radio_information = {0};
        if (radio_system_protocol_get_information(&radio_information) != ESP_OK) {
            snprintf(radio_information.firmware_version, 32, "Unknown");
        }

        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), "Firmware versions:");
        snprintf(text_buffer, sizeof(text_buffer), "Launcher    %s", app_description->version);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Radio       %s", radio_information.firmware_version);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
        uint16_t                      coprocessor_firmware_version;
        tanmatsu_coprocessor_handle_t coprocessor_handle = NULL;
        if (bsp_tanmatsu_coprocessor_get_handle(&coprocessor_handle) == ESP_OK) {
            tanmatsu_coprocessor_get_firmware_version(coprocessor_handle, &coprocessor_firmware_version);
        }
        snprintf(text_buffer, sizeof(text_buffer), "Coprocessor r%" PRIu16, coprocessor_firmware_version);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
#endif

        line++;

#ifdef CONFIG_IDF_TARGET_ESP32P4
        device_identity_t identity = {0};
        if (read_device_identity(&identity) != ESP_OK) {
            snprintf(identity.name, sizeof(identity.name), "Unknown");
            snprintf(identity.vendor, sizeof(identity.vendor), "Unknown");
            identity.revision = 0;
            identity.radio    = DEVICE_VARIANT_RADIO_NONE;
            snprintf(identity.region, sizeof(identity.region), "??");
            memset(identity.mac, 0, sizeof(identity.mac));
            memset(identity.uid, 0, sizeof(identity.uid));
        }
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), "Hardware identity:");
        snprintf(text_buffer, sizeof(text_buffer), "Name:                %s", identity.name);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Vendor:              %s",
                 strcmp(identity.vendor, "Nicolai") != 0 ? identity.vendor : "Nicolai Electronics");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Board revision:      %" PRIu8, identity.revision);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Radio:               %s", get_radio_name(identity.radio));
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Color:               %s", get_color_name(identity.color));
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Region:              %s", identity.region);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "APP SOC UID:         ");
        for (size_t i = 0; i < 16; i++) {
            snprintf(text_buffer + strlen(text_buffer), sizeof(text_buffer) - strlen(text_buffer), "%02x",
                     identity.uid[i]);
        }
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "APP SOC MAC:         ");
        for (size_t i = 0; i < 6; i++) {
            snprintf(text_buffer + strlen(text_buffer), sizeof(text_buffer) - strlen(text_buffer), "%02x%s",
                     identity.mac[i], i < 5 ? ":" : "");
        }
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "APP SOC waver rev:   %u.%u", identity.waver_rev_major,
                 identity.waver_rev_minor);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        line++;
#endif
    }
    display_blit_buffer(buffer);
}

void menu_device_information(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    pax_vec2_t position = menu_calc_position(buffer, theme);

    render(buffer, theme, position, false, true);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
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
        } else {
            render(buffer, theme, position, true, true);
        }
    }
}
