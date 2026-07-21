#pragma once

#include <stdbool.h>

#include "app.h"
#include "music_events.h"

bool sim_audio_init(int argc, char **argv);
bool sim_audio_should_exit_after_args(void);
void sim_audio_pump(void);
void sim_audio_set_mouse_x(float x);
void sim_audio_trigger_synthetic_onset(void);
void sim_audio_audio_viz_get(audio_viz_snapshot_t *out);
void sim_audio_music_get(music_snapshot_t *out);
