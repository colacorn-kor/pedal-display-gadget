#include <math.h>
#include <stdio.h>
#include <string.h>

#include "metronome_engine.h"

static int failures;

static void expect_true(int condition, const char *label)
{
    if (condition) return;
    fprintf(stderr, "FAIL %s\n", label);
    failures++;
}

static float peak_of(const float *samples, size_t frames)
{
    float peak = 0.0f;
    for (size_t i = 0; i < frames * 2u; i++) {
        float value = fabsf(samples[i]);
        if (value > peak) peak = value;
    }
    return peak;
}

int main(void)
{
    metronome_engine_t engine;
    const metronome_config_t config = { 120, 4, 2 };
    float first[64u * 2u];
    float interval[(12000u - 64u) * 2u];
    float second[64u * 2u];

    metronome_engine_init(&engine, &config);
    memset(first, 0, sizeof(first));
    metronome_engine_render(&engine, first, 64u);
    expect_true(engine.tick_count == 1u, "immediate downbeat");
    expect_true(engine.last_tick_kind == METRONOME_TICK_DOWNBEAT,
                "downbeat kind");
    expect_true(peak_of(first, 64u) > 0.4f, "downbeat is audible");

    metronome_engine_render(&engine, interval, 12000u - 64u);
    expect_true(engine.tick_count == 1u, "no early eighth note");
    metronome_engine_render(&engine, second, 64u);
    expect_true(engine.tick_count == 2u, "eighth-note sample timing");
    expect_true(engine.last_tick_kind == METRONOME_TICK_SUBDIVISION,
                "subdivision kind");
    expect_true(peak_of(second, 64u) > 0.15f &&
                peak_of(second, 64u) < peak_of(first, 64u),
                "subdivision is quieter");

    const metronome_config_t triplet = { 100, 3, 3 };
    metronome_engine_set_config(&engine, &triplet);
    float block[9600u * 2u];
    metronome_engine_render(&engine, block, 9600u);
    expect_true(engine.tick_count == 1u, "triplet no early tick");
    metronome_engine_render(&engine, first, 1u);
    expect_true(engine.tick_count == 2u,
                "triplet fractional interval timing");

    if (failures) return 1;
    printf("metronome engine tests passed\n");
    return 0;
}
