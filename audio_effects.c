#include "audio_effects.h"

#include <stddef.h>
#include <stdint.h>

#include "audio_playback.h"

#define EFFECT_CHUNK_FRAMES 128u

typedef struct {
    uint32_t frames;
    uint32_t frequency_start;
    int32_t frequency_delta;
    float amplitude;
} effect_voice_t;

static effect_voice_t voice_for(audio_effect_t effect)
{
    switch (effect) {
    case AUDIO_EFFECT_SCORE:
        return (effect_voice_t){ 1056u, 1100u, 650, 0.38f };
    case AUDIO_EFFECT_HIT:
        return (effect_voice_t){ 1440u, 280u, -150, 0.52f };
    case AUDIO_EFFECT_JUMP:
    default:
        return (effect_voice_t){ 1152u, 620u, 520, 0.42f };
    }
}

bool audio_effects_play(const char *owner_id, audio_effect_t effect)
{
    const effect_voice_t voice = voice_for(effect);
    float chunk[EFFECT_CHUNK_FRAMES * AUDIO_PLAYBACK_CHANNELS];
    uint32_t phase = 0u;
    uint32_t written_total = 0u;

    while (written_total < voice.frames) {
        uint32_t count = voice.frames - written_total;
        if (count > EFFECT_CHUNK_FRAMES) count = EFFECT_CHUNK_FRAMES;

        for (uint32_t i = 0u; i < count; i++) {
            const uint32_t frame = written_total + i;
            const float progress = (float)frame / (float)voice.frames;
            int32_t frequency = (int32_t)voice.frequency_start +
                (int32_t)(voice.frequency_delta * progress);
            if (frequency < 80) frequency = 80;
            const uint32_t phase_step = (uint32_t)(
                ((uint64_t)(uint32_t)frequency << 24u) /
                AUDIO_PLAYBACK_SAMPLE_RATE);
            const uint32_t shape = (phase >> 16u) & 0xffu;
            const float triangle = shape < 128u
                ? (float)shape / 64.0f - 1.0f
                : 3.0f - (float)shape / 64.0f;
            const float envelope = 1.0f - progress;
            const float sample = triangle * envelope * envelope *
                                 voice.amplitude;
            chunk[i * 2u] = sample;
            chunk[i * 2u + 1u] = sample;
            phase += phase_step;
            phase &= 0x00ffffffu;
        }

        const size_t written = audio_playback_write(
            owner_id, AUDIO_PLAYBACK_BUS_EFFECTS, chunk, count);
        written_total += (uint32_t)written;
        if (written < count) break;
    }
    return written_total > 0u;
}
