#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t onset_seq;
    float onset_strength;
    uint32_t onset_ms;
    float level;
    float bpm;
    bool pitch_valid;
    float f0;
    const char *note_name;
    int octave;
    float cents;
    float clarity;
} music_snapshot_t;

void music_events_init(void);
void music_events_process_block(float rms, float level);
void music_snapshot_get(music_snapshot_t *out);
