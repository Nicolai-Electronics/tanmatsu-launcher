#include <string.h>
#include "bsp/input.h"
#include "common/display.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "gui_element_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "wifi.h"
#include "wifi_connection.h"
#include "wifi_edit.h"
#include "wifi_settings.h"
// #include "shapes/pax_misc.h"

static const char* TAG = "WiFi scan";

extern bool wifi_stack_get_initialized(void);

static void wifi_scan_done_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data) {
}

static inline void wifi_desc_record(wifi_ap_record_t* record) {
    // Make a string representation of BSSID.
    char* bssid_str = malloc(3 * 6);
    if (!bssid_str) return;
    snprintf(bssid_str, 3 * 6, "%02X:%02X:%02X:%02X:%02X:%02X", record->bssid[0], record->bssid[1], record->bssid[2],
             record->bssid[3], record->bssid[4], record->bssid[5]);

    // Make a string representation of 11b/g/n modes.
    char* phy_str = malloc(9);
    if (!phy_str) {
        free(bssid_str);
        return;
    }
    *phy_str = 0;
    if (record->phy_11b | record->phy_11g | record->phy_11n) {
        strcpy(phy_str, " 1");
    }
    if (record->phy_11b) {
        strcat(phy_str, "/b");
    }
    if (record->phy_11g) {
        strcat(phy_str, "/g");
    }
    if (record->phy_11n) {
        strcat(phy_str, "/n");
    }
    phy_str[2] = '1';

    printf("AP %s %s rssi=%hhd%s\r\n", bssid_str, record->ssid, record->rssi, phy_str);
    free(bssid_str);
    free(phy_str);
}

static esp_err_t scan_for_networks(pax_buf_t* buffer, gui_theme_t* theme, wifi_ap_record_t** out_aps,
                                   uint16_t* out_aps_length) {
    if (wifi_stack_get_initialized()) {
        esp_err_t res;

        wifi_config_t wifi_config = {0};
        ESP_RETURN_ON_ERROR(esp_wifi_stop(), TAG, "Failed to stop WiFi");
        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Failed to set WiFi mode");
        ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Failed to set WiFi configuration");
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Failed to start WiFi");

        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, wifi_scan_done_handler, NULL);
        wifi_scan_config_t cfg = {
            .ssid      = NULL,
            .bssid     = NULL,
            .channel   = 0,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time = {.active = {0, 0}},
        };
        res = esp_wifi_scan_start(&cfg, true);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start scan");
            return res;
        }

        uint16_t num_ap = 0;
        res             = esp_wifi_scan_get_ap_num(&num_ap);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get number of APs");
            return res;
        }

        printf("Found %u APs\r\n", num_ap);

        wifi_ap_record_t* aps = malloc(sizeof(wifi_ap_record_t) * num_ap);
        if (!aps) {
            ESP_LOGE(TAG, "Out of memory (failed to allocate %zd bytes)", sizeof(wifi_ap_record_t) * num_ap);
            num_ap = 0;
            esp_wifi_scan_get_ap_records(&num_ap, NULL);
            return ESP_ERR_NO_MEM;
        }

        res = esp_wifi_scan_get_ap_records(&num_ap, aps);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to fetch AP records");
            free(aps);
            return res;
        }

        for (uint16_t i = 0; i < num_ap; i++) {
            wifi_desc_record(&aps[i]);
        }

        if (out_aps) {
            *out_aps        = aps;
            *out_aps_length = num_ap;
        } else {
            free(aps);
        }
    } else {
        message_dialog(get_icon(ICON_ERROR), "WiFi stack not initialized",
                       "The WiFi stack is not initialized. Please try again later.", "OK");
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons,
                   bool loading) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_WIFI), "WiFi network scan"}}), 1,
                                     ((gui_element_icontext_t[]){
                                         {get_icon(ICON_ESC), "/"},
                                         {get_icon(ICON_F1), "Back"},
                                     }),
                                     2, ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Add network"}}), 1);
    }
    menu_render(buffer, menu, position, theme, partial);
    if (menu_find_item(menu, 0) == NULL) {
        if (loading) {
            pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                          position.y0 + 18 * 0, "Scanning for WiFi networks...");
        } else {
            pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0,
                          position.y0 + 18 * 0, "No WiFi networks found");
        }
    }
    display_blit_buffer(buffer);
}

void menu_wifi_scan(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    menu_t menu = {0};
    menu_initialize(&menu);
    render(buffer, theme, &menu, position, false, true, true);

    wifi_ap_record_t* aps     = NULL;
    uint16_t          num_aps = 0;
    esp_err_t         res     = scan_for_networks(buffer, theme, &aps, &num_aps);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed (%d)", res);
        if (res != ESP_ERR_NOT_FOUND) {
            message_dialog(get_icon(ICON_ERROR), "An error occurred", "Scanning for WiFi networks failed", "OK");
        }
        return;
    }

    for (uint16_t i = 0; i < num_aps; i++) {
        wifi_ap_record_t* ap = &aps[i];
        char              label_buffer[128];
        char              type[5] = "other";
        char              bssid_str[18];
        if (ap->phy_11a) snprintf(type, sizeof(type), "11a");
        if (ap->phy_11ac) snprintf(type, sizeof(type), "11ac");
        if (ap->phy_11ax) snprintf(type, sizeof(type), "11ax");
        if (ap->phy_11b) snprintf(type, sizeof(type), "11b");
        if (ap->phy_11g) snprintf(type, sizeof(type), "11g");
        if (ap->phy_11n) snprintf(type, sizeof(type), "11n");
        if (ap->phy_lr) snprintf(type, sizeof(type), "lr");
        snprintf(bssid_str, 3 * 6, "%02X:%02X:%02X:%02X:%02X:%02X", ap->bssid[0], ap->bssid[1], ap->bssid[2],
                 ap->bssid[3], ap->bssid[4], ap->bssid[5]);
        snprintf(label_buffer, sizeof(label_buffer), "%s (%d dBm, %s network) [%s]", ap->ssid, ap->rssi, type,
                 bssid_str);
        menu_insert_item(&menu, label_buffer, NULL, (void*)ap, -1);
    }

    // populate_menu_from_wifi_entries(&menu);
    render(buffer, theme, &menu, position, false, true, false);
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
                                menu_free(&menu);
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, position, true, false, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, position, true, false, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                if (menu_find_item(&menu, 0) != NULL) {
                                    int index = wifi_settings_find_empty_slot();
                                    if (index >= 0) {
                                        wifi_ap_record_t* ap =
                                            (wifi_ap_record_t*)menu_get_callback_args(&menu, menu_get_position(&menu));
                                        bool stored =
                                            menu_wifi_edit(buffer, theme, index, true, (char*)ap->ssid, ap->authmode);
                                        if (stored) {
                                            menu_free(&menu);
                                            return;
                                        }
                                    } else {
                                        message_dialog(get_icon(ICON_ERROR), "Error",
                                                       "No empty slot, can not add another network", "Go back");
                                    }
                                    render(buffer, theme, &menu, position, false, false, false);
                                }
                                break;
                            }
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
            render(buffer, theme, &menu, position, true, true, false);
        }
    }
}
