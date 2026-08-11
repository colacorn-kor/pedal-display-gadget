#include <math.h>
#include <stdio.h>

#include "audio_effects.h"
#include "audio_playback.h"

static int failures;

static void expect_true(int condition, const char *label)
{
    if (condition) return;
    fprintf(stderr, "FAIL %s\n", label);
    failures++;
}

static float render_peak(void)
{
    float output[1536u * AUDIO_PLAYBACK_CHANNELS];
    float peak = 0.0f;
    audio_playback_render(output, 1536u);
    for (size_t i = 0; i < 1536u * AUDIO_PLAYBACK_CHANNELS; i++) {
        const float value = fabsf(output[i]);
        if (value > peak) peak = value;
    }
    return peak;
}

int main(void)
{
    expect_true(audio_playback_init(true), "playback init");
    expect_true(audio_playback_claim("game") == AUDIO_PLAYBACK_OK,
                "game claim");
    expect_true(audio_playback_play("game") == AUDIO_PLAYBACK_OK,
                "game transport");

    expect_true(audio_effects_play("game", AUDIO_EFFECT_JUMP),
                "jump queued");
    const float jump_peak = render_peak();
    expect_true(jump_peak > 0.2f, "jump audible");

    expect_true(audio_effects_play("game", AUDIO_EFFECT_HIT),
                "hit queued");
    const float hit_peak = render_peak();
    expect_true(hit_peak > jump_peak, "hit accent stronger than jump");

    expect_true(!audio_effects_play("other", AUDIO_EFFECT_SCORE),
                "non-owner effect rejected");
    expect_true(audio_playback_release("game") == AUDIO_PLAYBACK_OK,
                "release");
    audio_playback_shutdown();

    if (failures) return 1;
    printf("audio effects tests passed\n");
    return 0;
}
