#include "sim_audio_output.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "audio_playback.h"

#define OUTPUT_BLOCK_FRAMES 512u
#define OUTPUT_TARGET_FRAMES 1536u
#define OUTPUT_MAX_BLOCKS_PER_PUMP 4

static SDL_AudioDeviceID s_device;
static bool s_initialized;
static bool s_available;
static bool s_virtual_sink;
static uint32_t s_seen_generation;
static sim_audio_output_stats_t s_stats;

static bool parse_index(const char *text, int *out)
{
    char *end = NULL;
    long value;

    if (!text || !text[0] || !out) return false;
    value = strtol(text, &end, 10);
    if (!end || end[0] || value < 0 || value > INT_MAX) return false;
    *out = (int)value;
    return true;
}

static bool output_device_index(int argc, char **argv, int *out)
{
    *out = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--output-device") != 0) continue;
        if (i + 1 >= argc || !parse_index(argv[i + 1], out)) return false;
        i++;
    }
    return true;
}

static void update_stats(const float *samples, size_t frames)
{
    s_stats.submitted_frames += frames;
    for (size_t i = 0; i < frames; i++) {
        const float left = fabsf(samples[i * 2u]);
        const float right = fabsf(samples[i * 2u + 1u]);
        const float frame_peak = left > right ? left : right;
        if (frame_peak > 0.000001f) s_stats.nonzero_frames++;
        if (frame_peak > s_stats.peak) s_stats.peak = frame_peak;
    }
}

bool sim_audio_output_init(int argc, char **argv, bool virtual_sink)
{
    SDL_AudioSpec want;
    SDL_AudioSpec have;
    int device_index = -1;
    const char *device_name = NULL;

    if (s_initialized) return s_available;
    s_initialized = true;
    s_virtual_sink = virtual_sink;

    if (virtual_sink) {
        s_available = true;
        printf("I (sim) virtual stereo audio output for automated checks\n");
        return true;
    }

    if (!output_device_index(argc, argv, &device_index)) {
        fprintf(stderr, "E (sim) invalid --output-device index\n");
        return false;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "W (sim) SDL audio output unavailable: %s\n",
                SDL_GetError());
        return false;
    }

    const int output_count = SDL_GetNumAudioDevices(0);
    if (output_count <= 0) {
        fprintf(stderr,
                "W (sim) no SDL playback output device is currently available\n");
        return false;
    }
    if (device_index >= 0) {
        if (device_index >= output_count) {
            fprintf(stderr,
                    "W (sim) output device index out of range: %d\n",
                    device_index);
            return false;
        }
        device_name = SDL_GetAudioDeviceName(device_index, 0);
    }

    SDL_zero(want);
    want.freq = AUDIO_PLAYBACK_SAMPLE_RATE;
    want.format = AUDIO_F32SYS;
    want.channels = AUDIO_PLAYBACK_CHANNELS;
    want.samples = OUTPUT_BLOCK_FRAMES;
    want.callback = NULL;

    s_device = SDL_OpenAudioDevice(device_name, 0, &want, &have, 0);
    if (!s_device) {
        fprintf(stderr, "W (sim) audio output open failed: %s\n",
                SDL_GetError());
        return false;
    }
    if (have.freq != want.freq || have.format != want.format ||
        have.channels != want.channels) {
        fprintf(stderr,
                "W (sim) audio output format mismatch: %d Hz, %u ch\n",
                have.freq, (unsigned)have.channels);
        SDL_CloseAudioDevice(s_device);
        s_device = 0;
        return false;
    }

    SDL_PauseAudioDevice(s_device, 0);
    s_available = true;
    printf("I (sim) stereo audio output: %s (%d Hz, %d ch)\n",
           device_name ? device_name : "default output device",
           have.freq, (int)have.channels);
    fflush(stdout);
    return true;
}

void sim_audio_output_shutdown(void)
{
    if (s_device) {
        SDL_ClearQueuedAudio(s_device);
        SDL_CloseAudioDevice(s_device);
    }
    s_device = 0;
    s_initialized = false;
    s_available = false;
    s_virtual_sink = false;
}

void sim_audio_output_pump(void)
{
    float block[OUTPUT_BLOCK_FRAMES * AUDIO_PLAYBACK_CHANNELS];
    audio_playback_status_t status;

    if (!s_available || !audio_playback_is_available()) return;
    audio_playback_get_status(&status);
    if (status.generation != s_seen_generation) {
        s_seen_generation = status.generation;
        if (s_device) SDL_ClearQueuedAudio(s_device);
    }

    size_t queued_frames = s_device
        ? SDL_GetQueuedAudioSize(s_device) /
            (AUDIO_PLAYBACK_CHANNELS * sizeof(float))
        : 0u;
    int blocks = 0;
    do {
        audio_playback_render(block, OUTPUT_BLOCK_FRAMES);
        update_stats(block, OUTPUT_BLOCK_FRAMES);
        if (s_device &&
            SDL_QueueAudio(s_device, block, sizeof(block)) != 0) {
            fprintf(stderr, "W (sim) audio output queue failed: %s\n",
                    SDL_GetError());
            audio_playback_shutdown();
            sim_audio_output_shutdown();
            return;
        }
        queued_frames += OUTPUT_BLOCK_FRAMES;
        blocks++;
    } while (!s_virtual_sink &&
             queued_frames < OUTPUT_TARGET_FRAMES &&
             blocks < OUTPUT_MAX_BLOCKS_PER_PUMP);
}

void sim_audio_output_reset_stats(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
}

void sim_audio_output_get_stats(sim_audio_output_stats_t *out)
{
    if (out) *out = s_stats;
}
