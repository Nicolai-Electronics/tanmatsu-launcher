#include "home.h"
#include <string.h>
#include <time.h>
#include "appfs.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "common/display.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "gui_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "settings.h"
// #include "shapes/pax_misc.h"
#include "apps.h"
#include "charging_mode.h"
#include "icons.h"
#include "sdcard.h"
#include "usb_device.h"

typedef enum {
    ACTION_NONE,
    ACTION_APPS,
    ACTION_REPOSITORY,
    ACTION_SETTINGS,
    ACTION_LAST,
} menu_home_action_t;

typedef struct {
    char*          path;
    char*          type;
    char*          slug;
    char*          title;
    char*          description;
    char*          category;
    char*          author;
    char*          license;
    int            version;
    pax_buf_t*     icon;
    appfs_handle_t appfs_fd;
} launcher_app_t;

static bool populate_menu_from_appfs_entries(menu_t* menu) {
    bool           empty    = true;
    appfs_handle_t appfs_fd = appfsNextEntry(APPFS_INVALID_FD);
    while (appfs_fd != APPFS_INVALID_FD) {
        empty               = false;
        const char* slug    = NULL;
        const char* title   = NULL;
        uint16_t    version = 0xFFFF;
        appfsEntryInfoExt(appfs_fd, &slug, &title, &version, NULL);

        // if (!find_menu_item_for_type_and_slug(menu, "esp32", slug)) {
        //  AppFS entry has no metadata installed, create a simple menu entry anyway
        launcher_app_t* app = malloc(sizeof(launcher_app_t));
        if (app != NULL) {
            memset(app, 0, sizeof(launcher_app_t));
            app->appfs_fd = appfs_fd;
            app->type     = strdup("esp32");
            app->slug     = strdup(slug);
            app->title    = strdup(title);
            app->version  = version;
            app->icon     = malloc(sizeof(pax_buf_t));
            if (app->icon != NULL) {
                // pax_decode_png_buf(app->icon, (void*)dev_png_start, dev_png_end - dev_png_start, PAX_BUF_32_8888ARGB,
                // 0);
            }
            // menu_insert_item_icon(menu, (app->title != NULL) ? app->title : app->slug, NULL, (void*)app, -1,
            // app->icon);
            menu_insert_item(menu, (app->title != NULL) ? app->title : app->slug, NULL, (void*)app, -1);
        }
        //}
        appfs_fd = appfsNextEntry(appfs_fd);
    }
    return !empty;
}

static void execute_action(pax_buf_t* fb, menu_home_action_t action, gui_theme_t* theme) {
    switch (action) {
        case ACTION_APPS:
            menu_apps(fb, theme);
            break;
        case ACTION_SETTINGS:
            menu_settings(fb, theme);
            break;
        default:
            break;
    }
}

static void execute_app(launcher_app_t* app) {
    if (strcmp(app->type, "esp32") == 0) {
        appfsBootSelect(app->appfs_fd, NULL);
        esp_restart();
    } else if (strcmp(app->type, "python") == 0) {
        // TBD
    }
}

// ❌▵▫○𑗘◇
static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(
            buffer, theme, !partial, !partial || icons, !partial,
            ((gui_header_field_t[]){{get_icon(ICON_HOME), "Home"}}), 1,
            ((gui_header_field_t[]){{get_icon(ICON_F5), "Settings"}, {get_icon(ICON_F6), "USB mode"}}), 2,
            ((gui_header_field_t[]){{NULL, "↑ / ↓ / ← / → Navigate ⏎ Select"}}), 1);
    }
    menu_render_grid(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

static void keyboard_backlight(void) {
    uint8_t brightness;
    bsp_input_get_backlight_brightness(&brightness);
    brightness += 25;
    if (brightness > 100) {
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

void menu_home(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu);
    // populate_menu_from_appfs_entries(&menu);
    menu_insert_item_icon(&menu, "Apps", NULL, (void*)ACTION_APPS, -1, get_icon(ICON_APPS));
    // menu_insert_item_icon(&menu, "Nametag", NULL, (void*)ACTION_APPS, -1, get_icon(ICON_TAG));
    // menu_insert_item_icon(&menu, "Repository", NULL, (void*)ACTION_REPOSITORY, -1, get_icon(ICON_REPOSITORY));
    menu_insert_item_icon(&menu, "Settings", NULL, (void*)ACTION_SETTINGS, -1, get_icon(ICON_SETTINGS));

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
                            case BSP_INPUT_NAVIGATION_KEY_F4:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    keyboard_backlight();
                                } else {
                                    toggle_usb_mode();
                                }
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_MENU:
                                menu_settings(buffer, theme);
                                render(buffer, theme, &menu, position, false, true);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F5:
                                if (event.args_navigation.modifiers & BSP_INPUT_MODIFIER_FUNCTION) {
                                    display_backlight();
                                } else {
                                    menu_settings(buffer, theme);
                                    render(buffer, theme, &menu, position, false, true);
                                }
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F6:
                                toggle_usb_mode();
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
                            case BSP_INPUT_NAVIGATION_KEY_RETURN: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                if (arg >= (void*)ACTION_LAST) {
                                    execute_app((launcher_app_t*)arg);
                                } else {
                                    execute_action(buffer, (menu_home_action_t)arg, theme);
                                }
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
                            printf("Power button event! %u\r\n", event.args_action.state);
                            if (event.args_action.state) {
                                power_button_latch = true;
                            } else if (power_button_latch) {
                                power_button_latch = false;
                                charging_mode(buffer, theme);
                                render(buffer, theme, &menu, position, false, true);
                            }
                        case BSP_INPUT_ACTION_TYPE_SD_CARD:
                            printf("SD card event! %u\r\n", event.args_action.state);
                            break;
                        case BSP_INPUT_ACTION_TYPE_AUDIO_JACK:
                            printf("Audio jack event! %u\r\n", event.args_action.state);
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
