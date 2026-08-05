#include "spectrum_weighting.h"

#include <math.h>

#include "audio_config.h"

typedef struct {
    float frequency_hz;
    float correction_db;
} loudness_point_t;

/* 60-phon equal-loudness reference, normalized to 1kHz. The ISO contour
 * ends at 12.5kHz, so that last correction is held through 20kHz. */
static const loudness_point_t LOUDNESS_POINTS[] = {
    {    20.0f, -49.50f },
    {    31.5f, -39.07f },
    {    50.0f, -29.95f },
    {   100.0f, -18.64f },
    {   200.0f,  -9.85f },
    {   500.0f,  -2.04f },
    {  1000.0f,   0.00f },
    {  2000.0f,   0.05f },
    {  3150.0f,   3.59f },
    {  5000.0f,  -0.88f },
    {  8000.0f, -11.65f },
    { 12500.0f,  -8.62f },
    { 20000.0f,  -8.62f },
};

#define LOUDNESS_POINT_COUNT \
    ((int)(sizeof(LOUDNESS_POINTS) / sizeof(LOUDNESS_POINTS[0])))

const char *spectrum_weighting_name(spectrum_weighting_t weighting)
{
    static const char *const NAMES[SPECTRUM_WEIGHT_COUNT] = {
        "Flat",
        "A-weighted",
        "Flat(Loudness)",
        "A-weighted(Loudness)",
    };
    return weighting >= SPECTRUM_WEIGHT_FLAT &&
           weighting < SPECTRUM_WEIGHT_COUNT
        ? NAMES[weighting] : "";
}

bool spectrum_weighting_uses_a(spectrum_weighting_t weighting)
{
    return weighting == SPECTRUM_WEIGHT_A ||
           weighting == SPECTRUM_WEIGHT_A_LOUDNESS;
}

bool spectrum_weighting_uses_loudness(spectrum_weighting_t weighting)
{
    return weighting == SPECTRUM_WEIGHT_FLAT_LOUDNESS ||
           weighting == SPECTRUM_WEIGHT_A_LOUDNESS;
}

float spectrum_a_weighting_db(float frequency_hz)
{
    if (!isfinite(frequency_hz) || frequency_hz <= 0.0f) {
        return VIZ_DB_FLOOR;
    }

    const double f2 = (double)frequency_hz * (double)frequency_hz;
    const double c20 = 20.6 * 20.6;
    const double c107 = 107.7 * 107.7;
    const double c737 = 737.9 * 737.9;
    const double c12200 = 12200.0 * 12200.0;
    const double numerator = c12200 * f2 * f2;
    const double denominator =
        (f2 + c20) * sqrt((f2 + c107) * (f2 + c737)) *
        (f2 + c12200);
    if (!(denominator > 0.0)) return VIZ_DB_FLOOR;

    return (float)(20.0 * log10(numerator / denominator) + 2.0);
}

float spectrum_loudness_weighting_db(float frequency_hz)
{
    if (!isfinite(frequency_hz) || frequency_hz <= 0.0f) {
        return VIZ_DB_FLOOR;
    }
    if (frequency_hz <= LOUDNESS_POINTS[0].frequency_hz) {
        return LOUDNESS_POINTS[0].correction_db;
    }
    if (frequency_hz >=
        LOUDNESS_POINTS[LOUDNESS_POINT_COUNT - 1].frequency_hz) {
        return LOUDNESS_POINTS[LOUDNESS_POINT_COUNT - 1].correction_db;
    }

    for (int i = 1; i < LOUDNESS_POINT_COUNT; i++) {
        const loudness_point_t *upper = &LOUDNESS_POINTS[i];
        if (frequency_hz > upper->frequency_hz) continue;

        const loudness_point_t *lower = &LOUDNESS_POINTS[i - 1];
        const float position =
            logf(frequency_hz / lower->frequency_hz) /
            logf(upper->frequency_hz / lower->frequency_hz);
        return lower->correction_db +
               (upper->correction_db - lower->correction_db) * position;
    }
    return 0.0f;
}

float spectrum_weighting_db(float frequency_hz,
                            spectrum_weighting_t weighting)
{
    float correction_db = 0.0f;
    if (spectrum_weighting_uses_a(weighting)) {
        correction_db += spectrum_a_weighting_db(frequency_hz);
    }
    if (spectrum_weighting_uses_loudness(weighting)) {
        correction_db += spectrum_loudness_weighting_db(frequency_hz);
    }
    return correction_db;
}

float spectrum_weighting_apply_db(float normalized, float correction_db)
{
    if (!isfinite(normalized) || normalized <= 0.0f) return 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    if (!isfinite(correction_db)) return 0.0f;

    const float db = VIZ_DB_FLOOR +
        normalized * (VIZ_DB_TOP - VIZ_DB_FLOOR);
    float weighted = (db + correction_db - VIZ_DB_FLOOR) /
                     (VIZ_DB_TOP - VIZ_DB_FLOOR);
    if (weighted < 0.0f) weighted = 0.0f;
    if (weighted > 1.0f) weighted = 1.0f;
    return weighted;
}

float spectrum_weighting_apply(float normalized, float frequency_hz,
                               spectrum_weighting_t weighting)
{
    return spectrum_weighting_apply_db(
        normalized, spectrum_weighting_db(frequency_hz, weighting));
}
