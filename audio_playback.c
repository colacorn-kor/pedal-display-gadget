#include "audio_playback.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define MUSIC_QUEUE_FRAMES 8192u
#define EFFECTS_QUEUE_FRAMES 2048u

typedef struct {
    float *samples;
    uint32_t capacity;
    atomic_uint read_pos;
    atomic_uint write_pos;
    atomic_uint gain_bits;
} playback_bus_t;

static playback_bus_t s_buses[AUDIO_PLAYBACK_BUS_COUNT];
static atomic_int s_state;
static atomic_uint s_generation;
static atomic_uint s_master_gain_bits;
static bool s_initialized;
static bool s_available;
static char s_owner_id[32];

static uint32_t float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float bits_float(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float clamp_gain(float gain)
{
    if (!(gain >= 0.0f)) return 0.0f;
    if (gain > 1.0f) return 1.0f;
    return gain;
}

static bool bus_valid(audio_playback_bus_t bus)
{
    return bus >= AUDIO_PLAYBACK_BUS_MUSIC &&
           bus < AUDIO_PLAYBACK_BUS_COUNT;
}

static bool owner_matches(const char *owner_id)
{
    return owner_id && owner_id[0] && s_owner_id[0] &&
           strcmp(owner_id, s_owner_id) == 0;
}

static size_t bus_queued(const playback_bus_t *bus)
{
    const uint32_t read_pos = atomic_load_explicit(
        &bus->read_pos, memory_order_acquire);
    const uint32_t write_pos = atomic_load_explicit(
        &bus->write_pos, memory_order_acquire);
    return (size_t)(write_pos - read_pos);
}

static void bus_clear(playback_bus_t *bus)
{
    const uint32_t write_pos = atomic_load_explicit(
        &bus->write_pos, memory_order_acquire);
    atomic_store_explicit(&bus->read_pos, write_pos, memory_order_release);
}

static void clear_all_buses(void)
{
    for (int i = 0; i < AUDIO_PLAYBACK_BUS_COUNT; i++) {
        bus_clear(&s_buses[i]);
    }
}

static void reset_gains(void)
{
    atomic_store_explicit(
        &s_master_gain_bits, float_bits(1.0f), memory_order_release);
    for (int i = 0; i < AUDIO_PLAYBACK_BUS_COUNT; i++) {
        atomic_store_explicit(
            &s_buses[i].gain_bits, float_bits(1.0f), memory_order_release);
    }
}

static void transport_changed(void)
{
    atomic_fetch_add_explicit(&s_generation, 1u, memory_order_release);
}

static audio_playback_result_t require_owner(const char *owner_id)
{
    if (!s_available) return AUDIO_PLAYBACK_ERR_UNAVAILABLE;
    if (!owner_matches(owner_id)) return AUDIO_PLAYBACK_ERR_NOT_OWNER;
    return AUDIO_PLAYBACK_OK;
}

bool audio_playback_init(bool output_available)
{
    if (s_initialized) return s_available == output_available;

    s_initialized = true;
    s_available = false;
    s_owner_id[0] = '\0';
    atomic_init(&s_generation, 1u);
    atomic_init(&s_master_gain_bits, float_bits(1.0f));
    atomic_init(&s_state, AUDIO_PLAYBACK_UNAVAILABLE);

    const uint32_t capacities[AUDIO_PLAYBACK_BUS_COUNT] = {
        MUSIC_QUEUE_FRAMES,
        EFFECTS_QUEUE_FRAMES,
    };
    for (int i = 0; i < AUDIO_PLAYBACK_BUS_COUNT; i++) {
        playback_bus_t *bus = &s_buses[i];
        bus->capacity = capacities[i];
        atomic_init(&bus->read_pos, 0u);
        atomic_init(&bus->write_pos, 0u);
        atomic_init(&bus->gain_bits, float_bits(1.0f));
    }

    if (!output_available) {
        return true;
    }

    for (int i = 0; i < AUDIO_PLAYBACK_BUS_COUNT; i++) {
        playback_bus_t *bus = &s_buses[i];
        bus->samples = (float *)calloc(
            (size_t)bus->capacity * AUDIO_PLAYBACK_CHANNELS, sizeof(float));
        if (!bus->samples) {
            audio_playback_shutdown();
            s_initialized = true;
            atomic_store_explicit(
                &s_state, AUDIO_PLAYBACK_UNAVAILABLE, memory_order_release);
            return false;
        }
    }

    s_available = true;
    atomic_store_explicit(
        &s_state, AUDIO_PLAYBACK_STOPPED, memory_order_release);
    return true;
}

void audio_playback_shutdown(void)
{
    for (int i = 0; i < AUDIO_PLAYBACK_BUS_COUNT; i++) {
        free(s_buses[i].samples);
        s_buses[i].samples = NULL;
        s_buses[i].capacity = 0u;
    }
    s_owner_id[0] = '\0';
    s_available = false;
    s_initialized = false;
    atomic_store_explicit(
        &s_state, AUDIO_PLAYBACK_UNAVAILABLE, memory_order_release);
}

bool audio_playback_is_available(void)
{
    return s_available;
}

audio_playback_result_t audio_playback_claim(const char *owner_id)
{
    if (!s_available) return AUDIO_PLAYBACK_ERR_UNAVAILABLE;
    if (!owner_id || !owner_id[0] || strlen(owner_id) >= sizeof(s_owner_id)) {
        return AUDIO_PLAYBACK_ERR_INVALID;
    }
    if (s_owner_id[0]) {
        return owner_matches(owner_id)
            ? AUDIO_PLAYBACK_OK : AUDIO_PLAYBACK_ERR_BUSY;
    }

    clear_all_buses();
    reset_gains();
    atomic_store_explicit(
        &s_state, AUDIO_PLAYBACK_STOPPED, memory_order_release);
    memcpy(s_owner_id, owner_id, strlen(owner_id) + 1u);
    transport_changed();
    return AUDIO_PLAYBACK_OK;
}

audio_playback_result_t audio_playback_play(const char *owner_id)
{
    audio_playback_result_t result = require_owner(owner_id);
    if (result != AUDIO_PLAYBACK_OK) return result;
    if (atomic_load_explicit(&s_state, memory_order_acquire) !=
        AUDIO_PLAYBACK_PLAYING) {
        atomic_store_explicit(
            &s_state, AUDIO_PLAYBACK_PLAYING, memory_order_release);
        transport_changed();
    }
    return AUDIO_PLAYBACK_OK;
}

audio_playback_result_t audio_playback_pause(const char *owner_id)
{
    audio_playback_result_t result = require_owner(owner_id);
    if (result != AUDIO_PLAYBACK_OK) return result;
    if (atomic_load_explicit(&s_state, memory_order_acquire) !=
        AUDIO_PLAYBACK_PAUSED) {
        atomic_store_explicit(
            &s_state, AUDIO_PLAYBACK_PAUSED, memory_order_release);
        transport_changed();
    }
    return AUDIO_PLAYBACK_OK;
}

audio_playback_result_t audio_playback_stop(const char *owner_id)
{
    audio_playback_result_t result = require_owner(owner_id);
    if (result != AUDIO_PLAYBACK_OK) return result;
    atomic_store_explicit(
        &s_state, AUDIO_PLAYBACK_STOPPED, memory_order_release);
    clear_all_buses();
    transport_changed();
    return AUDIO_PLAYBACK_OK;
}

audio_playback_result_t audio_playback_release(const char *owner_id)
{
    audio_playback_result_t result = require_owner(owner_id);
    if (result != AUDIO_PLAYBACK_OK) return result;
    atomic_store_explicit(
        &s_state, AUDIO_PLAYBACK_STOPPED, memory_order_release);
    clear_all_buses();
    s_owner_id[0] = '\0';
    transport_changed();
    return AUDIO_PLAYBACK_OK;
}

audio_playback_result_t audio_playback_set_master_gain(
    const char *owner_id, float gain)
{
    audio_playback_result_t result = require_owner(owner_id);
    if (result != AUDIO_PLAYBACK_OK) return result;
    atomic_store_explicit(&s_master_gain_bits, float_bits(clamp_gain(gain)),
                          memory_order_release);
    return AUDIO_PLAYBACK_OK;
}

audio_playback_result_t audio_playback_set_bus_gain(
    const char *owner_id, audio_playback_bus_t bus, float gain)
{
    audio_playback_result_t result = require_owner(owner_id);
    if (result != AUDIO_PLAYBACK_OK) return result;
    if (!bus_valid(bus)) return AUDIO_PLAYBACK_ERR_INVALID;
    atomic_store_explicit(&s_buses[bus].gain_bits,
                          float_bits(clamp_gain(gain)),
                          memory_order_release);
    return AUDIO_PLAYBACK_OK;
}

size_t audio_playback_write(const char *owner_id,
                            audio_playback_bus_t bus,
                            const float *stereo_interleaved,
                            size_t frames)
{
    if (require_owner(owner_id) != AUDIO_PLAYBACK_OK ||
        !bus_valid(bus) || !stereo_interleaved || frames == 0u) {
        return 0u;
    }

    playback_bus_t *target = &s_buses[bus];
    const uint32_t read_pos = atomic_load_explicit(
        &target->read_pos, memory_order_acquire);
    const uint32_t write_pos = atomic_load_explicit(
        &target->write_pos, memory_order_relaxed);
    const uint32_t queued = write_pos - read_pos;
    size_t writable = target->capacity - queued;
    if (frames < writable) writable = frames;

    for (size_t i = 0; i < writable; i++) {
        const size_t dst_frame = (write_pos + (uint32_t)i) % target->capacity;
        target->samples[dst_frame * 2u] = stereo_interleaved[i * 2u];
        target->samples[dst_frame * 2u + 1u] =
            stereo_interleaved[i * 2u + 1u];
    }
    atomic_store_explicit(&target->write_pos,
                          write_pos + (uint32_t)writable,
                          memory_order_release);
    return writable;
}

void audio_playback_clear_bus(const char *owner_id,
                              audio_playback_bus_t bus)
{
    if (require_owner(owner_id) != AUDIO_PLAYBACK_OK || !bus_valid(bus)) {
        return;
    }
    bus_clear(&s_buses[bus]);
}

void audio_playback_get_status(audio_playback_status_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->state = (audio_playback_state_t)atomic_load_explicit(
        &s_state, memory_order_acquire);
    out->generation = atomic_load_explicit(
        &s_generation, memory_order_acquire);
    out->master_gain = bits_float(atomic_load_explicit(
        &s_master_gain_bits, memory_order_acquire));
    out->music_gain = bits_float(atomic_load_explicit(
        &s_buses[AUDIO_PLAYBACK_BUS_MUSIC].gain_bits, memory_order_acquire));
    out->effects_gain = bits_float(atomic_load_explicit(
        &s_buses[AUDIO_PLAYBACK_BUS_EFFECTS].gain_bits, memory_order_acquire));
    if (s_available) {
        out->queued_music_frames = bus_queued(
            &s_buses[AUDIO_PLAYBACK_BUS_MUSIC]);
        out->queued_effect_frames = bus_queued(
            &s_buses[AUDIO_PLAYBACK_BUS_EFFECTS]);
    }
    memcpy(out->owner_id, s_owner_id, sizeof(out->owner_id));
}

static void mix_bus(playback_bus_t *bus, float *output, size_t frames)
{
    const uint32_t read_pos = atomic_load_explicit(
        &bus->read_pos, memory_order_relaxed);
    const uint32_t write_pos = atomic_load_explicit(
        &bus->write_pos, memory_order_acquire);
    size_t readable = (size_t)(write_pos - read_pos);
    if (frames < readable) readable = frames;
    const float gain = bits_float(atomic_load_explicit(
        &bus->gain_bits, memory_order_acquire));

    for (size_t i = 0; i < readable; i++) {
        const size_t src_frame = (read_pos + (uint32_t)i) % bus->capacity;
        output[i * 2u] += bus->samples[src_frame * 2u] * gain;
        output[i * 2u + 1u] += bus->samples[src_frame * 2u + 1u] * gain;
    }
    atomic_store_explicit(&bus->read_pos,
                          read_pos + (uint32_t)readable,
                          memory_order_release);
}

void audio_playback_render(float *stereo_interleaved, size_t frames)
{
    if (!stereo_interleaved || frames == 0u) return;
    memset(stereo_interleaved, 0,
           frames * AUDIO_PLAYBACK_CHANNELS * sizeof(float));
    if (!s_available ||
        atomic_load_explicit(&s_state, memory_order_acquire) !=
            AUDIO_PLAYBACK_PLAYING) {
        return;
    }

    for (int i = 0; i < AUDIO_PLAYBACK_BUS_COUNT; i++) {
        mix_bus(&s_buses[i], stereo_interleaved, frames);
    }

    const float master_gain = bits_float(atomic_load_explicit(
        &s_master_gain_bits, memory_order_acquire));
    const size_t samples = frames * AUDIO_PLAYBACK_CHANNELS;
    for (size_t i = 0; i < samples; i++) {
        float sample = stereo_interleaved[i] * master_gain;
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        stereo_interleaved[i] = sample;
    }
}
