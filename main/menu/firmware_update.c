#include "firmware_update.h"
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
#include "icons.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "settings.h"
// #include "shapes/pax_misc.h"
#include "wifi_ota.h"

static void firmware_update_callback(const char* status_text) {
    printf("OTA status changed: %s\r\n", status_text);
    pax_buf_t* fb = display_get_buffer();
    pax_draw_rect(fb, 0xFFEEEAEE, 0, 65, fb->width, 32);
    pax_draw_text(fb, 0xFF340132, &chakrapetchmedium, 16, 20, 70, status_text);
    display_blit_buffer(fb);
}

void menu_firmware_update(pax_buf_t* buffer, gui_theme_t* theme) {
    pax_background(buffer, theme->palette.color_background);
    // gui_render_header(buffer, theme, "Firmware update");
    gui_render_header_adv(buffer, theme, ((gui_header_field_t[]){{get_icon(ICON_SYSTEM_UPDATE), "Firmware update"}}), 1,
                          NULL, 0);
    gui_render_footer_adv(buffer, theme, NULL, 0, NULL, 0);
    display_blit_buffer(buffer);
    ota_update("https://ota.tanmatsu.cloud/latest.bin", firmware_update_callback);
}
