#pragma once

#include "gui_footer.h"
#include "gui_style.h"
#include "pax_types.h"

void render_base_screen(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                        gui_header_field_t* header_left, size_t header_left_count, gui_header_field_t* header_right,
                        size_t header_right_count, gui_header_field_t* footer_left, size_t footer_left_count,
                        gui_header_field_t* footer_right, size_t footer_right_count);
void render_base_screen_statusbar(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                                  gui_header_field_t* header_left, size_t header_left_count,
                                  gui_header_field_t* footer_left, size_t footer_left_count,
                                  gui_header_field_t* footer_right, size_t footer_right_count);

void message_dialog(pax_buf_t* buffer, gui_theme_t* theme, const char* title, const char* message,
                    const char* action_text);
