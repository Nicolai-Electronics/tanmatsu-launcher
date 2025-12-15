#include "test_keyboard_stuck_keys.h"
#include "bsp/input.h"
#include "freertos/idf_additions.h"
#include "icons.h"
#include "menu/message_dialog.h"

static const char* title = "Keyboard: stuck keys test";

bool test_keyboard_stuck_keys(char* result_buffer, size_t result_buffer_size) {
    busy_dialog(get_icon(ICON_SYSTEM_UPDATE), title, "Don't press any keys!", true);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Scan all navigation keys
    for (bsp_input_scancode_t i = BSP_INPUT_SCANCODE_NONE; i < BSP_INPUT_SCANCODE_ESCAPED_CALCULATOR; i++) {
        bool      state = false;
        esp_err_t res   = bsp_input_read_scancode(i, &state);
        if (res != ESP_OK) {
            snprintf(result_buffer, result_buffer_size, "Communication error");
            return false;
        }
        if (state) {
            snprintf(result_buffer, result_buffer_size, "Key 0x%04X is stuck!", i);
            return false;
        }
    }

    snprintf(result_buffer, result_buffer_size, "Test passed, no stuck keys detected.");
    return true;
}
