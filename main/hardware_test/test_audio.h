#pragma once

// Full-screen audio output test: loops a short tune through the launcher's
// software audio mixer while showing the output routing (headphones or
// speaker) and the volume currently applied to that output. Returns when the
// user presses ESC / F1, after the tone task has been stopped and its mixer
// stream released.
void test_audio(void);
