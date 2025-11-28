#include "chakrapetchmedium.h"
#include "gui_style.h"
#include "pax_gfx.h"
#include "sdkconfig.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026) || defined(CONFIG_BSP_TARGET_ESP32_P4_FUNCTION_EV_BOARD)
const gui_theme_t theme = {
    .palette =
        {
            .color_foreground          = 0xFF340132,  // #340132
            .color_background          = 0xFFEEEAEE,  // #EEEAEE
            .color_active_foreground   = 0xFF340132,  // #340132
            .color_active_background   = 0xFFFFFFFF,  // #FFFFFF
            .color_highlight_primary   = 0xFF01BC99,  // #01BC99
            .color_highlight_secondary = 0xFFFFCF53,  // #FFCF53
            .color_highlight_tertiary  = 0xFFFF017F,  // #FF017F
        },
    .footer =
        {
            .height             = 32,
            .vertical_margin    = 7,
            .horizontal_margin  = 20,
            .text_height        = 16,
            .vertical_padding   = 20,
            .horizontal_padding = 0,
            .text_font          = &chakrapetchmedium,
            .palette =
                {
                    .color_foreground        = 0xFF340132,  // #340132
                    .color_background        = 0xFFEEEAEE,  // #EEEAEE
                    .color_active_foreground = 0xFF340132,  // #340132
                    .color_active_background = 0xFFFFFFFF,  // #FFFFFF
                },
        },
    .header =
        {
            .height             = 32,
            .vertical_margin    = 7,
            .horizontal_margin  = 20,
            .text_height        = 16,
            .vertical_padding   = 20,
            .horizontal_padding = 0,
            .text_font          = &chakrapetchmedium,
            .palette =
                {
                    .color_foreground        = 0xFF340132,  // #340132
                    .color_background        = 0xFFEEEAEE,  // #EEEAEE
                    .color_active_foreground = 0xFF340132,  // #340132
                    .color_active_background = 0xFFFFFFFF,  // #FFFFFF
                },
        },
    .menu =
        {
            .height                = 480 - 64,
            .vertical_margin       = 20,
            .horizontal_margin     = 30,
            .text_height           = 16,
            .vertical_padding      = 6,
            .horizontal_padding    = 6,
            .text_font             = &chakrapetchmedium,
            .list_entry_height     = 32,
            .grid_horizontal_count = 4,
            .grid_vertical_count   = 3,
            .palette =
                {
                    .color_foreground          = 0xFF340132,  // #340132
                    .color_background          = 0xFFEEEAEE,  // #EEEAEE
                    .color_active_foreground   = 0xFF340132,  // #340132
                    .color_active_background   = 0xFFFFFFFF,  // #FFFFFF
                    .color_highlight_primary   = 0xFF01BC99,  // #01BC99
                    .color_highlight_secondary = 0xFFFFCF53,  // #FFCF53
                    .color_highlight_tertiary  = 0xFFFF017F,  // #FF017F
                },
        },
};
#elif defined(CONFIG_BSP_TARGET_MCH2022)
const gui_theme_t theme = {
    .palette =
        {
            .color_foreground          = 0xFF340132,  // #340132
            .color_background          = 0xFFEEEAEE,  // #EEEAEE
            .color_active_foreground   = 0xFF340132,  // #340132
            .color_active_background   = 0xFFFFFFFF,  // #FFFFFF
            .color_highlight_primary   = 0xFF01BC99,  // #01BC99
            .color_highlight_secondary = 0xFFFFCF53,  // #FFCF53
            .color_highlight_tertiary  = 0xFFFF017F,  // #FF017F
        },
    .footer =
        {
            .height             = 24,
            .vertical_margin    = 0,
            .horizontal_margin  = 0,
            .text_height        = 18,
            .vertical_padding   = 0,
            .horizontal_padding = 0,
            .text_font          = pax_font_saira_regular,
            .palette =
                {
                    .color_foreground        = 0xFF340132,  // #340132
                    .color_background        = 0xFFEEEAEE,  // #EEEAEE
                    .color_active_foreground = 0xFF340132,  // #340132
                    .color_active_background = 0xFFFFFFFF,  // #FFFFFF
                },
        },
    .header =
        {
            .height             = 32,
            .vertical_margin    = 0,
            .horizontal_margin  = 0,
            .text_height        = 18,
            .vertical_padding   = 0,
            .horizontal_padding = 0,
            .text_font          = pax_font_saira_regular,
            .palette =
                {
                    .color_foreground        = 0xFF340132,  // #340132
                    .color_background        = 0xFFEEEAEE,  // #EEEAEE
                    .color_active_foreground = 0xFF340132,  // #340132
                    .color_active_background = 0xFFFFFFFF,  // #FFFFFF
                },
        },
    .menu =
        {
            .height                = 240 - 32 - 16,
            .vertical_margin       = 0,
            .horizontal_margin     = 0,
            .text_height           = 18,
            .vertical_padding      = 3,
            .horizontal_padding    = 3,
            .text_font             = pax_font_saira_regular,
            .list_entry_height     = 32,
            .grid_horizontal_count = 3,
            .grid_vertical_count   = 3,
            .palette =
                {
                    .color_foreground          = 0xFF340132,  // #340132
                    .color_background          = 0xFFEEEAEE,  // #EEEAEE
                    .color_active_foreground   = 0xFF340132,  // #340132
                    .color_active_background   = 0xFFFFFFFF,  // #FFFFFF
                    .color_highlight_primary   = 0xFF01BC99,  // #01BC99
                    .color_highlight_secondary = 0xFFFFCF53,  // #FFCF53
                    .color_highlight_tertiary  = 0xFFFF017F,  // #FF017F
                },
        },
};
#elif defined(CONFIG_BSP_TARGET_KAMI) || defined(CONFIG_BSP_TARGET_HACKERHOTEL_2024)
const gui_theme_t theme = {
    .palette =
        {
            .color_foreground          = 1,
            .color_background          = 0,
            .color_active_foreground   = 2,
            .color_active_background   = 0,
            .color_highlight_primary   = 2,
            .color_highlight_secondary = 2,
            .color_highlight_tertiary  = 2,
        },
    .footer =
        {
            .height             = 16,
            .vertical_margin    = 0,
            .horizontal_margin  = 0,
            .text_height        = 16,
            .vertical_padding   = 5,
            .horizontal_padding = 0,
            .text_font          = &chakrapetchmedium,
            .palette =
                {
                    .color_foreground          = 1,
                    .color_background          = 0,
                    .color_active_foreground   = 2,
                    .color_active_background   = 0,
                    .color_highlight_primary   = 2,
                    .color_highlight_secondary = 2,
                    .color_highlight_tertiary  = 2,
                },
        },
    .header =
        {
            .height             = 32,
            .vertical_margin    = 0,
            .horizontal_margin  = 0,
            .text_height        = 16,
            .vertical_padding   = 0,
            .horizontal_padding = 0,
            .text_font          = &chakrapetchmedium,
            .palette =
                {
                    .color_foreground          = 1,
                    .color_background          = 0,
                    .color_active_foreground   = 2,
                    .color_active_background   = 0,
                    .color_highlight_primary   = 2,
                    .color_highlight_secondary = 2,
                    .color_highlight_tertiary  = 2,
                },
        },
    .menu =
        {
            .height                = 240 - 32 - 16,
            .vertical_margin       = 0,
            .horizontal_margin     = 0,
            .text_height           = 16,
            .vertical_padding      = 3,
            .horizontal_padding    = 3,
            .text_font             = &chakrapetchmedium,
            .list_entry_height     = 32,
            .grid_horizontal_count = 3,
            .grid_vertical_count   = 3,
            .palette =
                {
                    .color_foreground          = 1,
                    .color_background          = 0,
                    .color_active_foreground   = 2,
                    .color_active_background   = 0,
                    .color_highlight_primary   = 2,
                    .color_highlight_secondary = 2,
                    .color_highlight_tertiary  = 2,
                },
        },
};
#else
#error "Unsupported target"
#endif

gui_theme_t* get_theme(void) {
    return (gui_theme_t*)&theme;
}