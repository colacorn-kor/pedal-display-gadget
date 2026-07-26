#pragma once

#include "audio_config.h"

#define AUDIO_LEVEL_FLOOR_DB (-120.0f)

typedef enum {
    AUDIO_INPUT_LINE = 0,
    AUDIO_INPUT_INST,
    AUDIO_INPUT_RANGE_COUNT,
} audio_input_range_t;

typedef struct {
    float rms_dbfs;
    float peak_dbfs;
    float adc_vrms;
    float input_vrms;
    float input_dbv;
    float input_dbu;
} audio_level_reading_t;

float audio_level_input_gain(audio_input_range_t range);
float audio_level_input_voltage_correction(audio_input_range_t range);
void audio_level_calculate(float rms, float sample_peak,
                           audio_input_range_t input_range,
                           audio_level_reading_t *out);
