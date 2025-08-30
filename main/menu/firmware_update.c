#include "firmware_update.h"
#include <stdint.h>
#include <string.h>
#include "bsp/input.h"
#include "chakrapetchmedium.h"
#include "common/display.h"
#include "gui_element_footer.h"
#include "gui_element_header.h"
#include "gui_style.h"
#include "icons.h"
#include "pax_gfx.h"
#include "pax_types.h"
#include "wifi_ota.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/tanmatsu-"
#elif defined(CONFIG_BSP_TARGET_KONSOOL)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/konsool-"
#elif defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/hackerhotel-2026-"
#elif defined(CONFIG_BSP_TARGET_ESP32_P4_FUNCTION_EV_BOARD)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/p4-function-ev-board-"
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/mch2022-"
#elif defined(CONFIG_BSP_TARGET_HACKERHOTEL_2024)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/hackerhotel-2024-"
#elif defined(CONFIG_BSP_TARGET_KAMI)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/kami-"
#elif defined(CONFIG_BSP_TARGET_WHY2025)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/why2025-"
#else
#error "Unsupported target for firmware update"
#endif

static void firmware_update_callback(const char* status_text, uint8_t progress) {
    printf("OTA status changed: %s\r\n", status_text);
    pax_buf_t* buffer = display_get_buffer();
    pax_draw_rect(buffer, 0xFFEEEAEE, 0, 85, buffer->width, 32);
    pax_draw_text(buffer, 0xFF340132, &chakrapetchmedium, 16, 20, 90, status_text);
    display_blit_buffer(buffer);
}

void menu_firmware_update(pax_buf_t* buffer, gui_theme_t* theme) {
    pax_background(buffer, theme->palette.color_background);
    gui_header_draw(buffer, theme, ((gui_element_icontext_t[]){{get_icon(ICON_SYSTEM_UPDATE), "Firmware update"}}), 1,
                    NULL, 0);
    gui_footer_draw(buffer, theme, NULL, 0, NULL, 0);

    bool staging = false;
    bsp_input_read_navigation_key(BSP_INPUT_NAVIGATION_KEY_F2, &staging);

    bool experimental = false;
    bsp_input_read_navigation_key(BSP_INPUT_NAVIGATION_KEY_F3, &experimental);

    if (experimental) {
        pax_draw_text(buffer, 0xFF340132, &chakrapetchmedium, 16, 20, 70, "Update target: experimental");
        display_blit_buffer(buffer);
        ota_update(OTA_BASE_URL "experimental.bin", firmware_update_callback);
    } else if (staging) {
        pax_draw_text(buffer, 0xFF340132, &chakrapetchmedium, 16, 20, 70, "Update target: staging");
        display_blit_buffer(buffer);
        ota_update(OTA_BASE_URL "staging.bin", firmware_update_callback);
    } else {
        pax_draw_text(buffer, 0xFF340132, &chakrapetchmedium, 16, 20, 70, "Update target: stable");
        display_blit_buffer(buffer);
        ota_update(OTA_BASE_URL "stable.bin", firmware_update_callback);
    }
}
