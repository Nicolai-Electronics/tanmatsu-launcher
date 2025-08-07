#pragma once

#include "app_metadata_parser.h"
#include "gui_style.h"
#include "pax_types.h"

void execute_app(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, app_t* app);

void menu_apps(pax_buf_t* buffer, gui_theme_t* theme);
