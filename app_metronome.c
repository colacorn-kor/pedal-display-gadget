#include "gadget_app.h"

#include <stdio.h>
#include <string.h>

#include "app_slots.h"
#include "audio_playback.h"
#include "metronome_engine.h"
#include "theme.h"

#define METRONOME_OWNER_ID "metronome"
#define METRONOME_BPM_MIN 40
#define METRONOME_BPM_MAX 220
#define METRONOME_BPM_STEP 5
#define METRONOME_FILL_TARGET 1536u
#define METRONOME_CHUNK_FRAMES 128u
#define METRONOME_SAVE_DELAY_MS 800u
#define METRONOME_BEAT_MAX 5
#define METRONOME_TICK_MAX 20

typedef enum {
    METRONOME_CONTROL_TEMPO = 0,
    METRONOME_CONTROL_METER,
    METRONOME_CONTROL_DIVISION,
    METRONOME_CONTROL_COUNT,
} metronome_control_t;

static const int METERS[] = { 2, 3, 4, 5 };
static const int SUBDIVISIONS[] = { 1, 2, 3, 4 };
static const char *const DIVISION_NAMES[] = {
    "Quarter", "Eighth", "Triplet", "Sixteenth",
};

static lv_obj_t *s_host;
static lv_obj_t *s_status;
static lv_obj_t *s_bpm_value;
static lv_obj_t *s_meter_value;
static lv_obj_t *s_division_value;
static lv_obj_t *s_play_icon;
static lv_obj_t *s_beats[METRONOME_BEAT_MAX];
static lv_obj_t *s_beat_labels[METRONOME_BEAT_MAX];
static lv_obj_t *s_ticks[METRONOME_TICK_MAX];
static lv_obj_t *s_controls[METRONOME_CONTROL_COUNT];
static lv_obj_t *s_control_titles[METRONOME_CONTROL_COUNT];
static lv_obj_t *s_control_values[METRONOME_CONTROL_COUNT];

static metronome_engine_t s_engine;
static metronome_control_t s_control;
static int s_bpm = 120;
static int s_meter_index = 2;
static int s_division_index;
static int s_last_visual_tick = -2;
static uint32_t s_started_ms;
static uint32_t s_settings_changed_ms;
static bool s_running;
static bool s_claimed;
static bool s_settings_dirty;

static const ui_theme_t *metronome_theme(void)
{
    return theme_for_app_color(app_slots_color(&APP_METRONOME));
}

static void style_group(lv_obj_t *obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    return label;
}

static lv_obj_t *make_block(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *block = lv_obj_create(parent);
    lv_obj_set_pos(block, x, y);
    lv_obj_set_size(block, w, h);
    lv_obj_remove_flag(block, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(block, 0, 0);
    lv_obj_set_style_radius(block, 3, 0);
    return block;
}

static metronome_config_t current_config(void)
{
    const metronome_config_t config = {
        s_bpm,
        METERS[s_meter_index],
        SUBDIVISIONS[s_division_index],
    };
    return config;
}

static uint8_t encode_options(void)
{
    int tempo_index = (s_bpm - METRONOME_BPM_MIN +
                       METRONOME_BPM_STEP / 2) / METRONOME_BPM_STEP;
    if (tempo_index < 0) tempo_index = 0;
    if (tempo_index > 36) tempo_index = 36;
    return (uint8_t)(1 + tempo_index * 4 + s_meter_index);
}

static void load_options(void)
{
    const uint8_t encoded = app_slots_options(&APP_METRONOME);
    if (encoded >= 1u && encoded <= 149u) {
        const int packed = (int)encoded - 1;
        s_bpm = METRONOME_BPM_MIN +
                (packed / 4) * METRONOME_BPM_STEP;
        s_meter_index = packed % 4;
    } else {
        s_bpm = 120;
        s_meter_index = 2;
    }
    s_settings_dirty = false;
}

static void mark_settings_changed(void)
{
    app_slots_set_mode_runtime(&APP_METRONOME,
                               (uint8_t)s_division_index);
    app_slots_set_options_runtime(&APP_METRONOME, encode_options());
    s_settings_dirty = true;
    s_settings_changed_ms = plat_millis();
}

static void save_settings_if_due(uint32_t now, bool force)
{
    if (!s_settings_dirty) return;
    if (!force && (uint32_t)(now - s_settings_changed_ms) <
                      METRONOME_SAVE_DELAY_MS) {
        return;
    }
    app_slots_save();
    s_settings_dirty = false;
}

static void fill_audio_queue(void)
{
    float chunk[METRONOME_CHUNK_FRAMES * AUDIO_PLAYBACK_CHANNELS];
    if (!s_claimed || !s_running) return;

    audio_playback_status_t status;
    audio_playback_get_status(&status);
    while (status.queued_effect_frames < METRONOME_FILL_TARGET) {
        size_t frames = METRONOME_FILL_TARGET -
                        status.queued_effect_frames;
        if (frames > METRONOME_CHUNK_FRAMES) {
            frames = METRONOME_CHUNK_FRAMES;
        }
        metronome_engine_render(&s_engine, chunk, frames);
        if (audio_playback_write(METRONOME_OWNER_ID,
                                 AUDIO_PLAYBACK_BUS_EFFECTS,
                                 chunk, frames) < frames) {
            break;
        }
        audio_playback_get_status(&status);
    }
}

static void restart_clock(void)
{
    const metronome_config_t config = current_config();
    metronome_engine_set_config(&s_engine, &config);
    s_started_ms = plat_millis();
    s_last_visual_tick = -2;
    if (s_claimed) {
        (void)audio_playback_stop(METRONOME_OWNER_ID);
        (void)audio_playback_play(METRONOME_OWNER_ID);
        fill_audio_queue();
    }
}

static void apply_clock_change(void)
{
    sm_set_tempo((float)s_bpm);
    mark_settings_changed();
    if (s_running) restart_clock();
}

static void update_control_text(void)
{
    if (!s_host) return;
    char text[24];
    (void)snprintf(text, sizeof(text), "%d", s_bpm);
    lv_label_set_text(s_bpm_value, text);
    (void)snprintf(text, sizeof(text), "%d/4", METERS[s_meter_index]);
    lv_label_set_text(s_meter_value, text);
    lv_label_set_text(s_division_value,
                      DIVISION_NAMES[s_division_index]);

    lv_label_set_text(s_control_values[METRONOME_CONTROL_TEMPO],
                      s_bpm_value ? lv_label_get_text(s_bpm_value) : "");
    lv_label_set_text(s_control_values[METRONOME_CONTROL_METER],
                      s_meter_value ? lv_label_get_text(s_meter_value) : "");
    lv_label_set_text(s_control_values[METRONOME_CONTROL_DIVISION],
                      DIVISION_NAMES[s_division_index]);
}

static int current_visual_tick(uint32_t now)
{
    if (!s_running) return -1;
    const uint64_t elapsed = (uint32_t)(now - s_started_ms);
    const uint64_t ticks = elapsed * (uint32_t)s_bpm *
                           (uint32_t)SUBDIVISIONS[s_division_index] /
                           60000u;
    const int ticks_per_bar = METERS[s_meter_index] *
                              SUBDIVISIONS[s_division_index];
    return (int)(ticks % (uint32_t)ticks_per_bar);
}

static void style_clock(int active_tick)
{
    if (!s_host) return;
    const ui_theme_t *theme = metronome_theme();
    const int beats = METERS[s_meter_index];
    const int subdivision = SUBDIVISIONS[s_division_index];
    const int active_beat = active_tick >= 0 ? active_tick / subdivision : -1;
    const int tick_count = beats * subdivision;
    const int beat_x0 = (480 - (beats - 1) * 60 - 52) / 2;

    lv_obj_set_style_bg_color(s_host, theme->bg, 0);
    lv_obj_set_style_bg_opa(s_host, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_bpm_value, theme->text, 0);
    lv_obj_set_style_text_color(s_meter_value, theme->text, 0);
    lv_obj_set_style_text_color(s_division_value, theme->text, 0);
    lv_obj_set_style_text_color(s_status, theme->accent2, 0);
    lv_obj_set_style_text_color(s_play_icon, theme->accent, 0);

    for (int i = 0; i < METRONOME_BEAT_MAX; i++) {
        const bool visible = i < beats;
        if (visible) {
            lv_obj_remove_flag(s_beats[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_x(s_beats[i], beat_x0 + i * 60);
        } else {
            lv_obj_add_flag(s_beats[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const bool active = i == active_beat;
        const lv_color_t accent = i == 0 ? theme->accent2 : theme->accent;
        lv_obj_set_style_border_width(s_beats[i], active ? 2 : 1, 0);
        lv_obj_set_style_border_color(s_beats[i], accent, 0);
        lv_obj_set_style_bg_color(s_beats[i], accent, 0);
        lv_obj_set_style_bg_opa(s_beats[i],
                                active ? LV_OPA_COVER : LV_OPA_20, 0);
        lv_obj_set_style_text_color(s_beat_labels[i],
                                    active ? theme->bg : theme->text, 0);
    }

    const int dot_spacing = 13;
    const int dot_x0 = (480 - (tick_count - 1) * dot_spacing - 8) / 2;
    for (int i = 0; i < METRONOME_TICK_MAX; i++) {
        if (i >= tick_count) {
            lv_obj_add_flag(s_ticks[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(s_ticks[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(s_ticks[i], dot_x0 + i * dot_spacing);
        const bool active = i == active_tick;
        const bool beat_tick = i % subdivision == 0;
        lv_obj_set_style_border_width(s_ticks[i], 0, 0);
        lv_obj_set_style_bg_color(s_ticks[i],
            beat_tick ? theme->accent2 : theme->accent, 0);
        lv_obj_set_style_bg_opa(s_ticks[i],
            active ? LV_OPA_COVER : (beat_tick ? LV_OPA_50 : LV_OPA_20), 0);
    }

    for (int i = 0; i < METRONOME_CONTROL_COUNT; i++) {
        const bool selected = i == (int)s_control;
        lv_obj_set_style_border_width(s_controls[i], selected ? 2 : 1, 0);
        lv_obj_set_style_border_color(s_controls[i],
                                      selected ? theme->accent : theme->grid, 0);
        lv_obj_set_style_bg_color(s_controls[i], theme->surface, 0);
        lv_obj_set_style_bg_opa(s_controls[i], LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(s_control_titles[i], theme->text, 0);
        lv_obj_set_style_text_opa(s_control_titles[i], LV_OPA_50, 0);
        lv_obj_set_style_text_color(s_control_values[i],
                                    selected ? theme->accent : theme->text, 0);
    }
}

static void refresh_ui(bool force)
{
    if (!s_host) return;
    const int tick = current_visual_tick(plat_millis());
    if (force) {
        update_control_text();
        lv_label_set_text(s_status, s_claimed ? "AUDIO" : "VISUAL");
        lv_label_set_text(s_play_icon,
                          s_running ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }
    if (force || tick != s_last_visual_tick) {
        s_last_visual_tick = tick;
        style_clock(tick);
    }
}

static void create_scene(void)
{
    static const char *const CONTROL_TITLES[] = {
        "TEMPO", "METER", "DIVISION",
    };

    s_host = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_host, 480, 320);
    lv_obj_set_pos(s_host, 0, 0);
    style_group(s_host);

    lv_obj_t *title = make_label(s_host, "METRONOME",
                                 &lv_font_montserrat_14);
    lv_obj_set_pos(title, 16, 12);
    s_status = make_label(s_host, "VISUAL", &lv_font_montserrat_12);
    lv_obj_set_width(s_status, 100);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_status, 364, 14);

    s_bpm_value = make_label(s_host, "120", &lv_font_montserrat_48);
    lv_obj_set_width(s_bpm_value, 180);
    lv_obj_set_style_text_align(s_bpm_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_bpm_value, 34, 45);
    lv_obj_t *bpm_label = make_label(s_host, "BPM",
                                     &lv_font_montserrat_14);
    lv_obj_set_pos(bpm_label, 220, 78);

    s_meter_value = make_label(s_host, "4/4", &lv_font_montserrat_28);
    lv_obj_set_width(s_meter_value, 105);
    lv_obj_set_style_text_align(s_meter_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_meter_value, 278, 47);
    s_division_value = make_label(s_host, "Quarter",
                                  &lv_font_montserrat_14);
    lv_obj_set_width(s_division_value, 150);
    lv_obj_set_style_text_align(s_division_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_division_value, 255, 84);

    for (int i = 0; i < METRONOME_BEAT_MAX; i++) {
        s_beats[i] = make_block(s_host, 94 + i * 60, 124, 52, 30);
        s_beat_labels[i] = make_label(s_beats[i], "",
                                      &lv_font_montserrat_14);
        char number[4];
        (void)snprintf(number, sizeof(number), "%d", i + 1);
        lv_label_set_text(s_beat_labels[i], number);
        lv_obj_center(s_beat_labels[i]);
    }
    for (int i = 0; i < METRONOME_TICK_MAX; i++) {
        s_ticks[i] = make_block(s_host, 0, 170, 8, 8);
        lv_obj_set_style_radius(s_ticks[i], LV_RADIUS_CIRCLE, 0);
    }

    s_play_icon = make_label(s_host, LV_SYMBOL_PLAY,
                             &lv_font_montserrat_28);
    lv_obj_set_width(s_play_icon, 60);
    lv_obj_set_style_text_align(s_play_icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_play_icon, 210, 193);

    for (int i = 0; i < METRONOME_CONTROL_COUNT; i++) {
        s_controls[i] = make_block(s_host, 12 + i * 156, 248, 144, 58);
        s_control_titles[i] = make_label(s_controls[i], CONTROL_TITLES[i],
                                         &lv_font_montserrat_12);
        lv_obj_set_width(s_control_titles[i], 132);
        lv_obj_set_style_text_align(s_control_titles[i],
                                    LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(s_control_titles[i], 6, 5);
        s_control_values[i] = make_label(s_controls[i], "",
                                         &lv_font_montserrat_14);
        lv_obj_set_width(s_control_values[i], 132);
        lv_obj_set_style_text_align(s_control_values[i],
                                    LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(s_control_values[i], 6, 29);
    }

    const ui_theme_t *theme = metronome_theme();
    lv_obj_set_style_text_color(title, theme->accent, 0);
    lv_obj_set_style_text_color(bpm_label, theme->text, 0);
    lv_obj_set_style_text_opa(bpm_label, LV_OPA_50, 0);
}

static void toggle_running(void)
{
    s_running = !s_running;
    if (s_running) {
        restart_clock();
    } else {
        s_last_visual_tick = -2;
        if (s_claimed) (void)audio_playback_stop(METRONOME_OWNER_ID);
    }
    refresh_ui(true);
}

static void set_bpm(int bpm)
{
    if (bpm < METRONOME_BPM_MIN) bpm = METRONOME_BPM_MIN;
    if (bpm > METRONOME_BPM_MAX) bpm = METRONOME_BPM_MAX;
    if (bpm == s_bpm) return;
    s_bpm = bpm;
    apply_clock_change();
    refresh_ui(true);
}

static void set_meter_index(int index)
{
    if (index < 0) index = 3;
    if (index > 3) index = 0;
    if (index == s_meter_index) return;
    s_meter_index = index;
    apply_clock_change();
    refresh_ui(true);
}

static void set_division_index(int index, bool direct)
{
    if (index < 0) index = 3;
    if (index > 3) index = 0;
    if (index == s_division_index) return;
    s_division_index = index;
    if (direct) mark_settings_changed();
    if (s_running) restart_clock();
    refresh_ui(true);
}

static void metronome_enter(int variant)
{
    (void)variant;
    audio_set_mode(AUDIO_NONE);
    load_options();
    sm_set_tempo((float)s_bpm);
    s_control = METRONOME_CONTROL_TEMPO;
    s_running = false;
    s_claimed = audio_playback_claim(METRONOME_OWNER_ID) ==
                AUDIO_PLAYBACK_OK;
    if (s_claimed) {
        (void)audio_playback_set_bus_gain(
            METRONOME_OWNER_ID, AUDIO_PLAYBACK_BUS_EFFECTS, 0.82f);
    }
    const metronome_config_t config = current_config();
    metronome_engine_init(&s_engine, &config);
    create_scene();
    refresh_ui(true);
}

static void metronome_exit(void)
{
    save_settings_if_due(plat_millis(), true);
    if (s_claimed) {
        (void)audio_playback_release(METRONOME_OWNER_ID);
    }
    s_claimed = false;
    s_running = false;
    if (s_host) lv_obj_delete(s_host);
    s_host = NULL;
    s_status = NULL;
    s_bpm_value = NULL;
    s_meter_value = NULL;
    s_division_value = NULL;
    s_play_icon = NULL;
    memset(s_beats, 0, sizeof(s_beats));
    memset(s_beat_labels, 0, sizeof(s_beat_labels));
    memset(s_ticks, 0, sizeof(s_ticks));
    memset(s_controls, 0, sizeof(s_controls));
    memset(s_control_titles, 0, sizeof(s_control_titles));
    memset(s_control_values, 0, sizeof(s_control_values));
    audio_set_mode(AUDIO_SPECTRUM);
}

static void metronome_render(void)
{
    if (!s_host) return;
    const uint32_t now = plat_millis();
    save_settings_if_due(now, false);

    const int external_bpm = (int)(sm_get_tempo() + 0.5f);
    if (external_bpm >= METRONOME_BPM_MIN &&
        external_bpm <= METRONOME_BPM_MAX && external_bpm != s_bpm) {
        s_bpm = external_bpm;
        mark_settings_changed();
        if (s_running) restart_clock();
        refresh_ui(true);
    }

    if (s_claimed && !audio_playback_is_available()) {
        s_claimed = false;
        refresh_ui(true);
    }
    fill_audio_queue();
    refresh_ui(false);
}

static bool metronome_on_event(ui_event_t event)
{
    if (event == EV_LEFT || event == EV_RIGHT) {
        const int delta = event == EV_LEFT ? -1 : 1;
        s_control = (metronome_control_t)(
            ((int)s_control + delta + METRONOME_CONTROL_COUNT) %
            METRONOME_CONTROL_COUNT);
        refresh_ui(true);
        return true;
    }
    if (event == EV_OK) {
        toggle_running();
        return true;
    }
    if (event != EV_UP && event != EV_DOWN) return false;

    const int delta = event == EV_UP ? 1 : -1;
    if (s_control == METRONOME_CONTROL_TEMPO) {
        set_bpm(s_bpm + delta * METRONOME_BPM_STEP);
    } else if (s_control == METRONOME_CONTROL_METER) {
        set_meter_index(s_meter_index + delta);
    } else {
        set_division_index(s_division_index + delta, true);
    }
    return true;
}

static void metronome_appearance_changed(void)
{
    refresh_ui(true);
}

static int metronome_mode_count(void)
{
    return 4;
}

static const char *metronome_mode_name(int idx)
{
    return idx >= 0 && idx < 4 ? DIVISION_NAMES[idx] : "";
}

static int metronome_mode_index(void)
{
    return s_division_index;
}

static void metronome_mode_set(int idx)
{
    set_division_index(idx, false);
}

static int metronome_meter_count(void)
{
    return 4;
}

static const char *metronome_meter_name(int idx)
{
    static const char *const NAMES[] = { "2/4", "3/4", "4/4", "5/4" };
    return idx >= 0 && idx < 4 ? NAMES[idx] : "";
}

static int metronome_meter_index(void)
{
    return s_meter_index;
}

static void metronome_meter_set(int idx)
{
    set_meter_index(idx);
}

static const app_choice_setting_t METRONOME_CHOICE_SETTINGS[] = {
    {
        .name = "Meter",
        .item_count = metronome_meter_count,
        .item_name = metronome_meter_name,
        .item_index = metronome_meter_index,
        .item_set = metronome_meter_set,
    },
};

const gadget_app_t APP_METRONOME = {
    .id = "metronome",
    .name = "Metronome",
    .audio_mode = AUDIO_NONE,
    .icon = NULL,
    .on_enter = metronome_enter,
    .on_exit = metronome_exit,
    .on_render = metronome_render,
    .on_event = metronome_on_event,
    .on_appearance_changed = metronome_appearance_changed,
    .mode_count = metronome_mode_count,
    .mode_name = metronome_mode_name,
    .mode_index = metronome_mode_index,
    .mode_set = metronome_mode_set,
    .choice_settings = METRONOME_CHOICE_SETTINGS,
    .choice_setting_count = 1,
    .input_sources = APP_INPUT_BUTTONS,
    .output_routes = APP_OUTPUT_DISPLAY |
                     APP_OUTPUT_AUX |
                     APP_OUTPUT_HEADPHONES,
};

#ifdef PEDAL_SIM
bool metronome_app_debug_running(void)
{
    return s_running;
}

int metronome_app_debug_bpm(void)
{
    return s_bpm;
}

int metronome_app_debug_meter(void)
{
    return METERS[s_meter_index];
}

int metronome_app_debug_subdivisions(void)
{
    return SUBDIVISIONS[s_division_index];
}

uint32_t metronome_app_debug_tick_count(void)
{
    return s_engine.tick_count;
}

bool metronome_app_debug_audio_claimed(void)
{
    return s_claimed;
}
#endif
