// SPDX-License-Identifier: MIT

#include "audio_mixer.h"
#include <stdint.h>
#include <string.h>
#include "bsp/audio.h"
#include "bsp/input.h"
#include "driver/i2s_common.h"
#include "driver/i2s_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

static char const* TAG = "audio_mixer";

#define MIXER_FRAME_BYTES   4                         // s16 stereo
#define MIXER_CHUNK_FRAMES  256                       // ~5.8 ms @ 44.1 kHz
#define MIXER_CHUNK_SAMPLES (MIXER_CHUNK_FRAMES * 2)  // L+R
#define MIXER_CHUNK_BYTES   (MIXER_CHUNK_FRAMES * MIXER_FRAME_BYTES)
#define MIXER_STREAM_BYTES  (8 * 1024)  // ~46 ms of headroom per plugin

#define MIXER_TASK_STACK    3072
#define MIXER_TASK_PRIORITY 7  // above plugin tasks (which run at 5)

// After every stream has gone silent we keep the I2S channel running and
// feed it silence chunks for a short drain window before disabling the
// hardware, so the last bit of audio sitting in the DMA queue actually
// plays out instead of being chopped off. The default I2S DMA queue is
// 6 descriptors x 240 frames = 1440 frames (~33 ms @ 44.1 kHz); we
// over-shoot to leave headroom for jitter and codec latency.
#define MIXER_DRAIN_CHUNKS 8  // 8 * 256 frames ≈ 46 ms @ 44.1 kHz

typedef struct {
    bool                 active;  // slot is allocated to a plugin
    bool                 paused;  // stream is paused (asp_audio_stop)
    TaskHandle_t         owner;
    StreamBufferHandle_t buf;
} mixer_stream_t;

static mixer_stream_t    g_streams[AUDIO_MIXER_MAX_STREAMS];
static SemaphoreHandle_t g_streams_mutex = NULL;
static i2s_chan_handle_t g_i2s           = NULL;
static TaskHandle_t      g_mixer_task    = NULL;
static bool              g_initialized   = false;
// I2S/amplifier are enabled by bsp_audio_initialize before the mixer task
// starts, so the initial state is "powered on". The mixer task drops it
// to "powered off" after its first idle drain pass. Only the mixer task
// reads or writes this, so no synchronization is needed.
static bool              g_powered_on = true;

// Scratch buffers for the mixer task. Static to keep them out of the task stack.
static int16_t g_in_buf[MIXER_CHUNK_SAMPLES];
static int32_t g_accum[MIXER_CHUNK_SAMPLES];
static int16_t g_out_buf[MIXER_CHUNK_SAMPLES];

// Re-enable the I2S DMA channel and turn the speaker amplifier back on
// before resuming mixing. The amplifier follows the current jack state so
// we don't drive the speaker while headphones are connected. Only called
// from the mixer task.
static void power_up(void) {
    if (g_powered_on) return;

    esp_err_t err = i2s_channel_enable(g_i2s);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
    }

    bool jack_inserted = false;
    if (bsp_input_read_action(BSP_INPUT_ACTION_TYPE_AUDIO_JACK, &jack_inserted) != ESP_OK) {
        // Couldn't read jack state — default to amp-on, matches the original
        // behaviour for boards that don't report jack state reliably.
        jack_inserted = false;
    }
    bsp_audio_set_amplifier(!jack_inserted);

    g_powered_on = true;
    ESP_LOGD(TAG, "Audio output powered up (jack=%d)", jack_inserted ? 1 : 0);
}

// Mute the amplifier first (avoids driving residual DMA samples into the
// speaker as a click) and then disable the I2S DMA channel. The mixer
// task only calls this once it has fed the DMA queue enough silence to
// flush any real audio that might still be in flight. Only called from
// the mixer task.
static void power_down(void) {
    if (!g_powered_on) return;

    bsp_audio_set_amplifier(false);

    esp_err_t err = i2s_channel_disable(g_i2s);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "i2s_channel_disable failed: %s", esp_err_to_name(err));
    }

    g_powered_on = false;
    ESP_LOGD(TAG, "Audio output powered down (idle)");
}

static void mixer_task_fn(void* arg) {
    (void)arg;
    // Number of consecutive silent chunks pushed into the DMA queue since
    // the last real audio sample. Once this reaches MIXER_DRAIN_CHUNKS the
    // hardware DMA queue is guaranteed to contain only silence, so it's
    // safe to power the amp/I2S down without cutting off the tail.
    int silence_chunks = 0;

    while (1) {
        memset(g_accum, 0, sizeof(g_accum));

        // Sum samples from every stream that produced data this chunk and
        // count how many sources contributed, so we can divide the mix down
        // to avoid clipping when multiple plugins play simultaneously.
        int active_count = 0;
        xSemaphoreTake(g_streams_mutex, portMAX_DELAY);
        for (int i = 0; i < AUDIO_MIXER_MAX_STREAMS; i++) {
            if (!g_streams[i].active || g_streams[i].paused || g_streams[i].buf == NULL) continue;
            size_t got = xStreamBufferReceive(g_streams[i].buf, g_in_buf, sizeof(g_in_buf), 0);
            if (got == 0) continue;
            active_count++;
            size_t n = got / sizeof(int16_t);
            for (size_t j = 0; j < n; j++) {
                g_accum[j] += g_in_buf[j];
            }
        }
        xSemaphoreGive(g_streams_mutex);

        if (active_count == 0) {
            // No producer had samples this round. If we're still powered
            // up, keep the I2S clock alive and push silence chunks into the
            // DMA queue until any previously-mixed audio has fully drained;
            // only then is it safe to disable the amp/I2S without chopping
            // off the tail of the last sound.
            if (!g_powered_on) {
                // Already idle: just block until a producer wakes us.
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                continue;
            }
            if (silence_chunks < MIXER_DRAIN_CHUNKS) {
                memset(g_out_buf, 0, sizeof(g_out_buf));
                size_t written = 0;
                i2s_channel_write(g_i2s, g_out_buf, sizeof(g_out_buf), &written, portMAX_DELAY);
                silence_chunks++;
                continue;
            }
            // DMA queue has been fully overwritten with silence — power
            // down and sleep until a producer pokes us via audio_mixer_write.
            power_down();
            silence_chunks = 0;
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }

        // Real audio this round — bring the hardware back up if we drifted
        // off, and reset the drain counter so a single late sample doesn't
        // immediately let us power down on the next iteration.
        if (!g_powered_on) {
            power_up();
        }
        silence_chunks = 0;

        // Per-source volume = total / N. Saturate as a safety net in case
        // a single source is already at the int16 boundary.
        int divisor = active_count;
        for (size_t j = 0; j < MIXER_CHUNK_SAMPLES; j++) {
            int32_t s = g_accum[j] / divisor;
            if (s > INT16_MAX)
                s = INT16_MAX;
            else if (s < INT16_MIN)
                s = INT16_MIN;
            g_out_buf[j] = (int16_t)s;
        }

        size_t written = 0;
        // Blocks until DMA has room — paces the whole mixer to the I2S rate.
        i2s_channel_write(g_i2s, g_out_buf, sizeof(g_out_buf), &written, portMAX_DELAY);
    }
}

esp_err_t audio_mixer_init(void) {
    if (g_initialized) return ESP_OK;

    esp_err_t err = bsp_audio_get_i2s_handle(&g_i2s);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get I2S handle: %d", err);
        return err;
    }
    if (g_i2s == NULL) {
        ESP_LOGE(TAG, "BSP returned NULL I2S handle; audio not initialized?");
        return ESP_ERR_INVALID_STATE;
    }

    g_streams_mutex = xSemaphoreCreateMutex();
    if (g_streams_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok =
        xTaskCreate(mixer_task_fn, "audio_mixer", MIXER_TASK_STACK, NULL, MIXER_TASK_PRIORITY, &g_mixer_task);
    if (ok != pdPASS) {
        vSemaphoreDelete(g_streams_mutex);
        g_streams_mutex = NULL;
        ESP_LOGE(TAG, "Failed to create mixer task");
        return ESP_ERR_NO_MEM;
    }

    g_initialized = true;
    ESP_LOGI(TAG, "Audio mixer started (chunk=%d frames, %d streams max)", MIXER_CHUNK_FRAMES, AUDIO_MIXER_MAX_STREAMS);
    return ESP_OK;
}

// Find an existing slot for `task`. Caller must hold g_streams_mutex.
// Returns slot index or -1.
static int find_slot_locked(TaskHandle_t task) {
    for (int i = 0; i < AUDIO_MIXER_MAX_STREAMS; i++) {
        if (g_streams[i].active && g_streams[i].owner == task) return i;
    }
    return -1;
}

// Free any slots whose owner task no longer exists. Caller must hold the mutex.
static void sweep_dead_locked(void) {
    for (int i = 0; i < AUDIO_MIXER_MAX_STREAMS; i++) {
        if (!g_streams[i].active) continue;
        eTaskState s = eTaskGetState(g_streams[i].owner);
        if (s == eDeleted || s == eInvalid) {
            vStreamBufferDelete(g_streams[i].buf);
            g_streams[i].active = false;
            g_streams[i].owner  = NULL;
            g_streams[i].buf    = NULL;
            g_streams[i].paused = false;
        }
    }
}

// Allocate a fresh slot for `task`. Caller must hold the mutex.
// Returns slot index or -1 on out-of-slots / out-of-memory.
static int alloc_slot_locked(TaskHandle_t task) {
    for (int i = 0; i < AUDIO_MIXER_MAX_STREAMS; i++) {
        if (!g_streams[i].active) {
            StreamBufferHandle_t buf = xStreamBufferCreate(MIXER_STREAM_BYTES, MIXER_FRAME_BYTES);
            if (buf == NULL) {
                ESP_LOGE(TAG, "Failed to allocate stream buffer for task %p", task);
                return -1;
            }
            g_streams[i].buf    = buf;
            g_streams[i].owner  = task;
            g_streams[i].paused = false;
            g_streams[i].active = true;
            ESP_LOGD(TAG, "Auto-registered audio stream %d for task %p", i, task);
            return i;
        }
    }
    return -1;
}

// Find existing slot or allocate one (sweeping dead slots first if full).
// Caller must hold the mutex.
static int find_or_alloc_slot_locked(TaskHandle_t task) {
    int idx = find_slot_locked(task);
    if (idx >= 0) return idx;
    idx = alloc_slot_locked(task);
    if (idx >= 0) return idx;
    sweep_dead_locked();
    return alloc_slot_locked(task);
}

// Public API: pre-registration is now optional — writes auto-register.
// Kept for backwards compatibility; returns true if a slot exists or was
// freshly allocated.
bool audio_mixer_register_stream(TaskHandle_t task) {
    if (!g_initialized || task == NULL) return false;
    xSemaphoreTake(g_streams_mutex, portMAX_DELAY);
    int idx = find_or_alloc_slot_locked(task);
    xSemaphoreGive(g_streams_mutex);
    return idx >= 0;
}

// Garbage-collect any slots whose owner task has been deleted. The `task`
// argument is ignored — kept in the signature for source compatibility.
void audio_mixer_unregister_stream(TaskHandle_t task) {
    (void)task;
    if (!g_initialized) return;
    xSemaphoreTake(g_streams_mutex, portMAX_DELAY);
    sweep_dead_locked();
    xSemaphoreGive(g_streams_mutex);
}

size_t audio_mixer_write(TaskHandle_t task, const void* samples, size_t size_bytes, int64_t timeout_ms) {
    if (!g_initialized || task == NULL || samples == NULL || size_bytes == 0) return 0;

    StreamBufferHandle_t buf = NULL;

    xSemaphoreTake(g_streams_mutex, portMAX_DELAY);
    int idx = find_or_alloc_slot_locked(task);
    if (idx >= 0 && !g_streams[idx].paused) {
        buf = g_streams[idx].buf;
    }
    xSemaphoreGive(g_streams_mutex);

    if (buf == NULL) return 0;

    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    size_t     sent  = xStreamBufferSend(buf, samples, size_bytes, ticks);

    // If the mixer is currently parked in ulTaskNotifyTake (idle, hardware
    // powered down) it won't notice the new samples until something prods
    // it. Notify on every successful send — it's cheap, and the mixer task
    // ignores spurious wake-ups since it always re-checks the streams.
    if (sent > 0 && g_mixer_task != NULL) {
        xTaskNotifyGive(g_mixer_task);
    }
    return sent;
}

bool audio_mixer_start(TaskHandle_t task) {
    if (!g_initialized || task == NULL) return false;
    bool ok = false;
    xSemaphoreTake(g_streams_mutex, portMAX_DELAY);
    int idx = find_or_alloc_slot_locked(task);
    if (idx >= 0) {
        g_streams[idx].paused = false;
        ok                    = true;
    }
    xSemaphoreGive(g_streams_mutex);
    return ok;
}

bool audio_mixer_stop(TaskHandle_t task) {
    if (!g_initialized || task == NULL) return false;

    StreamBufferHandle_t buf = NULL;
    xSemaphoreTake(g_streams_mutex, portMAX_DELAY);
    int idx = find_slot_locked(task);
    if (idx >= 0) {
        g_streams[idx].paused = true;
        buf                   = g_streams[idx].buf;
    }
    xSemaphoreGive(g_streams_mutex);

    // Reset outside the mutex; producer is the calling task itself, so it
    // can't be blocked on send while also calling stop.
    if (buf != NULL) {
        xStreamBufferReset(buf);
    }
    return true;
}
