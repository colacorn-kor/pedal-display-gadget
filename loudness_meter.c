#include "loudness_meter.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "audio_config.h"

#define CHUNK_SAMPLES (AUDIO_SAMPLE_RATE / 10)
#define MOMENTARY_CHUNKS 4
#define SHORT_TERM_CHUNKS 30
#define ABSOLUTE_GATE_LUFS (-70.0f)
#define RELATIVE_GATE_OFFSET_LU (-10.0f)
#define HISTOGRAM_TOP_LUFS 10.0f
#define HISTOGRAM_STEP_LU 0.1f
#define HISTOGRAM_BINS 801
#define TRUE_PEAK_FACTOR 4
#define TRUE_PEAK_TAPS 16
#define PI_D 3.1415926535897932384626433832795

typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float x1, x2;
    float y1, y2;
} biquad_t;

/* ITU-R BS.1770 K-weighting coefficients for 48 kHz. */
static biquad_t s_prefilter = {
    1.53512490f, -2.69169617f, 1.19839287f,
    -1.69065928f, 0.73248076f, 0, 0, 0, 0,
};
static biquad_t s_rlb = {
    1.0f, -2.0f, 1.0f,
    -1.99004745f, 0.99007225f, 0, 0, 0, 0,
};

static double s_chunk_energy;
static int s_chunk_samples;
static double s_chunks[SHORT_TERM_CHUNKS];
static int s_chunk_pos;
static int s_chunk_count;
static float s_histogram_energy[HISTOGRAM_BINS];
static uint32_t s_histogram_count[HISTOGRAM_BINS];
static uint32_t s_absolute_blocks;
static double s_absolute_energy;
static uint64_t s_total_samples;
static float s_true_coeff[TRUE_PEAK_FACTOR][TRUE_PEAK_TAPS];
static float s_true_ring[TRUE_PEAK_TAPS];
static int s_true_pos;
static int s_true_count;
static float s_true_peak;
static int s_coeff_ready;
static loudness_snapshot_t s_snapshot;

static float biquad_process(biquad_t *filter, float input)
{
    const float output = filter->b0 * input +
        filter->b1 * filter->x1 + filter->b2 * filter->x2 -
        filter->a1 * filter->y1 - filter->a2 * filter->y2;
    filter->x2 = filter->x1;
    filter->x1 = input;
    filter->y2 = filter->y1;
    filter->y1 = output;
    return output;
}

static float energy_to_lufs(double energy)
{
    if (!(energy > 0.0) || !isfinite(energy)) {
        return LOUDNESS_FLOOR_LUFS;
    }
    const double value = -0.691 + 10.0 * log10(energy);
    return value < LOUDNESS_FLOOR_LUFS
        ? LOUDNESS_FLOOR_LUFS : (float)value;
}

static double lufs_to_energy(float lufs)
{
    return pow(10.0, ((double)lufs + 0.691) / 10.0);
}

static double sinc(double value)
{
    if (fabs(value) < 1e-12) return 1.0;
    return sin(PI_D * value) / (PI_D * value);
}

static void true_peak_init(void)
{
    const double center = (TRUE_PEAK_TAPS - 1) * 0.5;
    for (int phase = 0; phase < TRUE_PEAK_FACTOR; phase++) {
        double sum = 0.0;
        const double fraction = (double)phase / TRUE_PEAK_FACTOR;
        for (int tap = 0; tap < TRUE_PEAK_TAPS; tap++) {
            const double window = 0.42 -
                0.5 * cos(2.0 * PI_D * tap / (TRUE_PEAK_TAPS - 1)) +
                0.08 * cos(4.0 * PI_D * tap / (TRUE_PEAK_TAPS - 1));
            const double coefficient =
                sinc((double)tap - center + fraction) * window;
            s_true_coeff[phase][tap] = (float)coefficient;
            sum += coefficient;
        }
        if (fabs(sum) > 1e-12) {
            for (int tap = 0; tap < TRUE_PEAK_TAPS; tap++) {
                s_true_coeff[phase][tap] = (float)(
                    (double)s_true_coeff[phase][tap] / sum);
            }
        }
    }
    s_coeff_ready = 1;
}

static void true_peak_feed(float sample)
{
    s_true_ring[s_true_pos] = sample;
    if (++s_true_pos >= TRUE_PEAK_TAPS) s_true_pos = 0;
    if (s_true_count < TRUE_PEAK_TAPS) s_true_count++;

    const float sample_magnitude = fabsf(sample);
    if (sample_magnitude > s_true_peak) s_true_peak = sample_magnitude;
    if (s_true_count < TRUE_PEAK_TAPS) return;

    for (int phase = 0; phase < TRUE_PEAK_FACTOR; phase++) {
        float interpolated = 0.0f;
        int index = s_true_pos - 1;
        if (index < 0) index = TRUE_PEAK_TAPS - 1;
        for (int tap = 0; tap < TRUE_PEAK_TAPS; tap++) {
            interpolated += s_true_coeff[phase][tap] * s_true_ring[index];
            if (--index < 0) index = TRUE_PEAK_TAPS - 1;
        }
        const float magnitude = fabsf(interpolated);
        if (magnitude > s_true_peak) s_true_peak = magnitude;
    }
}

static double recent_energy(int chunks)
{
    if (chunks <= 0 || s_chunk_count < chunks) return 0.0;
    double energy = 0.0;
    int index = s_chunk_pos - 1;
    if (index < 0) index = SHORT_TERM_CHUNKS - 1;
    for (int i = 0; i < chunks; i++) {
        energy += s_chunks[index];
        if (--index < 0) index = SHORT_TERM_CHUNKS - 1;
    }
    return energy / ((double)chunks * CHUNK_SAMPLES);
}

static int histogram_index(float lufs)
{
    int index = (int)floorf(
        (lufs - ABSOLUTE_GATE_LUFS) / HISTOGRAM_STEP_LU);
    if (index < 0) return -1;
    if (index >= HISTOGRAM_BINS) return HISTOGRAM_BINS - 1;
    return index;
}

static void update_integrated(double block_energy)
{
    const float block_lufs = energy_to_lufs(block_energy);
    if (block_lufs < ABSOLUTE_GATE_LUFS) return;

    const int bin = histogram_index(block_lufs);
    if (bin < 0) return;
    s_histogram_energy[bin] += (float)block_energy;
    s_histogram_count[bin]++;
    s_absolute_energy += block_energy;
    s_absolute_blocks++;

    const double absolute_mean =
        s_absolute_energy / (double)s_absolute_blocks;
    const float relative_gate =
        energy_to_lufs(absolute_mean) + RELATIVE_GATE_OFFSET_LU;
    const double relative_energy = lufs_to_energy(relative_gate);
    double gated_energy = 0.0;
    uint32_t gated_blocks = 0;

    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        if (s_histogram_count[i] == 0) continue;
        const double mean = s_histogram_energy[i] /
            (double)s_histogram_count[i];
        if (mean < relative_energy) continue;
        gated_energy += s_histogram_energy[i];
        gated_blocks += s_histogram_count[i];
    }

    s_snapshot.relative_gate_lufs = relative_gate;
    s_snapshot.integrated_blocks = gated_blocks;
    s_snapshot.integrated_lufs = gated_blocks > 0
        ? energy_to_lufs(gated_energy / gated_blocks)
        : LOUDNESS_FLOOR_LUFS;
}

static void finish_chunk(void)
{
    s_chunks[s_chunk_pos] = s_chunk_energy;
    if (++s_chunk_pos >= SHORT_TERM_CHUNKS) s_chunk_pos = 0;
    if (s_chunk_count < SHORT_TERM_CHUNKS) s_chunk_count++;

    if (s_chunk_count >= MOMENTARY_CHUNKS) {
        const double momentary = recent_energy(MOMENTARY_CHUNKS);
        s_snapshot.momentary_lufs = energy_to_lufs(momentary);
        update_integrated(momentary);
    }
    if (s_chunk_count >= SHORT_TERM_CHUNKS) {
        s_snapshot.short_term_lufs =
            energy_to_lufs(recent_energy(SHORT_TERM_CHUNKS));
    }

    s_chunk_energy = 0.0;
    s_chunk_samples = 0;
}

void loudness_meter_init(void)
{
    if (!s_coeff_ready) true_peak_init();
    loudness_meter_reset();
}

void loudness_meter_reset(void)
{
    if (!s_coeff_ready) true_peak_init();
    s_prefilter.x1 = s_prefilter.x2 = 0.0;
    s_prefilter.y1 = s_prefilter.y2 = 0.0;
    s_rlb.x1 = s_rlb.x2 = 0.0;
    s_rlb.y1 = s_rlb.y2 = 0.0;
    s_chunk_energy = 0.0;
    s_chunk_samples = 0;
    memset(s_chunks, 0, sizeof(s_chunks));
    s_chunk_pos = 0;
    s_chunk_count = 0;
    memset(s_histogram_energy, 0, sizeof(s_histogram_energy));
    memset(s_histogram_count, 0, sizeof(s_histogram_count));
    s_absolute_blocks = 0;
    s_absolute_energy = 0.0;
    s_total_samples = 0;
    memset(s_true_ring, 0, sizeof(s_true_ring));
    s_true_pos = 0;
    s_true_count = 0;
    s_true_peak = 0.0;
    s_snapshot = (loudness_snapshot_t) {
        .momentary_lufs = LOUDNESS_FLOOR_LUFS,
        .short_term_lufs = LOUDNESS_FLOOR_LUFS,
        .integrated_lufs = LOUDNESS_FLOOR_LUFS,
        .true_peak_dbtp = LOUDNESS_FLOOR_LUFS,
        .relative_gate_lufs = ABSOLUTE_GATE_LUFS,
    };
}

void loudness_meter_feed(const float *samples, int count)
{
    if (!samples || count <= 0) return;
    for (int i = 0; i < count; i++) {
        float sample = isfinite(samples[i]) ? samples[i] : 0.0f;
        true_peak_feed(sample);
        const float weighted = biquad_process(
            &s_rlb, biquad_process(&s_prefilter, sample));
        s_chunk_energy += (double)weighted * weighted;
        s_chunk_samples++;
        s_total_samples++;
        if (s_chunk_samples >= CHUNK_SAMPLES) finish_chunk();
    }

    s_snapshot.true_peak_linear = (float)s_true_peak;
    s_snapshot.true_peak_dbtp = s_true_peak > 0.0
        ? (float)(20.0 * log10(s_true_peak))
        : LOUDNESS_FLOOR_LUFS;
    s_snapshot.elapsed_ms = (uint32_t)(
        s_total_samples * 1000U / AUDIO_SAMPLE_RATE);
}

void loudness_meter_get(loudness_snapshot_t *out)
{
    if (out) *out = s_snapshot;
}
