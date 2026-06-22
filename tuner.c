/* ============================================================================
 *  tuner.c  —  anti-aliased decimation + MPM/NSDF pitch detector
 * ========================================================================== */
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#include "audio_config.h"
#include "tuner.h"

#define DEC               4
#define SR                (AUDIO_SAMPLE_RATE / DEC)
#define WIN               1024
#define HOP               256
#define FMIN              58.0f
#define FMAX              1100.0f
#define TAU_CALC_MIN      10
#define TAU_SEARCH_MIN    11
#define TAU_SEARCH_MAX    207
#define TAU_CALC_MAX      208
#define CLARITY_THRESHOLD 0.50f
#define MIN_RMS           0.0002f
#define A4                440.0f

#define FIR_TAPS          63
#define FIR_CUTOFF_HZ     1800.0f
#define PI_F              3.14159265358979323846f

static const char *NOTE[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static float s_fir_coeff[FIR_TAPS];
static float s_fir_state[FIR_TAPS];
static int s_fir_pos;
static int s_coeff_ready;
static int s_dec_count;

static float s_ring[WIN];
static int s_pos;
static int s_since;
static int s_filled;
static float s_window[WIN];
static float s_nsdf[TAU_CALC_MAX + 1];

static tuner_result_t s_result;
static _Atomic unsigned s_result_seq;

static void publish_result(const tuner_result_t *result)
{
    atomic_fetch_add_explicit(&s_result_seq, 1U, memory_order_acq_rel);
    s_result = *result;
    atomic_fetch_add_explicit(&s_result_seq, 1U, memory_order_release);
}

static void publish_unvoiced(float clarity)
{
    const tuner_result_t result = {
        .voiced = 0,
        .f0 = 0.0f,
        .name = "-",
        .octave = 0,
        .cents = 0.0f,
        .clarity = clarity,
    };
    publish_result(&result);
}

static void fir_init(void)
{
    float sum = 0.0f;
    float normalized_cutoff = FIR_CUTOFF_HZ / AUDIO_SAMPLE_RATE;
    int center = (FIR_TAPS - 1) / 2;

    for (int n = 0; n < FIR_TAPS; n++) {
        int m = n - center;
        float ideal = m == 0
                      ? 2.0f * normalized_cutoff
                      : sinf(2.0f * PI_F * normalized_cutoff * m) / (PI_F * m);
        float hamming = 0.54f - 0.46f *
                        cosf(2.0f * PI_F * n / (FIR_TAPS - 1));
        s_fir_coeff[n] = ideal * hamming;
        sum += s_fir_coeff[n];
    }
    for (int n = 0; n < FIR_TAPS; n++) s_fir_coeff[n] /= sum;
    s_coeff_ready = 1;
}

static float fir_process(float sample)
{
    s_fir_state[s_fir_pos] = sample;
    float output = 0.0f;
    int index = s_fir_pos;
    for (int tap = 0; tap < FIR_TAPS; tap++) {
        output += s_fir_coeff[tap] * s_fir_state[index];
        if (--index < 0) index = FIR_TAPS - 1;
    }
    if (++s_fir_pos >= FIR_TAPS) s_fir_pos = 0;
    return output;
}

void tuner_reset(void)
{
    if (!s_coeff_ready) fir_init();
    memset(s_fir_state, 0, sizeof(s_fir_state));
    memset(s_ring, 0, sizeof(s_ring));
    memset(s_nsdf, 0, sizeof(s_nsdf));
    s_fir_pos = 0;
    s_dec_count = 0;
    s_pos = 0;
    s_since = 0;
    s_filled = 0;
    publish_unvoiced(0.0f);
}

void tuner_init(void)
{
    fir_init();
    tuner_reset();
}

static int is_local_maximum(int tau)
{
    return s_nsdf[tau] > s_nsdf[tau - 1] &&
           s_nsdf[tau] >= s_nsdf[tau + 1];
}

static void analyze(void)
{
    int index = s_pos;
    float mean = 0.0f;
    for (int i = 0; i < WIN; i++) {
        s_window[i] = s_ring[index];
        mean += s_window[i];
        if (++index >= WIN) index = 0;
    }
    mean /= WIN;

    float energy = 0.0f;
    for (int i = 0; i < WIN; i++) {
        s_window[i] -= mean;
        energy += s_window[i] * s_window[i];
    }
    float rms = sqrtf(energy / WIN);
    if (!(rms >= MIN_RMS)) {
        publish_unvoiced(0.0f);
        return;
    }

    for (int tau = TAU_CALC_MIN; tau <= TAU_CALC_MAX; tau++) {
        float autocorrelation = 0.0f;
        float normalization = 0.0f;
        int count = WIN - tau;
        for (int i = 0; i < count; i++) {
            float a = s_window[i];
            float b = s_window[i + tau];
            autocorrelation += a * b;
            normalization += a * a + b * b;
        }
        s_nsdf[tau] = normalization > 1e-12f
                      ? 2.0f * autocorrelation / normalization : 0.0f;
    }

    float max_peak = -1.0f;
    for (int tau = TAU_SEARCH_MIN; tau <= TAU_SEARCH_MAX; tau++) {
        if (is_local_maximum(tau) && s_nsdf[tau] > max_peak) {
            max_peak = s_nsdf[tau];
        }
    }
    if (max_peak < CLARITY_THRESHOLD) {
        publish_unvoiced(max_peak > 0.0f ? max_peak : 0.0f);
        return;
    }

    float threshold = 0.90f * max_peak;
    int best = -1;
    for (int tau = TAU_SEARCH_MIN; tau <= TAU_SEARCH_MAX; tau++) {
        if (is_local_maximum(tau) && s_nsdf[tau] >= threshold) {
            best = tau;
            break;
        }
    }
    if (best < 0) {
        publish_unvoiced(max_peak);
        return;
    }

    float left = s_nsdf[best - 1];
    float center = s_nsdf[best];
    float right = s_nsdf[best + 1];
    float denominator = left - 2.0f * center + right;
    float shift = fabsf(denominator) > 1e-9f
                  ? 0.5f * (left - right) / denominator : 0.0f;
    if (shift < -0.5f) shift = -0.5f;
    else if (shift > 0.5f) shift = 0.5f;

    float f0 = (float)SR / (best + shift);
    if (!isfinite(f0) || f0 < FMIN || f0 > FMAX) {
        publish_unvoiced(max_peak);
        return;
    }

    float midi = 69.0f + 12.0f * log2f(f0 / A4);
    int note = (int)lroundf(midi);
    const tuner_result_t result = {
        .voiced = 1,
        .f0 = f0,
        .name = NOTE[((note % 12) + 12) % 12],
        .octave = note / 12 - 1,
        .cents = (midi - note) * 100.0f,
        .clarity = max_peak,
    };
    publish_result(&result);
}

int tuner_feed(const float *samples, int count)
{
    if (!samples || count <= 0) return 0;
    int produced = 0;

    for (int i = 0; i < count; i++) {
        float filtered = fir_process(samples[i]);
        if (++s_dec_count < DEC) continue;
        s_dec_count = 0;

        s_ring[s_pos] = filtered;
        if (++s_pos >= WIN) s_pos = 0;
        if (s_filled < WIN) s_filled++;

        if (++s_since >= HOP) {
            s_since = 0;
            if (s_filled >= WIN) {
                analyze();
                produced = 1;
            }
        }
    }
    return produced;
}

void tuner_get(tuner_result_t *out)
{
    if (!out) return;
    for (;;) {
        unsigned before = atomic_load_explicit(&s_result_seq, memory_order_acquire);
        if (before & 1U) continue;
        *out = s_result;
        atomic_thread_fence(memory_order_acquire);
        unsigned after = atomic_load_explicit(&s_result_seq, memory_order_acquire);
        if (before == after && !(after & 1U)) return;
    }
}
