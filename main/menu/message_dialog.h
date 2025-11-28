#pragma once

#include "bsp/input.h"
#include "gui_element_icontext.h"
#include "gui_style.h"
#include "pax_types.h"

typedef enum {
    MSG_DIALOG_RETURN_OK,
    MSG_DIALOG_RETURN_NO,
    MSG_DIALOG_RETURN_CANCEL,
} message_dialog_return_type_t;

void render_base_screen(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                        gui_element_icontext_t* header_left, size_t header_left_count,
                        gui_element_icontext_t* header_right, size_t header_right_count,
                        gui_element_icontext_t* footer_left, size_t footer_left_count,
                        gui_element_icontext_t* footer_right, size_t footer_right_count);
void render_base_screen_statusbar(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                                  gui_element_icontext_t* header_left, size_t header_left_count,
                                  gui_element_icontext_t* footer_left, size_t footer_left_count,
                                  gui_element_icontext_t* footer_right, size_t footer_right_count);

void message_dialog(pax_buf_t* icon, const char* title, const char* message, const char* action_text);
void busy_dialog(pax_buf_t* icon, const char* title, const char* message, bool header);

bsp_input_navigation_key_t adv_dialog(pax_buf_t* icon, const char* title, const char* message,
                                      gui_element_icontext_t* headers, int header_count);

message_dialog_return_type_t adv_dialog_ok(pax_buf_t* icon, const char* title, const char* message);
message_dialog_return_type_t adv_dialog_yes_no(pax_buf_t* icon, const char* title, const char* message);
message_dialog_return_type_t adv_dialog_yes_no_cancel(pax_buf_t* icon, const char* title, const char* message);

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define ADV_DIALOG_FOOTER_OK ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "OK"}}), 2
#define ADV_DIALOG_FOOTER_GOBACK \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Go back"}}), 2
#define ADV_DIALOG_FOOTER_YES_NO \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "No"}, {get_icon(ICON_F4), "Yes"}}), 3
#define ADV_DIALOG_FOOTER_YES_NO_CANCEL                          \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},       \
                                {get_icon(ICON_F1), "No"},       \
                                {get_icon(ICON_F4), "Yes"},      \
                                {get_icon(ICON_F6), "Cancel"}}), \
        4
#define ADV_DIALOG_FOOTER_OK_TEXT(...) \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), __VA_ARGS__}}), 2

#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define ADV_DIALOG_FOOTER_OK     ((gui_element_icontext_t[]){{NULL, "🅱"}, {NULL, "Ok"}}), 2
#define ADV_DIALOG_FOOTER_GOBACK ((gui_element_icontext_t[]){{NULL, "🅱"}, {NULL, "Go back"}}), 2
#define ADV_DIALOG_FOOTER_YES_NO ((gui_element_icontext_t[]){{NULL, "🅰"}, {NULL, "Yes"}, {NULL, "🅱"}, {NULL, "No"}}), 4
#define ADV_DIALOG_FOOTER_YES_NO_CANCEL \
    ((gui_element_icontext_t[]){{NULL, "🅰"}, {NULL, "Yes"}, {NULL, "🅱"}, {NULL, "No"}, {NULL, "Menu Cancel"}}), 5
#define ADV_DIALOG_FOOTER_OK_TEXT(...) ((gui_element_icontext_t[]){{NULL, "🅱"}, {NULL, __VA_ARGS__}}), 2

#else
#define ADV_DIALOG_FOOTER_OK            NULL, 0
#define ADV_DIALOG_FOOTER_GOBACK        NULL, 0
#define ADV_DIALOG_FOOTER_YES_NO        NULL, 0
#define ADV_DIALOG_FOOTER_YES_NO_CANCEL NULL, 0
#define ADV_DIALOG_FOOTER_OK_TEXT(...)  NULL, 0
#endif
