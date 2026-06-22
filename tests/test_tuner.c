#include <assert.h>
#include <math.h>

#include "audio_config.h"
#include "tuner.h"

#define PI_F 3.14159265358979323846f

static tuner_result_t detect(float frequency)
{
    tuner_reset();
    float block[256];
    int total = AUDIO_SAMPLE_RATE / 2;
    int generated = 0;
    while (generated < total) {
        int count = total - generated;
        if (count > (int)(sizeof(block) / sizeof(block[0]))) count = (int)(sizeof(block) / sizeof(block[0]));
        for (int i = 0; i < count; i++) {
            int sample_index = generated + i;
            block[i] = 0.4f * sinf(2.0f * PI_F * frequency * sample_index / AUDIO_SAMPLE_RATE);
        }
        tuner_feed(block, count);
        generated += count;
    }
    tuner_result_t result;
    tuner_get(&result);
    return result;
}

int main(void)
{
    tuner_init();
    const float frequencies[] = { 82.4069f, 440.0f, 1000.0f, 1090.0f };
    for (unsigned i = 0; i < sizeof(frequencies) / sizeof(frequencies[0]); i++) {
        tuner_result_t result = detect(frequencies[i]);
        assert(result.voiced);
        assert(fabsf(result.f0 - frequencies[i]) / frequencies[i] < 0.015f);
        assert(result.clarity >= 0.50f);
    }
    return 0;
}
