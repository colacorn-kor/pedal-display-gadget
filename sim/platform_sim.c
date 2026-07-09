#include "platform.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "tuner.h"

#define SIM_NVS_FILE "sim_nvs.bin"
#define INPUT_HOLD_MS 500u
#define INPUT_REPEAT_DELAY_MS 400u
#define INPUT_REPEAT_RATE_MS 120u
#define EVENT_QUEUE_CAP 64
#define SIM_SCREEN_W 480.0f

typedef struct {
    SDL_Keycode key;
    ui_event_t ev_short;
    ui_event_t ev_hold;
    bool has_hold;
    bool repeats;
    bool down;
    bool hold_fired;
    uint32_t down_ms;
    uint32_t next_repeat_ms;
} sim_button_t;

static sim_button_t s_buttons[] = {
    { .key = SDLK_UP, .ev_short = EV_UP, .repeats = true },
    { .key = SDLK_DOWN, .ev_short = EV_DOWN, .repeats = true },
    { .key = SDLK_LEFT, .ev_short = EV_LEFT, .repeats = true },
    { .key = SDLK_RIGHT, .ev_short = EV_RIGHT, .repeats = true },
    { .key = SDLK_RETURN, .ev_short = EV_OK },
    { .key = SDLK_KP_ENTER, .ev_short = EV_OK },
    { .key = SDLK_h, .ev_short = EV_HOME,
      .ev_hold = EV_HOME_HOLD, .has_hold = true },
    { .key = SDLK_f, .ev_short = EV_FOOTSW,
      .ev_hold = EV_FOOTSW_HOLD, .has_hold = true },
};

static ui_event_t s_events[EVENT_QUEUE_CAP];
static int s_q_head;
static int s_q_tail;
static bool s_initialized;
static bool s_quit;
static float s_mouse_x = SIM_SCREEN_W * 0.5f;
static uint32_t s_onset_seq;
static uint32_t s_onset_ms;
static float s_onset_strength;
static audio_mode_t s_audio_mode = AUDIO_SPECTRUM;
static viz_mode_t s_viz_mode = VIZ_MONITOR;
static int s_mute;

static const char *NOTE_NAMES[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static bool queue_push(ui_event_t ev)
{
    int next = (s_q_tail + 1) % EVENT_QUEUE_CAP;
    if (next == s_q_head) return false;
    s_events[s_q_tail] = ev;
    s_q_tail = next;
    return true;
}

static bool queue_pop(ui_event_t *ev)
{
    if (s_q_head == s_q_tail) return false;
    if (ev) *ev = s_events[s_q_head];
    s_q_head = (s_q_head + 1) % EVENT_QUEUE_CAP;
    return true;
}

static sim_button_t *button_for_key(SDL_Keycode key)
{
    const int n = (int)(sizeof(s_buttons) / sizeof(s_buttons[0]));
    for (int i = 0; i < n; i++) {
        if (s_buttons[i].key == key) return &s_buttons[i];
    }
    return NULL;
}

static void button_down(sim_button_t *button, uint32_t now)
{
    if (!button || button->down) return;
    button->down = true;
    button->hold_fired = false;
    button->down_ms = now;
    button->next_repeat_ms = now + INPUT_REPEAT_DELAY_MS;
    if (!button->has_hold) queue_push(button->ev_short);
}

static void button_up(sim_button_t *button)
{
    if (!button || !button->down) return;
    if (button->has_hold && !button->hold_fired) queue_push(button->ev_short);
    button->down = false;
    button->hold_fired = false;
}

static float clampf(float value, float lo, float hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static float onset_decay(uint32_t now)
{
    uint32_t age = now - s_onset_ms;
    if (age >= 260u) return 0.0f;
    return 1.0f - (float)age / 260.0f;
}

static void trigger_onset(uint32_t now)
{
    s_onset_seq++;
    s_onset_ms = now;
    s_onset_strength = 1.35f;
}

static int sdl_event_watch(void *userdata, SDL_Event *event)
{
    (void)userdata;
    const uint32_t now = SDL_GetTicks();

    if (event->type == SDL_QUIT) {
        s_quit = true;
        return 0;
    }

    if (event->type == SDL_WINDOWEVENT &&
        event->window.event == SDL_WINDOWEVENT_CLOSE) {
        s_quit = true;
        return 0;
    }

    if (event->type == SDL_MOUSEMOTION) {
        s_mouse_x = clampf((float)event->motion.x, 0.0f, SIM_SCREEN_W - 1.0f);
        return 0;
    }

    if (event->type == SDL_KEYDOWN && event->key.repeat == 0) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_SPACE) {
            trigger_onset(now);
            return 0;
        }
        if (key == SDLK_ESCAPE || key == SDLK_q) {
            s_quit = true;
            return 0;
        }
        button_down(button_for_key(key), now);
        return 0;
    }

    if (event->type == SDL_KEYUP) {
        button_up(button_for_key(event->key.keysym.sym));
    }

    return 0;
}

static void poll_button_timers(uint32_t now)
{
    const int n = (int)(sizeof(s_buttons) / sizeof(s_buttons[0]));
    for (int i = 0; i < n; i++) {
        sim_button_t *button = &s_buttons[i];
        if (!button->down) continue;

        uint32_t held = now - button->down_ms;
        if (button->has_hold && !button->hold_fired &&
            held >= INPUT_HOLD_MS) {
            button->hold_fired = true;
            queue_push(button->ev_hold);
        }

        if (button->repeats && held >= INPUT_REPEAT_DELAY_MS &&
            now >= button->next_repeat_ms) {
            queue_push(button->ev_short);
            button->next_repeat_ms = now + INPUT_REPEAT_RATE_MS;
        }
    }
}

static float sim_pitch_hz(void)
{
    float ratio = clampf(s_mouse_x / (SIM_SCREEN_W - 1.0f), 0.0f, 1.0f);
    return 82.4069f * powf(2.0f, ratio * 3.0f);
}

static void note_from_hz(float f0, const char **name, int *octave, float *cents)
{
    float midi = 69.0f + 12.0f * log2f(f0 / 440.0f);
    int nearest = (int)floorf(midi + 0.5f);
    int note = nearest % 12;
    if (note < 0) note += 12;
    if (name) *name = NOTE_NAMES[note];
    if (octave) *octave = nearest / 12 - 1;
    if (cents) *cents = (midi - (float)nearest) * 100.0f;
}

static void synth_music_snapshot(music_snapshot_t *out)
{
    if (!out) return;

    const uint32_t now = SDL_GetTicks();
    const float f0 = sim_pitch_hz();
    const char *name = "E";
    int octave = 2;
    float cents = 0.0f;
    note_from_hz(f0, &name, &octave, &cents);

    memset(out, 0, sizeof(*out));
    out->onset_seq = s_onset_seq;
    out->onset_ms = s_onset_ms;
    out->onset_strength = s_onset_strength;
    out->level = clampf(0.25f + 0.65f * onset_decay(now), 0.0f, 1.0f);
    out->bpm = 120.0f;
    out->pitch_valid = true;
    out->f0 = f0;
    out->note_name = name;
    out->octave = octave;
    out->cents = cents;
    out->clarity = 0.92f;
}

void plat_init(void)
{
    if (s_initialized) return;
    s_initialized = true;
    SDL_AddEventWatch(sdl_event_watch, NULL);
}

uint32_t plat_millis(void)
{
    return SDL_GetTicks();
}

bool plat_input_poll(ui_event_t *ev)
{
    poll_button_timers(SDL_GetTicks());
    return queue_pop(ev);
}

void plat_nvs_load(void *blob, size_t n, bool *found)
{
    if (found) *found = false;
    if (!blob || n == 0) return;

    FILE *file = fopen(SIM_NVS_FILE, "rb");
    if (!file) return;

    size_t got = fread(blob, 1, n, file);
    int extra = fgetc(file);
    fclose(file);

    if (got == n && extra == EOF && found) *found = true;
}

void plat_nvs_save(const void *blob, size_t n)
{
    if (!blob || n == 0) return;

    FILE *file = fopen(SIM_NVS_FILE, "wb");
    if (!file) {
        fprintf(stderr, "sim: failed to open %s for write\n", SIM_NVS_FILE);
        return;
    }
    if (fwrite(blob, 1, n, file) != n) {
        fprintf(stderr, "sim: failed to write %s\n", SIM_NVS_FILE);
    }
    fclose(file);
}

void plat_audio_viz_get(audio_viz_snapshot_t *out)
{
    if (!out) return;

    const float t = (float)SDL_GetTicks() * 0.001f;
    const float kick = onset_decay(SDL_GetTicks());
    float level = 0.34f + 0.18f * sinf(t * 2.4f) + 0.42f * kick;
    level = clampf(level, 0.0f, 1.0f);

    static float peaks[VIZ_POINTS];
    for (int i = 0; i < VIZ_POINTS; i++) {
        float x = (float)i / (float)(VIZ_POINTS - 1);
        float sweep = 0.5f + 0.5f * sinf(t * 3.0f + x * 23.0f);
        float ripple = 0.5f + 0.5f * sinf(t * 9.0f + x * 71.0f);
        float tilt = 1.0f - 0.55f * x;
        float value = (0.08f + 0.72f * sweep + 0.18f * ripple) * tilt;
        value = clampf(value * (0.45f + level), 0.0f, 1.0f);

        peaks[i] *= 0.965f;
        if (value > peaks[i]) peaks[i] = value;
        out->bars[i] = value;
        out->peaks[i] = peaks[i];
    }
    out->level = level;
}

void plat_music_get(music_snapshot_t *out)
{
    synth_music_snapshot(out);
}

void plat_lvgl_lock(void)
{
}

void plat_lvgl_unlock(void)
{
}

bool plat_sim_should_quit(void)
{
    return s_quit;
}

void audio_set_mode(audio_mode_t mode)
{
    if (mode == AUDIO_SPECTRUM || mode == AUDIO_TUNER) s_audio_mode = mode;
}

audio_mode_t audio_get_mode(void)
{
    return s_audio_mode;
}

void audio_set_viz_mode(viz_mode_t mode)
{
    if (mode == VIZ_MONITOR || mode == VIZ_DECOR) s_viz_mode = mode;
}

void audio_viz_snapshot_get(audio_viz_snapshot_t *out)
{
    plat_audio_viz_get(out);
}

void mute_set(int on)
{
    s_mute = on ? 1 : 0;
}

int mute_get(void)
{
    return s_mute;
}

void music_snapshot_get(music_snapshot_t *out)
{
    plat_music_get(out);
}

void tuner_init(void)
{
}

void tuner_reset(void)
{
}

int tuner_feed(const float *samples, int n)
{
    (void)samples;
    (void)n;
    return 0;
}

void tuner_get(tuner_result_t *out)
{
    if (!out) return;

    const float f0 = sim_pitch_hz();
    const char *name = "E";
    int octave = 2;
    float cents = 0.0f;
    note_from_hz(f0, &name, &octave, &cents);

    out->voiced = 1;
    out->f0 = f0;
    out->name = name;
    out->octave = octave;
    out->cents = cents;
    out->clarity = 0.92f;
}
