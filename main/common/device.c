#include "device.h"

bool device_has_lora(void) {
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
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
