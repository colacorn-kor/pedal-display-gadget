#pragma once

#include <stddef.h>
#include <stdint.h>

#include "audio_playback.h"

typedef enum {
    METRONOME_TICK_SUBDIVISION = 0,
    METRONOME_TICK_BEAT,
    METRONOME_TICK_DOWNBEAT,
} metronome_tick_kind_t;

typedef struct {
    int bpm;
    int beats_per_bar;
    int subdivisions;
} metronome_config_t;

typedef struct {
    metronome_config_t config;
    uint64_t sample_position;
    uint64_t next_tick_sample;
    uint32_t tick_count;
    uint32_t click_phase;
    uint32_t click_phase_step;
    uint32_t click_remaining;
    uint32_t click_length;
    float click_amplitude;
    metronome_tick_kind_t last_tick_kind;
} metronome_engine_t;

void metronome_engine_init(metronome_engine_t *engine,
                           const metronome_config_t *config);
void metronome_engine_set_config(metronome_engine_t *engine,
                                 const metronome_config_t *config);
size_t metronome_engine_render(metronome_engine_t *engine,
                               float *stereo_interleaved,
                               size_t frames);
