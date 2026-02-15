#include "chakrapetchmedium.h"
#include "device_settings.h"
#include "gui_style.h"
#include "pax_gfx.h"
#include "rajdhani.h"
#include "sdkconfig.h"

gui_theme_t theme = {0};

void theme_initialize(void) {
    gui_palette_t palette = {
        .color_foreground          = 0xFF340132,  // #340132
        .color_background          = 0xFFEEEAEE,  // #EEEAEE
        .color_active_foreground   = 0xFF340132,  // #340132
        .color_active_background   = 0xFFFFFFFF,  // #FFFFFF
        .color_highlight_primary   = 0xFF01BC99,  // #01BC99
        .color_highlight_secondary = 0xFFFFCF53,  // #FFCF53
        .color_highlight_tertiary  = 0xFFFF017F,  // #FF017F
    };

    theme_setting_t theme_setting = THEME_BLACK;
    device_settings_get_theme(&theme_setting);

    if (theme_setting == THEME_WHITE) {
        palette.color_foreground          = 0xFFFFFFFF;  // #FFFFFF
        palette.color_background          = 0xFF000000;  // #000000
        palette.color_active_foreground   = 0xFFFFFFFF;  // #FFFFFF
        palette.color_active_background   = 0xFF000000;  // #000000
        palette.color_highlight_primary   = 0xFF71B7B4;  // #71B7B4
        palette.color_highlight_secondary = 0xFFCFED58;  // #CFED58
        palette.color_highlight_tertiary  = 0xFFFE6158;  // #FE6158
    } else if (theme_setting == THEME_RED) {
        palette.color_foreground          = 0xFFC62828;  // #C62828
        palette.color_background          = 0xFFEEEAEE;  // #EEEAEE
        palette.color_active_foreground   = 0xFFC62828;  // #C62828
        palette.color_active_background   = 0xFFFFFFFF;  // #FFFFFF
        palette.color_highlight_primary   = 0xFF01BC99;  // #01BC99
        palette.color_highlight_secondary = 0xFFFFCF53;  // #FFCF53
        palette.color_highlight_tertiary  = 0xFFFF017F;  // #FF017F
    } else if (theme_setting == THEME_BLUE) {
        palette.color_foreground          = 0xFF283593;  // #283593
        palette.color_background          = 0xFFEEEAEE;  // #EEEAEE
        palette.color_active_foreground   = 0xFF283593;  // #283593
        palette.color_active_background   = 0xFFFFFFFF;  // #FFFFFF
        palette.color_highlight_primary   = 0xFF01BC99;  // #01BC99
        palette.color_highlight_secondary = 0xFFFFCF53;  // #FFCF53
        palette.color_highlight_tertiary  = 0xFFFF017F;  // #FF017F
    } else if (theme_setting == THEME_PURPLE) {
        palette.color_foreground          = 0xFF6A1B9A;  // #6A1B9A
        palette.color_background          = 0xFFEEEAEE;  // #EEEAEE
        palette.color_active_foreground   = 0xFF6A1B9A;  // #6A1B9A
        palette.color_active_background   = 0xFFFFFFFF;  // #FFFFFF
        palette.color_highlight_primary   = 0xFF01BC99;  // #01BC99
        palette.color_highlight_secondary = 0xFFFFCF53;  // #FFCF53
        palette.color_highlight_tertiary  = 0xFFFF017F;  // #FF017F
    } else if (theme_setting == THEME_ORANGE) {
        palette.color_foreground          = 0xFFEF6C00;  // #EF6C00
        palette.color_background          = 0xFFEEEAEE;  // #EEEAEE
        palette.color_active_foreground   = 0xFFEF6C00;  // #EF6C00
        palette.color_active_background   = 0xFFFFFFFF;  // #FFFFFF
        palette.color_highlight_primary   = 0xFF01BC99;  // #01BC99
        palette.color_highlight_secondary = 0xFFFFCF53;  // #FFCF53
        palette.color_highlight_tertiary  = 0xFFFF017F;  // #FF017F
    } else if (theme_setting == THEME_GREEN) {
        palette.color_foreground          = 0xFF2E7D32;  // #2E7D32
        palette.color_background          = 0xFFEEEAEE;  // #EEEAEE
        palette.color_active_foreground   = 0xFF2E7D32;  // #2E7D32
        palette.color_active_background   = 0xFFFFFFFF;  // #FFFFFF
        palette.color_highlight_primary   = 0xFF01BC99;  // #01BC99
        palette.color_highlight_secondary = 0xFFFFCF53;  // #FFCF53
        palette.color_highlight_tertiary  = 0xFFFF017F;  // #FF017F
    } else if (theme_setting == THEME_YELLOW) {
        palette.color_foreground          = 0xFFFFEB3B;  // #FFEB3B
        palette.color_background          = 0xFF000000;  // #000000
        palette.color_active_foreground   = 0xFFFFEB3B;  // #FFEB3B
        palette.color_active_background   = 0xFF000000;  // #000000
        palette.color_highlight_primary   = 0xFF01BC99;  // #01BC99
        palette.color_highlight_secondary = 0xFFFFCF53;  // #FFCF53
        palette.color_highlight_tertiary  = 0xFFFF017F;  // #FF017F
    } else if (theme_setting == THEME_C_BLUE) {
        palette.color_foreground          = 0xFF081fb8;  // #081fb8
        palette.color_background          = 0xFFEEEAEE;  // #EEEAEE
        palette.color_active_foreground   = 0xFF081fb8;  // #081fb8
        palette.color_active_background   = 0xFFFFFFFF;  // #FFFFFF
        palette.color_highlight_primary   = 0xFF01BC99;  // #01BC99
        palette.color_highlight_secondary = 0xFFFFCF53;  // #FFCF53
        palette.color_highlight_tertiary  = 0xFFFF017F;  // #FF017F
    } else if (theme_setting == THEME_C_RED) {
        palette.color_foreground          = 0xFFCD1C33;  // #CD1C33
        palette.color_background          = 0xFFEEEAEE;  // #EEEAEE
        palette.color_active_foreground   = 0xFFCD1C33;  // #CD1C33
        palette.color_active_background   = 0xFFFFFFFF;  // #FFFFFF
        palette.color_highlight_primary   = 0xFF01BC99;  // #01BC99
        palette.color_highlight_secondary = 0xFFFFCF53;  // #FFCF53
        palette.color_highlight_tertiary  = 0xFFFF017F;  // #FF017F
    }

#if defined(CONFIG_BSP_TARGET_KAMI) || defined(CONFIG_BSP_TARGET_HACKERHOTEL_2024)
    // Override for e-paper (red/black) devices
    palette.color_foreground          = 0xFF000000;  // #000000
    palette.color_background          = 0xFFFFFFFF;  // #FFFFFF
    palette.color_active_foreground   = 0xFF000000;  // #000000
    palette.color_active_background   = 0xFFFFFFFF;  // #FFFFFF
    palette.color_highlight_primary   = 0xFFFFFFFF;  // #FFFFFF
    palette.color_highlight_secondary = 0xFFFF0000;  // #FF0000
    palette.color_highlight_tertiary  = 0xFFFF0000;  // #FF0000
#endif

    memcpy((void*)&theme.palette, &palette, sizeof(gui_palette_t));
    memcpy((void*)&theme.footer.palette, &palette, sizeof(gui_palette_t));
    memcpy((void*)&theme.header.palette, &palette, sizeof(gui_palette_t));
    memcpy((void*)&theme.menu.palette, &palette, sizeof(gui_palette_t));
    memcpy((void*)&theme.progressbar.palette, &palette, sizeof(gui_palette_t));
    memcpy((void*)&theme.chat.palette, &palette, sizeof(gui_palette_t));

    theme.footer.height                  = 32;
    theme.footer.vertical_margin         = 7;
    theme.footer.horizontal_margin       = 20;
    theme.footer.text_height             = 16;
    theme.footer.vertical_padding        = 20;
    theme.footer.horizontal_padding      = 0;
    theme.footer.text_font               = &chakrapetchmedium;
    theme.header.height                  = 32;
    theme.header.vertical_margin         = 7;
    theme.header.horizontal_margin       = 20;
    theme.header.text_height             = 16;
    theme.header.vertical_padding        = 20;
    theme.header.horizontal_padding      = 0;
    theme.header.text_font               = &chakrapetchmedium;
    theme.menu.height                    = 480 - 64;
    theme.menu.vertical_margin           = 20;
    theme.menu.horizontal_margin         = 30;
    theme.menu.text_height               = 16;
    theme.menu.vertical_padding          = 6;
    theme.menu.horizontal_padding        = 6;
    theme.menu.text_font                 = &chakrapetchmedium;
    theme.menu.list_entry_height         = 32;
    theme.menu.grid_horizontal_count     = 4;
    theme.menu.grid_vertical_count       = 3;
    theme.progressbar.vertical_margin    = 5;
    theme.progressbar.horizontal_margin  = 5;
    theme.progressbar.vertical_padding   = 5;
    theme.progressbar.horizontal_padding = 5;
    theme.chat.height                    = 480 - 64;
    theme.chat.vertical_margin           = 20;
    theme.chat.horizontal_margin         = 30;
    theme.chat.text_height               = 24;
    theme.chat.vertical_padding          = 6;
    theme.chat.horizontal_padding        = 6;
    theme.chat.text_font                 = &rajdhani;
    theme.chat.list_entry_height         = 64;

#if defined(CONFIG_BSP_TARGET_MCH2022)
    footer.height             = 24;
    footer.vertical_margin    = 0;
    footer.horizontal_margin  = 0;
    footer.text_height        = 18;
    footer.vertical_padding   = 0;
    footer.horizontal_padding = 0;
    footer.text_font          = pax_font_saira_regular;

    header.height             = 32;
    header.vertical_margin    = 0;
    header.horizontal_margin  = 0;
    header.text_height        = 18;
    header.vertical_padding   = 0;
    header.horizontal_padding = 0;
    header.text_font          = pax_font_saira_regular;

    menu.height                = 240 - 32 - 16;
    menu.vertical_margin       = 0;
    menu.horizontal_margin     = 0;
    menu.text_height           = 18;
    menu.vertical_padding      = 3;
    menu.horizontal_padding    = 3;
    menu.text_font             = pax_font_saira_regular;
    menu.list_entry_height     = 32;
    menu.grid_horizontal_count = 3;
    menu.grid_vertical_count   = 3;
#endif

#if defined(CONFIG_BSP_TARGET_KAMI) || defined(CONFIG_BSP_TARGET_HACKERHOTEL_2024)
    footer.height             = 24;
    footer.vertical_margin    = 0;
    footer.horizontal_margin  = 0;
    footer.text_height        = 18;
    footer.vertical_padding   = 0;
    footer.horizontal_padding = 0;
    footer.text_font          = &chakrapetchmedium;

    header.height             = 32;
    header.vertical_margin    = 0;
    header.horizontal_margin  = 0;
    header.text_height        = 18;
    header.vertical_padding   = 0;
    header.horizontal_padding = 0;
    header.text_font          = &chakrapetchmedium;

    menu.height                = 240 - 32 - 16;
    menu.vertical_margin       = 0;
    menu.horizontal_margin     = 0;
    menu.text_height           = 18;
    menu.vertical_padding      = 3;
    menu.horizontal_padding    = 3;
    menu.text_font             = &chakrapetchmedium;
    menu.list_entry_height     = 32;
    menu.grid_horizontal_count = 3;
    menu.grid_vertical_count   = 3;
#endif
}

gui_theme_t* get_theme(void) {
    return (gui_theme_t*)&theme;
}