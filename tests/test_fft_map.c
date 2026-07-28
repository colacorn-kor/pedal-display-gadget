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

int main(void)
{
    float bars[VIZ_POINTS];
    float peaks[VIZ_POINTS];
    int sample_cursor = 0;

    memset(bars, 0, sizeof(bars));
    memset(peaks, 0, sizeof(peaks));
    if (fft_map_init() != ESP_OK) return fail("fft_map_init");
    fft_map_set_mode(VIZ_MONITOR);
    fft_map_set_tilt_db_oct(0.0f);

    /* Bin 2 at 48k/2048 is 46.875Hz. This specifically guards the low
     * resolution regression caused by the simulator's former 256-point DFT. */
    feed_tone(46.875f, 0.5f, 8192, &sample_cursor, bars, peaks);

    int strongest = 0;
    for (int i = 1; i < VIZ_POINTS; i++) {
        if (bars[i] > bars[strongest]) strongest = i;
    }
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

    printf("PASS: shared low-band mapping, peak hold, and silence release\n");
    return 0;
}
