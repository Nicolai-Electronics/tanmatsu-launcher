#include "signature_test.h"
#include <stdio.h>
#include <string.h>
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "device_settings.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/idf_additions.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/menu_helpers.h"
#include "menu/message_dialog.h"
#include "nvs_settings.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "pax_types.h"
#include "repository_client.h"
#include "signature.h"
#include "wifi_connection.h"

extern bool wifi_stack_get_initialized(void);

static char const TAG[] = "Signature test";

#define FOOTER_LEFT                                                                                          \
    ((gui_element_icontext_t[]){                                                                             \
        {get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}, {get_icon(ICON_F4), "Verify with server"}}), \
        3
#define FOOTER_RIGHT NULL, 0

#define BYTES_PER_LINE 16

static bool    public_key_available = false;
static uint8_t public_key[65]       = {0};

static int draw_line_font(pax_buf_t* buffer, gui_theme_t* theme, pax_font_t const* font, pax_vec2_t position, int line,
                          char const* text) {
    pax_draw_text(buffer, theme->palette.color_foreground, font, 16, position.x0, position.y0 + 18 * line, text);
    return line + 1;
}

static int draw_line(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, int line, char const* text) {
    return draw_line_font(buffer, theme, theme->menu.text_font, position, line, text);
}

static int draw_hex(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, int line, uint8_t const* data,
                    size_t length) {
    char message_buffer[BYTES_PER_LINE * 2 + 1];
    for (size_t offset = 0; offset < length; offset += BYTES_PER_LINE) {
        size_t line_length = length - offset;
        if (line_length > BYTES_PER_LINE) {
            line_length = BYTES_PER_LINE;
        }
        for (size_t i = 0; i < line_length; i++) {
            snprintf(&message_buffer[i * 2], 3, "%02x", data[offset + i]);
        }
        line = draw_line_font(buffer, theme, pax_font_sky_mono, position, line, message_buffer);
    }
    return line;
}

static void render(bool partial, bool icons) {
    gui_theme_t* theme  = get_theme();
    pax_buf_t*   buffer = display_get_buffer();

    pax_vec2_t position = menu_calc_position(buffer, theme);

    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_INFO), "Signature test"}}), 1,
                                     FOOTER_LEFT, FOOTER_RIGHT);
    }
    if (!partial) {
        int line = 0;
        if (!public_key_available) {
            line = draw_line(buffer, theme, position, line, "No signing key installed");
        } else {
            line = draw_line(buffer, theme, position, line, "Public key:");
            line = draw_hex(buffer, theme, position, line, public_key, sizeof(public_key));
        }
    }
    display_blit_buffer(buffer);
}

static void verify_with_server(void) {
    pax_buf_t* icon = get_icon(ICON_INFO);

    if (!public_key_available) {
        message_dialog(icon, "Signature test", "No signing key installed", "OK");
        return;
    }

    busy_dialog(icon, "Signature test", "Connecting to WiFi...", true);

    if (!wifi_stack_get_initialized()) {
        ESP_LOGE(TAG, "WiFi stack not initialized");
        message_dialog(icon, "Signature test", "WiFi stack not initialized", "OK");
        return;
    }

    if (!wifi_connection_is_connected()) {
        if (wifi_connect_try_all() != ESP_OK) {
            ESP_LOGE(TAG, "Not connected to WiFi");
            message_dialog(icon, "Signature test", "Failed to connect to WiFi network", "OK");
            return;
        }
    }

    uint8_t mac_address[6] = {0};
    if (esp_read_mac(mac_address, ESP_MAC_BASE) != ESP_OK) {
        message_dialog(icon, "Signature test", "Failed to read MAC address", "OK");
        return;
    }

    char server[128] = {0};
    nvs_settings_get_repo_server(server, sizeof(server), DEFAULT_REPO_SERVER);

    busy_dialog(icon, "Signature test", "Requesting challenge...", true);
    uint8_t challenge[64] = {0};
    if (!repository_signature_request(server, mac_address, challenge)) {
        message_dialog(icon, "Signature test", "Failed to request challenge from server", "OK");
        return;
    }

    busy_dialog(icon, "Signature test", "Signing challenge...", true);
    uint8_t signature[64] = {0};
    if (!signature_sign(challenge, sizeof(challenge), signature)) {
        message_dialog(icon, "Signature test", "Failed to sign challenge", "OK");
        return;
    }

    busy_dialog(icon, "Signature test", "Verifying signature...", true);
    bool verified = false;
    if (!repository_signature_verify(server, mac_address, signature, &verified)) {
        message_dialog(icon, "Signature test", "Failed to send signature to server", "OK");
        return;
    }

    if (verified) {
        message_dialog(icon, "Signature test", "Verification succeeded", "OK");
    } else {
        message_dialog(get_icon(ICON_ERROR), "Signature test", "Verification failed", "OK");
    }
}

void menu_signature_test(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    public_key_available = signature_read_public_key(public_key);

    render(false, true);
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
                            case BSP_INPUT_NAVIGATION_KEY_F4:
                                verify_with_server();
                                render(false, true);
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
        } else {
            render(true, true);
        }
    }
}
