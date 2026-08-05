/* ============================================================================
 *  fft_map.c - dual-resolution FFT mapped to 256 logarithmic display points
 * ========================================================================== */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "esp_dsp.h"
#include "audio_config.h"
#include "fft_map.h"

#define FFT_SIZE              2048
#define HIGH_HOP               512
#define LOW_DECIMATION           4
#define LOW_DECIMATOR_TAPS       7
#define LOW_HOP                512
#define HIGH_BIN_HZ \
    ((float)AUDIO_SAMPLE_RATE / (float)FFT_SIZE)
#define LOW_SAMPLE_RATE \
    ((float)AUDIO_SAMPLE_RATE / (float)LOW_DECIMATION)
#define LOW_BIN_HZ \
    (LOW_SAMPLE_RATE / (float)FFT_SIZE)
#define HIGH_FRAME_SEC \
    ((float)HIGH_HOP / (float)AUDIO_SAMPLE_RATE)
#define LOW_BLEND_START_HZ    300.0f
#define LOW_BLEND_END_HZ      500.0f

typedef struct {
    float average_ms;
    float attack_ms;
    float release_ms;
    int peak_hold;
    float peak_decay_per_second;
} viz_preset_t;

/* Values are expressed in real time, not frames, so hop sizes can change. */
static const viz_preset_t PRESETS[] = {
    { 65.0f, 0.0f, 220.0f, 1, 0.22f },
    {  0.0f, 0.0f,  55.0f, 0, 4.70f },
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
static float s_high_power[VIZ_POINTS];
static float s_low_power[VIZ_POINTS];
static float s_power_average[VIZ_POINTS];
static float s_display[VIZ_POINTS];
static float s_peak[VIZ_POINTS];

static float s_high_ring[FFT_SIZE];
static float s_low_ring[FFT_SIZE];
static int s_high_ring_pos;
static int s_low_ring_pos;
static int s_high_since_hop;
static int s_low_since_hop;
static int s_high_filled;
static int s_low_filled;
static float s_decimator_history[LOW_DECIMATOR_TAPS];
static int s_decimator_pos;
static int s_decimator_phase;
static int s_low_frame_due;
static int s_low_ready;

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
    return 1.0f - expf(-HIGH_FRAME_SEC / (milliseconds * 0.001f));
}

static void recalc_preset(void)
{
    s_average_coef = smoothing_coef(s_preset.average_ms);
    s_attack_coef = smoothing_coef(s_preset.attack_ms);
    s_release_coef = smoothing_coef(s_preset.release_ms);
    s_peak_decay = s_preset.peak_decay_per_second * HIGH_FRAME_SEC;
}

void fft_map_set_mode(viz_mode_t mode)
{
    if (mode != VIZ_MONITOR && mode != VIZ_DECOR) mode = VIZ_MONITOR;
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
    memset(s_high_power, 0, sizeof(s_high_power));
    memset(s_low_power, 0, sizeof(s_low_power));
    memset(s_power_average, 0, sizeof(s_power_average));
    memset(s_display, 0, sizeof(s_display));
    memset(s_peak, 0, sizeof(s_peak));
    s_high_ring_pos = 0;
    s_low_ring_pos = 0;
    s_high_since_hop = 0;
    s_low_since_hop = 0;
    s_high_filled = 0;
    s_low_filled = 0;
    memset(s_decimator_history, 0, sizeof(s_decimator_history));
    s_decimator_pos = 0;
    s_decimator_phase = 0;
    s_low_frame_due = 0;
    s_low_ready = 0;
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

    int lower = (int)floorf(position);
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

static float combined_power(int band)
{
    if (!s_low_ready || s_center[band] >= LOW_BLEND_END_HZ) {
        return s_high_power[band];
    }
    if (s_center[band] <= LOW_BLEND_START_HZ) {
        return s_low_power[band];
    }

    const float high_weight =
        (s_center[band] - LOW_BLEND_START_HZ) /
        (LOW_BLEND_END_HZ - LOW_BLEND_START_HZ);
    return s_low_power[band] * (1.0f - high_weight) +
           s_high_power[band] * high_weight;
}

static void compute_frame(float *out, float *peak_out)
{
    run_fft(s_high_ring, s_high_ring_pos, HIGH_BIN_HZ,
            VIZ_FREQ_HI_HZ, s_high_power);
    if (s_low_frame_due && s_low_filled >= FFT_SIZE) {
        run_fft(s_low_ring, s_low_ring_pos, LOW_BIN_HZ,
                LOW_BLEND_END_HZ, s_low_power);
        s_low_frame_due = 0;
        s_low_ready = 1;
    }

    for (int band = 0; band < VIZ_POINTS; band++) {
        const float normalized_power = combined_power(band);
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

static void feed_low_sample(float sample)
{
    s_low_ring[s_low_ring_pos] = sample;
    if (++s_low_ring_pos >= FFT_SIZE) s_low_ring_pos = 0;
    if (s_low_filled < FFT_SIZE) s_low_filled++;

    if (++s_low_since_hop >= LOW_HOP) {
        s_low_since_hop = 0;
        if (s_low_filled >= FFT_SIZE) s_low_frame_due = 1;
    }
}

static void feed_decimator(float sample)
{
    static const uint8_t weights[LOW_DECIMATOR_TAPS] = {
        1, 2, 3, 4, 3, 2, 1,
    };

    s_decimator_history[s_decimator_pos] = sample;
    if (++s_decimator_pos >= LOW_DECIMATOR_TAPS) s_decimator_pos = 0;
    if (++s_decimator_phase < LOW_DECIMATION) return;
    s_decimator_phase = 0;

    float filtered = 0.0f;
    int history_index = s_decimator_pos;
    for (int tap = 0; tap < LOW_DECIMATOR_TAPS; tap++) {
        filtered += (float)weights[tap] *
                    s_decimator_history[history_index];
        if (++history_index >= LOW_DECIMATOR_TAPS) history_index = 0;
    }
    feed_low_sample(filtered * (1.0f / 16.0f));
}

int fft_feed(const float *samples, int count, float *out, float *peak_out)
{
    if (!samples || !out || count <= 0) return 0;
    int produced = 0;

    for (int i = 0; i < count; i++) {
        const float sample = samples[i];
        s_high_ring[s_high_ring_pos] = sample;
        if (++s_high_ring_pos >= FFT_SIZE) s_high_ring_pos = 0;
        if (s_high_filled < FFT_SIZE) s_high_filled++;

        feed_decimator(sample);

        if (++s_high_since_hop >= HIGH_HOP) {
            s_high_since_hop = 0;
            if (s_high_filled >= FFT_SIZE) {
                compute_frame(out, peak_out);
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
