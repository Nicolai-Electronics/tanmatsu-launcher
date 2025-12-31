#pragma once

#include "pax_matrix.h"
#ifdef __cplusplus
extern "C" {
#endif  //__cplusplus

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "gui_menu.h"
#include "gui_style.h"

typedef struct {
    uint32_t timestamp;
    bool     sent_by_user;
    void*    message;
} gui_chat_message_metadata_t;

void chat_render(pax_buf_t* pax_buffer, menu_t* menu, pax_vec2_t position, gui_theme_t* theme, bool partial);

#ifdef __cplusplus
}
#endif  //__cplusplus
