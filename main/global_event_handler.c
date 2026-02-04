#include "global_event_handler.h"
#include <stdbool.h>
#include <stdint.h>
#include "bsp/audio.h"
#include "bsp/input.h"
#include "esp_err.h"
#include "sdcard.h"

static const char TAG[]              = "Event";
static int        input_hook_id      = -1;
static bool       power_button_latch = false;

static void handle_sdcard(bool inserted) {
    if (inserted) {
        ESP_LOGI(TAG, "SD card inserted");
        sd_mount();
    } else {
        ESP_LOGI(TAG, "SD card removed");
        sd_unmount();
    }
}

static void handle_audiojack(bool inserted) {
    ESP_LOGI(TAG, "Audio jack %s", inserted ? "inserted" : "removed");
    bsp_audio_set_amplifier(!inserted);
}

static void handle_volume(bool up, bool state) {
    ESP_LOGI(TAG, "Audio volume %s %s", up ? "up" : "down", state ? "pressed" : "released");
    if (state) {
        float percentage = 0;
        bsp_audio_get_volume(&percentage);
        if (up) {
            if (percentage >= 1.0f) percentage -= 1.0f;
        } else {
            if (percentage <= 99.0f) percentage += 1.0f;
        }
        bsp_audio_set_volume(percentage);
        ESP_LOGI(TAG, "Audio volume set to %.0f%%", percentage);
    }
}

static bool input_hook_callback(bsp_input_event_t* event, void* user_data) {
    if (event->type == INPUT_EVENT_TYPE_ACTION) {
        // Handle all action events
        switch (event->args_action.type) {
            case BSP_INPUT_ACTION_TYPE_POWER_BUTTON:
                if (event->args_action.state) {
                    power_button_latch = true;
                } else if (power_button_latch) {
                    power_button_latch = false;
                    // Trigger standby mode here
                }
                // For now don't handle this
                return false;  // Temporary
                break;
            case BSP_INPUT_ACTION_TYPE_SD_CARD:
                handle_sdcard(event->args_action.state);
                break;
            case BSP_INPUT_ACTION_TYPE_AUDIO_JACK:
                handle_audiojack(event->args_action.state);
                break;
            default:
                break;
        }
        return true;
    } else if (event->type == INPUT_EVENT_TYPE_NAVIGATION) {
        if (event->args_navigation.key == BSP_INPUT_NAVIGATION_KEY_VOLUME_UP) {
            handle_volume(true, event->args_navigation.state);
            return true;
        }
        if (event->args_navigation.key == BSP_INPUT_NAVIGATION_KEY_VOLUME_DOWN) {
            handle_volume(false, event->args_navigation.state);
            return true;
        }
    }
    return false;
}

esp_err_t global_event_handler_initialize(void) {
    input_hook_id = bsp_input_hook_register(input_hook_callback, NULL);
    if (input_hook_id < 0) {
        return ESP_FAIL;
    }

    // Initialize SD card
    bool      sdcard_inserted = false;
    esp_err_t res             = bsp_input_read_action(BSP_INPUT_ACTION_TYPE_SD_CARD, &sdcard_inserted);
    if (res == ESP_OK) {
        handle_sdcard(sdcard_inserted);
    } else {
        ESP_LOGE(TAG, "Failed to read SD card event (%s)", esp_err_to_name(res));
    }

    // Initialize audio jack
    bool audiojack_inserted = false;
    bsp_input_read_action(BSP_INPUT_ACTION_TYPE_AUDIO_JACK, &audiojack_inserted);
    if (res == ESP_OK) {
        handle_audiojack(audiojack_inserted);
    } else {
        ESP_LOGE(TAG, "Failed to read audio jack event (%s)", esp_err_to_name(res));
    }

    return ESP_OK;
}
