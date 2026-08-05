#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio_config.h"
#include "fft_map.h"

#define TEST_BLOCK_SIZE 256
#define TEST_TWO_PI 6.28318530717958647692f

typedef struct {
    float peak_hz;
    float peak_db;
    float width_hz;
    float side_db;
} tone_result_t;

static int fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static float norm_to_db(float normalized)
{
    return VIZ_DB_FLOOR + normalized * (VIZ_DB_TOP - VIZ_DB_FLOOR);
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

static void feed_constant(float value, int sample_count,
                          float *bars, float *peaks)
{
    float block[TEST_BLOCK_SIZE];
    for (int i = 0; i < TEST_BLOCK_SIZE; i++) block[i] = value;

    while (sample_count > 0) {
        const int count = sample_count < TEST_BLOCK_SIZE
            ? sample_count : TEST_BLOCK_SIZE;
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

static int nearest_band(float frequency)
{
    int nearest = 0;
    float best_distance = fabsf(fft_map_frequency_at(0) - frequency);

    for (int i = 1; i < VIZ_POINTS; i++) {
        const float distance = fabsf(fft_map_frequency_at(i) - frequency);
        if (distance < best_distance) {
            best_distance = distance;
            nearest = i;
        }
    }
    return nearest;
}

static tone_result_t measure_reference_tone(float tone_hz, float side_hz,
                                             float *bars, float *peaks)
{
    int sample_cursor = 0;
    memset(bars, 0, sizeof(float) * VIZ_POINTS);
    memset(peaks, 0, sizeof(float) * VIZ_POINTS);
    fft_map_reset();
    fft_map_set_mode(VIZ_REFERENCE);
    feed_tone(tone_hz, 0.5f, AUDIO_SAMPLE_RATE * 2,
              &sample_cursor, bars, peaks);

    const int strongest = strongest_band(bars);
    const float threshold = bars[strongest] -
        12.0f / (VIZ_DB_TOP - VIZ_DB_FLOOR);
    int first = strongest;
    int last = strongest;
    while (first > 0 && bars[first - 1] >= threshold) first--;
    while (last + 1 < VIZ_POINTS && bars[last + 1] >= threshold) last++;

    return (tone_result_t) {
        .peak_hz = fft_map_frequency_at(strongest),
        .peak_db = norm_to_db(bars[strongest]),
        .width_hz = fft_map_frequency_at(last) -
                    fft_map_frequency_at(first),
        .side_db = norm_to_db(bars[nearest_band(side_hz)]),
    };
}

static int count_major_lobes(const float *bars, float low_hz, float high_hz)
{
    const int strongest = strongest_band(bars);
    const float threshold = bars[strongest] -
        6.0f / (VIZ_DB_TOP - VIZ_DB_FLOOR);
    int lobe_count = 0;
    int inside_lobe = 0;

    for (int i = 0; i < VIZ_POINTS; i++) {
        const float frequency = fft_map_frequency_at(i);
        if (frequency < low_hz || frequency > high_hz) continue;

        const int above = bars[i] >= threshold;
        if (above && !inside_lobe) lobe_count++;
        inside_lobe = above;
    }
    return lobe_count;
}

static int check_crossover_sweep(float *bars, float *peaks)
{
    float block[TEST_BLOCK_SIZE];
    float phase = 0.0f;
    const int total_samples = AUDIO_SAMPLE_RATE * 4;
    int sample_cursor = 0;
    int examined_frames = 0;

    memset(bars, 0, sizeof(float) * VIZ_POINTS);
    memset(peaks, 0, sizeof(float) * VIZ_POINTS);
    fft_map_reset();
    fft_map_set_mode(VIZ_REFERENCE);

    while (sample_cursor < total_samples) {
        const int remaining = total_samples - sample_cursor;
        const int count = remaining < TEST_BLOCK_SIZE
            ? remaining : TEST_BLOCK_SIZE;
        for (int i = 0; i < count; i++) {
            const float progress = (float)(sample_cursor + i) /
                                   (float)(total_samples - 1);
            const float frequency = 250.0f * powf(700.0f / 250.0f,
                                                  progress);
            block[i] = 0.5f * sinf(phase);
            phase += TEST_TWO_PI * frequency /
                     (float)AUDIO_SAMPLE_RATE;
            if (phase >= TEST_TWO_PI) phase -= TEST_TWO_PI;
        }

        if (fft_feed(block, count, bars, peaks)) {
            const int strongest = strongest_band(bars);
            const float peak_hz = fft_map_frequency_at(strongest);
            if (peak_hz >= 280.0f && peak_hz <= 650.0f &&
                norm_to_db(bars[strongest]) > -30.0f) {
                examined_frames++;
                if (count_major_lobes(bars, 200.0f, 800.0f) > 1) {
                    fprintf(stderr,
                            "crossover sweep split near %.1fHz at sample %d\n",
                            peak_hz, sample_cursor);
                    return fail("crossover sweep produced two major peaks");
                }
            }
        }
        sample_cursor += count;
    }

    if (examined_frames < 20) {
        return fail("too few crossover sweep frames were examined");
    }
    printf("crossover sweep -> %d aligned reference frames\n",
           examined_frames);
    return 0;
}

static int check_monitor_envelopes(float *bars, float *peaks)
{
    int sample_cursor = 0;

    memset(bars, 0, sizeof(float) * VIZ_POINTS);
    memset(peaks, 0, sizeof(float) * VIZ_POINTS);
    fft_map_reset();
    fft_map_set_mode(VIZ_MONITOR);
    feed_tone(46.875f, 0.5f, AUDIO_SAMPLE_RATE,
              &sample_cursor, bars, peaks);

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
    return 0;
}

int main(void)
{
    float bars[VIZ_POINTS];
    float peaks[VIZ_POINTS];

    if (fft_map_init() != ESP_OK) return fail("fft_map_init");

    const tone_result_t tone30 =
        measure_reference_tone(30.0f, 35.0f, bars, peaks);
    const tone_result_t tone300 =
        measure_reference_tone(300.0f, 350.0f, bars, peaks);
    const tone_result_t tone3000 =
        measure_reference_tone(3000.0f, 3500.0f, bars, peaks);
    const tone_result_t tones[] = { tone30, tone300, tone3000 };
    const float expected_hz[] = { 30.0f, 300.0f, 3000.0f };

    float minimum_peak_db = tones[0].peak_db;
    float maximum_peak_db = tones[0].peak_db;
    for (int i = 0; i < 3; i++) {
        printf("tone %.0fHz -> peak %.1fHz / %.2fdBFS, "
               "-12dB width %.1fHz, proportional side %.2fdBFS\n",
               expected_hz[i], tones[i].peak_hz, tones[i].peak_db,
               tones[i].width_hz, tones[i].side_db);
        if (fabsf(log2f(tones[i].peak_hz / expected_hz[i])) > 0.04f) {
            return fail("reference tone peak mapped to wrong frequency");
        }
        if (tones[i].side_db > tones[i].peak_db - 20.0f) {
            return fail("reference tone leaked too strongly at +16.7 percent");
        }
        if (tones[i].peak_db < minimum_peak_db) {
            minimum_peak_db = tones[i].peak_db;
        }
        if (tones[i].peak_db > maximum_peak_db) {
            maximum_peak_db = tones[i].peak_db;
        }
    }
    if (maximum_peak_db - minimum_peak_db > 2.0f) {
        return fail("fixed-amplitude tone level changed across frequency");
    }

    memset(bars, 0, sizeof(bars));
    memset(peaks, 0, sizeof(peaks));
    fft_map_reset();
    fft_map_set_mode(VIZ_REFERENCE);
    feed_constant(0.5f, AUDIO_SAMPLE_RATE * 2, bars, peaks);
    for (int i = 0; i < VIZ_POINTS; i++) {
        if (bars[i] > 0.001f) {
            return fail("DC input was visible in the 20Hz spectrum");
        }
    }

    if (check_crossover_sweep(bars, peaks) != 0 ||
        check_monitor_envelopes(bars, peaks) != 0) {
        return 1;
    }

    printf("PASS: time-aligned reference tones, DC rejection, crossover "
           "sweep, peak hold, and silence release\n");
    return 0;
}
