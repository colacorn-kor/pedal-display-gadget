#include "platform.h"

#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "audio_playback.h"
#include "platform_sim.h"
#include "sim_audio.h"
#include "sim_audio_output.h"
#include "sim_midi.h"

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
    { .key = SDLK_BACKSPACE, .ev_short = EV_HOME,
      .ev_hold = EV_HOME_HOLD, .has_hold = true },
    { .key = SDLK_SPACE, .ev_short = EV_FOOTSW,
      .ev_hold = EV_FOOTSW_HOLD, .has_hold = true },
};

static ui_event_t s_events[EVENT_QUEUE_CAP];
static int s_q_head;
static int s_q_tail;
static bool s_initialized;
static bool s_audio_configured;
static bool s_output_configured;
static bool s_midi_configured;
static bool s_quit;
static bool s_smoke_test;
static bool s_renderer_benchmark;
static const char *s_preview;
static platform_game_input_mask_t s_direct_game_input;
static audio_mode_t s_audio_mode = AUDIO_SPECTRUM;
static viz_mode_t s_viz_mode = VIZ_MONITOR;
static int s_mute;

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

static int sdl_event_watch(void *userdata, SDL_Event *event)
{
    (void)userdata;

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
        sim_audio_set_mouse_x(clampf((float)event->motion.x,
                                     0.0f, SIM_SCREEN_W - 1.0f));
        return 0;
    }

    if (event->type == SDL_KEYDOWN && event->key.repeat == 0) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_z) {
            s_direct_game_input |= PLAT_GAME_INPUT_A;
            return 0;
        }
        if (key == SDLK_x) {
            s_direct_game_input |= PLAT_GAME_INPUT_B;
            return 0;
        }
        if (key == SDLK_a) {
            s_direct_game_input |= PLAT_GAME_INPUT_SELECT;
            return 0;
        }
        if (key == SDLK_s) {
            s_direct_game_input |= PLAT_GAME_INPUT_START;
            return 0;
        }
        if (key == SDLK_o) {
            sim_audio_trigger_synthetic_onset();
            return 0;
        }
        if (key == SDLK_ESCAPE) {
            s_quit = true;
            return 0;
        }
        button_down(button_for_key(key), SDL_GetTicks());
        return 0;
    }

    if (event->type == SDL_KEYUP) {
        const SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_z) s_direct_game_input &= ~PLAT_GAME_INPUT_A;
        else if (key == SDLK_x) s_direct_game_input &= ~PLAT_GAME_INPUT_B;
        else if (key == SDLK_a) s_direct_game_input &= ~PLAT_GAME_INPUT_SELECT;
        else if (key == SDLK_s) s_direct_game_input &= ~PLAT_GAME_INPUT_START;
        else button_up(button_for_key(key));
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

bool plat_sim_configure(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--smoke-test") == 0) {
            s_smoke_test = true;
        } else if (strcmp(argv[i], "--renderer-benchmark") == 0) {
            s_renderer_benchmark = true;
            s_smoke_test = true;
        } else if (strcmp(argv[i], "--preview") == 0 && i + 1 < argc) {
            s_preview = argv[++i];
            s_smoke_test = true;
        }
    }
    if (!sim_midi_init(argc, argv, s_smoke_test)) return false;
    s_midi_configured = true;
    if (sim_midi_should_exit_after_args()) return true;
    s_audio_configured = true;
    if (!sim_audio_init(argc, argv)) return false;
    if (!sim_audio_should_exit_after_args()) {
        const bool output_ready = sim_audio_output_init(
            argc, argv, s_smoke_test);
        if (!audio_playback_init(output_ready)) {
            sim_audio_output_shutdown();
            (void)audio_playback_init(false);
        }
        s_output_configured = true;
    }
    return true;
}

bool plat_sim_should_exit_after_args(void)
{
    return sim_audio_should_exit_after_args() ||
           sim_midi_should_exit_after_args();
}

void plat_init(void)
{
    if (s_initialized) return;
    s_initialized = true;
    if (!s_midi_configured) {
        (void)sim_midi_init(0, NULL, false);
        s_midi_configured = true;
    }
    if (!s_audio_configured) (void)sim_audio_init(0, NULL);
    if (!s_output_configured) {
        const bool output_ready = sim_audio_output_init(0, NULL, false);
        if (!audio_playback_init(output_ready)) {
            sim_audio_output_shutdown();
            (void)audio_playback_init(false);
        }
        s_output_configured = true;
    }
    SDL_AddEventWatch(sdl_event_watch, NULL);
}

platform_capability_mask_t plat_capabilities(void)
{
    platform_capability_mask_t capabilities =
        PLAT_CAP_DISPLAY |
        PLAT_CAP_AUDIO_ANALYSIS_INPUT |
        PLAT_CAP_MEDIA_STORAGE |
        PLAT_CAP_GAME_RUNTIME;
    if (audio_playback_is_available()) {
        capabilities |= PLAT_CAP_AUDIO_PLAYBACK_OUTPUT;
    }
    platform_midi_status_t midi;
    sim_midi_get_status(&midi);
    if (midi.input_available) capabilities |= PLAT_CAP_MIDI_INPUT;
    if (midi.output_available) capabilities |= PLAT_CAP_MIDI_OUTPUT;
    return capabilities;
}

uint32_t plat_millis(void)
{
    return SDL_GetTicks();
}

uint64_t plat_micros(void)
{
    return (uint64_t)((double)SDL_GetPerformanceCounter() * 1000000.0 /
                      (double)SDL_GetPerformanceFrequency());
}

bool plat_input_poll(ui_event_t *ev)
{
    sim_midi_pump();
    sim_audio_pump();
    sim_audio_output_pump();
    poll_button_timers(SDL_GetTicks());
    return queue_pop(ev);
}

platform_game_input_mask_t plat_game_input_state(void)
{
    platform_game_input_mask_t state = s_direct_game_input;
    const int count = (int)(sizeof(s_buttons) / sizeof(s_buttons[0]));
    for (int i = 0; i < count; i++) {
        if (!s_buttons[i].down) continue;
        switch (s_buttons[i].ev_short) {
        case EV_UP: state |= PLAT_GAME_INPUT_UP; break;
        case EV_DOWN: state |= PLAT_GAME_INPUT_DOWN; break;
        case EV_LEFT: state |= PLAT_GAME_INPUT_LEFT; break;
        case EV_RIGHT: state |= PLAT_GAME_INPUT_RIGHT; break;
        case EV_OK: state |= PLAT_GAME_INPUT_OK; break;
        default: break;
        }
    }
    return state;
}

void plat_game_input_publish(platform_game_input_mask_t held_mask)
{
    (void)held_mask;
}

void plat_nvs_load(void *blob, size_t n, bool *found)
{
    if (found) *found = false;
    if (s_smoke_test || !blob || n == 0) return;

    FILE *file = fopen(SIM_NVS_FILE, "rb");
    if (!file) return;

    size_t got = fread(blob, 1, n, file);
    int extra = fgetc(file);
    fclose(file);

    if (got == n && extra == EOF && found) *found = true;
}

void plat_nvs_save(const void *blob, size_t n)
{
    if (s_smoke_test || !blob || n == 0) return;

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
    sim_audio_audio_viz_get(out);
}

void plat_music_get(music_snapshot_t *out)
{
    sim_audio_music_get(out);
}

void plat_midi_get_status(platform_midi_status_t *out)
{
    sim_midi_get_status(out);
}

bool plat_midi_send_short(uint8_t status, uint8_t data1, uint8_t data2)
{
    return sim_midi_send_short(status, data1, data2);
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

bool plat_sim_is_smoke_test(void)
{
    return s_smoke_test;
}

bool plat_sim_is_renderer_benchmark(void)
{
    return s_renderer_benchmark;
}

const char *plat_sim_preview(void)
{
    return s_preview;
}

bool plat_sim_post_event(ui_event_t event)
{
    return queue_push(event);
}

void plat_sim_trigger_onset(void)
{
    sim_audio_trigger_synthetic_onset();
}

bool plat_sim_midi_inject_short(uint8_t status, uint8_t data1, uint8_t data2)
{
    return sim_midi_inject_short(status, data1, data2);
}

bool ui_post_event(ui_event_t event)
{
    return queue_push(event);
}

bool ui_post_scene(int content_idx, int theme_idx, const char *renderer_name)
{
    sm_load_scene_named(content_idx, theme_idx, renderer_name);
    return true;
}

bool ui_post_tempo(float bpm)
{
    sm_set_tempo(bpm);
    return true;
}

bool ui_post_mute_toggle(void)
{
    mute_set(!mute_get());
    return true;
}

void audio_set_mode(audio_mode_t mode)
{
    if (mode == AUDIO_SPECTRUM || mode == AUDIO_TUNER ||
        mode == AUDIO_METER ||
        mode == AUDIO_NONE) s_audio_mode = mode;
}

audio_mode_t audio_get_mode(void)
{
    return s_audio_mode;
}

void audio_set_viz_mode(viz_mode_t mode)
{
    if (mode == VIZ_MONITOR || mode == VIZ_DECOR ||
        mode == VIZ_REFERENCE) s_viz_mode = mode;
}

viz_mode_t audio_get_viz_mode(void)
{
    return s_viz_mode;
}

void audio_loudness_reset(void)
{
    sim_audio_loudness_reset();
}

void audio_viz_snapshot_get(audio_viz_snapshot_t *out)
{
    plat_audio_viz_get(out);
}

void audio_waveform_set_enabled(bool enabled)
{
    sim_audio_waveform_set_enabled(enabled);
}

void audio_waveform_snapshot_get(audio_waveform_snapshot_t *out)
{
    sim_audio_waveform_get(out);
}

void mute_set(int on)
{
    s_mute = on ? 1 : 0;
}

int mute_get(void)
{
    return s_mute;
}
