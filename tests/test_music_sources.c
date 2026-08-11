#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio_playback.h"
#include "music_lobby.h"
#include "wav_decoder.h"

static int failures;

static void expect_true(bool condition, const char *label)
{
    if (condition) return;
    fprintf(stderr, "FAIL %s\n", label);
    failures++;
}

static void expect_near(float actual, float expected, const char *label)
{
    if (fabsf(actual - expected) <= 0.0002f) return;
    fprintf(stderr, "FAIL %s: got %.6f expected %.6f\n",
            label, actual, expected);
    failures++;
}

static void write_le16(FILE *file, uint16_t value)
{
    const uint8_t bytes[] = {
        (uint8_t)value,
        (uint8_t)(value >> 8),
    };
    (void)fwrite(bytes, 1u, sizeof(bytes), file);
}

static void write_le32(FILE *file, uint32_t value)
{
    const uint8_t bytes[] = {
        (uint8_t)value,
        (uint8_t)(value >> 8),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 24),
    };
    (void)fwrite(bytes, 1u, sizeof(bytes), file);
}

static FILE *make_test_wav(void)
{
    const uint32_t frames = 240u;
    const uint32_t data_bytes = frames * 2u;
    FILE *file = tmpfile();
    if (!file) return NULL;

    (void)fwrite("RIFF", 1u, 4u, file);
    write_le32(file, 36u + data_bytes);
    (void)fwrite("WAVE", 1u, 4u, file);
    (void)fwrite("fmt ", 1u, 4u, file);
    write_le32(file, 16u);
    write_le16(file, 1u);
    write_le16(file, 1u);
    write_le32(file, 24000u);
    write_le32(file, 48000u);
    write_le16(file, 2u);
    write_le16(file, 16u);
    (void)fwrite("data", 1u, 4u, file);
    write_le32(file, data_bytes);

    for (uint32_t i = 0; i < frames; i++) {
        int16_t sample;
        if (i == 0u) sample = 0;
        else if (i == 1u) sample = 16384;
        else if (i == 2u) sample = -16384;
        else if (i == 3u) sample = 32767;
        else sample = (int16_t)((int)i * 97 - 12000);
        write_le16(file, (uint16_t)sample);
    }
    rewind(file);
    return file;
}

static void test_wav_decoder(void)
{
    float output[480u * AUDIO_PLAYBACK_CHANNELS];
    wav_decoder_t decoder;
    FILE *file = make_test_wav();

    expect_true(file != NULL, "temporary WAV creation");
    if (!file) return;
    expect_true(wav_decoder_open(&decoder, file), "WAV open");
    expect_true(decoder.sample_rate == 24000u &&
                decoder.channels == 1u,
                "WAV metadata");
    const size_t produced = wav_decoder_read(&decoder, output, 480u);
    expect_true(produced == 480u, "24k to 48k frame count");
    expect_near(output[0], 0.0f, "resample frame 0");
    expect_near(output[2], 0.25f, "resample interpolation");
    expect_near(output[4], 0.5f, "resample source frame");
    expect_near(output[1], output[0], "mono duplicate frame 0");
    expect_near(output[3], output[2], "mono duplicate interpolation");
    expect_true(wav_decoder_finished(&decoder), "WAV end state");
    expect_true(wav_decoder_duration_ms(&decoder) == 10u &&
                wav_decoder_position_ms(&decoder) == 10u,
                "WAV timing");
    wav_decoder_close(&decoder);

    float mixed[4];
    expect_true(audio_playback_init(true), "music mixer init");
    expect_true(audio_playback_claim("music-test") == AUDIO_PLAYBACK_OK,
                "music mixer claim");
    expect_true(audio_playback_write(
                    "music-test", AUDIO_PLAYBACK_BUS_MUSIC,
                    output, produced) == produced,
                "decoded WAV queued");
    expect_true(audio_playback_play("music-test") == AUDIO_PLAYBACK_OK,
                "decoded WAV play");
    audio_playback_render(mixed, 2u);
    expect_near(mixed[0], output[0], "decoded WAV mixer frame 0");
    expect_near(mixed[2], output[2], "decoded WAV mixer frame 1");
    expect_true(audio_playback_release("music-test") == AUDIO_PLAYBACK_OK,
                "music mixer release");
    audio_playback_shutdown();

    file = tmpfile();
    expect_true(file != NULL, "invalid WAV temporary file");
    if (file) {
        (void)fwrite("NOT_A_WAVE!!", 1u, 12u, file);
        rewind(file);
        expect_true(!wav_decoder_open(&decoder, file),
                    "invalid WAV rejected");
        expect_true(decoder.error == WAV_DECODER_ERR_CONTAINER,
                    "invalid WAV error state");
        wav_decoder_close(&decoder);
    }
}

static void test_lobby_music(void)
{
    music_lobby_t lobby;
    float output[512u * AUDIO_PLAYBACK_CHANNELS];
    float peak = 0.0f;
    bool stereo_differs = false;

    music_lobby_reset(&lobby);
    for (int block = 0; block < 48; block++) {
        expect_true(music_lobby_generate(&lobby, output, 512u) == 512u,
                    "lobby generated block");
        for (size_t i = 0; i < 512u; i++) {
            const float left = fabsf(output[i * 2u]);
            const float right = fabsf(output[i * 2u + 1u]);
            if (left > peak) peak = left;
            if (right > peak) peak = right;
            if (fabsf(output[i * 2u] - output[i * 2u + 1u]) > 0.0001f) {
                stereo_differs = true;
            }
        }
    }
    expect_true(peak > 0.05f && peak <= 1.0f,
                "lobby audible bounded signal");
    expect_true(stereo_differs, "lobby stereo image");
    expect_true(music_lobby_position_ms(&lobby) > 400u,
                "lobby position advances");
    expect_true(music_lobby_duration_ms() == 8000u,
                "lobby loop duration");
}

int main(void)
{
    test_wav_decoder();
    test_lobby_music();
    if (failures) return 1;
    printf("music WAV decoder and lobby synth tests passed\n");
    return 0;
}
