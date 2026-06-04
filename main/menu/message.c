#include "message.h"
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "gui_menu.h"
#include "icons.h"
#include "menu/about.h"
#include "menu/menu_helpers.h"
#include "message_dialog.h"
#include "pax_gfx.h"
#include "pax_types.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Close"}}), 2
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    18
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{NULL, "🅱 Close"}}), 1
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#else
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{NULL, "🅱 Close"}}), 1
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#endif

static void render(pax_buf_t* icon, char* title, char* message) {
    pax_buf_t*   buffer   = display_get_buffer();
    gui_theme_t* theme    = get_theme();
    pax_vec2_t   position = menu_calc_position(buffer, theme);
    render_base_screen_statusbar(buffer, theme, true, true, true, ((gui_element_icontext_t[]){{icon, title}}), 1,
                                 FOOTER_LEFT, FOOTER_RIGHT);
    pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0, position.y0, message);
    display_blit_buffer(buffer);
}

void message_screen(pax_buf_t* icon, char* title, char* message) {
    render(icon, title, message);

    QueueHandle_t input_event_queue = NULL;
    if (bsp_input_get_queue(&input_event_queue) != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS:
                                return;
                            default:
                                break;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        } else {
            render(icon, title, message);
        }
    }
}