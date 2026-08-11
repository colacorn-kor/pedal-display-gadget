#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio_playback.h"

static int failures;

static void expect_true(bool condition, const char *label)
{
    if (condition) return;
    fprintf(stderr, "FAIL %s\n", label);
    failures++;
}

static void expect_near(float actual, float expected, const char *label)
{
    if (fabsf(actual - expected) <= 0.00001f) return;
    fprintf(stderr, "FAIL %s: got %.6f expected %.6f\n",
            label, actual, expected);
    failures++;
}

int main(void)
{
    float output[8];
    audio_playback_status_t status;

    expect_true(audio_playback_init(false), "unavailable init");
    expect_true(!audio_playback_is_available(), "unavailable state");
    expect_true(audio_playback_claim("music") ==
                    AUDIO_PLAYBACK_ERR_UNAVAILABLE,
                "unavailable claim result");
    audio_playback_shutdown();

    expect_true(audio_playback_init(true), "available init");
    expect_true(audio_playback_is_available(), "available state");
    expect_true(audio_playback_claim("music") == AUDIO_PLAYBACK_OK,
                "owner claim");
    expect_true(audio_playback_claim("game") == AUDIO_PLAYBACK_ERR_BUSY,
                "second owner rejected");
    expect_true(audio_playback_set_bus_gain(
                    "music", AUDIO_PLAYBACK_BUS_EFFECTS, 0.5f) ==
                    AUDIO_PLAYBACK_OK,
                "effects gain");

    const float music[] = { 0.25f, -0.25f, 0.50f, -0.50f };
    const float effects[] = { 0.50f, 0.50f, 2.00f, 2.00f };
    expect_true(audio_playback_write(
                    "music", AUDIO_PLAYBACK_BUS_MUSIC, music, 2u) == 2u,
                "music write");
    expect_true(audio_playback_write(
                    "music", AUDIO_PLAYBACK_BUS_EFFECTS, effects, 2u) == 2u,
                "effects write");
    expect_true(audio_playback_play("music") == AUDIO_PLAYBACK_OK,
                "transport play");
    audio_playback_render(output, 2u);
    expect_near(output[0], 0.50f, "stereo mix left");
    expect_near(output[1], 0.00f, "stereo mix right");
    expect_near(output[2], 1.00f, "positive clip");
    expect_near(output[3], 0.50f, "second frame right");

    const float paused[] = { 0.75f, -0.75f };
    expect_true(audio_playback_write(
                    "music", AUDIO_PLAYBACK_BUS_MUSIC, paused, 1u) == 1u,
                "paused sample write");
    expect_true(audio_playback_pause("music") == AUDIO_PLAYBACK_OK,
                "transport pause");
    audio_playback_render(output, 1u);
    expect_near(output[0], 0.0f, "pause left silence");
    expect_near(output[1], 0.0f, "pause right silence");
    audio_playback_get_status(&status);
    expect_true(status.queued_music_frames == 1u,
                "pause preserves queued frames");

    expect_true(audio_playback_play("music") == AUDIO_PLAYBACK_OK,
                "transport resume");
    audio_playback_render(output, 1u);
    expect_near(output[0], 0.75f, "resume left");
    expect_near(output[1], -0.75f, "resume right");

    expect_true(audio_playback_stop("music") == AUDIO_PLAYBACK_OK,
                "transport stop");
    audio_playback_get_status(&status);
    expect_true(status.state == AUDIO_PLAYBACK_STOPPED,
                "stopped state");
    expect_true(status.queued_music_frames == 0u &&
                    status.queued_effect_frames == 0u,
                "stop clears queues");
    expect_true(audio_playback_release("music") == AUDIO_PLAYBACK_OK,
                "owner release");
    audio_playback_get_status(&status);
    expect_true(status.owner_id[0] == '\0', "owner cleared");
    expect_true(audio_playback_play("music") ==
                    AUDIO_PLAYBACK_ERR_NOT_OWNER,
                "released owner rejected");
    expect_true(audio_playback_claim("game") == AUDIO_PLAYBACK_OK,
                "next owner claim");
    audio_playback_get_status(&status);
    expect_near(status.master_gain, 1.0f, "next owner master reset");
    expect_near(status.effects_gain, 1.0f, "next owner bus reset");
    expect_true(audio_playback_release("game") == AUDIO_PLAYBACK_OK,
                "next owner release");

    audio_playback_shutdown();
    if (failures) return 1;
    printf("audio playback transport/mixer tests passed\n");
    return 0;
}
