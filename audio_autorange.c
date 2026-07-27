#include "audio_autorange.h"

#include <math.h>
#include <string.h>

#include "audio_config.h"

static float sanitize_sample(float sample)
{
    return isfinite(sample) ? sample : 0.0f;
}

typedef struct {
    float rms;
    float peak;
} sample_metrics_t;

static sample_metrics_t sample_metrics(const float *samples,
                                       size_t sample_count)
{
    double energy = 0.0;
    float peak = 0.0f;
    for (size_t i = 0; i < sample_count; i++) {
        const float sample = sanitize_sample(samples[i]);
        const float magnitude = fabsf(sample);
        energy += (double)sample * (double)sample;
        if (magnitude > peak) peak = magnitude;
    }
    return (sample_metrics_t) {
        .rms = sqrtf((float)(energy / (double)sample_count)),
        .peak = peak,
    };
}

float audio_autorange_adc_to_gg(audio_autorange_range_t range,
                                float adc_sample)
{
    const float sample = sanitize_sample(adc_sample);
    if (range == AUDIO_AUTORANGE_HOT) {
        return sample * AUDIO_DUAL_HOT_VOLTAGE_CORRECTION;
    }
    return sample *
           (AUDIO_DUAL_HOT_GAIN / AUDIO_DUAL_SENSITIVE_GAIN) *
           AUDIO_DUAL_SENSITIVE_VOLTAGE_CORRECTION;
}

float audio_autorange_adc_rms_to_input_vrms(
    audio_autorange_range_t range, float adc_rms)
{
    const float gg_rms =
        fabsf(audio_autorange_adc_to_gg(range, adc_rms));
    return gg_rms * AUDIO_GG_INPUT_FULL_SCALE_VPEAK;
}

void audio_autorange_reset(audio_autorange_t *state)
{
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->active = AUDIO_AUTORANGE_SENSITIVE;
}

void audio_autorange_process(audio_autorange_t *state,
                             const float *hot_adc,
                             const float *sensitive_adc,
                             float *output,
                             size_t sample_count,
                             audio_autorange_status_t *status)
{
    if (!state || !hot_adc || !sensitive_adc || !output ||
        sample_count == 0) {
        if (status) memset(status, 0, sizeof(*status));
        return;
    }

    const sample_metrics_t hot =
        sample_metrics(hot_adc, sample_count);
    const sample_metrics_t sensitive =
        sample_metrics(sensitive_adc, sample_count);
    const bool hot_clipped = hot.peak >= AUDIO_DUAL_ADC_CLIP_PEAK;
    const bool sensitive_clipped =
        sensitive.peak >= AUDIO_DUAL_ADC_CLIP_PEAK;

    if (!state->initialized) {
        state->active =
            sensitive.peak >= AUDIO_DUAL_SWITCH_TO_HOT_PEAK
                ? AUDIO_AUTORANGE_HOT
                : AUDIO_AUTORANGE_SENSITIVE;
        state->initialized = true;
    }

    const audio_autorange_range_t previous = state->active;
    audio_autorange_range_t target = previous;

    if (previous == AUDIO_AUTORANGE_SENSITIVE) {
        state->sensitive_below_samples = 0;
        if (sensitive.peak >= AUDIO_DUAL_SWITCH_TO_HOT_PEAK) {
            target = AUDIO_AUTORANGE_HOT;
        }
    } else if (sensitive.peak <=
               AUDIO_DUAL_SWITCH_TO_SENSITIVE_PEAK) {
        uint32_t remaining =
            UINT32_MAX - state->sensitive_below_samples;
        uint32_t increment = sample_count > remaining
            ? remaining : (uint32_t)sample_count;
        state->sensitive_below_samples += increment;
        if (state->sensitive_below_samples >=
            AUDIO_DUAL_RELEASE_SAMPLES) {
            target = AUDIO_AUTORANGE_SENSITIVE;
            state->sensitive_below_samples = 0;
        }
    } else {
        state->sensitive_below_samples = 0;
    }

    const bool switched = target != previous;
    for (size_t i = 0; i < sample_count; i++) {
        const float hot_sample = audio_autorange_adc_to_gg(
            AUDIO_AUTORANGE_HOT, hot_adc[i]);
        const float sensitive_sample = audio_autorange_adc_to_gg(
            AUDIO_AUTORANGE_SENSITIVE, sensitive_adc[i]);

        if (!switched || sensitive_clipped) {
            output[i] =
                target == AUDIO_AUTORANGE_HOT
                    ? hot_sample : sensitive_sample;
            continue;
        }

        const float old_sample =
            previous == AUDIO_AUTORANGE_HOT
                ? hot_sample : sensitive_sample;
        const float new_sample =
            target == AUDIO_AUTORANGE_HOT
                ? hot_sample : sensitive_sample;
        const float mix =
            (float)(i + 1U) / (float)sample_count;
        output[i] = old_sample + (new_sample - old_sample) * mix;
    }

    state->active = target;
    if (status) {
        *status = (audio_autorange_status_t) {
            .active = target,
            .hot_adc_rms = hot.rms,
            .hot_adc_peak = hot.peak,
            .sensitive_adc_rms = sensitive.rms,
            .sensitive_adc_peak = sensitive.peak,
            .hot_clipped = hot_clipped,
            .sensitive_clipped = sensitive_clipped,
            .output_clipped =
                target == AUDIO_AUTORANGE_HOT
                    ? hot_clipped : sensitive_clipped,
            .switched = switched,
        };
    }
}
