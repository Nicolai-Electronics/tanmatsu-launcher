// SPDX-License-Identifier: MIT

#include "audio_mixer.h"

#include <stdint.h>
#include <string.h>

#include "bsp/audio.h"
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

// Scratch buffers for the mixer task. Static to keep them out of the task stack.
static int16_t g_in_buf[MIXER_CHUNK_SAMPLES];
static int32_t g_accum[MIXER_CHUNK_SAMPLES];
static int16_t g_out_buf[MIXER_CHUNK_SAMPLES];

static void mixer_task_fn(void* arg) {
    (void)arg;
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

        // Per-source volume = total / N. Saturate as a safety net in case
        // a single source is already at the int16 boundary.
        int divisor = (active_count > 0) ? active_count : 1;
        for (size_t j = 0; j < MIXER_CHUNK_SAMPLES; j++) {
            int32_t s = g_accum[j] / divisor;
            if (s > INT16_MAX) s = INT16_MAX;
            else if (s < INT16_MIN) s = INT16_MIN;
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

    BaseType_t ok = xTaskCreate(mixer_task_fn, "audio_mixer", MIXER_TASK_STACK, NULL, MIXER_TASK_PRIORITY, &g_mixer_task);
    if (ok != pdPASS) {
        vSemaphoreDelete(g_streams_mutex);
        g_streams_mutex = NULL;
        ESP_LOGE(TAG, "Failed to create mixer task");
        return ESP_ERR_NO_MEM;
    }

    g_initialized = true;
    ESP_LOGI(TAG, "Audio mixer started (chunk=%d frames, %d streams max)", MIXER_CHUNK_FRAMES,
             AUDIO_MIXER_MAX_STREAMS);
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
    return xStreamBufferSend(buf, samples, size_bytes, ticks);
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
