#include <math.h>
#include <stdio.h>

#include "audio_level.h"

static int failures;

static void expect_near(const char *name, float actual,
                        float expected, float tolerance)
{
    if (fabsf(actual - expected) <= tolerance) return;
    fprintf(stderr, "%s: expected %.6f, got %.6f\n",
            name, expected, actual);
    failures++;
}

int main(void)
{
    audio_level_reading_t reading;

    audio_level_calculate(0.0f, 0.0f, AUDIO_INPUT_LINE, &reading);
    expect_near("silence rms dBFS", reading.rms_dbfs,
                AUDIO_LEVEL_FLOOR_DB, 0.001f);
    expect_near("silence peak dBFS", reading.peak_dbfs,
                AUDIO_LEVEL_FLOOR_DB, 0.001f);
    expect_near("silence volts", reading.adc_vrms, 0.0f, 0.000001f);
    expect_near("silence input volts", reading.input_vrms,
                0.0f, 0.000001f);

    audio_level_calculate(1.0f / sqrtf(2.0f), 1.0f,
                          AUDIO_INPUT_LINE, &reading);
    expect_near("full-scale sine RMS dBFS", reading.rms_dbfs,
                -3.010300f, 0.0005f);
    expect_near("full-scale sine peak dBFS", reading.peak_dbfs,
                0.0f, 0.0005f);
    expect_near("full-scale sine ADC Vrms", reading.adc_vrms,
                1.060660f, 0.0005f);
    expect_near("full-scale sine LINE input Vrms", reading.input_vrms,
                0.530330f, 0.0005f);

    expect_near("LINE gain", audio_level_input_gain(AUDIO_INPUT_LINE),
                2.0f, 0.0005f);
    expect_near("INST gain", audio_level_input_gain(AUDIO_INPUT_INST),
                7.818182f, 0.0005f);

    audio_level_calculate(
        0.25f * AUDIO_FRONTEND_LINE_GAIN /
            AUDIO_ADC_FULL_SCALE_VPEAK,
        1.0f, AUDIO_INPUT_LINE, &reading);
    expect_near("quarter volt LINE input", reading.input_vrms,
                0.25f, 0.0005f);
    expect_near("quarter volt input dBV", reading.input_dbv,
                -12.041200f, 0.0005f);

    audio_level_calculate(
        0.0775f * AUDIO_FRONTEND_LINE_GAIN /
            AUDIO_ADC_FULL_SCALE_VPEAK,
        1.0f, AUDIO_INPUT_LINE, &reading);
    expect_near("77.5 mV input is -20 dBu", reading.input_dbu,
                -20.0f, 0.0005f);

    audio_level_calculate(0.5f, 1.0f, AUDIO_INPUT_INST, &reading);
    expect_near("INST input voltage", reading.input_vrms,
                0.095930f, 0.0005f);

    if (failures != 0) {
        fprintf(stderr, "audio level tests failed: %d\n", failures);
        return 1;
    }

    printf("audio level tests passed\n");
    return 0;
}
