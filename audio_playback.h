#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_PLAYBACK_SAMPLE_RATE 48000
#define AUDIO_PLAYBACK_CHANNELS 2

typedef enum {
    AUDIO_PLAYBACK_BUS_MUSIC = 0,
    AUDIO_PLAYBACK_BUS_EFFECTS,
    AUDIO_PLAYBACK_BUS_COUNT,
} audio_playback_bus_t;

typedef enum {
    AUDIO_PLAYBACK_UNAVAILABLE = 0,
    AUDIO_PLAYBACK_STOPPED,
    AUDIO_PLAYBACK_PLAYING,
    AUDIO_PLAYBACK_PAUSED,
} audio_playback_state_t;

typedef enum {
    AUDIO_PLAYBACK_OK = 0,
    AUDIO_PLAYBACK_ERR_UNAVAILABLE,
    AUDIO_PLAYBACK_ERR_BUSY,
    AUDIO_PLAYBACK_ERR_NOT_OWNER,
    AUDIO_PLAYBACK_ERR_INVALID,
} audio_playback_result_t;

typedef struct {
    audio_playback_state_t state;
    uint32_t generation;
    size_t queued_music_frames;
    size_t queued_effect_frames;
    float master_gain;
    float music_gain;
    float effects_gain;
    char owner_id[32];
} audio_playback_status_t;

/* Platform startup supplies whether a real or virtual output backend exists. */
bool audio_playback_init(bool output_available);
void audio_playback_shutdown(void);
bool audio_playback_is_available(void);

audio_playback_result_t audio_playback_claim(const char *owner_id);
audio_playback_result_t audio_playback_play(const char *owner_id);
audio_playback_result_t audio_playback_pause(const char *owner_id);
audio_playback_result_t audio_playback_stop(const char *owner_id);
audio_playback_result_t audio_playback_release(const char *owner_id);

audio_playback_result_t audio_playback_set_master_gain(
    const char *owner_id, float gain);
audio_playback_result_t audio_playback_set_bus_gain(
    const char *owner_id, audio_playback_bus_t bus, float gain);
size_t audio_playback_write(const char *owner_id,
                            audio_playback_bus_t bus,
                            const float *stereo_interleaved,
                            size_t frames);
void audio_playback_clear_bus(const char *owner_id,
                              audio_playback_bus_t bus);

void audio_playback_get_status(audio_playback_status_t *out);

/* The platform output consumer pulls fixed-rate stereo PCM through this API. */
void audio_playback_render(float *stereo_interleaved, size_t frames);
