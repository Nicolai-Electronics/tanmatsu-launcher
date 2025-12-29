#include "about.h"
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "esp_netif_types.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "gui_style.h"
#include "icons.h"
#include "lwipopts.h"
#include "menu/message_dialog.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"
#include "wifi_connection.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT NULL, 0
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#endif

extern bool wifi_stack_get_initialized(void);

static wifi_ap_record_t connected_ap = {0};

static bool get_connection_info(void) {
    bool        radio_initialized = wifi_stack_get_initialized();
    wifi_mode_t mode              = WIFI_MODE_NULL;
    if (radio_initialized && esp_wifi_get_mode(&mode) == ESP_OK) {
        if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
            if (wifi_connection_is_connected() && esp_wifi_sta_get_ap_info(&connected_ap) == ESP_OK) {
                return true;
            }
        }
    }
    return false;
}

static const char* ip6_addr_type_to_string(esp_ip6_addr_type_t type) {
    switch (type) {
        case ESP_IP6_ADDR_IS_UNKNOWN:
            return "Unknown";
        case ESP_IP6_ADDR_IS_GLOBAL:
            return "Global";
        case ESP_IP6_ADDR_IS_LINK_LOCAL:
            return "Link Local";
        case ESP_IP6_ADDR_IS_SITE_LOCAL:
            return "Site Local";
        case ESP_IP6_ADDR_IS_UNIQUE_LOCAL:
            return "Unique Local";
        case ESP_IP6_ADDR_IS_IPV4_MAPPED_IPV6:
            return "IPv4 Mapped IPv6";
        default:
            return "Invalid Type";
    }
}

static void render(bool partial, bool icons) {
    gui_theme_t* theme  = get_theme();
    pax_buf_t*   buffer = display_get_buffer();

    char message_buffer[128] = {0};

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
                                     ((gui_element_icontext_t[]){{get_icon(ICON_INFO), "WiFi information"}}), 1,
                                     FOOTER_LEFT, FOOTER_RIGHT);
    }
    if (!partial) {
        bool connected = get_connection_info();
        if (!connected) {
            pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                          position.y0 + 18 * 0, "Not connected to a network");
        } else {
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(wifi_get_netif(), &ip_info);

            esp_ip6_addr_t ip6[LWIP_IPV6_NUM_ADDRESSES];
            int            ip6_addrs = esp_netif_get_all_ip6(wifi_get_netif(), ip6);

            snprintf(message_buffer, sizeof(message_buffer), "SSID: %s", connected_ap.ssid);
            pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                          position.y0 + 18 * 0, message_buffer);
            snprintf(message_buffer, sizeof(message_buffer), "BSSID: %02x:%02x:%02x:%02x:%02x:%02x",
                     connected_ap.bssid[0], connected_ap.bssid[1], connected_ap.bssid[2], connected_ap.bssid[3],
                     connected_ap.bssid[4], connected_ap.bssid[5]);
            pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                          position.y0 + 18 * 1, message_buffer);
            snprintf(message_buffer, sizeof(message_buffer), "Channel: %d", connected_ap.primary);
            pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                          position.y0 + 18 * 2, message_buffer);
            snprintf(message_buffer, sizeof(message_buffer), "RSSI: %d dBm", connected_ap.rssi);
            pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                          position.y0 + 18 * 3, message_buffer);

            snprintf(message_buffer, sizeof(message_buffer), "IP address: " IPSTR, IP2STR(&ip_info.ip));
            pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                          position.y0 + 18 * 5, message_buffer);
            snprintf(message_buffer, sizeof(message_buffer), "Gateway: " IPSTR, IP2STR(&ip_info.gw));
            pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                          position.y0 + 18 * 6, message_buffer);
            snprintf(message_buffer, sizeof(message_buffer), "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
            pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                          position.y0 + 18 * 7, message_buffer);

            if (ip6_addrs > 0) {
                snprintf(message_buffer, sizeof(message_buffer), "IPv6 addresses:");
                pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                              position.y0 + 18 * 8, message_buffer);

                for (int j = 0; j < ip6_addrs; ++j) {
                    esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&(ip6[j]));
                    snprintf(message_buffer, sizeof(message_buffer), "- %s: " IPV6STR,
                             ip6_addr_type_to_string(ipv6_type), IPV62STR(ip6[j]));
                    pax_draw_text(buffer, theme->palette.color_foreground, theme->menu.text_font, 16, position.x0,
                                  position.y0 + 18 * (9 + j), message_buffer);
                }
            }
        }
    }
    display_blit_buffer(buffer);
}

void menu_wifi_info(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

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
