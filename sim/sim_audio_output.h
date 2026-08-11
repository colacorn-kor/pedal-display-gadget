#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t submitted_frames;
    uint64_t nonzero_frames;
    float peak;
} sim_audio_output_stats_t;

bool sim_audio_output_init(int argc, char **argv, bool virtual_sink);
void sim_audio_output_shutdown(void);
void sim_audio_output_pump(void);
void sim_audio_output_reset_stats(void);
void sim_audio_output_get_stats(sim_audio_output_stats_t *out);
