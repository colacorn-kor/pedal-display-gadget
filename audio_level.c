#include "audio_level.h"

#include <math.h>
#include <stddef.h>

static float sanitize_unit(float value)
{
    if (!isfinite(value) || value <= 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static float amplitude_db(float value, float reference)
{
    if (!(value > 0.0f) || !(reference > 0.0f)) {
        return AUDIO_LEVEL_FLOOR_DB;
    }

    float db = 20.0f * log10f(value / reference);
    return db < AUDIO_LEVEL_FLOOR_DB ? AUDIO_LEVEL_FLOOR_DB : db;
}

void audio_level_calculate(float rms, float sample_peak,
                           audio_level_reading_t *out)
{
    if (!out) return;

    rms = sanitize_unit(rms);
    sample_peak = sanitize_unit(sample_peak);
    out->rms_dbfs = amplitude_db(rms, 1.0f);
    out->peak_dbfs = amplitude_db(sample_peak, 1.0f);
    out->adc_vrms = rms * AUDIO_ADC_FULL_SCALE_VPEAK;
    out->dbv = amplitude_db(out->adc_vrms, 1.0f);
    out->dbu = amplitude_db(out->adc_vrms, AUDIO_DBU_REFERENCE_VRMS);
}
