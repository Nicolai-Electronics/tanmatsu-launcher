#include <string.h>
#include "bsp/input.h"
#include "common/display.h"
#include "esp_eap_client.h"
#include "esp_err.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "gui_element_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/textbox.h"
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
                                     ((gui_element_icontext_t[]){{get_icon(ICON_WIFI), "Edit WiFi network"}}), 1,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},
                                                                 {get_icon(ICON_F1), "Exit without saving"},
                                                                 {get_icon(ICON_F4), "Save and exit"}}),
                                     3, ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Edit setting"}}), 1);
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
    size_t previous_position = menu_get_position(menu);
    while (menu_get_length(menu) > 0) {
        menu_remove_item(menu, 0);
    }

    char temp[129] = {0};
    memcpy(temp, settings->ssid, sizeof(settings->ssid));
    menu_insert_item_value(menu, "SSID", temp, NULL, (void*)ACTION_SSID, -1);

    menu_insert_item_value(menu, "Security", authmode_to_string(settings->authmode), NULL, (void*)ACTION_AUTHMODE, -1);

    if (settings->authmode != WIFI_AUTH_OPEN) {
        memset(temp, 0, sizeof(temp));
        memcpy(temp, settings->password, sizeof(settings->password));
        menu_insert_item_value(menu, "Password", temp, NULL, (void*)ACTION_PASSWORD, -1);
    }

    if (settings->authmode == WIFI_AUTH_ENTERPRISE || settings->authmode == WIFI_AUTH_WPA3_ENTERPRISE ||
        settings->authmode == WIFI_AUTH_WPA2_WPA3_ENTERPRISE) {
        memset(temp, 0, sizeof(temp));
        memcpy(temp, settings->identity, sizeof(settings->identity));
        menu_insert_item_value(menu, "Identity", temp, NULL, (void*)ACTION_IDENTITY, -1);

        memset(temp, 0, sizeof(temp));
        memcpy(temp, settings->username, sizeof(settings->username));
        menu_insert_item_value(menu, "Username", temp, NULL, (void*)ACTION_USERNAME, -1);

        menu_insert_item_value(menu, "Phase 2", phase2_to_string(settings->phase2), NULL, (void*)ACTION_PHASE2, -1);
    }

    if (previous_position >= menu_get_length(menu)) {
        previous_position = menu_get_length(menu) - 1;
    }
    menu_set_position(menu, previous_position);
}

/*static void menu_update(menu_t* menu, wifi_settings_t* settings) {
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
}*/

static void edit_ssid(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, wifi_settings_t* settings) {
    char temp[129] = {0};
    bool accepted  = false;
    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->ssid, sizeof(settings->ssid));
    menu_textbox(buffer, theme, "SSID", temp, sizeof(settings->ssid) + sizeof('\0'), true, &accepted);
    if (accepted) {
        memcpy(settings->ssid, temp, sizeof(settings->ssid));
        menu_set_value(menu, 0, temp);
    }
}

static void edit_authmode(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, menu_t* menu_authmode,
                          pax_vec2_t position, wifi_settings_t* settings, QueueHandle_t input_event_queue) {
    menu_set_position(menu_authmode, 0);
    for (size_t i = 0; i < menu_get_length(menu_authmode); i++) {
        if ((wifi_auth_mode_t)menu_get_callback_args(menu_authmode, i) == settings->authmode) {
            menu_set_position(menu_authmode, i);
            break;
        }
    }

    bool partial = false;
    while (1) {
        render_base_screen_statusbar(
            buffer, theme, !partial, true, !partial, ((gui_element_icontext_t[]){{get_icon(ICON_WIFI), "Security"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Go back"}}), 2,
            ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Select"}}), 1);
        menu_render(buffer, menu_authmode, position, theme, partial);
        display_blit_buffer(buffer);
        partial = true;
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
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(menu_authmode);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(menu_authmode);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                wifi_auth_mode_t authmode = (wifi_auth_mode_t)menu_get_callback_args(
                                    menu_authmode, menu_get_position(menu_authmode));
                                settings->authmode = authmode;
                                menu_set_value(menu, 1, authmode_to_string(authmode));
                                return;
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
        }
    }
}

static void edit_password(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, wifi_settings_t* settings) {
    char temp[129] = {0};
    bool accepted  = false;
    memset(temp, 0, sizeof(temp));
    memcpy(temp, settings->password, sizeof(settings->password));
    menu_textbox(buffer, theme, "Password", temp, sizeof(settings->password) + sizeof('\0'), true, &accepted);
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
    menu_textbox(buffer, theme, "Identity", temp, sizeof(settings->identity) + sizeof('\0'), true, &accepted);
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
    menu_textbox(buffer, theme, "Username", temp, sizeof(settings->username) + sizeof('\0'), true, &accepted);
    if (accepted) {
        memcpy(settings->username, temp, sizeof(settings->username));
        menu_set_value(menu, 4, temp);
    }
}

static void edit_phase2(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, menu_t* menu_phase2, pax_vec2_t position,
                        wifi_settings_t* settings, QueueHandle_t input_event_queue) {
    menu_set_position(menu_phase2, 0);
    for (size_t i = 0; i < menu_get_length(menu_phase2); i++) {
        if ((esp_eap_ttls_phase2_types)menu_get_callback_args(menu_phase2, i) == settings->phase2) {
            menu_set_position(menu_phase2, i);
            break;
        }
    }

    bool partial = false;
    while (1) {
        render_base_screen_statusbar(
            buffer, theme, !partial, true, !partial, ((gui_element_icontext_t[]){{get_icon(ICON_WIFI), "Phase 2"}}), 1,
            ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Go back"}}), 2,
            ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Select"}}), 1);
        menu_render(buffer, menu_phase2, position, theme, partial);
        display_blit_buffer(buffer);
        partial = true;
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
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(menu_phase2);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(menu_phase2);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                esp_eap_ttls_phase2_types phase2 = (esp_eap_ttls_phase2_types)menu_get_callback_args(
                                    menu_phase2, menu_get_position(menu_phase2));
                                settings->phase2 = phase2;
                                menu_set_value(menu, 5, phase2_to_string(phase2));
                                return;
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
        }
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
            message_dialog(get_icon(ICON_ERROR), "An error occurred", message, "Go back");
            return false;
        }
    }

    // Menus for enum selection
    menu_t menu_authmode = {0};
    menu_insert_item(&menu_authmode, "Open", NULL, (void*)WIFI_AUTH_OPEN, -1);
    menu_insert_item(&menu_authmode, "WEP", NULL, (void*)WIFI_AUTH_WEP, -1);
    menu_insert_item(&menu_authmode, "WPA (PSK)", NULL, (void*)WIFI_AUTH_WPA_PSK, -1);
    menu_insert_item(&menu_authmode, "WPA2 (PSK)", NULL, (void*)WIFI_AUTH_WPA2_PSK, -1);
    menu_insert_item(&menu_authmode, "WPA/WPA2 (PSK)", NULL, (void*)WIFI_AUTH_WPA_WPA2_PSK, -1);
    menu_insert_item(&menu_authmode, "WPA2 (enterprise)", NULL, (void*)WIFI_AUTH_ENTERPRISE, -1);
    menu_insert_item(&menu_authmode, "WPA3 (PSK)", NULL, (void*)WIFI_AUTH_WPA3_PSK, -1);
    menu_insert_item(&menu_authmode, "WPA2/WPA3 (PSK)", NULL, (void*)WIFI_AUTH_WPA2_WPA3_PSK, -1);
    menu_insert_item(&menu_authmode, "WAPI (PSK)", NULL, (void*)WIFI_AUTH_WAPI_PSK, -1);
    menu_insert_item(&menu_authmode, "OWE", NULL, (void*)WIFI_AUTH_OWE, -1);
    menu_insert_item(&menu_authmode, "WPA3 (ENT-192)", NULL, (void*)WIFI_AUTH_WPA3_ENT_192, -1);
    menu_insert_item(&menu_authmode, "DPP", NULL, (void*)WIFI_AUTH_DPP, -1);
    menu_insert_item(&menu_authmode, "WPA3 (enterprise)", NULL, (void*)WIFI_AUTH_WPA3_ENTERPRISE, -1);
    menu_insert_item(&menu_authmode, "WPA2/WPA3 (enterprise)", NULL, (void*)WIFI_AUTH_WPA2_WPA3_ENTERPRISE, -1);

    menu_t menu_phase2 = {0};
    menu_insert_item(&menu_phase2, "EAP", NULL, (void*)ESP_EAP_TTLS_PHASE2_EAP, -1);
    menu_insert_item(&menu_phase2, "MSCHAPV2", NULL, (void*)ESP_EAP_TTLS_PHASE2_MSCHAPV2, -1);
    menu_insert_item(&menu_phase2, "MSCHAP", NULL, (void*)ESP_EAP_TTLS_PHASE2_MSCHAP, -1);
    menu_insert_item(&menu_phase2, "PAP", NULL, (void*)ESP_EAP_TTLS_PHASE2_PAP, -1);
    menu_insert_item(&menu_phase2, "CHAP", NULL, (void*)ESP_EAP_TTLS_PHASE2_CHAP, -1);

    // Menu for parameters
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
                                menu_free(&menu_authmode);
                                menu_free(&menu_phase2);
                                return false;
                            case BSP_INPUT_NAVIGATION_KEY_F4:
                            case BSP_INPUT_NAVIGATION_KEY_START: {
                                esp_err_t res = wifi_settings_set(index, &settings);
                                if (res == ESP_OK) {
                                    return true;
                                } else {
                                    message_dialog(get_icon(ICON_ERROR), "Error", "Failed to save WiFi settings",
                                                   "Go back");
                                }
                                break;
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
                                    case ACTION_AUTHMODE:
                                        edit_authmode(buffer, theme, &menu, &menu_authmode, position, &settings,
                                                      input_event_queue);
                                        menu_populate(
                                            &menu,
                                            &settings);  // Repopulate menu to show or hide enterprise WiFi options
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
                                    case ACTION_PHASE2:
                                        edit_phase2(buffer, theme, &menu, &menu_phase2, position, &settings,
                                                    input_event_queue);
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
