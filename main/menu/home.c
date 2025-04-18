#include "home.h"
#include "appfs.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "chakrapetchmedium.h"
#include "common/display.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "wifi_ota.h"

typedef enum {
    ACTION_NONE,
    ACTION_APPS,
    ACTION_REPOSITORY,
    ACTION_FIRMWARE_UPDATE,
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

static void firmware_update_callback(const char* status_text) {
    printf("OTA status changed: %s\r\n", status_text);
    pax_buf_t* fb = display_get_buffer();

    pax_draw_rect(fb, 0xFFEEEAEE, 0, 65, fb->width, 32);
    pax_draw_text(fb, 0xFF340132, &chakrapetchmedium, 16, 20, 70, status_text);
    display_blit_buffer(fb);
}

static void start_firmware_update(gui_theme_t* theme) {
    pax_buf_t* fb = display_get_buffer();
    pax_background(fb, theme->palette.color_background);
    gui_render_header(fb, theme, "Firmware update");
    gui_render_footer(fb, theme, "", "");
    display_blit_buffer(fb);
    ota_update("https://ota.tanmatsu.cloud/latest.bin", firmware_update_callback);
}

static void execute_action(pax_buf_t* fb, menu_home_action_t action, gui_theme_t* theme) {
    switch (action) {
        case ACTION_SETTINGS:
            // menu_settings(fb);
            break;
        case ACTION_FIRMWARE_UPDATE:
            start_firmware_update(theme);
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

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, menu_style_t* menu_style, pax_vec2_t position,
                   bool partial) {
    if (!partial) {
        pax_background(buffer, theme->palette.color_background);
        gui_render_header(buffer, theme, "Home");
        // gui_render_footer(buffer, theme, "↑ / ↓ / ← / → Navigate   🅰 Start");
        gui_render_footer(buffer, theme, "▵ Keyboard backlight ▫ Display backlight ◇ USB mode",
                          "↑ / ↓ / ← / → Navigate ⏎ Select");
        // ❌▵▫○𑗘◇
    }
    menu_render_grid(buffer, menu, position, menu_style, theme, partial);
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
    // ...
}

void menu_home(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu, "Home", NULL);
    menu_insert_item(&menu, "Apps", NULL, (void*)ACTION_APPS, -1);
    menu_insert_item(&menu, "Repository", NULL, (void*)ACTION_REPOSITORY, -1);
    menu_insert_item(&menu, "Firmware update", NULL, (void*)ACTION_FIRMWARE_UPDATE, -1);
    menu_insert_item(&menu, "Settings", NULL, (void*)ACTION_SETTINGS, -1);

    populate_menu_from_appfs_entries(&menu);

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    menu_style_t menu_style = {
        .entry_height       = 32,
        .text_height        = 18,
        .fgColor            = 0xFF000000,
        .bgColor            = 0xFFFFFF00,
        .bgTextColor        = 0xFF000000,
        .selectedItemColor  = 0xFFfec859,
        .borderColor        = 0xFF491d88,
        .scrollbarBgColor   = 0xFFCCCCCC,
        .scrollbarFgColor   = 0xFF555555,
        .scrollbar          = false,
        .font               = pax_font_saira_regular,
        .grid_entry_count_x = 4,
        .grid_entry_count_y = 3,
    };
    memcpy(&menu_style.palette, &theme->palette, sizeof(gui_palette_t));

    render(buffer, theme, &menu, &menu_style, position, false);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_F2:
                                keyboard_backlight();
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F3:
                                display_backlight();
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_F6:
                                toggle_usb_mode();
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_LEFT:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, &menu_style, position, true);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, &menu_style, position, true);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous_row(&menu, &menu_style);
                                render(buffer, theme, &menu, &menu_style, position, true);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next_row(&menu, &menu_style);
                                render(buffer, theme, &menu, &menu_style, position, true);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                if (arg >= (void*)ACTION_LAST) {
                                    execute_app((launcher_app_t*)arg);
                                } else {
                                    execute_action(buffer, (menu_home_action_t)arg, theme);
                                }
                                render(buffer, theme, &menu, &menu_style, position, false);
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
        }
    }
}
