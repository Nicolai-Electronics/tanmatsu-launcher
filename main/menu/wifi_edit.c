#include <string.h>
#include "bsp/input.h"
#include "common/display.h"
#include "esp_eap_client.h"
#include "esp_err.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/textedit.h"
#include "message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "wifi.h"
#include "wifi_connection.h"
#include "wifi_settings.h"
// #include "shapes/pax_misc.h"

typedef enum {
    ACTION_NONE,
    ACTION_SSID,
    ACTION_AUTHMODE,
    ACTION_PASSWORD,
    ACTION_IDENTITY,
    ACTION_USERNAME,
    ACTION_PHASE2,
    ACTION_LAST,
} menu_wifi_edit_action_t;

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_WIFI), "Edit WiFi network"}}), 1,
                                     ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"},
                                                             {get_icon(ICON_F1), "Exit without saving"},
                                                             {get_icon(ICON_F4), "Save and exit"}}),
                                     3, ((gui_header_field_t[]){{NULL, "↑ / ↓ Navigate ⏎ Edit setting"}}), 1);
    }

    menu_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

const char* authmode_to_string(wifi_auth_mode_t authmode) {
    switch (authmode) {
        case WIFI_AUTH_OPEN:
            return "Open";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA (PSK)";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2 (PSK)";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2 (PSK)";
        case WIFI_AUTH_ENTERPRISE:
            return "WPA2 (enterprise)";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3 (PSK)";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/WPA3 (PSK)";
        case WIFI_AUTH_WAPI_PSK:
            return "WAPI (PSK)";
        case WIFI_AUTH_OWE:
            return "OWE";
        case WIFI_AUTH_WPA3_ENT_192:
            return "WPA3 (ENT-192)";
        case WIFI_AUTH_DPP:
            return "DPP";
        case WIFI_AUTH_WPA3_ENTERPRISE:
            return "WPA3 (enterprise)";
        case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
            return "WPA2/WPA3 (enterprise)";
        default:
            return "Unknown";
    }
}

const char* phase2_to_string(esp_eap_ttls_phase2_types phase2) {
    switch (phase2) {
        case ESP_EAP_TTLS_PHASE2_EAP:
            return "EAP";
        case ESP_EAP_TTLS_PHASE2_MSCHAPV2:
            return "MSCHAPV2";
        case ESP_EAP_TTLS_PHASE2_MSCHAP:
            return "MSCHAP";
        case ESP_EAP_TTLS_PHASE2_PAP:
            return "PAP";
        case ESP_EAP_TTLS_PHASE2_CHAP:
            return "CHAP";
        default:
            return "Unknown";
    }
}

static void menu_populate(menu_t* menu, wifi_settings_t* settings) {
    char temp[129] = {0};
    memcpy(temp, settings->ssid, sizeof(settings->ssid));
    menu_insert_item_value(menu, "SSID", temp, NULL, (void*)ACTION_SSID, -1);

    menu_insert_item_value(menu, "Security", authmode_to_string(settings->authmode), NULL, (void*)ACTION_AUTHMODE, -1);

    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->password, sizeof(settings->password));
    menu_insert_item_value(menu, "Password", temp, NULL, (void*)ACTION_PASSWORD, -1);

    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->identity, sizeof(settings->identity));
    menu_insert_item_value(menu, "Identity", temp, NULL, (void*)ACTION_IDENTITY, -1);

    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->username, sizeof(settings->username));
    menu_insert_item_value(menu, "Username", temp, NULL, (void*)ACTION_USERNAME, -1);

    menu_insert_item_value(menu, "Phase 2", phase2_to_string(settings->phase2), NULL, (void*)ACTION_PHASE2, -1);
}

static void menu_update(menu_t* menu, wifi_settings_t* settings) {
    char temp[129] = {0};
    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->ssid, sizeof(settings->ssid));
    menu_set_value(menu, 0, temp);

    menu_set_value(menu, 1, authmode_to_string(settings->authmode));

    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->password, sizeof(settings->password));
    menu_set_value(menu, 2, temp);

    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->identity, sizeof(settings->identity));
    menu_set_value(menu, 3, temp);

    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->username, sizeof(settings->username));
    menu_set_value(menu, 4, temp);

    menu_set_value(menu, 5, phase2_to_string(settings->phase2));
}

static void edit_ssid(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, wifi_settings_t* settings) {
    char temp[129] = {0};
    bool accepted  = false;
    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->ssid, sizeof(settings->ssid));
    menu_textedit(buffer, theme, "SSID", temp, sizeof(settings->ssid) + sizeof('\0'), true, &accepted);
    if (accepted) {
        memcpy(settings->ssid, temp, sizeof(settings->ssid));
        menu_set_value(menu, 0, temp);
    }
}

static void edit_password(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, wifi_settings_t* settings) {
    char temp[129] = {0};
    bool accepted  = false;
    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->password, sizeof(settings->password));
    menu_textedit(buffer, theme, "Password", temp, sizeof(settings->password) + sizeof('\0'), true, &accepted);
    if (accepted) {
        memcpy(settings->password, temp, sizeof(settings->password));
        menu_set_value(menu, 2, temp);
    }
}

static void edit_identity(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, wifi_settings_t* settings) {
    char temp[129] = {0};
    bool accepted  = false;
    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->identity, sizeof(settings->identity));
    menu_textedit(buffer, theme, "Identity", temp, sizeof(settings->identity) + sizeof('\0'), true, &accepted);
    if (accepted) {
        memcpy(settings->identity, temp, sizeof(settings->identity));
        menu_set_value(menu, 3, temp);
    }
}

static void edit_username(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, wifi_settings_t* settings) {
    char temp[129] = {0};
    bool accepted  = false;
    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->username, sizeof(settings->username));
    menu_textedit(buffer, theme, "Username", temp, sizeof(settings->username) + sizeof('\0'), true, &accepted);
    if (accepted) {
        memcpy(settings->username, temp, sizeof(settings->username));
        menu_set_value(menu, 4, temp);
    }
}

bool menu_wifi_edit(pax_buf_t* buffer, gui_theme_t* theme, uint8_t index, bool new_entry, char* new_ssid,
                    wifi_auth_mode_t authmode) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    wifi_settings_t settings = {0};
    if (new_entry) {
        memcpy(settings.ssid, new_ssid, sizeof(settings.ssid));
        settings.authmode = authmode;
    } else {
        esp_err_t res = wifi_settings_get(index, &settings);
        if (res != ESP_OK) {
            char message[128];
            snprintf(message, sizeof(message), "%s, failed to read WiFi settings at index %u", esp_err_to_name(res),
                     index);
            printf("%s\r\n", message);
            message_dialog(buffer, theme, "An error occurred", message, "Go back");
            return false;
        }
    }

    menu_t menu = {0};
    menu_initialize(&menu);

    menu_populate(&menu, &settings);

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    render(buffer, theme, &menu, position, false, true);
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
                                return false;
                            case BSP_INPUT_NAVIGATION_KEY_F4: {
                                esp_err_t res = wifi_settings_set(index, &settings);
                                if (res == ESP_OK) {
                                    return true;
                                } else {
                                    message_dialog(buffer, theme, "Error", "Failed to save WiFi settings", "Go back");
                                }
                            }
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                menu_wifi_edit_action_t action =
                                    (menu_wifi_edit_action_t)menu_get_callback_args(&menu, menu_get_position(&menu));
                                switch (action) {
                                    case ACTION_SSID:
                                        edit_ssid(buffer, theme, &menu, &settings);
                                        break;
                                    case ACTION_PASSWORD:
                                        edit_password(buffer, theme, &menu, &settings);
                                        break;
                                    case ACTION_IDENTITY:
                                        edit_identity(buffer, theme, &menu, &settings);
                                        break;
                                    case ACTION_USERNAME:
                                        edit_username(buffer, theme, &menu, &settings);
                                        break;
                                    default:
                                        break;
                                }
                                render(buffer, theme, &menu, position, false, false);
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
            render(buffer, theme, &menu, position, true, true);
        }
    }
}
