#pragma once

typedef enum {
    SPECTRUM_WEIGHT_FLAT = 0,
    SPECTRUM_WEIGHT_A,
    SPECTRUM_WEIGHT_COUNT,
} spectrum_weighting_t;

float spectrum_a_weighting_db(float frequency_hz);
float spectrum_weighting_apply(float normalized, float frequency_hz,
                               spectrum_weighting_t weighting);
