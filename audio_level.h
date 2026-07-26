#pragma once

#include "audio_config.h"

#define AUDIO_LEVEL_FLOOR_DB (-120.0f)

typedef struct {
    float rms_dbfs;
    float peak_dbfs;
    float adc_vrms;
    float dbv;
    float dbu;
} audio_level_reading_t;

void audio_level_calculate(float rms, float sample_peak,
                           audio_level_reading_t *out);
