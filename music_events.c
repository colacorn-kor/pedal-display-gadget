#include "music_events.h"

#include <math.h>
#include <stdatomic.h>
#include <string.h>

#include "app.h"
#include "esp_timer.h"
#include "tuner.h"

#define MUSIC_ONSET_ATTACK_COEF       0.50f
#define MUSIC_ONSET_RELEASE_COEF      0.02f
#define MUSIC_ONSET_RATIO             1.80f
#define MUSIC_ONSET_FLOOR             0.008f
#define MUSIC_ONSET_REFRACTORY_MS     80u
#define MUSIC_BPM_INTERVAL_MIN_MS     250u
#define MUSIC_BPM_INTERVAL_MAX_MS     2000u
#define MUSIC_BPM_INTERVALS           8
#define MUSIC_BPM_MIN_VALID_INTERVALS 4
#define MUSIC_PITCH_CLARITY_MIN       0.80f

static music_snapshot_t s_snapshots[2];
static _Atomic unsigned s_snapshot_seq[2];
static _Atomic int s_snapshot_ready;
static int s_snapshot_producer = 1;

static music_snapshot_t s_current;
static float s_slow_env;
static uint32_t s_last_onset_ms;
static uint32_t s_intervals[MUSIC_BPM_INTERVALS];
static int s_interval_pos;
static int s_interval_count;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void publish_current(void)
{
    const int slot = s_snapshot_producer;
    atomic_fetch_add_explicit(&s_snapshot_seq[slot], 1U, memory_order_acq_rel);
    s_snapshots[slot] = s_current;
    atomic_fetch_add_explicit(&s_snapshot_seq[slot], 1U, memory_order_release);
    atomic_store_explicit(&s_snapshot_ready, slot, memory_order_release);
    s_snapshot_producer ^= 1;
}

static void push_interval(uint32_t interval_ms)
{
    s_intervals[s_interval_pos] = interval_ms;
    s_interval_pos = (s_interval_pos + 1) % MUSIC_BPM_INTERVALS;
    if (s_interval_count < MUSIC_BPM_INTERVALS) s_interval_count++;
}

static float median_bpm(void)
{
    uint32_t valid[MUSIC_BPM_INTERVALS];
    int count = 0;
    for (int i = 0; i < s_interval_count; i++) {
        uint32_t interval = s_intervals[i];
        if (interval >= MUSIC_BPM_INTERVAL_MIN_MS &&
            interval <= MUSIC_BPM_INTERVAL_MAX_MS) {
            valid[count++] = interval;
        }
    }
    if (count < MUSIC_BPM_MIN_VALID_INTERVALS) return 0.0f;

    for (int i = 1; i < count; i++) {
        uint32_t value = valid[i];
        int j = i - 1;
        while (j >= 0 && valid[j] > value) {
            valid[j + 1] = valid[j];
            j--;
        }
        valid[j + 1] = value;
    }

    float median;
    if ((count & 1) != 0) {
        median = (float)valid[count / 2];
    } else {
        median = 0.5f * (float)(valid[count / 2 - 1] + valid[count / 2]);
    }
    return median > 0.0f ? 60000.0f / median : 0.0f;
}

static void update_onset(float rms, uint32_t t_ms)
{
    if (!(rms >= 0.0f) || !isfinite(rms)) rms = 0.0f;
    if (s_slow_env <= 0.0f) s_slow_env = rms;

    const float baseline = s_slow_env > 1e-6f ? s_slow_env : 1e-6f;
    const bool refractory_elapsed =
        s_current.onset_seq == 0 ||
        (uint32_t)(t_ms - s_last_onset_ms) >= MUSIC_ONSET_REFRACTORY_MS;
    if (refractory_elapsed &&
        rms > MUSIC_ONSET_RATIO * baseline + MUSIC_ONSET_FLOOR) {
        if (s_current.onset_seq > 0) push_interval(t_ms - s_last_onset_ms);
        s_last_onset_ms = t_ms;
        s_current.onset_seq++;
        s_current.onset_strength = rms / baseline;
        s_current.onset_ms = t_ms;
        s_current.bpm = median_bpm();
    }

    const float coef = rms > s_slow_env
        ? MUSIC_ONSET_ATTACK_COEF
        : MUSIC_ONSET_RELEASE_COEF;
    s_slow_env += coef * (rms - s_slow_env);
}

static void update_pitch(void)
{
    if (audio_get_mode() != AUDIO_TUNER) {
        s_current.pitch_valid = false;
        s_current.f0 = 0.0f;
        s_current.note_name = "-";
        s_current.octave = 0;
        s_current.cents = 0.0f;
        s_current.clarity = 0.0f;
        return;
    }

    tuner_result_t result;
    tuner_get(&result);
    s_current.pitch_valid =
        result.voiced && result.clarity > MUSIC_PITCH_CLARITY_MIN;
    s_current.f0 = result.f0;
    s_current.note_name = result.name ? result.name : "-";
    s_current.octave = result.octave;
    s_current.cents = result.cents;
    s_current.clarity = result.clarity;
}

void music_events_init(void)
{
    memset(s_snapshots, 0, sizeof(s_snapshots));
    memset(s_intervals, 0, sizeof(s_intervals));
    atomic_store_explicit(&s_snapshot_seq[0], 0U, memory_order_release);
    atomic_store_explicit(&s_snapshot_seq[1], 0U, memory_order_release);
    atomic_store_explicit(&s_snapshot_ready, 0, memory_order_release);
    s_snapshot_producer = 1;
    s_current = (music_snapshot_t) {
        .note_name = "-",
    };
    s_slow_env = 0.0f;
    s_last_onset_ms = 0;
    s_interval_pos = 0;
    s_interval_count = 0;
    publish_current();
}

void music_events_process_block(float rms, float level)
{
    if (!(level >= 0.0f) || !isfinite(level)) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    s_current.level = level;

    const uint32_t t_ms = now_ms();
    update_onset(rms, t_ms);
    update_pitch();
    publish_current();
}

void music_snapshot_get(music_snapshot_t *out)
{
    if (!out) return;

    for (;;) {
        int idx = atomic_load_explicit(&s_snapshot_ready, memory_order_acquire);
        unsigned before = atomic_load_explicit(&s_snapshot_seq[idx], memory_order_acquire);
        if (before & 1U) continue;

        *out = s_snapshots[idx];
        atomic_thread_fence(memory_order_acquire);

        unsigned after = atomic_load_explicit(&s_snapshot_seq[idx], memory_order_acquire);
        if (before == after && !(after & 1U)) return;
    }
}
