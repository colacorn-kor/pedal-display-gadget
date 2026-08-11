#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t step;
    uint32_t frame_in_step;
    uint32_t noise;
    float lead_phase;
    float bass_phase;
    float kick_phase;
} music_lobby_t;

void music_lobby_reset(music_lobby_t *music);
size_t music_lobby_generate(music_lobby_t *music,
                            float *stereo_interleaved,
                            size_t frames);
uint32_t music_lobby_position_ms(const music_lobby_t *music);
uint32_t music_lobby_duration_ms(void);
