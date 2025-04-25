#include "gui_osk_edit.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gui_osk.h"
#include "gui_style.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "sdkconfig.h"

esp_err_t gui_osk_edit_init(gui_osk_ctx_t* kb_ctx, pax_buf_t* pax_buffer, float aPosX, float aPosY, float aWidth,
                            float aHeight, const char* aTitle, const char* aHint, char* aOutput, size_t aOutputSize) {
    const pax_font_t* font = pax_font_saira_regular;
    gui_osk_init(pax_buffer, kb_ctx, 1024);
    gui_osk_set_content(kb_ctx, aOutput);
    kb_ctx->kb_font   = font;
    kb_ctx->text_font = font;

    pax_col_t bgColor      = 0xFFFFFFFF;
    pax_col_t shadowColor  = 0xFFC0C3C8;
    pax_col_t borderColor  = 0xFF0000AA;
    pax_col_t titleBgColor = 0xFF080764;
    pax_col_t titleColor   = 0xFFFFFFFF;
    pax_col_t selColor     = 0xff007fff;

    kb_ctx->text_col     = borderColor;
    kb_ctx->sel_text_col = bgColor;
    kb_ctx->sel_col      = selColor;
    kb_ctx->bg_col       = bgColor;

    kb_ctx->kb_font_size = 18;

    float titleHeight = 20;
    float hintHeight  = 14;

    pax_noclip(pax_buffer);
    pax_simple_rect(pax_buffer, shadowColor, aPosX + 5, aPosY + 5, aWidth, aHeight);
    pax_simple_rect(pax_buffer, bgColor, aPosX, aPosY, aWidth, aHeight);
    pax_outline_rect(pax_buffer, borderColor, aPosX, aPosY, aWidth, aHeight);
    pax_simple_rect(pax_buffer, titleBgColor, aPosX, aPosY, aWidth, titleHeight);
    pax_simple_line(pax_buffer, titleColor, aPosX + 1, aPosY + titleHeight, aPosX + aWidth - 2,
                    aPosY + titleHeight - 1);
    // pax_clip(pax_buffer, aPosX + 1, aPosY + 1, aWidth - 2, titleHeight - 2);
    pax_draw_text(pax_buffer, titleColor, font, titleHeight - 2, aPosX + 1, aPosY + 1, aTitle);
    // pax_clip(pax_buffer, aPosX + 1, aPosY + aHeight - hintHeight, aWidth - 2, hintHeight);
    pax_draw_text(pax_buffer, borderColor, font, hintHeight - 2, aPosX + 1, aPosY + aHeight - hintHeight, aHint);
    pax_noclip(pax_buffer);

    kb_ctx->x      = aPosX + 1;
    kb_ctx->y      = aPosY + titleHeight + 1;
    kb_ctx->width  = aWidth - 2;
    kb_ctx->height = aHeight - 3 - titleHeight - hintHeight;

    return ESP_OK;
}

esp_err_t gui_osk_edit_loop(gui_osk_ctx_t* kb_ctx, pax_buf_t* pax_buffer, bool* out_flush) {
    gui_osk_loop(kb_ctx);
    if (out_flush) {
        *out_flush = kb_ctx->dirty;
    }
    if (kb_ctx->dirty) {
        gui_osk_redraw(pax_buffer, kb_ctx);
    }
    return ESP_OK;
}

esp_err_t gui_osk_edit_destroy(gui_osk_ctx_t* kb_ctx, bool* out_accepted, char* aOutput, size_t aOutputSize) {
    if (out_accepted) {
        *out_accepted = kb_ctx->input_accepted;
    }
    if (kb_ctx->input_accepted) {
        memset(aOutput, 0, aOutputSize);
        strncpy(aOutput, kb_ctx->content, aOutputSize - 1);
    }
    gui_osk_destroy(kb_ctx);
    return ESP_OK;
}

esp_err_t gui_osk_edit_get_accepted(gui_osk_ctx_t* kb_ctx, bool* out_accepted) {
    if (out_accepted) {
        *out_accepted = kb_ctx->input_accepted;
    }
    return ESP_OK;
}

esp_err_t gui_osk_edit_navigation_event(gui_osk_ctx_t* kb_ctx, gui_osk_input_t button, bool state) {
    if (state) {
        gui_osk_press(kb_ctx, button);
    } else {
        gui_osk_release(kb_ctx, button);
    }
    return ESP_OK;
}
