#pragma once

#include "bsp/input.h"
#include "gui_footer.h"
#include "gui_style.h"
#include "pax_types.h"

typedef enum {
    MSG_DIALOG_RETURN_OK,
    MSG_DIALOG_RETURN_NO,
    MSG_DIALOG_RETURN_CANCEL,
} message_dialog_return_type_t;

void render_base_screen(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                        gui_header_field_t* header_left, size_t header_left_count, gui_header_field_t* header_right,
                        size_t header_right_count, gui_header_field_t* footer_left, size_t footer_left_count,
                        gui_header_field_t* footer_right, size_t footer_right_count);
void render_base_screen_statusbar(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                                  gui_header_field_t* header_left, size_t header_left_count,
                                  gui_header_field_t* footer_left, size_t footer_left_count,
                                  gui_header_field_t* footer_right, size_t footer_right_count);

bsp_input_navigation_key_t message_dialog(pax_buf_t* buffer, gui_theme_t* theme, const char* title, const char* message,
                                          gui_header_field_t* headers, int header_count);

message_dialog_return_type_t message_dialog_ok(pax_buf_t* buffer, gui_theme_t* theme, const char* title,
                                               const char* message);
message_dialog_return_type_t message_dialog_yes_no(pax_buf_t* buffer, gui_theme_t* theme, const char* title,
                                                   const char* message);
message_dialog_return_type_t message_dialog_yes_no_cancel(pax_buf_t* buffer, gui_theme_t* theme, const char* title,
                                                          const char* message);

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define MESSAGE_DIALOG_FOOTER_OK ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "OK"}}), 2
#define MESSAGE_DIALOG_FOOTER_GOBACK \
    ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Go back"}}), 2
#define MESSAGE_DIALOG_FOOTER_YES_NO \
    ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "No"}, {get_icon(ICON_F4), "Yes"}}), 3
#define MESSAGE_DIALOG_FOOTER_YES_NO_CANCEL                  \
    ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"},       \
                            {get_icon(ICON_F1), "No"},       \
                            {get_icon(ICON_F4), "Yes"},      \
                            {get_icon(ICON_F6), "Cancel"}}), \
        4

#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define MESSAGE_DIALOG_FOOTER_OK     ((gui_header_field_t[]){{NULL, "🅱"}, {NULL, "Ok"}}), 2
#define MESSAGE_DIALOG_FOOTER_GOBACK ((gui_header_field_t[]){{NULL, "🅱"}, {NULL, "Go back"}}), 2
#define MESSAGE_DIALOG_FOOTER_YES_NO ((gui_header_field_t[]){{NULL, "🅰"}, {NULL, "Yes"}, {NULL, "🅱"}, {NULL, "No"}}), 4
#define MESSAGE_DIALOG_FOOTER_YES_NO_CANCEL \
    ((gui_header_field_t[]){{NULL, "🅰"}, {NULL, "Yes"}, {NULL, "🅱"}, {NULL, "No"}, {NULL, "Menu Cancel"}}), 5

#else
#define MESSAGE_DIALOG_FOOTER_OK            NULL, 0
#define MESSAGE_DIALOG_FOOTER_GOBACK        NULL, 0
#define MESSAGE_DIALOG_FOOTER_YES_NO        NULL, 0
#define MESSAGE_DIALOG_FOOTER_YES_NO_CANCEL NULL, 0
#endif
