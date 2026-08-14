#include "firmware_update.h"
#include <stdint.h>
#include <string.h>
#include "bsp/input.h"
#include "chakrapetchmedium.h"
#include "common/display.h"
#include "common/theme.h"
#include "gui_element_footer.h"
#include "gui_element_header.h"
#include "gui_style.h"
#include "icons.h"
#include "message_dialog.h"
#include "pax_gfx.h"
#include "pax_types.h"
#include "wifi_ota.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/tanmatsu-"
#elif defined(CONFIG_BSP_TARGET_KAMI)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/kami-"
#elif defined(CONFIG_BSP_TARGET_ESP32_P4_FUNCTION_EV_BOARD)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/p4-function-ev-board-"
#elif defined(CONFIG_BSP_TARGET_ESP32_S31_KORVO_1)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/esp32-s31-korvo-1-"
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/mch2022-"
#elif defined(CONFIG_BSP_TARGET_HACKADAY2025)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/hackaday2025-"
#elif defined(CONFIG_TANMATSU_LAUNCHER_M5STACK_TAB5)
#define OTA_UNSUPPORTED_ON_THIS_TARGET
#else
#error "Unsupported target for firmware update"
#endif

static void firmware_update_callback(const char* status_text, uint8_t progress) {
    printf("OTA status changed: %s (%u%%)\r\n", status_text, progress);
    progress_dialog(get_icon(ICON_SYSTEM_UPDATE), "Firmware update", status_text ? status_text : "Updating...",
                    progress, true);
}

void ota_update_experimental(void) {
#ifdef OTA_UNSUPPORTED_ON_THIS_TARGET
    message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Firmware update",
                   "OTA updates are not available for the M5Stack Tab5 yet. Use USB flashing instead.", "Close");
#else
    ota_update(OTA_BASE_URL "experimental.bin", firmware_update_callback);
#endif
}

void ota_update_staging(void) {
#ifdef OTA_UNSUPPORTED_ON_THIS_TARGET
    message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Firmware update",
                   "OTA updates are not available for the M5Stack Tab5 yet. Use USB flashing instead.", "Close");
#else
    ota_update(OTA_BASE_URL "staging.bin", firmware_update_callback);
#endif
}

void ota_update_stable(void) {
#ifdef OTA_UNSUPPORTED_ON_THIS_TARGET
    message_dialog(get_icon(ICON_SYSTEM_UPDATE), "Firmware update",
                   "OTA updates are not available for the M5Stack Tab5 yet. Use USB flashing instead.", "Close");
#else
    ota_update(OTA_BASE_URL "stable.bin", firmware_update_callback);
#endif
}
