#pragma once

#include <complex.h>
#include "pax_types.h"

typedef enum {
    ICON_ESC,
    ICON_F1,
    ICON_F2,
    ICON_F3,
    ICON_F4,
    ICON_F5,
    ICON_F6,
    ICON_EXTENSION,
    ICON_HOME,
    ICON_APPS,
    ICON_REPOSITORY,
    ICON_TAG,
    ICON_DEV,
    ICON_SYSTEM_UPDATE,
    ICON_SETTINGS,
    ICON_WIFI,
    ICON_INFO,
    ICON_LAST
} icon_t;

void       load_icons(void);
pax_buf_t* get_icon(icon_t icon);
