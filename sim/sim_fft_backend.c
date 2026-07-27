#include "esp_dsp.h"

#include <math.h>
#include <stdbool.h>

#define SIM_FFT_TWO_PI 6.28318530717958647692f

static int s_max_fft_size;

static bool is_power_of_two(int value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

esp_err_t dsps_fft2r_init_fc32(float *table, int table_size)
{
    (void)table;
    if (!is_power_of_two(table_size)) return ESP_ERR_INVALID_SIZE;
    s_max_fft_size = table_size;
    return ESP_OK;
}

void dsps_wind_hann_f32(float *window, int length)
{
    if (!window || length < 2) return;

    const float scale = 1.0f / (float)(length - 1);
    for (int i = 0; i < length; i++) {
        window[i] = 0.5f *
                    (1.0f - cosf(SIM_FFT_TWO_PI * (float)i * scale));
    }
}

esp_err_t dsps_fft2r_fc32(float *data, int length)
{
    if (!data || !is_power_of_two(length) || length > s_max_fft_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    /* Decimation in frequency leaves bins in bit-reversed order, matching
     * ESP-DSP's contract before dsps_bit_rev_fc32(). */
    for (int span = length; span >= 2; span >>= 1) {
        const int half = span >> 1;
        for (int base = 0; base < length; base += span) {
            for (int i = 0; i < half; i++) {
                const int even = base + i;
                const int odd = even + half;
                const float even_real = data[2 * even];
                const float even_imag = data[2 * even + 1];
                const float odd_real = data[2 * odd];
                const float odd_imag = data[2 * odd + 1];
                const float difference_real = even_real - odd_real;
                const float difference_imag = even_imag - odd_imag;
                const float phase =
                    -SIM_FFT_TWO_PI * (float)i / (float)span;
                const float cosine = cosf(phase);
                const float sine = sinf(phase);

                data[2 * even] = even_real + odd_real;
                data[2 * even + 1] = even_imag + odd_imag;
                data[2 * odd] =
                    difference_real * cosine - difference_imag * sine;
                data[2 * odd + 1] =
                    difference_real * sine + difference_imag * cosine;
            }
        }
    }
    return ESP_OK;
}

esp_err_t dsps_bit_rev_fc32(float *data, int length)
{
    if (!data || !is_power_of_two(length)) return ESP_ERR_INVALID_SIZE;

    int reversed = 0;
    for (int i = 1; i < length - 1; i++) {
        int bit = length >> 1;
        while (reversed & bit) {
            reversed ^= bit;
            bit >>= 1;
        }
        reversed ^= bit;
        if (i < reversed) {
            const float real = data[2 * i];
            const float imag = data[2 * i + 1];
            data[2 * i] = data[2 * reversed];
            data[2 * i + 1] = data[2 * reversed + 1];
            data[2 * reversed] = real;
            data[2 * reversed + 1] = imag;
        }
    }
    return ESP_OK;
}
