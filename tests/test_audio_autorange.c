#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio_autorange.h"
#include "audio_config.h"

#define BLOCK 256

static int failures;

static void expect_true(const char *name, int condition)
{
    if (condition) return;
    fprintf(stderr, "FAIL %s\n", name);
    failures++;
}

static void expect_near(const char *name, float actual,
                        float expected, float tolerance)
{
    if (fabsf(actual - expected) <= tolerance) return;
    fprintf(stderr, "FAIL %s: got %.7f expected %.7f\n",
            name, actual, expected);
    failures++;
}

static void fill_equivalent(float input_normalized,
                            float hot[BLOCK],
                            float sensitive[BLOCK])
{
    const float hot_adc =
        input_normalized / AUDIO_DUAL_HOT_VOLTAGE_CORRECTION;
    const float sensitive_adc =
        input_normalized /
        ((AUDIO_DUAL_HOT_GAIN / AUDIO_DUAL_SENSITIVE_GAIN) *
         AUDIO_DUAL_SENSITIVE_VOLTAGE_CORRECTION);

    for (int i = 0; i < BLOCK; i++) {
        hot[i] = hot_adc;
        sensitive[i] = sensitive_adc;
    }
}

int main(void)
{
    float hot[BLOCK];
    float sensitive[BLOCK];
    float output[BLOCK];
    audio_autorange_t state;
    audio_autorange_status_t status;

    audio_autorange_reset(&state);
    fill_equivalent(0.020f, hot, sensitive);
    audio_autorange_process(
        &state, hot, sensitive, output, BLOCK, &status);
    expect_true("quiet signal selects sensitive",
                status.active == AUDIO_AUTORANGE_SENSITIVE);
    expect_near("sensitive path input scale",
                output[BLOCK - 1], 0.020f, 0.000001f);

    fill_equivalent(0.030f, hot, sensitive);
    for (int i = 0; i < BLOCK; i++) {
        hot[i] =
            0.035f / AUDIO_DUAL_HOT_VOLTAGE_CORRECTION;
    }
    audio_autorange_process(
        &state, hot, sensitive, output, BLOCK, &status);
    expect_true("high signal switches hot", status.switched);
    expect_true("hot active",
                status.active == AUDIO_AUTORANGE_HOT);
    expect_true("mismatched switch starts near sensitive",
                output[0] > 0.030f && output[0] < 0.0301f);
    expect_near("mismatched switch ends at hot",
                output[BLOCK - 1], 0.035f, 0.000001f);

    fill_equivalent(0.010f, hot, sensitive);
    for (uint32_t samples = 0;
         samples + BLOCK < AUDIO_DUAL_RELEASE_SAMPLES;
         samples += BLOCK) {
        audio_autorange_process(
            &state, hot, sensitive, output, BLOCK, &status);
        expect_true("hysteresis holds hot",
                    status.active == AUDIO_AUTORANGE_HOT);
    }
    while (state.active == AUDIO_AUTORANGE_HOT) {
        audio_autorange_process(
            &state, hot, sensitive, output, BLOCK, &status);
    }
    expect_true("release returns sensitive", status.switched);

    audio_autorange_reset(&state);
    fill_equivalent(0.010f, hot, sensitive);
    audio_autorange_process(
        &state, hot, sensitive, output, BLOCK, &status);
    for (int i = 0; i < BLOCK; i++) {
        hot[i] = 0.040f;
        sensitive[i] = 1.0f;
    }
    audio_autorange_process(
        &state, hot, sensitive, output, BLOCK, &status);
    expect_true("clipped sensitive switches hot",
                status.active == AUDIO_AUTORANGE_HOT);
    expect_true("sensitive clip reported", status.sensitive_clipped);
    expect_true("hot output not clipped", !status.output_clipped);
    expect_near("clipped switch bypasses clipped crossfade",
                output[0],
                0.040f * AUDIO_DUAL_HOT_VOLTAGE_CORRECTION,
                0.000001f);

    audio_autorange_reset(&state);
    fill_equivalent(0.020f, hot, sensitive);
    sensitive[0] = NAN;
    audio_autorange_process(
        &state, hot, sensitive, output, BLOCK, &status);
    expect_true("nonfinite sample sanitized", isfinite(output[0]));

    if (failures) {
        fprintf(stderr, "%d audio autorange test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("audio autorange tests passed\n");
    return EXIT_SUCCESS;
}
