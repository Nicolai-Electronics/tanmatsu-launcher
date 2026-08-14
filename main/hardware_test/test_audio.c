// SPDX-License-Identifier: MIT
//
// Audio output hardware test.
//
// Plays a short tune (Ode to Joy) in a loop through the launcher's software
// audio mixer and shows the state of the audio path while it does: whether
// the output is routed to the headphone jack or the speaker amplifier, and
// which volume is currently applied to that output.
//
// The waveform is generated on the fly (sine, phase accumulator) and the
// melody is a one-byte-per-note table, so nothing but a few dozen bytes of
// rodata is spent on the tune itself.
//
// Audio is produced by a dedicated task rather than by the UI task: the mixer
// keys its streams on the task handle and can only release a slot once the
// owning task is gone, so writing from the long-lived UI task would hold a
// mixer slot (and its 8 kB stream buffer) forever.

#include "test_audio.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio_mixer.h"
#include "bsp/audio.h"
#include "bsp/input.h"
#include "common/display.h"
#include "common/theme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "global_event_handler.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/menu_helpers.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "pax_types.h"

static char const* TAG = "test_audio";

#define TEXT_FONT pax_font_sky_mono
#define TEXT_SIZE 18

// The mixer is hardwired to 16-bit signed stereo at this rate.
#define SAMPLE_RATE_HZ 44100
#define CHUNK_FRAMES   256  // matches the mixer's own chunk size, ~5.8 ms
#define CHUNK_SAMPLES  (CHUNK_FRAMES * 2)

// Peak amplitude of the generated tone, ~-8.7 dBFS. The mixer divides its sum
// by the number of streams that produced samples, so this drops to half if a
// plugin happens to play audio at the same time. Actual loudness is set by the
// codec through the launcher's per-output volume.
#define TONE_AMPLITUDE 12000

#define ENV_ATTACK_SAMPLES  (SAMPLE_RATE_HZ / 200)  // 5 ms
#define ENV_RELEASE_SAMPLES (SAMPLE_RATE_HZ / 100)  // 10 ms
#define NOTE_GAP_SAMPLES    (SAMPLE_RATE_HZ / 33)   // ~30 ms of silence at the end of every note

#define TONE_TASK_STACK_SIZE 6144
#define TONE_TASK_PRIORITY   5  // plugin level; the mixer task runs above this at 7
#define WRITE_TIMEOUT_MS     100
#define WRITE_MAX_ZERO_TRIES 20  // ~2 s of the mixer refusing samples before we give up

// ---------------------------------------------------------------------------
// Tune
// ---------------------------------------------------------------------------

enum {
    N_REST = 0,
    N_D4,
    N_E4,
    N_FS4,
    N_G4,
    N_A4,
};

static const float       k_freq[] = {0.0f, 293.66f, 329.63f, 369.99f, 392.00f, 440.00f};
static char const* const k_name[] = {"--", "D4", "E4", "F#4", "G4", "A4"};

// High nibble: pitch (index into k_freq), low nibble: duration in eighth
// notes. An eighth is 250 ms, so a quarter note is half a second (120 bpm).
#define NOTE(id, eighths) ((uint8_t)(((id) << 4) | (eighths)))

static const uint8_t k_melody[] = {
    // Ode to Joy in D major, first two phrases.
    NOTE(N_FS4, 2),  NOTE(N_FS4, 2), NOTE(N_G4, 2),  NOTE(N_A4, 2),   //
    NOTE(N_A4, 2),   NOTE(N_G4, 2),  NOTE(N_FS4, 2), NOTE(N_E4, 2),   //
    NOTE(N_D4, 2),   NOTE(N_D4, 2),  NOTE(N_E4, 2),  NOTE(N_FS4, 2),  //
    NOTE(N_FS4, 3),  NOTE(N_E4, 1),  NOTE(N_E4, 4),                   //
    NOTE(N_FS4, 2),  NOTE(N_FS4, 2), NOTE(N_G4, 2),  NOTE(N_A4, 2),   //
    NOTE(N_A4, 2),   NOTE(N_G4, 2),  NOTE(N_FS4, 2), NOTE(N_E4, 2),   //
    NOTE(N_D4, 2),   NOTE(N_D4, 2),  NOTE(N_E4, 2),  NOTE(N_FS4, 2),  //
    NOTE(N_E4, 3),   NOTE(N_D4, 1),  NOTE(N_D4, 4),                   //
    NOTE(N_REST, 2),                                                  // audible seam before the loop repeats
};

#define MELODY_LENGTH (sizeof(k_melody) / sizeof(k_melody[0]))

// Synthesizer state, owned by the tone task.
static size_t   s_note     = 0;     // index into k_melody
static uint32_t s_note_pos = 0;     // samples produced for the current note
static uint32_t s_note_len = 0;     // length of the current note in samples
static uint32_t s_gate_len = 0;     // samples of the current note that make sound
static float    s_phase    = 0.0f;  // oscillator phase in turns, kept in [0, 1)
static float    s_step     = 0.0f;  // phase increment per sample

// ---------------------------------------------------------------------------
// Playback task state
// ---------------------------------------------------------------------------

static StackType_t*     s_stack          = NULL;
static StaticTask_t*    s_tcb            = NULL;
static TaskHandle_t     s_task           = NULL;
static volatile bool    s_stop_requested = false;
static volatile bool    s_task_exited    = false;
static volatile bool    s_stalled        = false;
static volatile uint8_t s_display_note   = N_REST;  // published for the UI

static void audio_stop(void);

static void note_start(void) {
    uint8_t note    = k_melody[s_note];
    uint8_t id      = note >> 4;
    uint8_t eighths = note & 0x0F;

    s_note_len     = (uint32_t)eighths * (SAMPLE_RATE_HZ / 4);
    s_gate_len     = (s_note_len > NOTE_GAP_SAMPLES) ? (s_note_len - NOTE_GAP_SAMPLES) : s_note_len;
    s_step         = k_freq[id] / (float)SAMPLE_RATE_HZ;
    // Restarting the phase at every note keeps the tone deterministic; the
    // attack ramp below makes the discontinuity inaudible.
    s_phase        = 0.0f;
    s_note_pos     = 0;
    s_display_note = id;
}

// Renders `frames` stereo frames of the tune into `out` (L/R interleaved).
static void render_chunk(int16_t* out, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        if (s_note_pos >= s_note_len) {
            s_note = (s_note + 1) % MELODY_LENGTH;
            note_start();
        }

        float gain = 0.0f;
        if (s_step > 0.0f && s_note_pos < s_gate_len) {
            uint32_t remaining = s_gate_len - s_note_pos;
            if (s_note_pos < ENV_ATTACK_SAMPLES) {
                gain = (float)s_note_pos / (float)ENV_ATTACK_SAMPLES;
            } else if (remaining < ENV_RELEASE_SAMPLES) {
                gain = (float)remaining / (float)ENV_RELEASE_SAMPLES;
            } else {
                gain = 1.0f;
            }
        }

        int16_t sample = 0;
        if (gain > 0.0f) {
            sample = (int16_t)(TONE_AMPLITUDE * gain * sinf(s_phase * 6.2831853f));
        }

        s_phase += s_step;
        if (s_phase >= 1.0f) s_phase -= 1.0f;
        s_note_pos++;

        *out++ = sample;  // left
        *out++ = sample;  // right
    }
}

static void tone_task(void* arg) {
    (void)arg;
    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    int16_t      chunk[CHUNK_SAMPLES];

    audio_mixer_start(self);

    while (!s_stop_requested) {
        render_chunk(chunk, CHUNK_FRAMES);

        // The mixer accepts partial writes (its stream buffer is byte
        // granular), so resume at the offset instead of dropping the rest:
        // losing half a frame would swap left and right for good.
        size_t total       = sizeof(chunk);
        size_t offset      = 0;
        int    zero_writes = 0;
        while (offset < total && !s_stop_requested) {
            size_t written = audio_mixer_write(self, (uint8_t*)chunk + offset, total - offset, WRITE_TIMEOUT_MS);
            if (written == 0) {
                if (++zero_writes > WRITE_MAX_ZERO_TRIES) {
                    ESP_LOGW(TAG, "Mixer is not accepting samples, stopping playback");
                    s_stalled = true;
                    goto done;
                }
                continue;
            }
            offset      += written;
            zero_writes  = 0;
        }
    }

done:
    // Pausing (and flushing) from the stream's own producer task: doing this
    // from the UI task instead would race with a blocked write and silently
    // leave queued samples behind.
    audio_mixer_stop(self);
    s_task_exited = true;
    vTaskDelete(NULL);
}

// Starts the tone task. Returns false if no mixer stream is available, in
// which case nothing is started and the dialog just reports the problem.
static bool audio_start(void) {
    s_stop_requested = false;
    s_task_exited    = false;
    s_stalled        = false;
    s_display_note   = N_REST;

    // Start at the beginning of the tune on every run.
    s_note     = MELODY_LENGTH - 1;
    s_note_pos = 0;
    s_note_len = 0;
    s_phase    = 0.0f;
    s_step     = 0.0f;

    // A plugin may have left the codec at a different rate; the mixer assumes
    // this one.
    bsp_audio_set_rate(SAMPLE_RATE_HZ);

    // The task stack and TCB are ours: the mixer inspects the task state
    // through the stored handle when releasing the stream, so the TCB has to
    // stay valid until after we unregister.
    s_stack = malloc(TONE_TASK_STACK_SIZE * sizeof(StackType_t));
    s_tcb   = malloc(sizeof(StaticTask_t));
    if (s_stack == NULL || s_tcb == NULL) {
        ESP_LOGE(TAG, "Failed to allocate tone task memory");
        goto fail;
    }

    s_task = xTaskCreateStatic(tone_task, "audiotest", TONE_TASK_STACK_SIZE, NULL, TONE_TASK_PRIORITY, s_stack, s_tcb);
    if (s_task == NULL) {
        ESP_LOGE(TAG, "Failed to create tone task");
        goto fail;
    }

    if (!audio_mixer_register_stream(s_task)) {
        // No free mixer slot: stop the task again instead of letting it spin
        // on writes that can never succeed. Tear down through audio_stop so
        // the slot is released in case the task did manage to claim one for
        // itself before we got here.
        ESP_LOGE(TAG, "No audio mixer stream available");
        audio_stop();
        return false;
    }

    return true;

fail:
    free(s_stack);
    free(s_tcb);
    s_stack = NULL;
    s_tcb   = NULL;
    return false;
}

static void audio_stop(void) {
    if (s_task == NULL) return;

    s_stop_requested = true;

    // The task checks the flag between writes, so it exits within one write
    // timeout. The forced delete below is only a safety net.
    for (int timeout_ms = 1000; !s_task_exited && timeout_ms > 0; timeout_ms -= 10) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (!s_task_exited) {
        ESP_LOGW(TAG, "Tone task did not stop in time, deleting it");
        vTaskDelete(s_task);
    }

    // Give the idle task a moment to take the TCB off the termination list,
    // so the static TCB can be reused if the test is started again.
    vTaskDelay(pdMS_TO_TICKS(50));

    // Releases the mixer slot and its stream buffer. Must happen before the
    // TCB is freed: the mixer reads the task state through the handle.
    audio_mixer_unregister_stream(s_task);

    free(s_stack);
    free(s_tcb);
    s_stack = NULL;
    s_tcb   = NULL;
    s_task  = NULL;
}

// ---------------------------------------------------------------------------
// Dialog
// ---------------------------------------------------------------------------

static void draw_line(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, int line, char const* label,
                      char const* value) {
    char text_buffer[64];
    snprintf(text_buffer, sizeof(text_buffer), "%-9s %s", label, value);
    pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                  position.y0 + (TEXT_SIZE + 2) * line, text_buffer);
}

static void render(void) {
    pax_buf_t*   buffer   = display_get_buffer();
    gui_theme_t* theme    = get_theme();
    pax_vec2_t   position = menu_calc_position(buffer, theme);

    bool headphones = global_event_handler_headphones_inserted();

    render_base_screen_statusbar(
        buffer, theme, true, true, true,
        ((gui_element_icontext_t[]){{get_icon(headphones ? ICON_HEADPHONES : ICON_SPEAKER), "Audio test"}}), 1,
        ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2, NULL, 0);

    char text_buffer[64];
    int  line = 0;

    draw_line(buffer, theme, position, line++, "Output:", headphones ? "Headphones" : "Speaker (amplifier)");

    snprintf(text_buffer, sizeof(text_buffer), "%u %%", global_event_handler_get_volume());
    draw_line(buffer, theme, position, line++, "Volume:", text_buffer);

    uint8_t note = s_display_note;
    if (s_task != NULL && note != N_REST) {
        snprintf(text_buffer, sizeof(text_buffer), "%-3s  %u Hz", k_name[note], (unsigned)(k_freq[note] + 0.5f));
    } else {
        snprintf(text_buffer, sizeof(text_buffer), "--");
    }
    draw_line(buffer, theme, position, line++, "Note:", text_buffer);

    draw_line(buffer, theme, position, line++, "Melody:", "Ode to Joy (looping)");
    draw_line(buffer, theme, position, line++, "Format:", "44100 Hz, 16 bit stereo");

    char const* state = "Playing";
    if (s_task == NULL) {
        state = "Unavailable";
    } else if (s_stalled) {
        state = "Stalled";
    }
    draw_line(buffer, theme, position, line++, "Mixer:", state);

    line++;
    draw_line(buffer, theme, position, line++, "", "Use the volume keys to adjust the level.");

    display_blit_buffer(buffer);
}

void test_audio(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    audio_start();

    render();

    bool running = true;
    while (running) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(250)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                                running = false;
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        } else {
            render();
        }
    }

    audio_stop();
}
