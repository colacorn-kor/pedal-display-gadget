#include <assert.h>
#include <math.h>

#include "audio_config.h"
#include "tuner.h"

#define PI_F 3.14159265358979323846f

static tuner_result_t detect(float frequency, int harmonic_rich)
{
    tuner_reset();
    float block[256];
    /* 0.5 s contains 15.5 cycles at 31 Hz and leaves more than twelve
     * post-warmup HOP intervals after filling the 1536-sample/8 kHz window. */
    int total = AUDIO_SAMPLE_RATE / 2;
    int generated = 0;
    while (generated < total) {
        int count = total - generated;
        if (count > (int)(sizeof(block) / sizeof(block[0]))) count = (int)(sizeof(block) / sizeof(block[0]));
        for (int i = 0; i < count; i++) {
            int sample_index = generated + i;
            float phase = 2.0f * PI_F * frequency * sample_index / AUDIO_SAMPLE_RATE;
            if (harmonic_rich) {
                /* A deliberately stronger second harmonic catches octave-up
                 * regressions in the low bass/drop-tuning range. */
                block[i] = 0.18f * sinf(phase)
                         + 0.40f * sinf(2.0f * phase)
                         + 0.22f * sinf(3.0f * phase)
                         + 0.12f * sinf(4.0f * phase);
            } else {
                block[i] = 0.4f * sinf(phase);
            }
        }
        tuner_feed(block, count);
        generated += count;
    }
    tuner_result_t result;
    tuner_get(&result);
    return result;
}

static void assert_detected(float frequency, int harmonic_rich)
{
    tuner_result_t result = detect(frequency, harmonic_rich);
    assert(result.voiced);
    assert(fabsf(result.f0 - frequency) / frequency < 0.015f);
    assert(result.clarity >= 0.50f);
}

int main(void)
{
    tuner_init();
    const float frequencies[] = {
        30.0f, 31.0f, 41.2f, 73.4f, 82.4f,
        440.0f, 1000.0f, 1090.0f, 1290.0f, 1300.0f
    };
    for (unsigned i = 0; i < sizeof(frequencies) / sizeof(frequencies[0]); i++) {
        assert_detected(frequencies[i], 0);
    }

    const float low_harmonic_rich[] = { 31.0f, 41.2f, 73.4f, 82.4f };
    for (unsigned i = 0; i < sizeof(low_harmonic_rich) / sizeof(low_harmonic_rich[0]); i++) {
        assert_detected(low_harmonic_rich[i], 1);
    }
    return 0;
}
