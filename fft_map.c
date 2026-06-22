/* ============================================================================
 *  fft_map.c  —  normalized 2048-point FFT to 256 log display points
 * ========================================================================== */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "esp_dsp.h"
#include "audio_config.h"
#include "fft_map.h"

#define FFT_SIZE   2048
#define HOP        512
#define F_LO       20.0f
#define F_HI       20000.0f
#define BIN_HZ     ((float)AUDIO_SAMPLE_RATE / FFT_SIZE)
#define DB_FLOOR   (-60.0f)
#define DB_TOP     0.0f
#define FRAME_SEC  ((float)HOP / AUDIO_SAMPLE_RATE)

typedef struct {
    float tilt_db_oct;
    float average_ms;
    float attack_ms;
    float release_ms;
    int peak_hold;
    float peak_decay_per_second;
} viz_preset_t;

/* Values are expressed in real time, not frames, so HOP can change safely. */
static const viz_preset_t PRESETS[] = {
    { 0.0f, 130.0f, 0.0f,  0.0f, 1, 1.40f },  /* monitor */
    { 4.0f,   0.0f, 0.0f, 48.0f, 0, 4.70f },  /* decorative */
};

static viz_mode_t s_mode = VIZ_MONITOR;
static viz_preset_t s_preset;
static float s_average_coef;
static float s_attack_coef;
static float s_release_coef;
static float s_peak_decay;

static uint16_t s_lo[VIZ_POINTS];
static uint16_t s_hi[VIZ_POINTS];
static float s_center[VIZ_POINTS];
static float s_tilt[VIZ_POINTS];
static float s_power_average[VIZ_POINTS];
static float s_display[VIZ_POINTS];
static float s_peak[VIZ_POINTS];

static float s_ring[FFT_SIZE];
static int s_ring_pos;
static int s_since_hop;
static int s_filled;
static float s_window[FFT_SIZE];
static float s_window_sum;
static __attribute__((aligned(16))) float s_fft[FFT_SIZE * 2];

static float smoothing_coef(float milliseconds)
{
    if (milliseconds <= 0.0f) return 1.0f;
    return 1.0f - expf(-FRAME_SEC / (milliseconds * 0.001f));
}

static void recalc_preset(void)
{
    s_average_coef = smoothing_coef(s_preset.average_ms);
    s_attack_coef = smoothing_coef(s_preset.attack_ms);
    s_release_coef = smoothing_coef(s_preset.release_ms);
    s_peak_decay = s_preset.peak_decay_per_second * FRAME_SEC;
    for (int i = 0; i < VIZ_POINTS; i++) {
        s_tilt[i] = s_preset.tilt_db_oct * log2f(s_center[i] / F_LO);
    }
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
    memset(s_ring, 0, sizeof(s_ring));
    memset(s_power_average, 0, sizeof(s_power_average));
    memset(s_display, 0, sizeof(s_display));
    memset(s_peak, 0, sizeof(s_peak));
    s_ring_pos = 0;
    s_since_hop = 0;
    s_filled = 0;
}

esp_err_t fft_map_init(void)
{
    esp_err_t err = dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    if (err != ESP_OK) return err;

    dsps_wind_hann_f32(s_window, FFT_SIZE);
    s_window_sum = 0.0f;
    for (int i = 0; i < FFT_SIZE; i++) s_window_sum += s_window[i];
    if (!(s_window_sum > 0.0f)) return ESP_ERR_INVALID_STATE;

    float ratio = powf(F_HI / F_LO, 1.0f / VIZ_POINTS);
    float frequency = F_LO;
    for (int i = 0; i < VIZ_POINTS; i++) {
        float low_frequency = frequency;
        float high_frequency = frequency * ratio;
        frequency = high_frequency;

        /* Half-open frequency ranges avoid counting every boundary bin twice. */
        int low_bin = (int)ceilf(low_frequency / BIN_HZ);
        int high_bin = (int)ceilf(high_frequency / BIN_HZ) - 1;
        if (low_bin < 1) low_bin = 1;
        if (high_bin < low_bin) high_bin = low_bin;
        if (high_bin > FFT_SIZE / 2) high_bin = FFT_SIZE / 2;
        if (low_bin > high_bin) low_bin = high_bin;

        s_lo[i] = (uint16_t)low_bin;
        s_hi[i] = (uint16_t)high_bin;
        s_center[i] = sqrtf(low_frequency * high_frequency);
    }

    fft_map_reset();
    fft_map_set_mode(s_mode);
    return ESP_OK;
}

static void compute_frame(float *out, float *peak_out)
{
    int ring_index = s_ring_pos;
    for (int i = 0; i < FFT_SIZE; i++) {
        s_fft[2 * i] = s_ring[ring_index] * s_window[i];
        s_fft[2 * i + 1] = 0.0f;
        if (++ring_index >= FFT_SIZE) ring_index = 0;
    }

    dsps_fft2r_fc32(s_fft, FFT_SIZE);
    dsps_bit_rev_fc32(s_fft, FFT_SIZE);

    const float power_scale = 4.0f / (s_window_sum * s_window_sum);
    for (int band = 0; band < VIZ_POINTS; band++) {
        float max_power = 0.0f;
        for (int bin = s_lo[band]; bin <= s_hi[band]; bin++) {
            float real = s_fft[2 * bin];
            float imag = s_fft[2 * bin + 1];
            float power = real * real + imag * imag;
            if (power > max_power) max_power = power;
        }

        float normalized_power = max_power * power_scale;
        s_power_average[band] += s_average_coef *
                                 (normalized_power - s_power_average[band]);

        float db = 10.0f * log10f(fmaxf(s_power_average[band], 1e-12f)) +
                   s_tilt[band];
        float normalized = (db - DB_FLOOR) / (DB_TOP - DB_FLOOR);
        if (normalized < 0.0f) normalized = 0.0f;
        else if (normalized > 1.0f) normalized = 1.0f;

        float coefficient = normalized > s_display[band]
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

int fft_feed(const float *samples, int count, float *out, float *peak_out)
{
    if (!samples || !out || count <= 0) return 0;
    int produced = 0;

    for (int i = 0; i < count; i++) {
        s_ring[s_ring_pos] = samples[i];
        if (++s_ring_pos >= FFT_SIZE) s_ring_pos = 0;
        if (s_filled < FFT_SIZE) s_filled++;

        if (++s_since_hop >= HOP) {
            s_since_hop = 0;
            if (s_filled >= FFT_SIZE) {
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

float fft_map_db_to_norm(float db)
{
    if (!isfinite(db)) return 0.0f;
    float normalized = (db - DB_FLOOR) / (DB_TOP - DB_FLOOR);
    return normalized < 0.0f ? 0.0f : (normalized > 1.0f ? 1.0f : normalized);
}
