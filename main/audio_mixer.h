// SPDX-License-Identifier: MIT
// Software audio mixer for the Tanmatsu launcher.
//
// The BSP exposes a single I2S output channel. Without coordination, two
// plugins writing to it concurrently interleave their DMA buffers and produce
// chopped audio. This module owns the I2S channel exclusively and gives each
// plugin task its own FreeRTOS StreamBuffer; a mixer task drains every active
// stream, sums samples in int32 (saturating to int16), and writes the result
// to I2S.
//
// Audio format is fixed: 16-bit signed PCM, stereo (L/R interleaved). The
// mixer assumes the BSP's I2S channel is configured at the matching rate
// (44.1 kHz by default). asp_audio_set_rate still passes through to the BSP
// unchanged; if plugins disagree on rate the last writer wins and others play
// at the wrong pitch.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AUDIO_MIXER_MAX_STREAMS 8

esp_err_t audio_mixer_init(void);

// Allocate a stream for a plugin task. New streams start in the running
// state, so plugins that just write samples without calling start work.
bool audio_mixer_register_stream(TaskHandle_t task);

void audio_mixer_unregister_stream(TaskHandle_t task);

// Resume mixing this stream (idempotent). Returns false if the task has no
// registered stream.
bool audio_mixer_start(TaskHandle_t task);

// Pause mixing this stream and discard any samples currently queued. Pending
// blocking writes from the plugin will unblock with a short return.
bool audio_mixer_stop(TaskHandle_t task);

// Returns the number of bytes accepted into the stream. 0 when the stream
// is unknown, paused, or the timeout elapsed before the entire chunk fit.
size_t audio_mixer_write(TaskHandle_t task, const void* samples, size_t size_bytes, int64_t timeout_ms);
