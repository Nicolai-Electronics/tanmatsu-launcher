#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/i2c.h"
#include "bsp/input.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

/* The Espressif Tab5 BSP and badge-bsp both use a bsp/ include namespace.
 * Keep their APIs isolated here and declare only the ABI used by this bridge. */
typedef struct {
    struct {
        mipi_dsi_phy_clock_source_t phy_clk_src;
        uint32_t lane_bit_rate_mbps;
    } dsi_bus;
} tab5_display_config_t;
typedef struct { void* dummy; } tab5_touch_config_t;

esp_err_t bsp_i2c_init(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);
esp_err_t bsp_display_new(const tab5_display_config_t* config, esp_lcd_panel_handle_t* panel,
                          esp_lcd_panel_io_handle_t* io);
esp_err_t bsp_display_brightness_set(int percentage);
esp_err_t bsp_display_brightness_init(void);
esp_err_t bsp_touch_new(const tab5_touch_config_t* config, esp_lcd_touch_handle_t* touch);
esp_err_t bsp_feature_enable(int feature, bool enable);

static const char* TAG = "tab5-bsp-compat";
static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t panel_io;
static esp_lcd_touch_handle_t touch;
static QueueHandle_t input_queue;
static SemaphoreHandle_t i2c_semaphore;
static uint8_t backlight = 100;

esp_err_t bsp_device_get_name(char* output, uint8_t length) {
    if (!output) return ESP_ERR_INVALID_ARG;
    strlcpy(output, "M5Stack Tab5", length);
    return ESP_OK;
}

esp_err_t bsp_device_get_manufacturer(char* output, uint8_t length) {
    if (!output) return ESP_ERR_INVALID_ARG;
    strlcpy(output, "M5Stack", length);
    return ESP_OK;
}

esp_err_t bsp_device_initialize_custom(void) {
    /* BSP_FEATURE_WIFI = 5. This powers the onboard ESP32-C6 before ESP-Hosted starts. */
    return bsp_feature_enable(5, true);
}

esp_err_t bsp_i2c_primary_bus_initialize(void) {
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "initialize Tab5 internal I2C");
    i2c_semaphore = xSemaphoreCreateMutex();
    return i2c_semaphore ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t bsp_i2c_primary_bus_get_handle(i2c_master_bus_handle_t* handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    *handle = bsp_i2c_get_handle();
    return *handle ? ESP_OK : ESP_FAIL;
}

esp_err_t bsp_i2c_primary_bus_get_semaphore(SemaphoreHandle_t* semaphore) {
    if (!semaphore) return ESP_ERR_INVALID_ARG;
    *semaphore = i2c_semaphore;
    return ESP_OK;
}

esp_err_t bsp_i2c_primary_bus_claim(void) {
    return xSemaphoreTake(i2c_semaphore, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t bsp_i2c_primary_bus_release(void) {
    return xSemaphoreGive(i2c_semaphore) == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t bsp_display_initialize(const bsp_display_configuration_t* configuration) {
    (void)configuration;
    const tab5_display_config_t config = {
        .dsi_bus = {.phy_clk_src = 0, .lane_bit_rate_mbps = 1000},
    };
    ESP_RETURN_ON_ERROR(bsp_display_new(&config, &panel, &panel_io), TAG, "initialize display");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG, "enable display");
    backlight = 100;
    return bsp_display_brightness_set(backlight);
}

esp_err_t bsp_display_get_parameters(size_t* h, size_t* v, bsp_display_color_format_t* format,
                                     bsp_display_endianness_t* endian) {
    if (!panel) return ESP_FAIL;
    if (h) *h = 720;
    if (v) *v = 1280;
    if (format) *format = BSP_DISPLAY_COLOR_FORMAT_16_565RGB;
    if (endian) *endian = BSP_DISPLAY_ENDIAN_LITTLE;
    return ESP_OK;
}

esp_err_t bsp_display_get_panel(esp_lcd_panel_handle_t* output) {
    if (!output || !panel) return ESP_ERR_INVALID_ARG;
    *output = panel;
    return ESP_OK;
}

esp_err_t bsp_display_get_panel_io(esp_lcd_panel_io_handle_t* output) {
    if (!output || !panel) return ESP_ERR_INVALID_ARG;
    *output = panel_io;
    return ESP_OK;
}

bsp_display_rotation_t bsp_display_get_default_rotation(void) { return BSP_DISPLAY_ROTATION_90; }

esp_err_t bsp_display_blit(size_t x, size_t y, size_t width, size_t height, const void* buffer) {
    if (!panel || !buffer) return ESP_ERR_INVALID_ARG;
    return esp_lcd_panel_draw_bitmap(panel, x, y, width, height, buffer);
}

esp_err_t bsp_display_get_backlight_brightness(uint8_t* percentage) {
    if (!percentage) return ESP_ERR_INVALID_ARG;
    *percentage = backlight;
    return ESP_OK;
}

esp_err_t bsp_display_set_backlight_brightness(uint8_t percentage) {
    if (percentage > 100) return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(bsp_display_brightness_set(percentage), TAG, "set backlight");
    backlight = percentage;
    return ESP_OK;
}

esp_err_t bsp_input_initialize(void) {
    input_queue = xQueueCreate(32, sizeof(bsp_input_event_t));
    ESP_RETURN_ON_FALSE(input_queue, ESP_ERR_NO_MEM, TAG, "create input queue");
    const tab5_touch_config_t config = {0};
    return bsp_touch_new(&config, &touch);
}

esp_err_t bsp_input_get_queue(QueueHandle_t* output) {
    if (!output || !input_queue) return ESP_ERR_INVALID_ARG;
    *output = input_queue;
    return ESP_OK;
}

esp_err_t bsp_input_inject_event(bsp_input_event_t* event) {
    if (!event || !input_queue) return ESP_ERR_INVALID_ARG;
    return xQueueSend(input_queue, event, pdMS_TO_TICKS(10)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

bool bsp_input_needs_on_screen_keyboard(void) { return false; }

esp_err_t bsp_input_read_navigation_key(bsp_input_navigation_key_t key, bool* state) {
    (void)key;
    if (!state) return ESP_ERR_INVALID_ARG;
    *state = false;
    return ESP_OK;
}

esp_err_t bsp_input_read_action(bsp_input_action_type_t action, bool* state) {
    (void)action;
    if (!state) return ESP_ERR_INVALID_ARG;
    *state = false;
    return ESP_OK;
}

esp_err_t bsp_input_get_touch_coordinates(uint16_t* x, uint16_t* y, uint16_t* strength, uint8_t* count,
                                          uint8_t max_count) {
    if (!touch) return ESP_FAIL;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_read_data(touch), TAG, "read touch");
    esp_lcd_touch_get_coordinates(touch, x, y, strength, count, max_count);
    return ESP_OK;
}
