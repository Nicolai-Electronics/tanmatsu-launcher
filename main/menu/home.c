#include "home.h"
#include <string.h>
#include <sys/unistd.h>
#include <time.h>
#include "apps.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/power.h"
#include "charging_mode.h"
#include "common/display.h"
#include "common/theme.h"
#include "coprocessor_management.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "gui_element_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "information.h"
#include "menu/menu_rftest.h"
#include "menu/message_dialog.h"
#include "menu/nametag.h"
#include "menu_repository_client.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "settings.h"
#include "tools.h"
#include "usb_device.h"

static const char TAG[] = "home menu";

typedef enum {
    ACTION_NONE,
    ACTION_APPS,
    ACTION_NAMETAG,
    ACTION_REPOSITORY,
    ACTION_SETTINGS,
    ACTION_TOOLS,
    ACTION_INFORMATION,
    ACTION_RFTEST,
} menu_home_action_t;

static void execute_action(pax_buf_t* fb, menu_home_action_t action, gui_theme_t* theme) {
    switch (action) {
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
        default:
            break;
    }
}

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_F5), "Settings"}, {get_icon(ICON_F6), "USB mode"}}), 2
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ / ← / → | ⏎ Select"}}), 1
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "🅼 Settings 🅰 Select"}}), 1
#else
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{NULL, "F5 Settings"}, {NULL, "F6 USB mode"}}), 2
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ / ← / → | ⏎ Select"}}), 1
#endif

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_HOME), "Home"}}), 1, FOOTER_LEFT,
                                     FOOTER_RIGHT);
    }
    menu_render_grid(buffer, menu, position, theme, partial);
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
        usb_mode_set(USB_DEBUG);
    } else {
        usb_mode_set(USB_DEVICE);
    }
}

void menu_home(void) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    menu_insert_item_icon(&menu, "Apps", NULL, (void*)ACTION_APPS, -1, get_icon(ICON_APPS));
    if (access("/sd/nametag.png", F_OK) == 0 || access("/int/nametag.png", F_OK) == 0) {
        menu_insert_item_icon(&menu, "Nametag", NULL, (void*)ACTION_NAMETAG, -1, get_icon(ICON_TAG));
    }
    menu_insert_item_icon(&menu, "Repository", NULL, (void*)ACTION_REPOSITORY, -1, get_icon(ICON_REPOSITORY));
    menu_insert_item_icon(&menu, "Settings", NULL, (void*)ACTION_SETTINGS, -1, get_icon(ICON_SETTINGS));
    menu_insert_item_icon(&menu, "Tools", NULL, (void*)ACTION_TOOLS, -1, get_icon(ICON_EXTENSION));
    menu_insert_item_icon(&menu, "Information", NULL, (void*)ACTION_INFORMATION, -1, get_icon(ICON_INFO));
    if (access("/int/rftest_local.bin", F_OK) == 0) {
        menu_insert_item_icon(&menu, "RF test", NULL, (void*)ACTION_RFTEST, -1, get_icon(ICON_DEV));
    }

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    bool power_button_latch = false;

    render(buffer, theme, &menu, position, false, true);

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
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F2:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_BOOTLOADER);
                                }
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F3:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_APPLICATION);
                                }
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F4:
                            case BSP_INPUT_NAVIGATION_KEY_START:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    keyboard_backlight();
                                }
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_MENU:
                            case BSP_INPUT_NAVIGATION_KEY_F5:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    display_backlight();
                                } else {
                                    menu_settings();
                                    render(buffer, theme, &menu, position, false, true);
                                }
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F6:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    coprocessor_flash(true);
                                } else {
                                    toggle_usb_mode();
                                }
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_LEFT:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous_row(&menu, theme);
                                render(buffer, theme, &menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next_row(&menu, theme);
                                render(buffer, theme, &menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                execute_action(buffer, (menu_home_action_t)arg, theme);
                                render(buffer, theme, &menu, position, false, true);
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
                                render(buffer, theme, &menu, position, false, true);
                            }
                            break;
                        case BSP_INPUT_ACTION_TYPE_SD_CARD:
                            ESP_LOGI(TAG, "Unhandled: SD card event (%u)\r\n", event.args_action.state);
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
            render(buffer, theme, &menu, position, true, true);
        }
    }
}
