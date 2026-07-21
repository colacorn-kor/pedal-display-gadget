#include "sim_audio.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "audio_config.h"
#include "tuner.h"

#define SIM_BLOCK_SIZE 256
#define SIM_DFT_BINS (SIM_BLOCK_SIZE / 2 + 1)
#define SIM_QUEUE_CAP 8192
#define SIM_DEQUEUE_FRAMES 512
#define SIM_SCREEN_W 480.0f
#define SIM_F_LO 20.0f
#define SIM_F_HI 20000.0f
#define SIM_DB_FLOOR (-60.0f)
#define SIM_DB_TOP 0.0f
#define SIM_PI 3.14159265358979323846f

static SDL_AudioDeviceID s_capture_device;
static SDL_AudioSpec s_capture_spec;
static bool s_initialized;
static bool s_exit_after_args;
static bool s_capture_active;
static bool s_synthetic_fallback;
static bool s_warned_fallback;

static float s_queue[SIM_QUEUE_CAP];
static int s_queue_head;
static int s_queue_tail;
static int s_queue_count;

static audio_viz_snapshot_t s_viz;
static int s_band_lo[VIZ_POINTS];
static int s_band_hi[VIZ_POINTS];
static bool s_dft_ready;
static audio_mode_t s_active_mode = AUDIO_SPECTRUM;
static bool s_active_mode_ready;

static float s_mouse_x = SIM_SCREEN_W * 0.5f;
static uint32_t s_onset_seq;
static uint32_t s_onset_ms;
static float s_onset_strength;
static float s_synthetic_peaks[VIZ_POINTS];
static float s_synthetic_phase;
static uint32_t s_synthetic_last_ms;
static float s_synthetic_sample_credit;

static const char *NOTE_NAMES[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static float clampf(float value, float lo, float hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static void usage(void)
{
    fprintf(stderr,
            "Usage: pedal_sim.exe [--list-audio] [--audio-device N]\n");
}

static bool parse_device_index(const char *text, int *out)
{
    char *end = NULL;
    long value;

    if (!text || !*text || !out) return false;
    value = strtol(text, &end, 10);
    if (!end || *end != '\0' || value < 0 || value > INT_MAX) return false;

    *out = (int)value;
    return true;
}

static bool parse_args(int argc, char **argv, int *device_index)
{
    *device_index = -1;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--list-audio") == 0) {
            s_exit_after_args = true;
            continue;
        }

        if (strcmp(arg, "--audio-device") == 0) {
            if (i + 1 >= argc || !parse_device_index(argv[i + 1], device_index)) {
                usage();
                return false;
            }
            i++;
            continue;
        }

        usage();
        return false;
    }

    return true;
}

static void list_capture_devices(void)
{
    int count = SDL_GetNumAudioDevices(1);

    printf("Capture audio devices:\n");
    if (count <= 0) {
        printf("  (none)\n");
        return;
    }

    for (int i = 0; i < count; i++) {
        const char *name = SDL_GetAudioDeviceName(i, 1);
        printf("  %d: %s\n", i, name ? name : "(unknown)");
    }
}

static void enter_synthetic_fallback(void)
{
    s_capture_active = false;
    s_synthetic_fallback = true;

    if (!s_warned_fallback) {
        fprintf(stderr, "W (sim) no capture device; synthetic audio fallback\n");
        s_warned_fallback = true;
    }
}

static bool open_capture_device(int device_index)
{
    SDL_AudioSpec want;
    const char *device_name = NULL;
    int count = SDL_GetNumAudioDevices(1);

    if (count <= 0) {
        enter_synthetic_fallback();
        return true;
    }

    if (device_index >= 0) {
        if (device_index >= count) {
            fprintf(stderr, "E (sim) audio device index out of range: %d\n",
                    device_index);
            list_capture_devices();
            return false;
        }
        device_name = SDL_GetAudioDeviceName(device_index, 1);
    }

    SDL_zero(want);
    want.freq = AUDIO_SAMPLE_RATE;
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = SIM_BLOCK_SIZE;

    s_capture_device = SDL_OpenAudioDevice(device_name, 1, &want,
                                           &s_capture_spec, 0);
    if (!s_capture_device) {
        s_capture_device = SDL_OpenAudioDevice(device_name, 1, &want,
                                               &s_capture_spec,
                                               SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    }

    if (!s_capture_device) {
        enter_synthetic_fallback();
        return true;
    }

    if (s_capture_spec.freq != AUDIO_SAMPLE_RATE ||
        s_capture_spec.format != AUDIO_F32SYS ||
        (s_capture_spec.channels != 1 && s_capture_spec.channels != 2)) {
        fprintf(stderr,
                "W (sim) unsupported capture format (%d Hz, format 0x%x, %d ch)\n",
                s_capture_spec.freq,
                (unsigned)s_capture_spec.format,
                (int)s_capture_spec.channels);
        SDL_CloseAudioDevice(s_capture_device);
        s_capture_device = 0;
        enter_synthetic_fallback();
        return true;
    }

    s_capture_active = true;
    s_synthetic_fallback = false;
    SDL_PauseAudioDevice(s_capture_device, 0);

    printf("I (sim) audio capture: %s (%d Hz, %d ch)\n",
           device_name ? device_name : "default capture device",
           s_capture_spec.freq,
           (int)s_capture_spec.channels);
    return true;
}

static void init_dft_bins(void)
{
    const float bin_hz = (float)AUDIO_SAMPLE_RATE / (float)SIM_BLOCK_SIZE;
    const float ratio = powf(SIM_F_HI / SIM_F_LO, 1.0f / (float)VIZ_POINTS);
    float frequency = SIM_F_LO;

    for (int i = 0; i < VIZ_POINTS; i++) {
        const float low_frequency = frequency;
        const float high_frequency = frequency * ratio;
        int low_bin;
        int high_bin;

        frequency = high_frequency;
        low_bin = (int)ceilf(low_frequency / bin_hz);
        high_bin = (int)ceilf(high_frequency / bin_hz) - 1;
        if (low_bin < 1) low_bin = 1;
        if (high_bin < low_bin) high_bin = low_bin;
        if (high_bin > SIM_BLOCK_SIZE / 2) high_bin = SIM_BLOCK_SIZE / 2;
        if (low_bin > high_bin) low_bin = high_bin;

        s_band_lo[i] = low_bin;
        s_band_hi[i] = high_bin;
    }

    s_dft_ready = true;
}

static void reset_visualizer(void)
{
    memset(&s_viz, 0, sizeof(s_viz));
}

static void handle_mode_change(audio_mode_t mode)
{
    if (s_active_mode_ready && mode == s_active_mode) return;

    s_active_mode = mode;
    s_active_mode_ready = true;
    if (mode == AUDIO_TUNER) {
        tuner_reset();
    } else {
        reset_visualizer();
    }
}

static void queue_sample(float sample)
{
    if (s_queue_count == SIM_QUEUE_CAP) {
        s_queue_head = (s_queue_head + 1) % SIM_QUEUE_CAP;
        s_queue_count--;
    }

    s_queue[s_queue_tail] = sample;
    s_queue_tail = (s_queue_tail + 1) % SIM_QUEUE_CAP;
    s_queue_count++;
}

static bool dequeue_block(float *out)
{
    if (s_queue_count < SIM_BLOCK_SIZE) return false;

    for (int i = 0; i < SIM_BLOCK_SIZE; i++) {
        out[i] = s_queue[s_queue_head];
        s_queue_head = (s_queue_head + 1) % SIM_QUEUE_CAP;
    }
    s_queue_count -= SIM_BLOCK_SIZE;
    return true;
}

static float block_rms(const float *block)
{
    float sum = 0.0f;
    for (int i = 0; i < SIM_BLOCK_SIZE; i++) {
        sum += block[i] * block[i];
    }
    return sqrtf(sum / (float)SIM_BLOCK_SIZE);
}

static float level_from_rms(float rms)
{
    float level = rms * 3.0f;
    if (!isfinite(level) || level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    return level;
}

static void decay_visualizer(float level)
{
    for (int i = 0; i < VIZ_POINTS; i++) {
        s_viz.bars[i] *= 0.92f;
        s_viz.peaks[i] *= 0.96f;
        if (s_viz.peaks[i] < s_viz.bars[i]) s_viz.peaks[i] = s_viz.bars[i];
    }
    s_viz.level = level;
}

static void update_visualizer_from_block(const float *block, float level)
{
    float magnitude[SIM_DFT_BINS];

    if (!s_dft_ready) init_dft_bins();

    for (int k = 0; k < SIM_DFT_BINS; k++) {
        float real = 0.0f;
        float imag = 0.0f;
        for (int n = 0; n < SIM_BLOCK_SIZE; n++) {
            const float window = 0.5f - 0.5f *
                cosf(2.0f * SIM_PI * (float)n / (float)(SIM_BLOCK_SIZE - 1));
            const float sample = block[n] * window;
            const float phase = 2.0f * SIM_PI * (float)k * (float)n /
                                (float)SIM_BLOCK_SIZE;
            real += sample * cosf(phase);
            imag -= sample * sinf(phase);
        }
        magnitude[k] = sqrtf(real * real + imag * imag) /
                       ((float)SIM_BLOCK_SIZE * 0.5f);
    }

    for (int i = 0; i < VIZ_POINTS; i++) {
        float max_mag = 0.0f;
        for (int bin = s_band_lo[i]; bin <= s_band_hi[i]; bin++) {
            if (magnitude[bin] > max_mag) max_mag = magnitude[bin];
        }

        float db = 20.0f * log10f(max_mag + 1e-7f);
        float value = (db - SIM_DB_FLOOR) / (SIM_DB_TOP - SIM_DB_FLOOR);
        value = clampf(value, 0.0f, 1.0f);

        s_viz.peaks[i] *= 0.96f;
        if (value > s_viz.peaks[i]) s_viz.peaks[i] = value;
        s_viz.bars[i] = value;
    }
    s_viz.level = level;
}

static void process_block(const float *block)
{
    float rms = block_rms(block);
    float level = level_from_rms(rms);
    audio_mode_t mode = audio_get_mode();

    handle_mode_change(mode);
    if (mode == AUDIO_TUNER) {
        tuner_feed(block, SIM_BLOCK_SIZE);
        music_events_process_block(rms, level);
        decay_visualizer(level);
        return;
    }

    music_events_process_block(rms, level);
    update_visualizer_from_block(block, level);
}

static void pump_capture_audio(void)
{
    const int channels = (int)s_capture_spec.channels;
    const Uint32 frame_bytes = (Uint32)(channels * (int)sizeof(float));
    float raw[SIM_DEQUEUE_FRAMES * 2];
    float block[SIM_BLOCK_SIZE];

    for (;;) {
        Uint32 available = SDL_GetQueuedAudioSize(s_capture_device);
        Uint32 wanted = (Uint32)sizeof(raw);
        Uint32 got;
        int frames;

        if (available < frame_bytes) break;
        if (wanted > available) wanted = available;
        wanted -= wanted % frame_bytes;
        if (wanted == 0) break;

        got = SDL_DequeueAudio(s_capture_device, raw, wanted);
        if (got == 0) break;

        frames = (int)(got / frame_bytes);
        for (int i = 0; i < frames; i++) {
            float sample = raw[i * channels];
            if (channels == 2) {
                sample = 0.5f * (sample + raw[i * channels + 1]);
            }
            queue_sample(sample);
        }
    }

    while (dequeue_block(block)) {
        process_block(block);
    }
}

static float synthetic_onset_decay(uint32_t now)
{
    uint32_t age = now - s_onset_ms;
    if (age >= 260u) return 0.0f;
    return 1.0f - (float)age / 260.0f;
}

static float synthetic_pitch_hz(void)
{
    float ratio = clampf(s_mouse_x / (SIM_SCREEN_W - 1.0f), 0.0f, 1.0f);
    return 82.4069f * powf(2.0f, ratio * 3.0f);
}

static void synthetic_note_from_hz(float f0,
                                   const char **name,
                                   int *octave,
                                   float *cents)
{
    float midi = 69.0f + 12.0f * log2f(f0 / 440.0f);
    int nearest = (int)floorf(midi + 0.5f);
    int note = nearest % 12;

    if (note < 0) note += 12;
    if (name) *name = NOTE_NAMES[note];
    if (octave) *octave = nearest / 12 - 1;
    if (cents) *cents = (midi - (float)nearest) * 100.0f;
}

static void fill_synthetic_audio_block(float *block)
{
    const float f0 = synthetic_pitch_hz();
    const float inc = 2.0f * SIM_PI * f0 / (float)AUDIO_SAMPLE_RATE;

    for (int i = 0; i < SIM_BLOCK_SIZE; i++) {
        float sample = 0.18f * sinf(s_synthetic_phase) +
                       0.035f * sinf(2.0f * s_synthetic_phase);
        block[i] = sample;
        s_synthetic_phase += inc;
        if (s_synthetic_phase >= 2.0f * SIM_PI) {
            s_synthetic_phase -= 2.0f * SIM_PI;
        }
    }
}

static void pump_synthetic_audio(void)
{
    uint32_t now = SDL_GetTicks();
    float block[SIM_BLOCK_SIZE];
    int blocks = 0;

    if (s_synthetic_last_ms == 0) {
        s_synthetic_last_ms = now;
        s_synthetic_sample_credit = (float)SIM_BLOCK_SIZE;
    } else {
        uint32_t elapsed_ms = now - s_synthetic_last_ms;
        if (elapsed_ms > 100u) elapsed_ms = 100u;
        s_synthetic_last_ms = now;
        s_synthetic_sample_credit +=
            (float)elapsed_ms * ((float)AUDIO_SAMPLE_RATE / 1000.0f);
    }

    while (s_synthetic_sample_credit >= (float)SIM_BLOCK_SIZE && blocks < 8) {
        fill_synthetic_audio_block(block);
        process_block(block);
        s_synthetic_sample_credit -= (float)SIM_BLOCK_SIZE;
        blocks++;
    }
}

static void synthetic_viz_get(audio_viz_snapshot_t *out)
{
    const float t = (float)SDL_GetTicks() * 0.001f;
    const float kick = synthetic_onset_decay(SDL_GetTicks());
    float level = 0.34f + 0.18f * sinf(t * 2.4f) + 0.42f * kick;

    level = clampf(level, 0.0f, 1.0f);
    for (int i = 0; i < VIZ_POINTS; i++) {
        float x = (float)i / (float)(VIZ_POINTS - 1);
        float sweep = 0.5f + 0.5f * sinf(t * 3.0f + x * 23.0f);
        float ripple = 0.5f + 0.5f * sinf(t * 9.0f + x * 71.0f);
        float tilt = 1.0f - 0.55f * x;
        float value = (0.08f + 0.72f * sweep + 0.18f * ripple) * tilt;

        value = clampf(value * (0.45f + level), 0.0f, 1.0f);
        s_synthetic_peaks[i] *= 0.965f;
        if (value > s_synthetic_peaks[i]) s_synthetic_peaks[i] = value;
        out->bars[i] = value;
        out->peaks[i] = s_synthetic_peaks[i];
    }
    out->level = level;
}

static void synthetic_music_get(music_snapshot_t *out)
{
    const uint32_t now = SDL_GetTicks();
    const float f0 = synthetic_pitch_hz();
    const char *name = "E";
    int octave = 2;
    float cents = 0.0f;

    synthetic_note_from_hz(f0, &name, &octave, &cents);

    memset(out, 0, sizeof(*out));
    out->onset_seq = s_onset_seq;
    out->onset_ms = s_onset_ms;
    out->onset_strength = s_onset_strength;
    out->level = clampf(0.25f + 0.65f * synthetic_onset_decay(now),
                        0.0f, 1.0f);
    out->bpm = 120.0f;
    out->pitch_valid = true;
    out->f0 = f0;
    out->note_name = name;
    out->octave = octave;
    out->cents = cents;
    out->clarity = 0.92f;
}

bool sim_audio_init(int argc, char **argv)
{
    int device_index = -1;

    if (s_initialized) return true;
    s_initialized = true;

    tuner_init();
    music_events_init();
    init_dft_bins();

    if (argc < 0) argc = 0;
    if (!parse_args(argc, argv, &device_index)) return false;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        enter_synthetic_fallback();
        return true;
    }

    if (s_exit_after_args) {
        list_capture_devices();
        return true;
    }

    return open_capture_device(device_index);
}

bool sim_audio_should_exit_after_args(void)
{
    return s_exit_after_args;
}

void sim_audio_pump(void)
{
    if (s_capture_active) {
        pump_capture_audio();
    } else if (s_synthetic_fallback) {
        pump_synthetic_audio();
    }
}

void sim_audio_set_mouse_x(float x)
{
    s_mouse_x = clampf(x, 0.0f, SIM_SCREEN_W - 1.0f);
}

void sim_audio_trigger_synthetic_onset(void)
{
    if (!s_synthetic_fallback) return;

    s_onset_seq++;
    s_onset_ms = SDL_GetTicks();
    s_onset_strength = 1.35f;
}

void sim_audio_audio_viz_get(audio_viz_snapshot_t *out)
{
    if (!out) return;

    if (s_capture_active) {
        *out = s_viz;
        return;
    }

    synthetic_viz_get(out);
}

void sim_audio_music_get(music_snapshot_t *out)
{
    if (!out) return;

    if (s_capture_active) {
        music_snapshot_get(out);
        return;
    }

    synthetic_music_get(out);
}
