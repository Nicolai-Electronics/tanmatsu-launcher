#include <string.h>
#include "bsp/device.h"
#include "bsp/i2c.h"
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "esp_log.h"
#include "gui_style.h"
#include "icons.h"
#include "lora_information.h"
#include "menu/menu_helpers.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"
#include "scd4x.h"

static const char TAG[] = "Sensors menu";

static scd4x_handle_t scd4x_handle      = {0};
static bool           scd4x_initialized = false;
static uint16_t       scd4x_co2         = 0;
static float          scd4x_temperature = 0;
static float          scd4x_humidity    = 0;

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    18
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{NULL, "🅱 Back"}}), 1
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#endif

static void render(void) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    pax_vec2_t position = menu_calc_position(buffer, theme);

    render_base_screen_statusbar(buffer, theme, true, true, true,
                                 ((gui_element_icontext_t[]){{get_icon(ICON_INFO), "Sensors"}}), 1, FOOTER_LEFT,
                                 FOOTER_RIGHT);

    char text_buffer[256];
    snprintf(text_buffer, sizeof(text_buffer), "%04" PRIu16, scd4x_co2);
    pax_vec2f co2_text_size = pax_text_size(TEXT_FONT, 64, text_buffer);

    pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, 18, position.x0, position.y0, "CO2");
    pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, 64, position.x0 - 4, position.y0 + 20,
                  text_buffer);
    pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, 18, position.x0 + co2_text_size.x,
                  position.y0 + 64, "PPM");

    snprintf(text_buffer, sizeof(text_buffer), "%.2f *c %.2f %%", scd4x_temperature, scd4x_humidity);
    pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, 18, position.x0, position.y0 + 80, text_buffer);

    display_blit_buffer(buffer);
}

void menu_sensors(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    if (!scd4x_initialized) {
        i2c_master_bus_handle_t i2c_bus_handle_internal;
        esp_err_t               res = bsp_i2c_primary_bus_get_handle(&i2c_bus_handle_internal);
        if (res == ESP_OK) {
            res = scd4x_initialize(&scd4x_handle, i2c_bus_handle_internal);
            if (res == ESP_OK) {
                scd4x_initialized = true;
                ESP_LOGI(TAG, "SCD4X initialized");
            } else {
                ESP_LOGW(TAG, "SCD4X failed to initialize");
            }
        }
    }

    if (scd4x_initialized) {
        if (scd4x_start_periodic_measurement(&scd4x_handle) == ESP_OK) {
            ESP_LOGI(TAG, "SCD4X periodic measurement started");
        } else {
            ESP_LOGW(TAG, "SCD4X failed to start periodic measurement");
        }
    }

    while (1) {
        if (scd4x_initialized) {
            bool      scd4x_data_ready = false;
            esp_err_t res              = scd4x_get_data_ready_status(&scd4x_handle, &scd4x_data_ready);
            if (res != ESP_OK) {
                ESP_LOGW(TAG, "SCD4X failed to read data ready status");
            } else if (!scd4x_data_ready) {
                ESP_LOGI(TAG, "SCD4X no data ready");
            } else {
                res = scd4x_read_measurement(&scd4x_handle, &scd4x_co2, &scd4x_temperature, &scd4x_humidity);
                ESP_LOGI(TAG,
                         "SCD4X measurement: CO2 = %" PRIu16 " ppm, temperature = %.2f Celsius, humidity = %.2f %%",
                         scd4x_co2, scd4x_temperature, scd4x_humidity);
            }
        }

        render();

        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
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

    if (scd4x_initialized) {
        if (scd4x_stop_periodic_measurement(&scd4x_handle) == ESP_OK) {
            ESP_LOGI(TAG, "SCD4X periodic measurement stopped");
        } else {
            ESP_LOGW(TAG, "SCD4X failed to stop periodic measurement");
        }
    }
}
