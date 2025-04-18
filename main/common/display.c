#include "common/display.h"
#include "bsp/display.h"
#include "esp_err.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "pax_gfx.h"

#define DSI_PANEL

static esp_lcd_panel_handle_t       display_lcd_panel    = NULL;
static esp_lcd_panel_io_handle_t    display_lcd_panel_io = NULL;
static size_t                       display_h_res        = 0;
static size_t                       display_v_res        = 0;
static lcd_color_rgb_pixel_format_t display_color_format;
static pax_buf_t                    fb = {0};

SemaphoreHandle_t display_semaphore = NULL;

IRAM_ATTR static bool notify_display_flush_ready(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t* edata,
                                                 void* user_ctx) {
    xSemaphoreGiveFromISR(display_semaphore, NULL);
    return false;
}

void display_init(void) {
    vSemaphoreCreateBinary(display_semaphore);
    xSemaphoreGive(display_semaphore);

    ESP_ERROR_CHECK(bsp_display_get_panel(&display_lcd_panel));
    ESP_ERROR_CHECK(bsp_display_get_panel_io(&display_lcd_panel_io));
    ESP_ERROR_CHECK(bsp_display_get_parameters(&display_h_res, &display_v_res, &display_color_format));

    pax_buf_init(&fb, NULL, display_h_res, display_v_res, PAX_BUF_16_565RGB);
    pax_buf_reversed(&fb, false);
    pax_buf_set_orientation(&fb, PAX_O_ROT_CW);

#ifdef DSI_PANEL
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = notify_display_flush_ready,
    };

    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(display_lcd_panel, &cbs, NULL));
#else
    if (lcd_panel_io_handle) {
        esp_lcd_panel_io_callbacks_t cbs = {
            .on_color_trans_done = notify_display_flush_ready,
        };
        ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(lcd_panel_io_handle, &cbs, display));
    }
#endif
}

pax_buf_t* display_get_buffer(void) {
    return &fb;
}

void display_blit_buffer(pax_buf_t* fb) {
    xSemaphoreTake(display_semaphore, portMAX_DELAY);
    size_t display_h_res, display_v_res;
    ESP_ERROR_CHECK(bsp_display_get_parameters(&display_h_res, &display_v_res, NULL));
    ESP_ERROR_CHECK(
        esp_lcd_panel_draw_bitmap(display_lcd_panel, 0, 0, display_h_res, display_v_res, pax_buf_get_pixels(fb)));
}

void display_blit(void) {
    display_blit_buffer(&fb);
}