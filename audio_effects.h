#pragma once

#include <stdbool.h>

typedef enum {
    AUDIO_EFFECT_JUMP = 0,
    AUDIO_EFFECT_SCORE,
    AUDIO_EFFECT_HIT,
} audio_effect_t;

bool audio_effects_play(const char *owner_id, audio_effect_t effect);
