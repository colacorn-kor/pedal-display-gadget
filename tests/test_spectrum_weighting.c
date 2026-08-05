#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio_config.h"
#include "spectrum_weighting.h"

static int failures;

static void expect_near(const char *name, float actual,
                        float expected, float tolerance)
{
    if (fabsf(actual - expected) <= tolerance) return;
    fprintf(stderr, "%s: expected %.3f, got %.3f\n",
            name, expected, actual);
    failures++;
}

static void expect_text(const char *name, const char *actual,
                        const char *expected)
{
    if (strcmp(actual, expected) == 0) return;
    fprintf(stderr, "%s: expected '%s', got '%s'\n",
            name, expected, actual);
    failures++;
}

int main(void)
{
    expect_near("A 20Hz", spectrum_a_weighting_db(20.0f),
                -50.4f, 0.3f);
    expect_near("A 100Hz", spectrum_a_weighting_db(100.0f),
                -19.1f, 0.2f);
    expect_near("A 1kHz", spectrum_a_weighting_db(1000.0f),
                0.0f, 0.1f);
    expect_near("A 10kHz", spectrum_a_weighting_db(10000.0f),
                -2.5f, 0.2f);
    expect_near("Loudness 20Hz",
                spectrum_loudness_weighting_db(20.0f),
                -49.50f, 0.05f);
    expect_near("Loudness 100Hz",
                spectrum_loudness_weighting_db(100.0f),
                -18.64f, 0.05f);
    expect_near("Loudness 1kHz",
                spectrum_loudness_weighting_db(1000.0f),
                0.0f, 0.01f);
    expect_near("Loudness 3.15kHz",
                spectrum_loudness_weighting_db(3150.0f),
                3.59f, 0.05f);
    expect_near("Loudness log interpolation",
                spectrum_loudness_weighting_db(10000.0f),
                -10.135f, 0.05f);

    expect_text("Flat name",
                spectrum_weighting_name(SPECTRUM_WEIGHT_FLAT),
                "Flat");
    expect_text("A loudness name",
                spectrum_weighting_name(SPECTRUM_WEIGHT_A_LOUDNESS),
                "A-weighted(Loudness)");

    const float minus_twelve =
        (-12.0f - VIZ_DB_FLOOR) / (VIZ_DB_TOP - VIZ_DB_FLOOR);
    expect_near("Flat pass-through",
                spectrum_weighting_apply(
                    minus_twelve, 100.0f, SPECTRUM_WEIGHT_FLAT),
                minus_twelve, 0.0001f);
    expect_near("A-weighted 1kHz",
                spectrum_weighting_apply(
                    minus_twelve, 1000.0f, SPECTRUM_WEIGHT_A),
                minus_twelve, 0.002f);
    expect_near("A-weighted 100Hz dB",
                VIZ_DB_FLOOR + spectrum_weighting_apply(
                    minus_twelve, 100.0f, SPECTRUM_WEIGHT_A) *
                    (VIZ_DB_TOP - VIZ_DB_FLOOR),
                -31.1f, 0.2f);
    expect_near("Flat loudness 100Hz dB",
                VIZ_DB_FLOOR + spectrum_weighting_apply(
                    minus_twelve, 100.0f,
                    SPECTRUM_WEIGHT_FLAT_LOUDNESS) *
                    (VIZ_DB_TOP - VIZ_DB_FLOOR),
                -30.64f, 0.1f);
    expect_near("A loudness 100Hz dB",
                VIZ_DB_FLOOR + spectrum_weighting_apply(
                    minus_twelve, 100.0f,
                    SPECTRUM_WEIGHT_A_LOUDNESS) *
                    (VIZ_DB_TOP - VIZ_DB_FLOOR),
                -49.74f, 0.3f);

    if (failures != 0) {
        fprintf(stderr, "spectrum weighting tests failed: %d\n", failures);
        return 1;
    }
    printf("spectrum weighting tests passed\n");
    return 0;
}
