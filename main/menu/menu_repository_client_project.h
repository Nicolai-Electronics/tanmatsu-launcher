#pragma once

#include "cJSON.h"
#include "gui_style.h"
#include "pax_types.h"

// Returns the slug of an interpreter that still needs to be installed for the project just
// handled, or NULL if none is needed. The caller must act on this (via
// menu_repository_client_prompt_install_interpreter below) only after this function has fully
// returned, i.e. not from a context that is itself still nested inside another install's call
// stack: installing an interpreter involves its own HTTPS/TLS handshake, which combined with an
// already-deep call stack can overflow the task's stack.
const char* menu_repository_client_project(pax_buf_t* buffer, gui_theme_t* theme, cJSON* project, bool is_plugin);

// Prompt the user to install the given interpreter, using the standard project install dialog.
// Must be called from a shallow stack depth (see note above) — not from within another
// in-progress install's call stack.
void menu_repository_client_prompt_install_interpreter(pax_buf_t* buffer, gui_theme_t* theme,
                                                        const char* interpreter_slug);
