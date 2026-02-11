#include "home.h"
#include <string.h>
#include <sys/unistd.h>
#include <time.h>
#include "addon.h"
#include "apps.h"
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/power.h"
#include "charging_mode.h"
#include "chat/chat.h"
#include "common/display.h"
#include "common/theme.h"
#include "coprocessor_management.h"
#include "device_information.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "information.h"
#include "menu/lora_information.h"
#include "menu/menu_rftest.h"
#include "menu/message_dialog.h"
#include "menu/nametag.h"
#include "menu_repository_client.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "radio_ota.h"
#include "sdcard.h"
#include "settings.h"
#include "tools.h"
#include "usb_device.h"

static const char TAG[] = "home menu";

extern bool wifi_stack_get_initialized(void);
extern bool wifi_stack_get_version_mismatch(void);
extern bool wifi_stack_get_task_done(void);

typedef enum {
    ACTION_NONE,
    ACTION_RADIO_OTA,
    ACTION_DOWNLOAD_ICONS,
    ACTION_APPS,
    ACTION_NAMETAG,
    ACTION_REPOSITORY,
    ACTION_SETTINGS,
    ACTION_TOOLS,
    ACTION_INFORMATION,
    ACTION_RFTEST,
    ACTION_CHAT,
    ACTION_LORA_INFORMATION,
} menu_home_action_t;

static void execute_action(menu_home_action_t action) {
    pax_buf_t*   fb    = display_get_buffer();
    gui_theme_t* theme = get_theme();
    switch (action) {
        case ACTION_RADIO_OTA:
            radio_ota_update();
            esp_restart();
            break;
        case ACTION_DOWNLOAD_ICONS:
            download_icons();
            esp_restart();
        case ACTION_APPS:
            menu_apps(fb, theme);
            break;
        case ACTION_NAMETAG:
            menu_nametag(fb, theme);
            break;
        case ACTION_REPOSITORY:
            menu_repository_client(fb, theme);
            break;
        case ACTION_SETTINGS:
            menu_settings();
            break;
        case ACTION_TOOLS:
            menu_tools();
            break;
        case ACTION_INFORMATION:
            menu_information();
            break;
        case ACTION_RFTEST:
            menu_rftest();
            break;
        case ACTION_CHAT:
            menu_chat();
            break;
        case ACTION_LORA_INFORMATION:
            menu_lora_information();
            break;
        default:
            break;
    }
}

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define FOOTER_LEFT                                                 \
    ((gui_element_icontext_t[]){{get_icon(ICON_F2), "Tools"},       \
                                {get_icon(ICON_F3), "Information"}, \
                                {get_icon(ICON_F5), "Settings"},    \
                                {get_icon(ICON_F6), "USB mode"}}),  \
        4
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ / ← / → | ⏎ Select"}}), 1
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "🅼 Settings 🅰 Select"}}), 1
#else
#define FOOTER_LEFT                                                                                   \
    ((gui_element_icontext_t[]){                                                                      \
        {NULL, "F2 Tools"}, {NULL, "F3 Information"}, {NULL, "F5 Settings"}, {NULL, "F6 USB mode"}}), \
        4
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ / ← / → | ⏎ Select"}}), 1
#endif

static void describe_addon(addon_location_t location, char* description, size_t description_size) {
    addon_descriptor_t* descriptor = NULL;
    esp_err_t           res        = addon_get_descriptor(location, &descriptor);
    if (res == ESP_OK && descriptor) {
        const char* location_str = location == ADDON_LOCATION_INTERNAL ? "Internal" : "CATT";
        switch (descriptor->descriptor_type) {
            case ADDON_TYPE_UNINITIALIZED:
                snprintf(&description[strlen(description)], description_size - strlen(description),
                         "%s add-on: uninitialized add-on detected\r\n", location_str);
                break;
            case ADDON_TYPE_BINARY_SAO:
                snprintf(&description[strlen(description)], description_size - strlen(description), "%s add-on: %s\r\n",
                         location_str, descriptor->binary_sao.name);
                break;
            case ADDON_TYPE_JSON:
                snprintf(&description[strlen(description)], description_size - strlen(description),
                         "%s add-on: (json descriptor)\r\n", location_str);
                break;
            case ADDON_TYPE_HEXPANSION:
                snprintf(&description[strlen(description)], description_size - strlen(description), "%s add-on: %s\r\n",
                         location_str, descriptor->catt.name);
                break;
            case ADDON_TYPE_CATT:
                snprintf(&description[strlen(description)], description_size - strlen(description), "%s add-on: %s\r\n",
                         location_str, descriptor->catt.name);
                break;
            default:
                break;
        }
    }
}

static void describe_addons(char* description, size_t description_size) {
    describe_addon(ADDON_LOCATION_INTERNAL, description, description_size);
    describe_addon(ADDON_LOCATION_EXTERNAL, &description[strlen(description)], description_size - strlen(description));
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons,
                   bool provisioned, bool name_match) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_HOME), "Home"}}), 1, FOOTER_LEFT,
                                     FOOTER_RIGHT);
    }
    menu_render_grid(buffer, menu, position, theme, partial);

    if (wifi_stack_get_task_done()) {
        pax_simple_rect(buffer, theme->menu.palette.color_background, position.x0,
                        pax_buf_get_height(buffer) - theme->footer.height - theme->footer.vertical_margin - 18 * 3,
                        pax_buf_get_width(buffer) - position.x0 * 2, 18 * 2);
        if (wifi_stack_get_version_mismatch()) {
            pax_draw_text(
                buffer, 0xFFFF0000, theme->footer.text_font, 16, position.x0,
                pax_buf_get_height(buffer) - theme->footer.height - theme->footer.vertical_margin - 18 * 3,
                "Radio firmware version mismatch!\r\nPlease flash the radio firmware using the recovery website.");
        } else if (!wifi_stack_get_initialized()) {
            pax_draw_text(buffer, 0xFFFF0000, theme->footer.text_font, 16, position.x0,
                          pax_buf_get_height(buffer) - theme->footer.height - theme->footer.vertical_margin - 18 * 3,
                          "Radio communication error!\r\nPlease flash the radio firmware using the recovery website.");
        } else if (!provisioned) {
            pax_draw_text(buffer, 0xFF0000FF, theme->footer.text_font, 16, position.x0,
                          pax_buf_get_height(buffer) - theme->footer.height - theme->footer.vertical_margin - 18 * 3,
                          "Device not provisioned!\r\nPlease contact the manufacturer.");
        } else if (!name_match) {
            pax_draw_text(buffer, 0xFF0000FF, theme->footer.text_font, 16, position.x0,
                          pax_buf_get_height(buffer) - theme->footer.height - theme->footer.vertical_margin - 18 * 3,
                          "Device type mismatch!\r\nPlease contact the manufacturer.");
        } else {
            char description[256] = {0};
            describe_addons(description, sizeof(description));

            pax_draw_text(buffer, 0xFF000000, theme->footer.text_font, 16, position.x0,
                          pax_buf_get_height(buffer) - theme->footer.height - theme->footer.vertical_margin - 18 * 3,
                          description);
        }
    }
    display_blit_buffer(buffer);
}

static void keyboard_backlight(void) {
    uint8_t brightness;
    bsp_input_get_backlight_brightness(&brightness);
    if (brightness != 100) {
        brightness = 100;
    } else {
        brightness = 0;
    }
    printf("Keyboard brightness: %u%%\r\n", brightness);
    bsp_input_set_backlight_brightness(brightness);
}

static void display_backlight(void) {
    uint8_t brightness;
    bsp_display_get_backlight_brightness(&brightness);
    brightness += 5;
    if (brightness > 100) {
        brightness = 10;
    }
    printf("Display brightness: %u%%\r\n", brightness);
    bsp_display_set_backlight_brightness(brightness);
}

static void toggle_usb_mode(void) {
    if (usb_mode_get() == USB_DEVICE) {
        busy_dialog(get_icon(ICON_USB), "USB mode", "Debug USB-serial & JTAG peripheral", true);
        usb_mode_set(USB_DEBUG);
        vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
        busy_dialog(get_icon(ICON_USB), "USB mode", "BadgeLink mode", true);
        usb_mode_set(USB_DEVICE);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void is_provisioned(bool* out_provisioned, bool* out_name_match) {
    device_identity_t identity = {0};
    esp_err_t         res      = read_device_identity(&identity);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device identity: %s", esp_err_to_name(res));
        return;
    }
    *out_provisioned       = strlen(identity.name) > 0;
    char expected_name[32] = {0};
    bsp_device_get_name(expected_name, sizeof(expected_name) - 1);
    *out_name_match = strcmp(identity.name, expected_name) == 0;
}

void menu_home(void) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    bool provisioned = false;
    bool name_match  = false;
    is_provisioned(&provisioned, &name_match);

    menu_t menu = {0};
    menu_initialize(&menu);
    if (wifi_stack_get_version_mismatch()) {
        menu_insert_item_icon(&menu, "Update radio", NULL, (void*)ACTION_RADIO_OTA, -1, get_icon(ICON_SYSTEM_UPDATE));
    }
    if (get_icons_missing()) {
        menu_insert_item_icon(&menu, "Download icons", NULL, (void*)ACTION_DOWNLOAD_ICONS, -1,
                              get_icon(ICON_DOWNLOADING));
    }
    menu_insert_item_icon(&menu, "Apps", NULL, (void*)ACTION_APPS, -1, get_icon(ICON_APPS));
    if (access("/sd/nametag.png", F_OK) == 0 || access("/int/nametag.png", F_OK) == 0) {
        menu_insert_item_icon(&menu, "Nametag", NULL, (void*)ACTION_NAMETAG, -1, get_icon(ICON_BADGE));
    }
    menu_insert_item_icon(&menu, "Repository", NULL, (void*)ACTION_REPOSITORY, -1, get_icon(ICON_STOREFRONT));
    menu_insert_item_icon(&menu, "Settings", NULL, (void*)ACTION_SETTINGS, -1, get_icon(ICON_SETTINGS));
    //  menu_insert_item_icon(&menu, "Tools", NULL, (void*)ACTION_TOOLS, -1, get_icon(ICON_EXTENSION));
    //  menu_insert_item_icon(&menu, "Information", NULL, (void*)ACTION_INFORMATION, -1, get_icon(ICON_INFO));
    if (access("/int/rftest_local.bin", F_OK) == 0) {
        menu_insert_item_icon(&menu, "RF test", NULL, (void*)ACTION_RFTEST, -1, get_icon(ICON_BUG_REPORT));
    }
    // menu_insert_item_icon(&menu, "Chat", NULL, (void*)ACTION_CHAT, -1, get_icon(ICON_GLOBE)); // Soon...
    menu_insert_item_icon(&menu, "LoRa info", NULL, (void*)ACTION_LORA_INFORMATION, -1, get_icon(ICON_INFO));

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    bool power_button_latch = false;

    render(buffer, theme, &menu, position, false, true, provisioned, name_match);

    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
                                }
                                render(buffer, theme, &menu, position, false, true, provisioned, name_match);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F2:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_BOOTLOADER);
                                } else {
                                    execute_action(ACTION_TOOLS);
                                }
                                render(buffer, theme, &menu, position, false, true, provisioned, name_match);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F3:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_APPLICATION);
                                } else {
                                    execute_action(ACTION_INFORMATION);
                                }
                                render(buffer, theme, &menu, position, false, true, provisioned, name_match);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F4:
                            case BSP_INPUT_NAVIGATION_KEY_START:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    keyboard_backlight();
                                }
                                render(buffer, theme, &menu, position, false, true, provisioned, name_match);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_MENU:
                            case BSP_INPUT_NAVIGATION_KEY_F5:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    display_backlight();
                                } else {
                                    execute_action(ACTION_SETTINGS);
                                }
                                render(buffer, theme, &menu, position, false, true, provisioned, name_match);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F6:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    coprocessor_flash(true);
                                } else {
                                    toggle_usb_mode();
                                }
                                render(buffer, theme, &menu, position, false, true, provisioned, name_match);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_SUPER:
                                execute_action(ACTION_APPS);
                                render(buffer, theme, &menu, position, false, true, provisioned, name_match);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_LEFT:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, position, true, false, provisioned, name_match);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, position, true, false, provisioned, name_match);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous_row(&menu, theme);
                                render(buffer, theme, &menu, position, true, false, provisioned, name_match);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next_row(&menu, theme);
                                render(buffer, theme, &menu, position, true, false, provisioned, name_match);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                execute_action((menu_home_action_t)arg);
                                render(buffer, theme, &menu, position, false, true, provisioned, name_match);
                                break;
                            }
                            default:
                                break;
                        }
                    }
                    break;
                }
                case INPUT_EVENT_TYPE_ACTION:
                    switch (event.args_action.type) {
                        case BSP_INPUT_ACTION_TYPE_POWER_BUTTON:
                            if (event.args_action.state) {
                                power_button_latch = true;
                            } else if (power_button_latch) {
                                power_button_latch = false;
                                charging_mode(buffer, theme);
                                render(buffer, theme, &menu, position, false, true, provisioned, name_match);
                            }
                            break;
                        case BSP_INPUT_ACTION_TYPE_SD_CARD:
                            if (event.args_action.state) {
                                ESP_LOGI(TAG, "SD card inserted");
                                sd_mount();
                            } else {
                                ESP_LOGI(TAG, "SD card removed");
                                sd_unmount();
                            }
                            break;
                        case BSP_INPUT_ACTION_TYPE_AUDIO_JACK:
                            ESP_LOGI(TAG, "Unhandled: audio jack event (%u)\r\n", event.args_action.state);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
        } else {
            render(buffer, theme, &menu, position, true, true, provisioned, name_match);
        }
    }
}
