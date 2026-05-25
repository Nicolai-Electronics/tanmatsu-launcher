#pragma once

#include "pax_types.h"

// Render the static synthwave background (sun, mountains, mountain
// detail lines, top grid line) into `fb`. The original drawing code —
// preserved here so non-Tanmatsu paths and any future static screenshots
// can still grab a single frame without spinning up the animator task.
void synthwave(pax_buf_t* fb);

// Render one animation step of the bottom grid into `fb`. Each call
// advances the horizontal scan-lines down one pixel; calling this in a
// loop produces the scrolling "synth grid" effect.
void synthwave_step(pax_buf_t* fb);

// Start a background task that owns the framebuffer and continuously
// redraws the synthwave background, animates the bottom grid, and draws
// the message overlay set via synthwave_animation_set_message. Idempotent
// — calling it while the animator is already running is a no-op. While
// the animator is running, no other code may render to or blit the
// display; call synthwave_animation_stop first.
void synthwave_animation_start(void);

// Stop the animator task. Blocks until the task has finished its last
// frame and released the framebuffer, so the caller is free to render
// immediately afterwards. Idempotent.
void synthwave_animation_stop(void);

// Update the message overlay shown by the animator. Thread-safe; the
// string is copied into an internal buffer. Calls made before
// synthwave_animation_start are silently dropped.
void synthwave_animation_set_message(const char* message);
