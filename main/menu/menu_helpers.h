#pragma once

#include "gui_menu.h"
#include "gui_style.h"
#include "pax_types.h"
#include "gui_element_icontext.h"

// Common footer macros used by most menu screens
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define MENU_FOOTER_BACK       ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define MENU_FOOTER_NAV_SELECT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Select"}}), 1
#define MENU_FOOTER_BACK_ONLY  MENU_FOOTER_BACK
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define MENU_FOOTER_BACK       NULL, 0
#define MENU_FOOTER_NAV_SELECT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | 🅱 Back 🅰 Select"}}), 1
#define MENU_FOOTER_BACK_ONLY  NULL, 0
#else
#define MENU_FOOTER_BACK       NULL, 0
#define MENU_FOOTER_NAV_SELECT NULL, 0
#define MENU_FOOTER_BACK_ONLY  NULL, 0
#endif

// Calculate the standard menu content area from theme and buffer
pax_vec2_t menu_calc_position(pax_buf_t* buffer, gui_theme_t* theme);

// Callback for when a menu item is selected (RETURN/A pressed).
// Return true to exit the menu loop, false to continue.
typedef bool (*menu_action_cb_t)(void* action_arg, void* user_ctx);

// Run a standard list menu event loop.
// Renders header/footer, handles UP/DOWN/BACK/ENTER navigation.
// Calls action_cb when an item is selected.
// Also handles HOME key as back if home_is_back is true.
void menu_run_list(menu_t* menu, gui_element_icontext_t* header, size_t header_count,
                   gui_element_icontext_t* footer_left, size_t footer_left_count,
                   gui_element_icontext_t* footer_right, size_t footer_right_count,
                   menu_action_cb_t action_cb, void* user_ctx, bool home_is_back);

// Run a standard grid menu event loop (uses menu_render_grid instead of menu_render).
void menu_run_grid(menu_t* menu, gui_element_icontext_t* header, size_t header_count,
                   gui_element_icontext_t* footer_left, size_t footer_left_count,
                   gui_element_icontext_t* footer_right, size_t footer_right_count,
                   menu_action_cb_t action_cb, void* user_ctx, bool home_is_back);
