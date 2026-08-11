#include "metronome_engine.h"

#include <string.h>

#define CLICK_LENGTH_MS 24u

static metronome_config_t sanitize_config(const metronome_config_t *config)
{
    metronome_config_t safe = { 120, 4, 1 };
    if (!config) return safe;
    if (config->bpm >= 40 && config->bpm <= 220) safe.bpm = config->bpm;
    if (config->beats_per_bar >= 2 && config->beats_per_bar <= 5) {
        safe.beats_per_bar = config->beats_per_bar;
    }
    if (config->subdivisions >= 1 && config->subdivisions <= 4) {
        safe.subdivisions = config->subdivisions;
    }
    return safe;
}

static uint64_t tick_sample(const metronome_engine_t *engine,
                            uint32_t tick_index)
{
    const uint64_t numerator =
        (uint64_t)tick_index * AUDIO_PLAYBACK_SAMPLE_RATE * 60u;
    const uint32_t denominator =
        (uint32_t)(engine->config.bpm * engine->config.subdivisions);
    return numerator / denominator;
}

static void begin_click(metronome_engine_t *engine)
{
    const uint32_t tick_in_bar = engine->tick_count %
        (uint32_t)(engine->config.beats_per_bar *
                   engine->config.subdivisions);
    const bool downbeat = tick_in_bar == 0u;
    const bool beat = tick_in_bar %
        (uint32_t)engine->config.subdivisions == 0u;
    uint32_t frequency;

    if (downbeat) {
        engine->last_tick_kind = METRONOME_TICK_DOWNBEAT;
        engine->click_amplitude = 0.82f;
        frequency = 2200u;
    } else if (beat) {
        engine->last_tick_kind = METRONOME_TICK_BEAT;
        engine->click_amplitude = 0.60f;
        frequency = 1600u;
    } else {
        engine->last_tick_kind = METRONOME_TICK_SUBDIVISION;
        engine->click_amplitude = 0.34f;
        frequency = 1100u;
    }

    engine->click_phase = 0u;
    engine->click_phase_step = (uint32_t)(
        ((uint64_t)frequency << 24u) / AUDIO_PLAYBACK_SAMPLE_RATE);
    engine->click_length =
        AUDIO_PLAYBACK_SAMPLE_RATE * CLICK_LENGTH_MS / 1000u;
    engine->click_remaining = engine->click_length;
    engine->tick_count++;
    engine->next_tick_sample = tick_sample(engine, engine->tick_count);
}

static float render_click_sample(metronome_engine_t *engine)
{
    if (engine->click_remaining == 0u) return 0.0f;

    const uint32_t phase = (engine->click_phase >> 16u) & 0xffu;
    const float triangle = phase < 128u
        ? (float)phase / 64.0f - 1.0f
        : 3.0f - (float)phase / 64.0f;
    const float envelope =
        (float)engine->click_remaining / (float)engine->click_length;
    const float sample = triangle * envelope * envelope *
                         engine->click_amplitude;

    engine->click_phase += engine->click_phase_step;
    engine->click_phase &= 0x00ffffffu;
    engine->click_remaining--;
    return sample;
}

void metronome_engine_init(metronome_engine_t *engine,
                           const metronome_config_t *config)
{
    if (!engine) return;
    memset(engine, 0, sizeof(*engine));
    engine->config = sanitize_config(config);
    engine->last_tick_kind = METRONOME_TICK_DOWNBEAT;
}

void metronome_engine_set_config(metronome_engine_t *engine,
                                 const metronome_config_t *config)
{
    metronome_engine_init(engine, config);
}

size_t metronome_engine_render(metronome_engine_t *engine,
                               float *stereo_interleaved,
                               size_t frames)
{
    if (!engine || !stereo_interleaved || frames == 0u) return 0u;

    for (size_t i = 0; i < frames; i++) {
        if (engine->sample_position >= engine->next_tick_sample) {
            begin_click(engine);
        }
        const float sample = render_click_sample(engine);
        stereo_interleaved[i * 2u] = sample;
        stereo_interleaved[i * 2u + 1u] = sample;
        engine->sample_position++;
    }
    return frames;
}
