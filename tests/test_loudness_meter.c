#include <assert.h>
#include <math.h>

#include "audio_config.h"
#include "loudness_meter.h"

#define PI_D 3.1415926535897932384626433832795

static loudness_snapshot_t feed_tone(float frequency, float amplitude,
                                     float seconds, float phase_offset)
{
    loudness_meter_reset();
    float block[256];
    const int total = (int)(AUDIO_SAMPLE_RATE * seconds);
    int generated = 0;
    while (generated < total) {
        int count = total - generated;
        if (count > (int)(sizeof(block) / sizeof(block[0]))) {
            count = (int)(sizeof(block) / sizeof(block[0]));
        }
        for (int i = 0; i < count; i++) {
            const double phase = 2.0 * PI_D * frequency *
                (generated + i) / AUDIO_SAMPLE_RATE + phase_offset;
            block[i] = amplitude * (float)sin(phase);
        }
        loudness_meter_feed(block, count);
        generated += count;
    }
    loudness_snapshot_t result;
    loudness_meter_get(&result);
    return result;
}

static void expect_near(float actual, float expected, float tolerance)
{
    assert(fabsf(actual - expected) <= tolerance);
}

int main(void)
{
    loudness_meter_init();

    loudness_snapshot_t full = feed_tone(997.0f, 1.0f, 3.2f, 0.0f);
    expect_near(full.momentary_lufs, -3.01f, 0.15f);
    expect_near(full.short_term_lufs, -3.01f, 0.15f);
    expect_near(full.integrated_lufs, -3.01f, 0.15f);
    assert(full.true_peak_dbtp > -0.10f);
    assert(full.true_peak_dbtp < 0.20f);

    loudness_snapshot_t quiet = feed_tone(997.0f, 0.1f, 3.2f, 0.0f);
    expect_near(quiet.momentary_lufs, -23.01f, 0.15f);
    expect_near(quiet.integrated_lufs, -23.01f, 0.15f);

    loudness_snapshot_t intersample = feed_tone(
        12000.0f, 1.0f, 0.5f, (float)(PI_D * 0.25));
    assert(intersample.true_peak_linear > 0.90f);
    assert(intersample.true_peak_linear < 1.20f);

    float silence[AUDIO_SAMPLE_RATE / 10] = { 0 };
    loudness_meter_reset();
    for (int i = 0; i < 40; i++) {
        loudness_meter_feed(silence,
                            (int)(sizeof(silence) / sizeof(silence[0])));
    }
    loudness_snapshot_t silent;
    loudness_meter_get(&silent);
    assert(silent.momentary_lufs == LOUDNESS_FLOOR_LUFS);
    assert(silent.short_term_lufs == LOUDNESS_FLOOR_LUFS);
    assert(silent.integrated_lufs == LOUDNESS_FLOOR_LUFS);
    assert(silent.true_peak_dbtp == LOUDNESS_FLOOR_LUFS);
    return 0;
}
