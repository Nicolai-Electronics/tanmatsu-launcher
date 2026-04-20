#include "device_settings.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/led.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "lora.h"
#include "nvs_settings_hardware.h"
#include "nvs_settings_lora.h"

esp_err_t device_settings_apply(void) {
    uint8_t display_brightness = 100;
    nvs_settings_get_display_brightness(&display_brightness, DEFAULT_DISPLAY_BRIGHTNESS);
    bsp_display_set_backlight_brightness(display_brightness);

    uint8_t keyboard_brightness = 0;
    nvs_settings_get_keyboard_brightness(&keyboard_brightness, DEFAULT_KEYBOARD_BRIGHTNESS);
    bsp_input_set_backlight_brightness(keyboard_brightness);

    uint8_t led_brightness = 100;
    nvs_settings_get_led_brightness(&led_brightness, DEFAULT_LED_BRIGHTNESS);
    bsp_led_set_brightness(led_brightness);
    return ESP_OK;
}

void device_settings_get_default_http_user_agent(char* out_value, size_t max_length) {
    char                  device_name[64] = {0};
    const esp_app_desc_t* app_description = esp_app_get_description();
    bsp_device_get_name(device_name, sizeof(device_name));
    snprintf(out_value, max_length, "%s/%s", device_name, app_description->version);
}

esp_err_t device_settings_get_lora_frequency(uint32_t* out_value) {
    esp_err_t res = nvs_settings_get_lora_frequency(out_value, 0);
    if (res != ESP_OK || out_value == 0) {
        lora_protocol_status_params_t status = {0};
        lora_get_status(&status);
        *out_value = status.chip_type == LORA_PROTOCOL_CHIP_SX1268 ? 433875000 : 869618000;
    }
    return res;
}
