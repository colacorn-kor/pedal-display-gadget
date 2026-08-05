#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio_config.h"
#include "fft_map.h"

#define TEST_BLOCK_SIZE 256
#define TEST_TWO_PI 6.28318530717958647692f

static int fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static void feed_tone(float frequency, float amplitude, int sample_count,
                      int *sample_cursor, float *bars, float *peaks)
{
    float block[TEST_BLOCK_SIZE];

    while (sample_count > 0) {
        int count = sample_count < TEST_BLOCK_SIZE
                    ? sample_count : TEST_BLOCK_SIZE;
        for (int i = 0; i < count; i++) {
            float phase = TEST_TWO_PI * frequency *
                          (float)(*sample_cursor) /
                          (float)AUDIO_SAMPLE_RATE;
            block[i] = amplitude * sinf(phase);
            (*sample_cursor)++;
        }
        fft_feed(block, count, bars, peaks);
        sample_count -= count;
    }
}

static int strongest_band(const float *bars)
{
    int strongest = 0;
    for (int i = 1; i < VIZ_POINTS; i++) {
        if (bars[i] > bars[strongest]) strongest = i;
    }
    return strongest;
}

static int check_tone_shape(float tone_hz, float minimum_peak_hz,
                            float maximum_peak_hz, float maximum_width_hz,
                            float *bars, float *peaks)
{
    int sample_cursor = 0;
    memset(bars, 0, sizeof(float) * VIZ_POINTS);
    memset(peaks, 0, sizeof(float) * VIZ_POINTS);
    fft_map_reset();
    feed_tone(tone_hz, 0.5f, AUDIO_SAMPLE_RATE,
              &sample_cursor, bars, peaks);

    const int strongest = strongest_band(bars);
    const float peak_hz = fft_map_frequency_at(strongest);
    if (peak_hz < minimum_peak_hz || peak_hz > maximum_peak_hz) {
        fprintf(stderr, "tone %.1fHz peaked at %.1fHz\n", tone_hz, peak_hz);
        return fail("tone peak mapped to wrong frequency");
    }

    const float threshold = bars[strongest] -
        12.0f / (VIZ_DB_TOP - VIZ_DB_FLOOR);
    int first = strongest;
    int last = strongest;
    while (first > 0 && bars[first - 1] >= threshold) first--;
    while (last + 1 < VIZ_POINTS && bars[last + 1] >= threshold) last++;
    const float width_hz = fft_map_frequency_at(last) -
                           fft_map_frequency_at(first);
    if (width_hz > maximum_width_hz) {
        fprintf(stderr,
                "tone %.1fHz -12dB width %.1fHz (%0.1f..%0.1fHz)\n",
                tone_hz, width_hz,
                fft_map_frequency_at(first), fft_map_frequency_at(last));
        return fail("tone occupied an implausibly wide display area");
    }
    printf("tone %.1fHz -> peak %.1fHz, -12dB width %.1fHz\n",
           tone_hz, peak_hz, width_hz);
    return 0;
}

int main(void)
{
    float bars[VIZ_POINTS];
    float peaks[VIZ_POINTS];
    int sample_cursor = 0;

    memset(bars, 0, sizeof(bars));
    memset(peaks, 0, sizeof(peaks));
    if (fft_map_init() != ESP_OK) return fail("fft_map_init");
    fft_map_set_mode(VIZ_MONITOR);

    if (check_tone_shape(41.0f, 34.0f, 48.0f, 25.0f,
                         bars, peaks) != 0 ||
        check_tone_shape(108.0f, 98.0f, 118.0f, 45.0f,
                         bars, peaks) != 0 ||
        check_tone_shape(1037.0f, 980.0f, 1100.0f, 160.0f,
                         bars, peaks) != 0) {
        return 1;
    }

    fft_map_reset();
    sample_cursor = 0;
    memset(bars, 0, sizeof(bars));
    memset(peaks, 0, sizeof(peaks));
    feed_tone(46.875f, 0.5f, 8192, &sample_cursor, bars, peaks);

    const int strongest = strongest_band(bars);
    if (strongest >= 64) return fail("low tone mapped outside low bands");
    if (bars[strongest] < 0.75f) return fail("low tone level too small");
    if (peaks[strongest] + 1e-6f < bars[strongest]) {
        return fail("peak envelope below current spectrum");
    }

    feed_tone(0.0f, 0.0f, AUDIO_SAMPLE_RATE,
              &sample_cursor, bars, peaks);
    if (peaks[strongest] < bars[strongest] + 0.20f) {
        return fail("monitor peak hold did not outlast release trace");
    }

    feed_tone(0.0f, 0.0f, AUDIO_SAMPLE_RATE * 4,
              &sample_cursor, bars, peaks);
    for (int i = 0; i < VIZ_POINTS; i++) {
        if (bars[i] > 0.001f) {
            return fail("monitor spectrum did not release to display floor");
        }
        if (peaks[i] > 0.001f) {
            return fail("monitor peak hold did not decay to display floor");
        }
    }

    printf("PASS: multi-resolution tone shape, peak hold, and silence release\n");
    return 0;
}
