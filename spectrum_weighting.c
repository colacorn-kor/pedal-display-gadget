#include "spectrum_weighting.h"

#include <math.h>

#include "audio_config.h"

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

float spectrum_weighting_apply(float normalized, float frequency_hz,
                               spectrum_weighting_t weighting)
{
    if (!isfinite(normalized) || normalized <= 0.0f) return 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    if (weighting != SPECTRUM_WEIGHT_A) return normalized;

    const float db = VIZ_DB_FLOOR +
        normalized * (VIZ_DB_TOP - VIZ_DB_FLOOR);
    float weighted = (db + spectrum_a_weighting_db(frequency_hz) -
                      VIZ_DB_FLOOR) /
                     (VIZ_DB_TOP - VIZ_DB_FLOOR);
    if (weighted < 0.0f) weighted = 0.0f;
    if (weighted > 1.0f) weighted = 1.0f;
    return weighted;
}
