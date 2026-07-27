#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    AUDIO_AUTORANGE_SENSITIVE = 0,
    AUDIO_AUTORANGE_HOT,
} audio_autorange_range_t;

typedef struct {
    audio_autorange_range_t active;
    uint32_t sensitive_below_samples;
    bool initialized;
} audio_autorange_t;

typedef struct {
    audio_autorange_range_t active;
    float hot_adc_peak;
    float sensitive_adc_peak;
    bool hot_clipped;
    bool sensitive_clipped;
    bool output_clipped;
    bool switched;
} audio_autorange_status_t;

void audio_autorange_reset(audio_autorange_t *state);
void audio_autorange_process(audio_autorange_t *state,
                             const float *hot_adc,
                             const float *sensitive_adc,
                             float *output,
                             size_t sample_count,
                             audio_autorange_status_t *status);

