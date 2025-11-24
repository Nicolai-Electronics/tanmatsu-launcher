#include "test_keyboard.h"
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "freertos/idf_additions.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "shapes/pax_misc.h"
#include "shapes/pax_rects.h"

typedef struct {
    const char*          name;
    bsp_input_scancode_t scancode;
    bool                 pressed;
    bool                 was_pressed;
    bool                 changed;
} key_t;

key_t keys[] = {
    {.name = "ESC", .scancode = BSP_INPUT_SCANCODE_ESC, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "F1", .scancode = BSP_INPUT_SCANCODE_F1, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "F2", .scancode = BSP_INPUT_SCANCODE_F2, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "F3", .scancode = BSP_INPUT_SCANCODE_F3, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "F4", .scancode = BSP_INPUT_SCANCODE_F4, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "F5", .scancode = BSP_INPUT_SCANCODE_F5, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "F6", .scancode = BSP_INPUT_SCANCODE_F6, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "BS", .scancode = BSP_INPUT_SCANCODE_BACKSPACE, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "`", .scancode = BSP_INPUT_SCANCODE_GRAVE, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "1", .scancode = BSP_INPUT_SCANCODE_1, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "2", .scancode = BSP_INPUT_SCANCODE_2, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "3", .scancode = BSP_INPUT_SCANCODE_3, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "4", .scancode = BSP_INPUT_SCANCODE_4, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "5", .scancode = BSP_INPUT_SCANCODE_5, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "6", .scancode = BSP_INPUT_SCANCODE_6, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "7", .scancode = BSP_INPUT_SCANCODE_7, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "8", .scancode = BSP_INPUT_SCANCODE_8, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "9", .scancode = BSP_INPUT_SCANCODE_9, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "0", .scancode = BSP_INPUT_SCANCODE_0, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "-", .scancode = BSP_INPUT_SCANCODE_MINUS, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "=", .scancode = BSP_INPUT_SCANCODE_EQUAL, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "TAB", .scancode = BSP_INPUT_SCANCODE_TAB, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "Q", .scancode = BSP_INPUT_SCANCODE_Q, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "W", .scancode = BSP_INPUT_SCANCODE_W, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "E", .scancode = BSP_INPUT_SCANCODE_E, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "R", .scancode = BSP_INPUT_SCANCODE_R, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "T", .scancode = BSP_INPUT_SCANCODE_T, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "Y", .scancode = BSP_INPUT_SCANCODE_Y, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "U", .scancode = BSP_INPUT_SCANCODE_U, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "I", .scancode = BSP_INPUT_SCANCODE_I, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "O", .scancode = BSP_INPUT_SCANCODE_O, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "P", .scancode = BSP_INPUT_SCANCODE_P, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "[", .scancode = BSP_INPUT_SCANCODE_LEFTBRACE, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "]", .scancode = BSP_INPUT_SCANCODE_RIGHTBRACE, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "Fn", .scancode = BSP_INPUT_SCANCODE_FN, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "A", .scancode = BSP_INPUT_SCANCODE_A, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "S", .scancode = BSP_INPUT_SCANCODE_S, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "D", .scancode = BSP_INPUT_SCANCODE_D, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "F", .scancode = BSP_INPUT_SCANCODE_F, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "G", .scancode = BSP_INPUT_SCANCODE_G, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "H", .scancode = BSP_INPUT_SCANCODE_H, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "J", .scancode = BSP_INPUT_SCANCODE_J, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "K", .scancode = BSP_INPUT_SCANCODE_K, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "L", .scancode = BSP_INPUT_SCANCODE_L, .pressed = false, .was_pressed = false, .changed = true},
    {.name = ";", .scancode = BSP_INPUT_SCANCODE_SEMICOLON, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "'", .scancode = BSP_INPUT_SCANCODE_APOSTROPHE, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "ENTER", .scancode = BSP_INPUT_SCANCODE_ENTER, .pressed = false, .was_pressed = false, .changed = true},
    {.name        = "LSHIFT",
     .scancode    = BSP_INPUT_SCANCODE_LEFTSHIFT,
     .pressed     = false,
     .was_pressed = false,
     .changed     = true},
    {.name = "Z", .scancode = BSP_INPUT_SCANCODE_Z, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "X", .scancode = BSP_INPUT_SCANCODE_X, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "C", .scancode = BSP_INPUT_SCANCODE_C, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "V", .scancode = BSP_INPUT_SCANCODE_V, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "B", .scancode = BSP_INPUT_SCANCODE_B, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "N", .scancode = BSP_INPUT_SCANCODE_N, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "M", .scancode = BSP_INPUT_SCANCODE_M, .pressed = false, .was_pressed = false, .changed = true},
    {.name = ",", .scancode = BSP_INPUT_SCANCODE_COMMA, .pressed = false, .was_pressed = false, .changed = true},
    {.name = ".", .scancode = BSP_INPUT_SCANCODE_DOT, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "/", .scancode = BSP_INPUT_SCANCODE_SLASH, .pressed = false, .was_pressed = false, .changed = true},
    {.name        = "UP",
     .scancode    = BSP_INPUT_SCANCODE_ESCAPED_GREY_UP,
     .pressed     = false,
     .was_pressed = false,
     .changed     = true},
    {.name        = "RSHIFT",
     .scancode    = BSP_INPUT_SCANCODE_RIGHTSHIFT,
     .pressed     = false,
     .was_pressed = false,
     .changed     = true},
    {.name = "CTRL", .scancode = BSP_INPUT_SCANCODE_LEFTCTRL, .pressed = false, .was_pressed = false, .changed = true},
    {.name        = "META",
     .scancode    = BSP_INPUT_SCANCODE_ESCAPED_LEFTMETA,
     .pressed     = false,
     .was_pressed = false,
     .changed     = true},
    {.name = "ALT", .scancode = BSP_INPUT_SCANCODE_LEFTALT, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "\\", .scancode = BSP_INPUT_SCANCODE_BACKSLASH, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "S", .scancode = BSP_INPUT_SCANCODE_SPACE, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "P", .scancode = BSP_INPUT_SCANCODE_SPACE, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "A", .scancode = BSP_INPUT_SCANCODE_SPACE, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "C", .scancode = BSP_INPUT_SCANCODE_SPACE, .pressed = false, .was_pressed = false, .changed = true},
    {.name = "E", .scancode = BSP_INPUT_SCANCODE_SPACE, .pressed = false, .was_pressed = false, .changed = true},
    {.name        = "ALTGR",
     .scancode    = BSP_INPUT_SCANCODE_ESCAPED_RALT,
     .pressed     = false,
     .was_pressed = false,
     .changed     = true},
    {.name        = "LEFT",
     .scancode    = BSP_INPUT_SCANCODE_ESCAPED_GREY_LEFT,
     .pressed     = false,
     .was_pressed = false,
     .changed     = true},
    {.name        = "DOWN",
     .scancode    = BSP_INPUT_SCANCODE_ESCAPED_GREY_DOWN,
     .pressed     = false,
     .was_pressed = false,
     .changed     = true},
    {.name        = "RIGHT",
     .scancode    = BSP_INPUT_SCANCODE_ESCAPED_GREY_RIGHT,
     .pressed     = false,
     .was_pressed = false,
     .changed     = true},
};

static void draw_keyboard(void) {
    pax_buf_t* buffer = display_get_buffer();

    for (int column = 0; column < 8; column++) {
        uint32_t index = column;
        if (keys[index].changed) {
            keys[index].changed = false;
            uint32_t color      = keys[index].pressed ? 0xFFFFFF00 : keys[index].was_pressed ? 0xFFFFFFFF : 0xFF000000;
            float    x          = 10 + column * 95 + (column >= 4 ? 20 : 0);
            pax_simple_rect(buffer, color, x, 0, 95, 60);
            pax_outline_rect(buffer, 0xFF0000FF, x, 0, 95, 60);
            pax_center_text(buffer, (keys[index].pressed || keys[index].was_pressed) ? 0xFF000000 : 0xFFFFFFFF,
                            pax_font_sky_mono, 12, x + (95.0f / 2.0f), 30, keys[index].name);
        }
    }

    for (int row = 1; row < 6; row++) {
        for (int column = 0; column < 13; column++) {
            uint32_t index = 8 + column + (row - 1) * 13;
            if (keys[index].changed) {
                keys[index].changed = false;
                uint32_t color = keys[index].pressed ? 0xFFFFFF00 : keys[index].was_pressed ? 0xFFFFFFFF : 0xFF000000;
                float    x     = 10 + column * 60;
                float    y     = row * 60;
                pax_simple_rect(buffer, color, x, y, 60, 60);
                pax_outline_rect(buffer, 0xFF0000FF, x, y, 60, 60);
                pax_center_text(buffer, (keys[index].pressed || keys[index].was_pressed) ? 0xFF000000 : 0xFFFFFFFF,
                                pax_font_sky_mono, 12, x + (60.0f / 2.0f), y + 30, keys[index].name);
            }
        }
    }

    display_blit_buffer(buffer);
}

void test_keyboard(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    pax_buf_t* buffer = display_get_buffer();
    pax_background(buffer, 0xFF000000);
    pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 16, pax_buf_get_height(buffer) - 48,
                  "Press ESC and backspace simultaneously to exit");

    bool esc_pressed       = false;
    bool backspace_pressed = false;
    bool exiting           = false;

    for (uint32_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        keys[i].pressed     = false;
        keys[i].was_pressed = false;
        keys[i].changed     = true;
    }

    while (1) {
        bsp_input_event_t event;

        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (event.type == INPUT_EVENT_TYPE_SCANCODE) {
                bsp_input_scancode_t scancode = event.args_scancode.scancode & ~BSP_INPUT_SCANCODE_RELEASE_MODIFIER;
                for (uint32_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
                    if (keys[i].scancode == scancode) {
                        keys[i].was_pressed = keys[i].pressed;
                        keys[i].pressed     = !(event.args_scancode.scancode & BSP_INPUT_SCANCODE_RELEASE_MODIFIER);
                        keys[i].changed     = true;
                    }
                }
                if (scancode == BSP_INPUT_SCANCODE_ESC) {
                    esc_pressed = !(event.args_scancode.scancode & BSP_INPUT_SCANCODE_RELEASE_MODIFIER);
                } else if (scancode == BSP_INPUT_SCANCODE_BACKSPACE) {
                    backspace_pressed = !(event.args_scancode.scancode & BSP_INPUT_SCANCODE_RELEASE_MODIFIER);
                }
                if (esc_pressed && backspace_pressed) {
                    exiting = true;
                }
                if (!esc_pressed && !backspace_pressed && exiting) {
                    return;
                }
                draw_keyboard();
            }
        }
    }
}