#include <string.h>
#include "appfs.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "chakrapetchmedium.h"
#include "common/display.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "home.h"
#include "icons.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "settings.h"
// #include "shapes/pax_misc.h"
#include "wifi_ota.h"

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

static void execute_app(launcher_app_t* app) {
    if (strcmp(app->type, "esp32") == 0) {
        appfsBootSelect(app->appfs_fd, NULL);
        esp_restart();
    } else if (strcmp(app->type, "python") == 0) {
        // TBD
    }
}

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t* menu, pax_vec2_t position, bool partial) {
    if (!partial) {
        pax_background(buffer, theme->palette.color_background);
        // gui_render_header(buffer, theme, "Apps");
        // gui_render_footer(buffer, theme, "↑ / ↓ / ← / → Navigate   🅰 Start");
        // gui_render_footer(buffer, theme, "Esc / ❌ Back", "↑ / ↓ Navigate ⏎ Select");
        gui_render_header_adv(buffer, theme, &((gui_icontext_t){get_icon(ICON_APPS), "Apps"}), 1);
        gui_render_footer_adv(buffer, theme,
                              ((gui_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
                              "↑ / ↓ Navigate ⏎ Start app");
        // ❌▵▫○𑗘◇
    }
    menu_render_grid(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

static void apps_free(menu_t* menu) {
    for (;;) {
        void*           arg = menu_get_callback_args(menu, menu_get_position(menu));
        launcher_app_t* app = (launcher_app_t*)arg;
        if (app == NULL) break;

        if (app->icon != NULL) {
            free(app->icon);
        }
        free(app);
    }
}

void menu_apps(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    menu_t menu = {0};
    menu_initialize(&menu, "Home", NULL);
    populate_menu_from_appfs_entries(&menu);

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    render(buffer, theme, &menu, position, false);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                                menu_free(&menu);
                                apps_free(&menu);
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(&menu);
                                render(buffer, theme, &menu, position, true);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(buffer, theme, &menu, position, true);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN: {
                                void*           arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                launcher_app_t* app = (launcher_app_t*)arg;
                                execute_app(app);
                                render(buffer, theme, &menu, position, false);
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
