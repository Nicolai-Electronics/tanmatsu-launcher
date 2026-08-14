#include "tab5_keyboard.h"

#ifdef CONFIG_TANMATSU_LAUNCHER_M5STACK_TAB5

#include <string.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hid_keyboard.h"

#define TAB5_KBD_ADDRESS       0x6d
#define TAB5_KBD_SDA           GPIO_NUM_0
#define TAB5_KBD_SCL           GPIO_NUM_1
#define TAB5_KBD_INTERRUPT     GPIO_NUM_50
#define TAB5_KBD_REG_INT_CFG   0x00
#define TAB5_KBD_REG_EVENT_NUM 0x02
#define TAB5_KBD_REG_MODE      0x10
#define TAB5_KBD_REG_HID_EVENT 0x30
#define TAB5_KBD_MODE_HID      1
#define TAB5_KBD_INT_HID       (1U << 1)

static const char* TAG = "tab5-keyboard";
static i2c_master_dev_handle_t keyboard;

static esp_err_t write_register(uint8_t reg, uint8_t value) {
    uint8_t data[] = {reg, value};
    return i2c_master_transmit(keyboard, data, sizeof(data), 100);
}

static esp_err_t read_register(uint8_t reg, void* data, size_t length) {
    return i2c_master_transmit_receive(keyboard, &reg, 1, data, length, 100);
}

static void keyboard_task(void* context) {
    (void)context;
    uint8_t active_modifier = 0;
    uint8_t active_key = 0;
    while (true) {
        if (gpio_get_level(TAB5_KBD_INTERRUPT) == 0) {
            uint8_t count = 0;
            if (read_register(TAB5_KBD_REG_EVENT_NUM, &count, 1) == ESP_OK) {
                while (count--) {
                    uint8_t event[2] = {0xff, 0xff};
                    if (read_register(TAB5_KBD_REG_HID_EVENT, event, sizeof(event)) != ESP_OK ||
                        (event[0] == 0xff && event[1] == 0xff)) {
                        break;
                    }
                    uint8_t keys[6] = {0};
                    if (event[1] != 0) {
                        active_modifier = event[0];
                        active_key = event[1];
                        keys[0] = active_key;
                    } else {
                        active_modifier = 0;
                        active_key = 0;
                    }
                    hid_kbd_process_report(active_modifier, keys);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

esp_err_t tab5_keyboard_init(void) {
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = TAB5_KBD_SDA,
        .scl_io_num = TAB5_KBD_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), TAG, "create Ext.Port1 I2C bus");
    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TAB5_KBD_ADDRESS,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &device_config, &keyboard), TAG, "add A164");
    ESP_RETURN_ON_ERROR(i2c_master_probe(bus, TAB5_KBD_ADDRESS, 100), TAG, "probe A164");
    ESP_RETURN_ON_ERROR(write_register(TAB5_KBD_REG_MODE, TAB5_KBD_MODE_HID), TAG, "select HID mode");
    ESP_RETURN_ON_ERROR(write_register(TAB5_KBD_REG_INT_CFG, TAB5_KBD_INT_HID), TAG, "enable HID interrupt");

    gpio_config_t interrupt_config = {
        .pin_bit_mask = 1ULL << TAB5_KBD_INTERRUPT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&interrupt_config), TAG, "configure interrupt");
    ESP_RETURN_ON_FALSE(xTaskCreate(keyboard_task, TAG, 4096, NULL, 8, NULL) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "start keyboard task");
    ESP_LOGI(TAG, "A164 keyboard initialized in HID mode");
    return ESP_OK;
}

#else

esp_err_t tab5_keyboard_init(void) { return ESP_ERR_NOT_SUPPORTED; }

#endif
