#pragma once

#include <stdbool.h>

typedef enum {
    SPECTRUM_WEIGHT_FLAT = 0,
    SPECTRUM_WEIGHT_A,
    SPECTRUM_WEIGHT_FLAT_LOUDNESS,
    SPECTRUM_WEIGHT_A_LOUDNESS,
    SPECTRUM_WEIGHT_COUNT,
} spectrum_weighting_t;

const char *spectrum_weighting_name(spectrum_weighting_t weighting);
bool spectrum_weighting_uses_a(spectrum_weighting_t weighting);
bool spectrum_weighting_uses_loudness(spectrum_weighting_t weighting);
float spectrum_a_weighting_db(float frequency_hz);
float spectrum_loudness_weighting_db(float frequency_hz);
float spectrum_weighting_db(float frequency_hz,
                            spectrum_weighting_t weighting);
float spectrum_weighting_apply_db(float normalized, float correction_db);
float spectrum_weighting_apply(float normalized, float frequency_hz,
                               spectrum_weighting_t weighting);
