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
    pax_buf_t* buffer = display_get_buffer();
    pax_draw_rect(buffer, 0xFFEEEAEE, 0, 85, buffer->width, 32);
    pax_draw_text(buffer, 0xFF340132, &chakrapetchmedium, 16, 20, 90, status_text);
    display_blit_buffer(buffer);
}

void menu_firmware_update(pax_buf_t* buffer, gui_theme_t* theme) {
    pax_background(buffer, theme->palette.color_background);
    // gui_render_header(buffer, theme, "Firmware update");
    gui_render_header_adv(buffer, theme, ((gui_header_field_t[]){{get_icon(ICON_SYSTEM_UPDATE), "Firmware update"}}), 1,
                          NULL, 0);
    gui_render_footer_adv(buffer, theme, NULL, 0, NULL, 0);

    bool staging = false;
    bsp_input_read_navigation_key(BSP_INPUT_NAVIGATION_KEY_F2, &staging);

    bool experimental = false;
    bsp_input_read_navigation_key(BSP_INPUT_NAVIGATION_KEY_F3, &experimental);

    if (experimental) {
        pax_draw_text(buffer, 0xFF340132, &chakrapetchmedium, 16, 20, 70, "Update target: experimental");
        display_blit_buffer(buffer);
        ota_update("https://selfsigned.ota.tanmatsu.cloud/experimental.bin", firmware_update_callback);
    } else if (staging) {
        pax_draw_text(buffer, 0xFF340132, &chakrapetchmedium, 16, 20, 70, "Update target: staging");
        display_blit_buffer(buffer);
        ota_update("https://selfsigned.ota.tanmatsu.cloud/staging.bin", firmware_update_callback);
    } else {
        pax_draw_text(buffer, 0xFF340132, &chakrapetchmedium, 16, 20, 70, "Update target: stable");
        display_blit_buffer(buffer);
        ota_update("https://selfsigned.ota.tanmatsu.cloud/stable.bin", firmware_update_callback);
    }
}
