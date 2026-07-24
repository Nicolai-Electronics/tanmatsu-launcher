#include "device.h"

bool device_has_lora(void) {
#if defined(CONFIG_BSP_TARGET_TANMATSU)
    return true;
#else
    return false;
#endif
}

bool device_has_rtc(void) {
#if defined(CONFIG_BSP_TARGET_TANMATSU)
    return true;
#else
    return false;
#endif
}

bool device_has_usb_switching(void) {
#if defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32S3)
    return true;
#else
    return false;
#endif
}

bool device_has_provisioning(void) {
#if defined(CONFIG_BSP_TARGET_TANMATSU)
    return true;
#else
    return false;
#endif
}

bool device_has_sdcard(void) {
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KAMI) || defined(CONFIG_BSP_TARGET_MCH2022) || \
    defined(CONFIG_BSP_TARGET_ESP32_P4_FUNCTION_EV_BOARD)
    return true;
#else
    return false;
#endif
}
