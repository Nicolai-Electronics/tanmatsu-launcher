// SPDX-License-Identifier: MIT
#include "test_led.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "bsp/input.h"
#include "bsp/led.h"
#include "common/display.h"
#include "common/theme.h"
#include "freertos/FreeRTOS.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/menu_helpers.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "pax_types.h"

#define TEXT_FONT pax_font_sky_mono
#define TEXT_SIZE 18

#define LED_PREVIEW_RADIUS  14
#define LED_PREVIEW_SPACING 40

typedef struct {
    char const* name;
    uint32_t    rgb;
} led_color_t;

static const led_color_t k_colors[] = {
    {"White", 0xFFFFFF},
    {"Red", 0xFF0000},
    {"Green", 0x00FF00},
    {"Blue", 0x0000FF},
};

static uint32_t s_led_count  = 0;
static size_t   s_color      = 0;      // index into k_colors
static bool     s_single     = false;  // only light up the LED at s_single_index
static uint32_t s_single_idx = 0;

static void update_leds(void) {
    for (uint32_t i = 0; i < s_led_count; i++) {
        bool on = !s_single || i == s_single_idx;
        bsp_led_set_pixel(i, on ? k_colors[s_color].rgb : 0x000000);
    }
    bsp_led_send();
}

static void draw_line(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, int line, char const* label,
                      char const* value) {
    char text_buffer[64];
    snprintf(text_buffer, sizeof(text_buffer), "%-9s %s", label, value);
    pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                  position.y0 + (TEXT_SIZE + 2) * line, text_buffer);
}

static void render(void) {
    pax_buf_t*   buffer   = display_get_buffer();
    gui_theme_t* theme    = get_theme();
    pax_vec2_t   position = menu_calc_position(buffer, theme);

    render_base_screen_statusbar(
        buffer, theme, true, true, true, ((gui_element_icontext_t[]){{get_icon(ICON_BUG_REPORT), "LED test"}}), 1,
        ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2,
        ((gui_element_icontext_t[]){{NULL, "1-4 Color | ← / → Single LED | ⏎ All LEDs"}}), 1);

    char text_buffer[64];
    int  line = 0;

    draw_line(buffer, theme, position, line++, "Color:", k_colors[s_color].name);
    if (s_single) {
        snprintf(text_buffer, sizeof(text_buffer), "Single LED (%lu of %lu)", (unsigned long)(s_single_idx + 1),
                 (unsigned long)s_led_count);
    } else {
        snprintf(text_buffer, sizeof(text_buffer), "All LEDs (%lu)", (unsigned long)s_led_count);
    }
    draw_line(buffer, theme, position, line++, "Mode:", text_buffer);

    // Preview of the LED state
    float y = position.y0 + (TEXT_SIZE + 2) * line + LED_PREVIEW_SPACING / 2 + 4;
    for (uint32_t i = 0; i < s_led_count; i++) {
        float x  = position.x0 + LED_PREVIEW_RADIUS + 2 + i * LED_PREVIEW_SPACING;
        bool  on = !s_single || i == s_single_idx;
        if (on) {
            pax_draw_circle(buffer, 0xFF000000 | k_colors[s_color].rgb, x, y, LED_PREVIEW_RADIUS);
        }
        pax_outline_circle(buffer, theme->palette.color_foreground, x, y, LED_PREVIEW_RADIUS);
    }
    line += 3;

    draw_line(buffer, theme, position, line++, "1", "All white");
    draw_line(buffer, theme, position, line++, "2 / 3 / 4", "Red / Green / Blue");
    draw_line(buffer, theme, position, line++, "← / →", "Light up a single LED and cycle");
    draw_line(buffer, theme, position, line++, "⏎", "Light up all LEDs");

    display_blit_buffer(buffer);
}

void test_led(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    s_led_count = 0;
    bsp_led_get_count(&s_led_count);
    s_color      = 0;
    s_single     = false;
    s_single_idx = 0;

    bsp_led_set_mode(false);
    update_leds();
    render();

    bool running = true;
    while (running) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        bool changed = false;
        switch (event.type) {
            case INPUT_EVENT_TYPE_NAVIGATION: {
                if (!event.args_navigation.state) {
                    break;
                }
                switch (event.args_navigation.key) {
                    case BSP_INPUT_NAVIGATION_KEY_ESC:
                    case BSP_INPUT_NAVIGATION_KEY_F1:
                    case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                        running = false;
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_LEFT:
                        if (s_led_count > 0) {
                            if (s_single) {
                                s_single_idx = (s_single_idx + s_led_count - 1) % s_led_count;
                            }
                            s_single = true;
                            changed  = true;
                        }
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                        if (s_led_count > 0) {
                            if (s_single) {
                                s_single_idx = (s_single_idx + 1) % s_led_count;
                            }
                            s_single = true;
                            changed  = true;
                        }
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_RETURN:
                    case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                    case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS:
                        s_single = false;
                        changed  = true;
                        break;
                    default:
                        break;
                }
                break;
            }
            case INPUT_EVENT_TYPE_KEYBOARD: {
                char key = event.args_keyboard.ascii;
                if (key == '1') {
                    // Back to the initial state: every LED lit up in white
                    s_color  = 0;
                    s_single = false;
                    changed  = true;
                } else if (key >= '2' && key <= '4') {
                    s_color = key - '1';
                    changed = true;
                }
                break;
            }
            default:
                break;
        }
        if (changed) {
            update_leds();
            render();
        }
    }

    bsp_led_clear();
    bsp_led_set_mode(true);
}
