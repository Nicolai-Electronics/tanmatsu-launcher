#include "global_event_handler.h"
#include <stdbool.h>
#include <stdint.h>
#include "bsp/audio.h"
#include "bsp/input.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_settings_hardware.h"
#include "sdcard.h"

static const char TAG[]                  = "Event";
static int        input_hook_id          = -1;
static bool       power_button_latch     = false;
static bool       headphones_inserted    = false;

#define VOLUME_DEFAULT_PERCENT 50
#define VOLUME_STEP_PERCENT     5

static uint8_t get_active_volume(void) {
    uint8_t v = VOLUME_DEFAULT_PERCENT;
    if (headphones_inserted) {
        nvs_settings_get_headphone_volume(&v, VOLUME_DEFAULT_PERCENT);
    } else {
        nvs_settings_get_speaker_volume(&v, VOLUME_DEFAULT_PERCENT);
    }
    return v;
}

static void set_active_volume(uint8_t percent) {
    if (percent > 100) percent = 100;
    if (headphones_inserted) {
        nvs_settings_set_headphone_volume(percent);
    } else {
        nvs_settings_set_speaker_volume(percent);
    }
    bsp_audio_set_volume((float)percent);
}

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
    headphones_inserted = inserted;
    bsp_audio_set_amplifier(!inserted);
    // Re-apply the per-output volume since speaker and headphone settings
    // are stored separately.
    bsp_audio_set_volume((float)get_active_volume());
}

static void handle_volume(bool up, bool state) {
    ESP_LOGI(TAG, "Audio volume %s %s", up ? "up" : "down", state ? "pressed" : "released");
    if (!state) return;

    int next = (int)get_active_volume() + (up ? VOLUME_STEP_PERCENT : -VOLUME_STEP_PERCENT);
    if (next < 0) next = 0;
    if (next > 100) next = 100;
    set_active_volume((uint8_t)next);
    ESP_LOGI(TAG, "Audio volume set to %d%%", next);
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

    // Initialize audio jack (and apply the appropriate persisted volume)
    bool audiojack_inserted = false;
    res                     = bsp_input_read_action(BSP_INPUT_ACTION_TYPE_AUDIO_JACK, &audiojack_inserted);
    if (res == ESP_OK) {
        handle_audiojack(audiojack_inserted);
    } else {
        ESP_LOGE(TAG, "Failed to read audio jack event (%s)", esp_err_to_name(res));
        // Apply default volume anyway so audio isn't silent at boot.
        handle_audiojack(false);
    }

    return ESP_OK;
}
