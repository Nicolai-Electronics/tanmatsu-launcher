#include "nametag.h"
#include "bsp/input.h"
#include "common/display.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_codecs.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"
#include "bsp/led.h"

static const char* TAG = "nametag";

// #include "shapes/pax_misc.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT  ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT NULL, 0
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#endif

pax_buf_t nametag_pax_buf = {0};
// void*     nametag_buffer  = NULL;

static bool load_nametag(void) {
    /*nametag_buffer = heap_caps_calloc(1, 800 * 480 * 4, MALLOC_CAP_SPIRAM);
    if (nametag_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for nametag");
        return false;
    }*/
    FILE* fd = fopen("/sd/nametag.png", "rb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file");
        // free(nametag_buffer);
        // nametag_buffer = NULL;
        return false;
    }
    // pax_buf_init(&nametag_pax_buf, &nametag_buffer, 800, 480, PAX_BUF_32_8888ARGB);
    if (!pax_decode_png_fd(&nametag_pax_buf, fd, PAX_BUF_32_8888ARGB, 0)) {  // CODEC_FLAG_EXISTING)) {
        ESP_LOGE(TAG, "Failed to decode png file");
        // free(nametag_buffer);
        // nametag_buffer = NULL;
        return false;
    }
    fclose(fd);
    return true;
}

static void render_nametag(pax_buf_t* buffer) {
    pax_draw_image(buffer, &nametag_pax_buf, 0, 0);
    display_blit_buffer(buffer);
}

uint8_t led_buffer[6 * 3] = {0};

static void set_led_color(uint8_t led, uint32_t color) {
    led_buffer[led * 3 + 0] = (color >> 8) & 0xFF;  // G
    led_buffer[led * 3 + 1] = (color >> 16) & 0xFF; // R
    led_buffer[led * 3 + 2] = (color >> 0) & 0xFF;  // B
}

void menu_nametag(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    if (!load_nametag()) {
        return;
    }

    render_nametag(buffer);
    set_led_color(0, 0xFC0303);
    set_led_color(1, 0xFC6F03);
    set_led_color(2, 0xF4FC03);
    set_led_color(3, 0xFC03E3);
    set_led_color(4, 0x0303FC);
    set_led_color(5, 0x03FC03);
    bsp_led_write(led_buffer, sizeof(led_buffer));

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
                                // free(nametag_buffer);
                                // nametag_buffer = NULL;
                                pax_buf_destroy(&nametag_pax_buf);
                                set_led_color(0, 0x000000);
                                set_led_color(1, 0x000000);
                                set_led_color(2, 0x000000);
                                set_led_color(3, 0x000000);
                                set_led_color(4, 0x000000);
                                set_led_color(5, 0x000000);
                                bsp_led_write(led_buffer, sizeof(led_buffer));
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
        }
    }
}
