/* ============================================================================
 *  fft_map.c - time-aligned multi-resolution FFT for a logarithmic display
 * ========================================================================== */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "esp_dsp.h"
#include "audio_config.h"
#include "fft_map.h"

#define FFT_SIZE                 2048
#define HIGH_HOP                 1024
#define LOW_DECIMATION              4
#define LOW_DECIMATOR_TAPS          7
#define LOW_HOP                    512
#define ULTRA_DECIMATION             4
#define ULTRA_HOP                  128
#define OUTPUT_FRAME_SAMPLES \
    (LOW_HOP * LOW_DECIMATION)
#define OUTPUT_FRAME_SEC \
    ((float)OUTPUT_FRAME_SAMPLES / (float)AUDIO_SAMPLE_RATE)

#define HIGH_BIN_HZ \
    ((float)AUDIO_SAMPLE_RATE / (float)FFT_SIZE)
#define LOW_SAMPLE_RATE \
    ((float)AUDIO_SAMPLE_RATE / (float)LOW_DECIMATION)
#define LOW_BIN_HZ \
    (LOW_SAMPLE_RATE / (float)FFT_SIZE)
#define ULTRA_SAMPLE_RATE \
    (LOW_SAMPLE_RATE / (float)ULTRA_DECIMATION)
#define ULTRA_BIN_HZ \
    (ULTRA_SAMPLE_RATE / (float)FFT_SIZE)

/* Each older spectrum shifts its FFT window center by one branch hop. */
#define HIGH_HISTORY_DEPTH        16
#define LOW_HISTORY_DEPTH          7
#define MONITOR_HIGH_AGE           3
#define REFERENCE_HIGH_AGE        15
#define REFERENCE_LOW_AGE          6

/* Crossfades use log-frequency weights because the display axis is logarithmic. */
#define ULTRA_BLEND_START_HZ     90.0f
#define ULTRA_BLEND_END_HZ      140.0f
#define LOW_BLEND_START_HZ      380.0f
#define LOW_BLEND_END_HZ        520.0f
#define ANALYZER_DC_BLOCK_HZ      2.0f
#define TWO_PI                    6.28318530717958647692f

typedef struct {
    float average_ms;
    float attack_ms;
    float release_ms;
    int peak_hold;
    float peak_decay_per_second;
} viz_preset_t;

typedef struct {
    float history[LOW_DECIMATOR_TAPS];
    int position;
    int phase;
} decimator_t;

/* Values are expressed in real time, not frames, so hop sizes can change. */
static const viz_preset_t PRESETS[] = {
    [VIZ_MONITOR] =   { 65.0f, 0.0f, 220.0f, 1, 0.22f },
    [VIZ_DECOR] =     {  0.0f, 0.0f,  55.0f, 0, 4.70f },
    [VIZ_REFERENCE] = {  0.0f, 0.0f,   0.0f, 0, 0.00f },
};

static viz_mode_t s_mode = VIZ_MONITOR;
static viz_preset_t s_preset;
static float s_average_coef;
static float s_attack_coef;
static float s_release_coef;
static float s_peak_decay;

static float s_band_low[VIZ_POINTS];
static float s_band_high[VIZ_POINTS];
static float s_center[VIZ_POINTS];
static float s_high_history[HIGH_HISTORY_DEPTH][VIZ_POINTS];
static float s_low_history[LOW_HISTORY_DEPTH][VIZ_POINTS];
static float s_ultra_power[VIZ_POINTS];
static int s_high_history_pos;
static int s_low_history_pos;
static int s_high_history_count;
static int s_low_history_count;
static int s_ultra_ready;

static float s_power_average[VIZ_POINTS];
static float s_display[VIZ_POINTS];
static float s_peak[VIZ_POINTS];

static float s_high_ring[FFT_SIZE];
static float s_low_ring[FFT_SIZE];
static float s_ultra_ring[FFT_SIZE];
static int s_high_ring_pos;
static int s_low_ring_pos;
static int s_ultra_ring_pos;
static int s_high_since_hop;
static int s_low_since_hop;
static int s_ultra_since_hop;
static int s_high_filled;
static int s_low_filled;
static int s_ultra_filled;
static decimator_t s_low_decimator;
static decimator_t s_ultra_decimator;

static float s_dc_previous_input;
static float s_dc_previous_output;
static float s_dc_coefficient;

static float s_window[FFT_SIZE];
static float s_window_sum;
#if defined(_MSC_VER)
#define FFT_ALIGNED __declspec(align(16))
#else
#define FFT_ALIGNED __attribute__((aligned(16)))
#endif
static FFT_ALIGNED float s_fft[FFT_SIZE * 2];
#undef FFT_ALIGNED

static float smoothing_coef(float milliseconds)
{
    if (milliseconds <= 0.0f) return 1.0f;
    return 1.0f - expf(-OUTPUT_FRAME_SEC / (milliseconds * 0.001f));
}

static void recalc_preset(void)
{
    s_average_coef = smoothing_coef(s_preset.average_ms);
    s_attack_coef = smoothing_coef(s_preset.attack_ms);
    s_release_coef = smoothing_coef(s_preset.release_ms);
    s_peak_decay = s_preset.peak_decay_per_second * OUTPUT_FRAME_SEC;
}

void fft_map_set_mode(viz_mode_t mode)
{
    if (mode != VIZ_MONITOR && mode != VIZ_DECOR &&
        mode != VIZ_REFERENCE) {
        mode = VIZ_MONITOR;
    }
    s_mode = mode;
    s_preset = PRESETS[(int)mode];
    recalc_preset();
}

viz_mode_t fft_map_get_mode(void)
{
    return s_mode;
}

void fft_map_reset(void)
{
    memset(s_high_ring, 0, sizeof(s_high_ring));
    memset(s_low_ring, 0, sizeof(s_low_ring));
    memset(s_ultra_ring, 0, sizeof(s_ultra_ring));
    memset(s_high_history, 0, sizeof(s_high_history));
    memset(s_low_history, 0, sizeof(s_low_history));
    memset(s_ultra_power, 0, sizeof(s_ultra_power));
    memset(s_power_average, 0, sizeof(s_power_average));
    memset(s_display, 0, sizeof(s_display));
    memset(s_peak, 0, sizeof(s_peak));
    memset(&s_low_decimator, 0, sizeof(s_low_decimator));
    memset(&s_ultra_decimator, 0, sizeof(s_ultra_decimator));

    s_high_ring_pos = 0;
    s_low_ring_pos = 0;
    s_ultra_ring_pos = 0;
    s_high_since_hop = 0;
    s_low_since_hop = 0;
    s_ultra_since_hop = 0;
    s_high_filled = 0;
    s_low_filled = 0;
    s_ultra_filled = 0;
    s_high_history_pos = 0;
    s_low_history_pos = 0;
    s_high_history_count = 0;
    s_low_history_count = 0;
    s_ultra_ready = 0;
    s_dc_previous_input = 0.0f;
    s_dc_previous_output = 0.0f;
}

esp_err_t fft_map_init(void)
{
    esp_err_t err = dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    if (err != ESP_OK) return err;

    dsps_wind_hann_f32(s_window, FFT_SIZE);
    s_window_sum = 0.0f;
    for (int i = 0; i < FFT_SIZE; i++) s_window_sum += s_window[i];
    if (!(s_window_sum > 0.0f)) return ESP_ERR_INVALID_STATE;

    const float ratio = powf(VIZ_FREQ_HI_HZ / VIZ_FREQ_LO_HZ,
                             1.0f / (float)VIZ_POINTS);
    float frequency = VIZ_FREQ_LO_HZ;
    for (int i = 0; i < VIZ_POINTS; i++) {
        s_band_low[i] = frequency;
        frequency *= ratio;
        s_band_high[i] = frequency;
        s_center[i] = sqrtf(s_band_low[i] * s_band_high[i]);
    }

    s_dc_coefficient = expf(
        -TWO_PI * ANALYZER_DC_BLOCK_HZ / (float)AUDIO_SAMPLE_RATE);
    fft_map_reset();
    fft_map_set_mode(s_mode);
    return ESP_OK;
}

static float bin_power(int bin)
{
    if (bin < 1) bin = 1;
    if (bin > FFT_SIZE / 2) bin = FFT_SIZE / 2;
    const float real = s_fft[2 * bin];
    const float imag = s_fft[2 * bin + 1];
    return real * real + imag * imag;
}

static float interpolated_power(float frequency, float bin_hz)
{
    float position = frequency / bin_hz;
    if (position < 1.0f) position = 1.0f;
    if (position > (float)(FFT_SIZE / 2)) {
        position = (float)(FFT_SIZE / 2);
    }

    const int lower = (int)floorf(position);
    int upper = lower + 1;
    if (upper > FFT_SIZE / 2) upper = FFT_SIZE / 2;
    const float fraction = position - (float)lower;
    const float low_power = bin_power(lower);
    const float high_power = bin_power(upper);
    return low_power + fraction * (high_power - low_power);
}

static float mapped_band_power(int band, float bin_hz)
{
    const float low = s_band_low[band];
    const float high = s_band_high[band];
    if (high - low <= bin_hz) {
        return interpolated_power(s_center[band], bin_hz);
    }

    float maximum = interpolated_power(low, bin_hz);
    const float high_edge = interpolated_power(high, bin_hz);
    if (high_edge > maximum) maximum = high_edge;

    int first = (int)ceilf(low / bin_hz);
    int last = (int)floorf(high / bin_hz);
    if (first < 1) first = 1;
    if (last > FFT_SIZE / 2) last = FFT_SIZE / 2;
    for (int bin = first; bin <= last; bin++) {
        const float power = bin_power(bin);
        if (power > maximum) maximum = power;
    }
    return maximum;
}

static void run_fft(const float *ring, int ring_pos, float bin_hz,
                    float maximum_frequency, float *power_out)
{
    int ring_index = ring_pos;
    for (int i = 0; i < FFT_SIZE; i++) {
        s_fft[2 * i] = ring[ring_index] * s_window[i];
        s_fft[2 * i + 1] = 0.0f;
        if (++ring_index >= FFT_SIZE) ring_index = 0;
    }

    dsps_fft2r_fc32(s_fft, FFT_SIZE);
    dsps_bit_rev_fc32(s_fft, FFT_SIZE);

    const float power_scale = 4.0f / (s_window_sum * s_window_sum);
    for (int band = 0; band < VIZ_POINTS; band++) {
        power_out[band] = s_center[band] <= maximum_frequency
            ? mapped_band_power(band, bin_hz) * power_scale
            : 0.0f;
    }
}

static float log_blend_weight(float frequency, float start, float end)
{
    if (frequency <= start) return 0.0f;
    if (frequency >= end) return 1.0f;
    float weight = logf(frequency / start) / logf(end / start);
    return weight * weight * (3.0f - 2.0f * weight);
}

static float blend_power(float low, float high, float weight)
{
    return low * (1.0f - weight) + high * weight;
}

static const float *high_history_at(int age)
{
    if (age < 0 || s_high_history_count <= age) return NULL;
    int index = s_high_history_pos - 1 - age;
    while (index < 0) index += HIGH_HISTORY_DEPTH;
    return s_high_history[index];
}

static const float *low_history_at(int age)
{
    if (age < 0 || s_low_history_count <= age) return NULL;
    int index = s_low_history_pos - 1 - age;
    while (index < 0) index += LOW_HISTORY_DEPTH;
    return s_low_history[index];
}

static void compute_display(const float *high, const float *low,
                            const float *ultra, float *out, float *peak_out)
{
    for (int band = 0; band < VIZ_POINTS; band++) {
        const float frequency = s_center[band];
        float normalized_power;

        if (ultra && frequency < ULTRA_BLEND_END_HZ) {
            const float weight = log_blend_weight(
                frequency, ULTRA_BLEND_START_HZ, ULTRA_BLEND_END_HZ);
            normalized_power = blend_power(ultra[band], low[band], weight);
        } else if (frequency < LOW_BLEND_END_HZ) {
            const float weight = log_blend_weight(
                frequency, LOW_BLEND_START_HZ, LOW_BLEND_END_HZ);
            normalized_power = blend_power(low[band], high[band], weight);
        } else {
            normalized_power = high[band];
        }

        s_power_average[band] += s_average_coef *
            (normalized_power - s_power_average[band]);
        const float db = 10.0f *
            log10f(fmaxf(s_power_average[band], 1e-12f));
        float normalized = (db - VIZ_DB_FLOOR) /
                           (VIZ_DB_TOP - VIZ_DB_FLOOR);
        if (normalized < 0.0f) normalized = 0.0f;
        else if (normalized > 1.0f) normalized = 1.0f;

        const float coefficient = normalized > s_display[band]
            ? s_attack_coef : s_release_coef;
        s_display[band] += coefficient * (normalized - s_display[band]);
        out[band] = s_display[band];

        if (s_preset.peak_hold) {
            if (s_display[band] > s_peak[band]) s_peak[band] = s_display[band];
            else {
                s_peak[band] -= s_peak_decay;
                if (s_peak[band] < 0.0f) s_peak[band] = 0.0f;
            }
        } else {
            s_peak[band] = 0.0f;
        }
        if (peak_out) peak_out[band] = s_peak[band];
    }
}

static void ring_push(float *ring, int *position, int *filled, float sample)
{
    ring[*position] = sample;
    if (++(*position) >= FFT_SIZE) *position = 0;
    if (*filled < FFT_SIZE) (*filled)++;
}

static int decimator_feed(decimator_t *decimator, int factor,
                          float sample, float *out)
{
    static const uint8_t weights[LOW_DECIMATOR_TAPS] = {
        1, 2, 3, 4, 3, 2, 1,
    };

    decimator->history[decimator->position] = sample;
    if (++decimator->position >= LOW_DECIMATOR_TAPS) {
        decimator->position = 0;
    }
    if (++decimator->phase < factor) return 0;
    decimator->phase = 0;

    float filtered = 0.0f;
    int history_index = decimator->position;
    for (int tap = 0; tap < LOW_DECIMATOR_TAPS; tap++) {
        filtered += (float)weights[tap] *
                    decimator->history[history_index];
        if (++history_index >= LOW_DECIMATOR_TAPS) history_index = 0;
    }
    *out = filtered * (1.0f / 16.0f);
    return 1;
}

static float dc_block(float sample)
{
    const float output = sample - s_dc_previous_input +
                         s_dc_coefficient * s_dc_previous_output;
    s_dc_previous_input = sample;
    s_dc_previous_output = output;
    return output;
}

static int feed_high(float sample)
{
    ring_push(s_high_ring, &s_high_ring_pos, &s_high_filled, sample);
    if (++s_high_since_hop < HIGH_HOP) return 0;
    s_high_since_hop = 0;
    if (s_high_filled < FFT_SIZE) return 0;

    run_fft(s_high_ring, s_high_ring_pos, HIGH_BIN_HZ,
            VIZ_FREQ_HI_HZ, s_high_history[s_high_history_pos]);
    if (++s_high_history_pos >= HIGH_HISTORY_DEPTH) {
        s_high_history_pos = 0;
    }
    if (s_high_history_count < HIGH_HISTORY_DEPTH) {
        s_high_history_count++;
    }
    return 1;
}

static int feed_low(float sample)
{
    ring_push(s_low_ring, &s_low_ring_pos, &s_low_filled, sample);
    if (++s_low_since_hop < LOW_HOP) return 0;
    s_low_since_hop = 0;
    if (s_low_filled < FFT_SIZE) return 0;

    run_fft(s_low_ring, s_low_ring_pos, LOW_BIN_HZ,
            LOW_BLEND_END_HZ, s_low_history[s_low_history_pos]);
    if (++s_low_history_pos >= LOW_HISTORY_DEPTH) s_low_history_pos = 0;
    if (s_low_history_count < LOW_HISTORY_DEPTH) s_low_history_count++;
    return 1;
}

static int feed_ultra(float sample)
{
    ring_push(s_ultra_ring, &s_ultra_ring_pos, &s_ultra_filled, sample);
    if (++s_ultra_since_hop < ULTRA_HOP) return 0;
    s_ultra_since_hop = 0;
    if (s_ultra_filled < FFT_SIZE) return 0;
    if (s_mode != VIZ_REFERENCE) return 0;

    run_fft(s_ultra_ring, s_ultra_ring_pos, ULTRA_BIN_HZ,
            ULTRA_BLEND_END_HZ, s_ultra_power);
    s_ultra_ready = 1;
    return 1;
}

int fft_feed(const float *samples, int count, float *out, float *peak_out)
{
    if (!samples || !out || count <= 0) return 0;
    int produced = 0;

    for (int i = 0; i < count; i++) {
        const float sample = dc_block(samples[i]);
        feed_high(sample);

        float low_sample;
        int low_frame = 0;
        int ultra_frame = 0;
        if (decimator_feed(&s_low_decimator, LOW_DECIMATION,
                           sample, &low_sample)) {
            low_frame = feed_low(low_sample);

            float ultra_sample;
            if (decimator_feed(&s_ultra_decimator,
                               ULTRA_DECIMATION,
                               low_sample, &ultra_sample)) {
                ultra_frame = feed_ultra(ultra_sample);
            }
        }

        if (s_mode == VIZ_REFERENCE && ultra_frame && s_ultra_ready) {
            const float *high = high_history_at(REFERENCE_HIGH_AGE);
            const float *low = low_history_at(REFERENCE_LOW_AGE);
            if (high && low) {
                compute_display(high, low, s_ultra_power, out, peak_out);
                produced = 1;
            }
        } else if (low_frame) {
            const float *high = high_history_at(MONITOR_HIGH_AGE);
            const float *low = low_history_at(0);
            if (high && low) {
                compute_display(high, low, NULL, out, peak_out);
                produced = 1;
            }
        }
    }
    return produced;
}

int fft_map_num_points(void)
{
    return VIZ_POINTS;
}

float fft_map_frequency_at(int index)
{
    return index >= 0 && index < VIZ_POINTS ? s_center[index] : 0.0f;
}

float fft_map_db_to_norm(float db)
{
    if (!isfinite(db)) return 0.0f;
    const float normalized = (db - VIZ_DB_FLOOR) /
                             (VIZ_DB_TOP - VIZ_DB_FLOOR);
    return normalized < 0.0f ? 0.0f :
           (normalized > 1.0f ? 1.0f : normalized);
}
