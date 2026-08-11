#include "music_lobby.h"

#include <string.h>

#include "audio_playback.h"

#define LOBBY_STEP_FRAMES 6000u
#define LOBBY_STEP_COUNT 64u
#define LOBBY_KICK_FRAMES 4200u
#define LOBBY_SNARE_FRAMES 3000u

/* Original E-minor loop. Frequencies are stored directly to keep the
 * generator small and deterministic on both PC and embedded builds. */
static const uint16_t LEAD_HZ[LOBBY_STEP_COUNT] = {
    659, 0, 784, 0, 988, 0, 784, 0,
    587, 0, 740, 0, 880, 0, 740, 0,
    523, 0, 659, 0, 784, 0, 988, 0,
    740, 0, 659, 0, 587, 0, 494, 0,
    659, 0, 784, 0, 1047, 0, 988, 0,
    587, 0, 740, 0, 880, 0, 988, 0,
    784, 0, 659, 0, 587, 0, 659, 0,
    494, 0, 587, 0, 659, 0, 0, 0,
};

static const uint16_t BASS_HZ[16] = {
    165, 165, 196, 196,
    147, 147, 220, 220,
    131, 131, 165, 165,
    147, 147, 123, 123,
};

static float wrap_phase(float phase)
{
    while (phase >= 1.0f) phase -= 1.0f;
    return phase;
}

static float triangle(float phase)
{
    return phase < 0.5f
        ? 1.0f - phase * 4.0f
        : phase * 4.0f - 3.0f;
}

static float step_envelope(uint32_t frame)
{
    const uint32_t attack = 180u;
    const uint32_t release = 1500u;
    float envelope = 1.0f;
    if (frame < attack) envelope = (float)frame / (float)attack;
    const uint32_t remaining = LOBBY_STEP_FRAMES - frame;
    if (remaining < release) {
        const float tail = (float)remaining / (float)release;
        if (tail < envelope) envelope = tail;
    }
    return envelope;
}

static float percussion(music_lobby_t *music)
{
    float sample = 0.0f;

    if ((music->step % 4u) == 0u &&
        music->frame_in_step < LOBBY_KICK_FRAMES) {
        const float age =
            (float)music->frame_in_step / (float)LOBBY_KICK_FRAMES;
        const float envelope = (1.0f - age) * (1.0f - age);
        const float frequency = 92.0f - 42.0f * age;
        music->kick_phase = wrap_phase(
            music->kick_phase + frequency /
            (float)AUDIO_PLAYBACK_SAMPLE_RATE);
        sample += triangle(music->kick_phase) * envelope * 0.28f;
    }

    if (((music->step % 8u) == 4u ||
         (music->step % 8u) == 7u) &&
        music->frame_in_step < LOBBY_SNARE_FRAMES) {
        music->noise = music->noise * 1664525u + 1013904223u;
        const float noise =
            (float)((music->noise >> 9) & 0x7fffu) / 16384.0f - 1.0f;
        const float envelope = 1.0f -
            (float)music->frame_in_step / (float)LOBBY_SNARE_FRAMES;
        sample += noise * envelope * 0.10f;
    }
    return sample;
}

void music_lobby_reset(music_lobby_t *music)
{
    if (!music) return;
    memset(music, 0, sizeof(*music));
    music->noise = 0x47474d55u;
}

size_t music_lobby_generate(music_lobby_t *music,
                            float *stereo_interleaved,
                            size_t frames)
{
    if (!music || !stereo_interleaved) return 0u;

    for (size_t i = 0; i < frames; i++) {
        const uint16_t lead_hz = LEAD_HZ[music->step];
        const uint16_t bass_hz = BASS_HZ[music->step / 4u];
        const float envelope = step_envelope(music->frame_in_step);

        float lead = 0.0f;
        if (lead_hz) {
            music->lead_phase = wrap_phase(
                music->lead_phase + (float)lead_hz /
                (float)AUDIO_PLAYBACK_SAMPLE_RATE);
            lead = (music->lead_phase < 0.25f ? 1.0f : -1.0f) *
                   envelope * 0.17f;
        }

        music->bass_phase = wrap_phase(
            music->bass_phase + (float)bass_hz /
            (float)AUDIO_PLAYBACK_SAMPLE_RATE);
        const float bass = triangle(music->bass_phase) * 0.19f;
        const float drums = percussion(music);
        const float pan = (music->step & 4u) ? 0.70f : 0.42f;

        stereo_interleaved[i * 2u] =
            bass * 0.82f + drums + lead * (1.0f - pan);
        stereo_interleaved[i * 2u + 1u] =
            bass * 0.82f + drums + lead * pan;

        music->frame_in_step++;
        if (music->frame_in_step >= LOBBY_STEP_FRAMES) {
            music->frame_in_step = 0u;
            music->step = (music->step + 1u) % LOBBY_STEP_COUNT;
            music->lead_phase = 0.0f;
            music->kick_phase = 0.0f;
        }
    }
    return frames;
}

uint32_t music_lobby_position_ms(const music_lobby_t *music)
{
    if (!music) return 0u;
    const uint64_t frame =
        (uint64_t)music->step * LOBBY_STEP_FRAMES + music->frame_in_step;
    return (uint32_t)(frame * 1000u / AUDIO_PLAYBACK_SAMPLE_RATE);
}

uint32_t music_lobby_duration_ms(void)
{
    return (uint32_t)(
        (uint64_t)LOBBY_STEP_FRAMES * LOBBY_STEP_COUNT * 1000u /
        AUDIO_PLAYBACK_SAMPLE_RATE);
}
