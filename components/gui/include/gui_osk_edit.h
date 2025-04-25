#pragma once

#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gui_osk.h"
#include "pax_gfx.h"

esp_err_t gui_osk_edit_init(gui_osk_ctx_t* kb_ctx, pax_buf_t* pax_buffer, float aPosX, float aPosY, float aWidth,
                            float aHeight, const char* aTitle, const char* aHint, char* aOutput, size_t aOutputSize);
esp_err_t gui_osk_edit_loop(gui_osk_ctx_t* kb_ctx, pax_buf_t* pax_buffer, bool* out_flush);
esp_err_t gui_osk_edit_destroy(gui_osk_ctx_t* kb_ctx, bool* out_accepted, char* aOutput, size_t aOutputSize);
esp_err_t gui_osk_edit_get_accepted(gui_osk_ctx_t* kb_ctx, bool* out_accepted);
esp_err_t gui_osk_edit_navigation_event(gui_osk_ctx_t* kb_ctx, gui_osk_input_t button, bool state);
