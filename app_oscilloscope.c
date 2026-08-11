#include "gadget_app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app_slots.h"
#include "audio_config.h"
#include "theme.h"

#define SCREEN_W 480
#define SCREEN_H 320
#define PLOT_X 24
#define PLOT_Y 54
#define PLOT_W 432
#define PLOT_H 224
#define GRID_COLUMNS 8
#define GRID_ROWS 6
#define SCOPE_OPTIONS_VALID 0x80u
#define SCOPE_OPTIONS_TIME_MASK 0x03u
#define SCOPE_OPTIONS_SCALE_SHIFT 2u
#define SCOPE_OPTIONS_SCALE_MASK 0x0cu
#define SCOPE_TRIGGER_FLOOR 0.003f

static const int TIMEBASE_MS[] = { 2, 5, 10, 20 };
static const char *const TIMEBASE_NAMES[] = {
    "2 ms", "5 ms", "10 ms", "20 ms",
};
static const float SCALE_FS[] = { 0.10f, 0.20f, 0.50f, 1.00f };
static const char *const SCALE_NAMES[] = {
    "+/-0.10 FS", "+/-0.20 FS", "+/-0.50 FS", "+/-1.00 FS",
};

static lv_obj_t *s_root;
static lv_obj_t *s_plot;
static lv_obj_t *s_wave_line;
static lv_obj_t *s_time_label;
static lv_obj_t *s_scale_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_level_label;
static lv_point_precise_t s_points[PLOT_W];
static audio_waveform_snapshot_t s_snapshot;
static int s_time_index;
static int s_scale_index;
static bool s_held;
static bool s_rebuild_pending;
static uint32_t s_drawn_sequence;

static const ui_theme_t *scope_theme(void)
{
    return theme_for_app_color(app_slots_color(&APP_OSCILLOSCOPE));
}

static uint8_t current_options(void)
{
    return SCOPE_OPTIONS_VALID |
           (uint8_t)(s_time_index & SCOPE_OPTIONS_TIME_MASK) |
           (uint8_t)((s_scale_index << SCOPE_OPTIONS_SCALE_SHIFT) &
                     SCOPE_OPTIONS_SCALE_MASK);
}

static void load_options(void)
{
    const uint8_t options = app_slots_options(&APP_OSCILLOSCOPE);
    if (!(options & SCOPE_OPTIONS_VALID)) {
        s_time_index = 2;
        s_scale_index = 2;
        return;
    }
    s_time_index = options & SCOPE_OPTIONS_TIME_MASK;
    s_scale_index = (options & SCOPE_OPTIONS_SCALE_MASK) >>
                    SCOPE_OPTIONS_SCALE_SHIFT;
}

static void save_options(bool persist)
{
    if (persist) {
        app_slots_set_options(&APP_OSCILLOSCOPE, current_options());
    } else {
        app_slots_set_options_runtime(&APP_OSCILLOSCOPE,
                                      current_options());
    }
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    return label;
}

static void make_grid_line(const ui_theme_t *theme, int x, int y,
                           int width, int height, bool major)
{
    lv_obj_t *line = lv_obj_create(s_root);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, width, height);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_set_style_bg_color(line, major ? theme->accent : theme->grid, 0);
    lv_obj_set_style_bg_opa(line, major ? LV_OPA_50 : LV_OPA_30, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_header(void)
{
    if (!s_time_label) return;
    lv_label_set_text(s_time_label, TIMEBASE_NAMES[s_time_index]);
    lv_label_set_text(s_scale_label, SCALE_NAMES[s_scale_index]);
}

static void build_ui(void)
{
    const ui_theme_t *theme = scope_theme();
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_size(s_root, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_root, theme->bg, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_radius(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = make_label(s_root, "OSCILLOSCOPE",
                                 &lv_font_montserrat_14, theme->accent);
    lv_obj_set_pos(title, PLOT_X, 14);

    s_time_label = make_label(s_root, "", &lv_font_montserrat_14,
                              theme->text);
    lv_obj_set_width(s_time_label, 72);
    lv_obj_set_style_text_align(s_time_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_time_label, 292, 14);

    s_scale_label = make_label(s_root, "", &lv_font_montserrat_14,
                               theme->text);
    lv_obj_set_width(s_scale_label, 100);
    lv_obj_set_style_text_align(s_scale_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_scale_label, 360, 14);

    s_plot = lv_obj_create(s_root);
    lv_obj_set_pos(s_plot, PLOT_X, PLOT_Y);
    lv_obj_set_size(s_plot, PLOT_W, PLOT_H);
    lv_obj_set_style_bg_color(s_plot, theme->surface, 0);
    lv_obj_set_style_bg_opa(s_plot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_plot, theme->grid, 0);
    lv_obj_set_style_border_width(s_plot, 1, 0);
    lv_obj_set_style_radius(s_plot, 0, 0);
    lv_obj_set_style_pad_all(s_plot, 0, 0);
    lv_obj_clear_flag(s_plot, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 1; i < GRID_COLUMNS; i++) {
        const int x = PLOT_X + (PLOT_W * i) / GRID_COLUMNS;
        make_grid_line(theme, x, PLOT_Y, 1, PLOT_H, false);
    }
    for (int i = 1; i < GRID_ROWS; i++) {
        const int y = PLOT_Y + (PLOT_H * i) / GRID_ROWS;
        make_grid_line(theme, PLOT_X, y, PLOT_W, 1,
                       i == GRID_ROWS / 2);
    }

    s_wave_line = lv_line_create(s_root);
    lv_obj_set_pos(s_wave_line, PLOT_X, PLOT_Y);
    lv_obj_set_size(s_wave_line, PLOT_W, PLOT_H);
    lv_obj_set_style_line_color(s_wave_line, theme->accent, 0);
    lv_obj_set_style_line_width(s_wave_line, 2, 0);
    lv_obj_set_style_line_rounded(s_wave_line, true, 0);
    lv_line_set_points_mutable(s_wave_line, s_points, PLOT_W);

    s_level_label = make_label(s_root, "-INF dBFS",
                               &lv_font_montserrat_12, theme->text);
    lv_obj_set_pos(s_level_label, PLOT_X, 291);

    s_status_label = make_label(s_root, "WAITING",
                                &lv_font_montserrat_12, theme->accent2);
    lv_obj_set_width(s_status_label, 150);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_status_label, 306, 291);
    update_header();
}

static void destroy_ui(void)
{
    if (s_root) lv_obj_delete(s_root);
    s_root = NULL;
    s_plot = NULL;
    s_wave_line = NULL;
    s_time_label = NULL;
    s_scale_label = NULL;
    s_status_label = NULL;
    s_level_label = NULL;
}

static int waveform_window_samples(void)
{
    return (AUDIO_SAMPLE_RATE * TIMEBASE_MS[s_time_index]) / 1000;
}

static int find_window_start(int count, int window, bool *triggered)
{
    int latest = count - window;
    if (latest < 0) latest = 0;
    *triggered = false;
    if (count < 2 || window < 2) return latest;

    int16_t peak = 0;
    for (int i = latest; i < count; i++) {
        int32_t magnitude = s_snapshot.samples[i];
        if (magnitude < 0) magnitude = -magnitude;
        if (magnitude > peak) peak = (int16_t)magnitude;
    }
    if ((float)peak / 32768.0f < SCOPE_TRIGGER_FLOOR) return latest;

    const int pretrigger = window / 4;
    const int final_crossing = count - (window - pretrigger) - 1;
    for (int i = final_crossing; i >= pretrigger; i--) {
        if (s_snapshot.samples[i] <= 0 &&
            s_snapshot.samples[i + 1] > 0) {
            *triggered = true;
            return i - pretrigger;
        }
    }
    return latest;
}

static void draw_waveform(void)
{
    int window = waveform_window_samples();
    if (window > s_snapshot.count) window = s_snapshot.count;
    if (window < 2) {
        lv_label_set_text(s_status_label, s_held ? "HOLD" : "WAITING");
        return;
    }

    bool triggered = false;
    const int start = find_window_start(s_snapshot.count, window,
                                        &triggered);
    const float scale = SCALE_FS[s_scale_index];
    double energy = 0.0;
    for (int i = 0; i < window; i++) {
        const float sample = (float)s_snapshot.samples[start + i] / 32768.0f;
        energy += (double)sample * (double)sample;
    }

    for (int x = 0; x < PLOT_W; x++) {
        const float position = (float)x * (float)(window - 1) /
                               (float)(PLOT_W - 1);
        const int index = (int)position;
        const float fraction = position - (float)index;
        const int next = index + 1 < window ? index + 1 : index;
        const float a = (float)s_snapshot.samples[start + index] / 32768.0f;
        const float b = (float)s_snapshot.samples[start + next] / 32768.0f;
        const float sample = a + (b - a) * fraction;
        int y = PLOT_H / 2 - (int)(sample *
                (float)(PLOT_H / 2 - 4) / scale);
        if (y < 1) y = 1;
        if (y > PLOT_H - 2) y = PLOT_H - 2;
        s_points[x].x = x;
        s_points[x].y = y;
    }
    lv_line_set_points_mutable(s_wave_line, s_points, PLOT_W);

    const float rms = sqrtf((float)(energy / (double)window));
    char text[24];
    if (rms <= 0.000001f) {
        snprintf(text, sizeof(text), "-INF dBFS");
    } else {
        snprintf(text, sizeof(text), "%.1f dBFS", 20.0f * log10f(rms));
    }
    lv_label_set_text(s_level_label, text);
    lv_label_set_text(s_status_label,
                      s_held ? "HOLD" :
                      (triggered ? "TRIGGERED" : "FREE RUN"));
    s_drawn_sequence = s_snapshot.frame_sequence;
}

static void scope_enter(int variant)
{
    (void)variant;
    load_options();
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_held = false;
    s_rebuild_pending = false;
    s_drawn_sequence = 0;
    audio_set_mode(AUDIO_SPECTRUM);
    audio_set_viz_mode(VIZ_MONITOR);
    audio_waveform_set_enabled(true);
    build_ui();
}

static void scope_exit(void)
{
    audio_waveform_set_enabled(false);
    save_options(true);
    destroy_ui();
    s_rebuild_pending = false;
}

static void scope_render(void)
{
    if (!s_root) return;
    if (s_rebuild_pending) {
        destroy_ui();
        build_ui();
        s_rebuild_pending = false;
        if (s_snapshot.count > 1) draw_waveform();
    }
    if (s_held) return;

    audio_waveform_snapshot_get(&s_snapshot);
    if (s_snapshot.frame_sequence == 0 ||
        s_snapshot.frame_sequence == s_drawn_sequence) return;
    draw_waveform();
}

static void set_time_index(int index, bool persist)
{
    if (index < 0) index = 0;
    if (index >= 4) index = 3;
    s_time_index = index;
    save_options(persist);
    update_header();
    if (s_snapshot.count > 1) draw_waveform();
}

static void set_scale_index(int index, bool persist)
{
    if (index < 0) index = 0;
    if (index >= 4) index = 3;
    s_scale_index = index;
    save_options(persist);
    update_header();
    if (s_snapshot.count > 1) draw_waveform();
}

static bool scope_on_event(ui_event_t event)
{
    if (event == EV_LEFT) {
        set_time_index(s_time_index - 1, false);
        return true;
    }
    if (event == EV_RIGHT) {
        set_time_index(s_time_index + 1, false);
        return true;
    }
    if (event == EV_UP) {
        set_scale_index(s_scale_index - 1, false);
        return true;
    }
    if (event == EV_DOWN) {
        set_scale_index(s_scale_index + 1, false);
        return true;
    }
    if (event == EV_OK) {
        s_held = !s_held;
        if (s_held) {
            lv_label_set_text(s_status_label, "HOLD");
        }
        return true;
    }
    return false;
}

static void scope_appearance_changed(void)
{
    if (s_root) s_rebuild_pending = true;
}

static int scope_mode_count(void)
{
    return 1;
}

static const char *scope_mode_name(int index)
{
    return index == 0 ? "Triggered" : "";
}

static int scope_mode_index(void)
{
    return 0;
}

static void scope_mode_set(int index)
{
    (void)index;
}

static int timebase_count(void)
{
    return 4;
}

static const char *timebase_name(int index)
{
    return index >= 0 && index < 4 ? TIMEBASE_NAMES[index] : "";
}

static int timebase_index(void)
{
    return s_time_index;
}

static void timebase_set(int index)
{
    set_time_index(index, true);
}

static int scale_count(void)
{
    return 4;
}

static const char *scale_name(int index)
{
    return index >= 0 && index < 4 ? SCALE_NAMES[index] : "";
}

static int scale_index(void)
{
    return s_scale_index;
}

static void scale_set(int index)
{
    set_scale_index(index, true);
}

static const app_choice_setting_t SCOPE_CHOICE_SETTINGS[] = {
    {
        .name = "Timebase",
        .item_count = timebase_count,
        .item_name = timebase_name,
        .item_index = timebase_index,
        .item_set = timebase_set,
    },
    {
        .name = "Scale",
        .item_count = scale_count,
        .item_name = scale_name,
        .item_index = scale_index,
        .item_set = scale_set,
    },
};

const gadget_app_t APP_OSCILLOSCOPE = {
    .id = "oscilloscope",
    .name = "Oscilloscope",
    .audio_mode = AUDIO_SPECTRUM,
    .icon = NULL,
    .on_enter = scope_enter,
    .on_exit = scope_exit,
    .on_render = scope_render,
    .on_event = scope_on_event,
    .on_appearance_changed = scope_appearance_changed,
    .mode_count = scope_mode_count,
    .mode_name = scope_mode_name,
    .mode_index = scope_mode_index,
    .mode_set = scope_mode_set,
    .choice_settings = SCOPE_CHOICE_SETTINGS,
    .choice_setting_count = 2,
    .input_sources = APP_INPUT_BUTTONS,
    .output_routes = APP_OUTPUT_DISPLAY,
    .required_capabilities = PLAT_CAP_DISPLAY |
                             PLAT_CAP_AUDIO_ANALYSIS_INPUT,
};

#ifdef PEDAL_SIM
int oscilloscope_app_debug_time_index(void)
{
    return s_time_index;
}

int oscilloscope_app_debug_scale_index(void)
{
    return s_scale_index;
}

bool oscilloscope_app_debug_held(void)
{
    return s_held;
}

uint32_t oscilloscope_app_debug_drawn_sequence(void)
{
    return s_drawn_sequence;
}
#endif
