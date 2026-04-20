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
#elif defined(CONFIG_BSP_TARGET_KONSOOL)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/konsool-"
#elif defined(CONFIG_BSP_TARGET_ESP32_P4_FUNCTION_EV_BOARD)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/p4-function-ev-board-"
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/mch2022-"
#elif defined(CONFIG_BSP_TARGET_KAMI)
#define OTA_BASE_URL "https://selfsigned.ota.tanmatsu.cloud/kami-"
#else
#error "Unsupported target for firmware update"
#endif

static void firmware_update_callback(const char* status_text, uint8_t progress) {
    printf("OTA status changed: %s (%u%%)\r\n", status_text, progress);
    progress_dialog(get_icon(ICON_SYSTEM_UPDATE), "Firmware update", status_text ? status_text : "Updating...",
                    progress, true);
}

void ota_update_experimental(void) {
    ota_update(OTA_BASE_URL "experimental.bin", firmware_update_callback);
}

void ota_update_staging(void) {
    ota_update(OTA_BASE_URL "staging.bin", firmware_update_callback);
}

void ota_update_stable(void) {
    ota_update(OTA_BASE_URL "stable.bin", firmware_update_callback);
}
