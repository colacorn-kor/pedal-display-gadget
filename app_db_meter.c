#include "gadget_app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app_slots.h"
#include "audio_autorange.h"
#include "audio_level.h"
#include "platform.h"
#include "theme.h"

#define SCREEN_W 480
#define SCREEN_H 320
#define METER_X 22
#define METER_Y 139
#define METER_W 436
#define METER_H 26
#define METER_FLOOR_DB (-72.0f)
#define METER_TOP_DB 0.0f
#define METER_ZONE_COUNT 3
#define PEAK_HOLD_MS 1000U
#define METER_SAMPLE_MS 50U
#define METER_DISPLAY_MS 200U
#define METER_HISTORY_BUCKETS 80
#define DB_OPTIONS_INPUT_MASK 0x01u
#define DB_OPTIONS_AVERAGE_SHIFT 1u
#define DB_OPTIONS_AVERAGE_MASK 0x06u
#define DB_OPTIONS_SAVE_DELAY_MS 750U
#define DB_ACCENT_LABEL_MAX 5

typedef enum {
    DB_AVERAGE_LIVE = 0,
    DB_AVERAGE_1S,
    DB_AVERAGE_3S,
    DB_AVERAGE_COUNT,
} db_average_mode_t;

typedef enum {
    DB_CONTROL_INPUT = 0,
    DB_CONTROL_AVERAGE,
} db_control_t;

typedef enum {
    DB_MODE_LEVEL = 0,
    DB_MODE_LOUDNESS,
    DB_MODE_RANGE_DIAGNOSTICS,
} db_meter_mode_t;

typedef struct {
    float lo_db;
    float hi_db;
    uint32_t color;
} meter_zone_t;

typedef struct {
    double energy;
    uint64_t samples;
    uint32_t end_ms;
} meter_power_bucket_t;

static const meter_zone_t METER_ZONES[METER_ZONE_COUNT] = {
    { -72.0f, -18.0f, 0x2DD4BF },
    { -18.0f,  -6.0f, 0xFBBF24 },
    {  -6.0f,   0.0f, 0xF43F5E },
};

static const int SCALE_DB[] = { -72, -60, -48, -36, -24, -12, 0 };
#define SCALE_COUNT ((int)(sizeof(SCALE_DB) / sizeof(SCALE_DB[0])))

static lv_obj_t *s_root;
static lv_obj_t *s_input_control;
static lv_obj_t *s_input_control_label;
static lv_obj_t *s_average_control;
static lv_obj_t *s_average_control_label;
static lv_obj_t *s_rms_caption;
static lv_obj_t *s_rms_value;
static lv_obj_t *s_peak_value;
static lv_obj_t *s_voltage_value;
static lv_obj_t *s_dbv_value;
static lv_obj_t *s_dbu_value;
static lv_obj_t *s_loudness_momentary;
static lv_obj_t *s_loudness_short;
static lv_obj_t *s_loudness_integrated;
static lv_obj_t *s_loudness_true_peak;
static lv_obj_t *s_loudness_status;
static lv_obj_t *s_diag_status;
static lv_obj_t *s_diag_hot_rms;
static lv_obj_t *s_diag_hot_peak;
static lv_obj_t *s_diag_hot_input;
static lv_obj_t *s_diag_sensitive_rms;
static lv_obj_t *s_diag_sensitive_peak;
static lv_obj_t *s_diag_sensitive_input;
static lv_obj_t *s_diag_comparison;
static lv_obj_t *s_zone_fill[METER_ZONE_COUNT];
static lv_obj_t *s_meter_track[METER_ZONE_COUNT];
static lv_obj_t *s_peak_tick;
static lv_obj_t *s_accent_label[DB_ACCENT_LABEL_MAX];
static int s_accent_label_count;
static audio_viz_snapshot_t s_snapshot;
static float s_display_rms_db = METER_FLOOR_DB;
static float s_peak_hold_db = METER_FLOOR_DB;
static float s_window_peak;
static meter_power_bucket_t s_power_history[METER_HISTORY_BUCKETS];
static double s_last_energy_total;
static uint64_t s_last_sample_total;
static int s_history_head;
static int s_history_count;
static audio_input_range_t s_input_range = AUDIO_INPUT_LINE;
static audio_input_source_t s_input_source = AUDIO_INPUT_SOURCE_LEGACY;
static bool s_input_clipped;
static db_meter_mode_t s_mode = DB_MODE_LEVEL;
static db_average_mode_t s_average_mode = DB_AVERAGE_LIVE;
static db_control_t s_control = DB_CONTROL_INPUT;
static bool s_rebuild_pending;
static bool s_options_dirty;
static uint32_t s_options_changed_ms;
static uint32_t s_peak_hold_until;
static uint32_t s_last_sample_ms;
static uint32_t s_last_display_ms;
static int s_last_zone_width[METER_ZONE_COUNT];
static int s_last_peak_x = -1;
static char s_rms_text[16];
static char s_peak_text[16];
static char s_voltage_text[16];
static char s_dbv_text[16];
static char s_dbu_text[16];

static const ui_theme_t *db_meter_theme(void)
{
    return theme_for_app_color(app_slots_color(&APP_DB_METER));
}

static uint32_t mix_hex(uint32_t base, uint32_t overlay, uint8_t opacity)
{
    uint32_t inverse = 255U - opacity;
    uint32_t red = ((((base >> 16) & 0xffU) * inverse) +
                    (((overlay >> 16) & 0xffU) * opacity) + 127U) / 255U;
    uint32_t green = ((((base >> 8) & 0xffU) * inverse) +
                      (((overlay >> 8) & 0xffU) * opacity) + 127U) / 255U;
    uint32_t blue = (((base & 0xffU) * inverse) +
                     ((overlay & 0xffU) * opacity) + 127U) / 255U;
    return (red << 16) | (green << 8) | blue;
}

static float clampf(float value, float lo, float hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static int db_to_x(float db)
{
    float normalized = (clampf(db, METER_FLOOR_DB, METER_TOP_DB) -
                        METER_FLOOR_DB) /
                       (METER_TOP_DB - METER_FLOOR_DB);
    return (int)(normalized * (METER_W - 1) + 0.5f);
}

static void style_rect(lv_obj_t *obj, uint32_t color)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

static const char *average_mode_name(db_average_mode_t mode)
{
    if (mode == DB_AVERAGE_1S) return "AVG 1s";
    if (mode == DB_AVERAGE_3S) return "AVG 3s";
    return "LIVE";
}

static const char *rms_caption(db_average_mode_t mode)
{
    if (mode == DB_AVERAGE_1S) return "1 SECOND AVERAGE";
    if (mode == DB_AVERAGE_3S) return "3 SECOND AVERAGE";
    return "LIVE RMS LEVEL";
}

static uint8_t current_options(void)
{
    return (uint8_t)s_input_range |
           ((uint8_t)s_average_mode << DB_OPTIONS_AVERAGE_SHIFT);
}

static void load_options(void)
{
    const uint8_t options = app_slots_options(&APP_DB_METER);
    const unsigned input = options & DB_OPTIONS_INPUT_MASK;
    const unsigned average =
        (options & DB_OPTIONS_AVERAGE_MASK) >> DB_OPTIONS_AVERAGE_SHIFT;

    s_input_range = input < AUDIO_INPUT_RANGE_COUNT
        ? (audio_input_range_t)input : AUDIO_INPUT_LINE;
    s_average_mode = average < DB_AVERAGE_COUNT
        ? (db_average_mode_t)average : DB_AVERAGE_LIVE;
    s_options_dirty = false;
}

static void save_options_if_due(uint32_t now, bool force)
{
    if (!s_options_dirty) return;
    if (!force && now - s_options_changed_ms < DB_OPTIONS_SAVE_DELAY_MS) {
        return;
    }
    app_slots_set_options(&APP_DB_METER, current_options());
    s_options_dirty = false;
}

static lv_obj_t *create_control(const ui_theme_t *theme, int x, int width,
                                lv_obj_t **label)
{
    lv_obj_t *control = lv_obj_create(s_root);
    lv_obj_set_size(control, width, 27);
    lv_obj_set_pos(control, x, 4);
    lv_obj_remove_flag(control, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(control, 4, 0);
    lv_obj_set_style_pad_all(control, 0, 0);
    lv_obj_set_style_border_width(control, 1, 0);
    lv_obj_set_style_bg_opa(control, LV_OPA_TRANSP, 0);

    *label = make_label(control, "", &lv_font_montserrat_12, theme->text);
    lv_obj_set_size(*label, width - 4, 20);
    lv_obj_center(*label);
    lv_obj_set_style_text_align(*label, LV_TEXT_ALIGN_CENTER, 0);
    return control;
}

static void style_control(lv_obj_t *control, lv_obj_t *label, bool selected)
{
    const ui_theme_t *theme = db_meter_theme();
    lv_obj_set_style_border_color(
        control, selected ? theme->accent : theme->grid, 0);
    lv_obj_set_style_bg_color(control, theme->accent, 0);
    lv_obj_set_style_bg_opa(control, selected ? LV_OPA_30 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(label, theme->text, 0);
    lv_obj_set_style_text_opa(label, selected ? LV_OPA_COVER : LV_OPA_60, 0);
}

static void refresh_controls(void)
{
    if (!s_input_control || !s_input_control_label ||
        !s_average_control || !s_average_control_label ||
        !s_rms_caption) {
        return;
    }

    char text[32];
#if AUDIO_DUAL_RANGE
    const char *range_name =
        s_input_source == AUDIO_INPUT_SOURCE_HOT ? "HOT" : "SENSITIVE";
    snprintf(text, sizeof(text), "AUTO  %s%s", range_name,
             s_input_clipped ? " CLIP" : "");
#else
    const char *input_name =
        s_input_range == AUDIO_INPUT_INST ? "INST" : "LINE";

    snprintf(text, sizeof(text), "INPUT  %s %.2fx", input_name,
             audio_level_input_gain(s_input_range));
#endif
    lv_label_set_text(s_input_control_label, text);
    snprintf(text, sizeof(text), "WINDOW  %s",
             average_mode_name(s_average_mode));
    lv_label_set_text(s_average_control_label, text);
    lv_label_set_text(s_rms_caption, rms_caption(s_average_mode));

    style_control(s_input_control, s_input_control_label,
#if AUDIO_DUAL_RANGE
                  false);
#else
                  s_control == DB_CONTROL_INPUT);
#endif
    style_control(s_average_control, s_average_control_label,
                  s_control == DB_CONTROL_AVERAGE);
}

static void mark_options_changed(void)
{
    s_options_dirty = true;
    s_options_changed_ms = plat_millis();
}

#if !AUDIO_DUAL_RANGE
static void set_input_range(int idx)
{
    if (idx < 0 || idx >= AUDIO_INPUT_RANGE_COUNT ||
        idx == (int)s_input_range) {
        return;
    }
    s_input_range = (audio_input_range_t)idx;
    mark_options_changed();
    refresh_controls();
}

static int input_setting_count(void)
{
    return AUDIO_INPUT_RANGE_COUNT;
}

static const char *input_setting_name(int idx)
{
    static char names[AUDIO_INPUT_RANGE_COUNT][20];
    if (idx < 0 || idx >= AUDIO_INPUT_RANGE_COUNT) return "";

    const audio_input_range_t range = (audio_input_range_t)idx;
    snprintf(names[idx], sizeof(names[idx]), "%s %.2fx",
             range == AUDIO_INPUT_INST ? "INST" : "LINE",
             audio_level_input_gain(range));
    return names[idx];
}

static int input_setting_index(void)
{
    return (int)s_input_range;
}
#endif

static void set_average_mode(int idx)
{
    if (idx < 0 || idx >= DB_AVERAGE_COUNT ||
        idx == (int)s_average_mode) {
        return;
    }
    s_average_mode = (db_average_mode_t)idx;
    mark_options_changed();
    refresh_controls();
}

static int window_setting_count(void)
{
    return DB_AVERAGE_COUNT;
}

static const char *window_setting_name(int idx)
{
    return idx >= 0 && idx < DB_AVERAGE_COUNT
        ? average_mode_name((db_average_mode_t)idx) : "";
}

static int window_setting_index(void)
{
    return (int)s_average_mode;
}

static const app_choice_setting_t DB_METER_CHOICE_SETTINGS[] = {
#if !AUDIO_DUAL_RANGE
    {
        .name = "Input",
        .item_count = input_setting_count,
        .item_name = input_setting_name,
        .item_index = input_setting_index,
        .item_set = set_input_range,
    },
#endif
    {
        .name = "Window",
        .item_count = window_setting_count,
        .item_name = window_setting_name,
        .item_index = window_setting_index,
        .item_set = set_average_mode,
    },
};

static void set_text_if_changed(lv_obj_t *label, char *previous,
                                size_t previous_size, const char *text)
{
    if (!label || strcmp(previous, text) == 0) return;
    snprintf(previous, previous_size, "%s", text);
    lv_label_set_text(label, previous);
}

static void reset_power_history(const audio_viz_snapshot_t *snapshot)
{
    memset(s_power_history, 0, sizeof(s_power_history));
    s_history_head = 0;
    s_history_count = 0;
    s_last_energy_total = snapshot ? snapshot->meter_energy_total : 0.0;
    s_last_sample_total = snapshot ? snapshot->meter_sample_total : 0;
}

static void capture_power_bucket(const audio_viz_snapshot_t *snapshot,
                                 uint32_t now)
{
    if (snapshot->meter_sample_total < s_last_sample_total ||
        snapshot->meter_energy_total < s_last_energy_total) {
        reset_power_history(snapshot);
        return;
    }

    const uint64_t samples =
        snapshot->meter_sample_total - s_last_sample_total;
    const double energy =
        snapshot->meter_energy_total - s_last_energy_total;
    s_last_sample_total = snapshot->meter_sample_total;
    s_last_energy_total = snapshot->meter_energy_total;
    if (samples == 0 || energy < 0.0) return;

    s_power_history[s_history_head] = (meter_power_bucket_t) {
        .energy = energy,
        .samples = samples,
        .end_ms = now,
    };
    s_history_head = (s_history_head + 1) % METER_HISTORY_BUCKETS;
    if (s_history_count < METER_HISTORY_BUCKETS) s_history_count++;
}

static float selected_rms(float live_rms, uint32_t now)
{
    uint32_t window_ms;
    if (s_average_mode == DB_AVERAGE_1S) {
        window_ms = 1000U;
    } else if (s_average_mode == DB_AVERAGE_3S) {
        window_ms = 3000U;
    } else {
        return live_rms;
    }

    double energy = 0.0;
    uint64_t samples = 0;
    for (int i = 0; i < s_history_count; i++) {
        int idx = s_history_head - 1 - i;
        if (idx < 0) idx += METER_HISTORY_BUCKETS;
        const meter_power_bucket_t *bucket = &s_power_history[idx];
        if (now - bucket->end_ms >= window_ms) break;
        energy += bucket->energy;
        samples += bucket->samples;
    }
    if (samples == 0) return live_rms;
    return sqrtf((float)(energy / (double)samples));
}

static void create_meter(const ui_theme_t *theme)
{
    const uint32_t background = lv_color_to_u32(theme->bg) & 0xffffffU;

    for (int i = 0; i < METER_ZONE_COUNT; i++) {
        int x0 = db_to_x(METER_ZONES[i].lo_db);
        int x1 = db_to_x(METER_ZONES[i].hi_db);
        int width = x1 - x0 + 1;

        s_meter_track[i] = lv_obj_create(s_root);
        lv_obj_set_size(s_meter_track[i], width, METER_H);
        lv_obj_set_pos(s_meter_track[i], METER_X + x0, METER_Y);
        style_rect(
            s_meter_track[i],
            mix_hex(background, METER_ZONES[i].color, 52));

        s_zone_fill[i] = lv_obj_create(s_root);
        lv_obj_set_size(s_zone_fill[i], 0, METER_H);
        lv_obj_set_pos(s_zone_fill[i], METER_X + x0, METER_Y);
        style_rect(s_zone_fill[i], METER_ZONES[i].color);
        s_last_zone_width[i] = 0;
    }

    s_peak_tick = lv_obj_create(s_root);
    lv_obj_set_size(s_peak_tick, 2, METER_H + 8);
    lv_obj_set_pos(s_peak_tick, METER_X, METER_Y - 4);
    style_rect(s_peak_tick, lv_color_to_u32(theme->text) & 0xffffffU);

    for (int i = 0; i < SCALE_COUNT; i++) {
        char text[8];
        int x = db_to_x((float)SCALE_DB[i]);
        lv_snprintf(text, sizeof(text), "%d", SCALE_DB[i]);
        lv_obj_t *label = make_label(s_root, text, &lv_font_montserrat_12,
                                     theme->text);
        lv_obj_set_width(label, 34);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        if (i == 0) {
            lv_obj_set_x(label, METER_X - 4);
        } else if (i == SCALE_COUNT - 1) {
            lv_obj_set_x(label, METER_X + METER_W - 28);
        } else {
            lv_obj_set_x(label, METER_X + x - 17);
        }
        lv_obj_set_y(label, METER_Y + METER_H + 5);
    }
}

static void create_metric_column(const ui_theme_t *theme, int x,
                                 const char *caption, lv_obj_t **value)
{
    lv_obj_t *caption_label = make_label(s_root, caption,
                                         &lv_font_montserrat_12,
                                         theme->accent);
    if (s_accent_label_count < DB_ACCENT_LABEL_MAX) {
        s_accent_label[s_accent_label_count++] = caption_label;
    }
    lv_obj_set_width(caption_label, 130);
    lv_obj_set_style_text_align(caption_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(caption_label, x, 220);

    *value = make_label(s_root, "--", &lv_font_montserrat_28, theme->text);
    lv_obj_set_width(*value, 130);
    lv_obj_set_style_text_align(*value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(*value, x, 239);
}

static void create_root(const ui_theme_t *theme, const char *title_text)
{
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_root, 0, 0);
    style_rect(s_root, lv_color_to_u32(theme->bg) & 0xffffffU);

    lv_obj_t *title = make_label(s_root, title_text,
                                 &lv_font_montserrat_14, theme->accent);
    s_accent_label[s_accent_label_count++] = title;
    lv_obj_set_pos(title, 14, 9);
}

static void create_level_view(const ui_theme_t *theme)
{
    create_root(theme, "dB METER");
    s_input_control = create_control(theme, 112, 176,
                                     &s_input_control_label);
    s_average_control = create_control(theme, 296, 168,
                                       &s_average_control_label);

    s_rms_caption = make_label(s_root, "", &lv_font_montserrat_12,
                               theme->text);
    lv_obj_set_pos(s_rms_caption, 20, 42);
    s_rms_value = make_label(s_root, "-72.0", &lv_font_montserrat_48,
                             theme->text);
    lv_obj_set_width(s_rms_value, 235);
    lv_obj_set_pos(s_rms_value, 16, 56);
    lv_obj_t *rms_unit = make_label(s_root, "dBFS",
                                    &lv_font_montserrat_14, theme->accent);
    s_accent_label[s_accent_label_count++] = rms_unit;
    lv_obj_set_pos(rms_unit, 205, 91);

    lv_obj_t *peak_caption = make_label(s_root, "SAMPLE PEAK",
                                        &lv_font_montserrat_12,
                                        theme->text);
    lv_obj_set_width(peak_caption, 200);
    lv_obj_set_style_text_align(peak_caption, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(peak_caption, 260, 42);
    s_peak_value = make_label(s_root, "-72.0 dBFS",
                              &lv_font_montserrat_28, theme->accent2);
    lv_obj_set_width(s_peak_value, 200);
    lv_obj_set_style_text_align(s_peak_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_peak_value, 260, 66);

    refresh_controls();
    create_meter(theme);
    create_metric_column(theme, 20, "INPUT V RMS*", &s_voltage_value);
    create_metric_column(theme, 175, "INPUT dBV*", &s_dbv_value);
    create_metric_column(theme, 330, "INPUT dBu*", &s_dbu_value);

    lv_obj_t *footer = make_label(
        s_root,
#if AUDIO_DUAL_RANGE
        "* GG input scale | auto range | calibration pending",
#else
        "* PCM1808 3.0 Vpp FS | nominal gain | calibration pending",
#endif
        &lv_font_montserrat_12, theme->text);
    lv_obj_set_width(footer, SCREEN_W);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(footer, LV_OPA_50, 0);
    lv_obj_set_pos(footer, 0, 294);
}

static lv_obj_t *create_loudness_metric(const ui_theme_t *theme,
                                        int x, int y, int width,
                                        const char *caption,
                                        const lv_font_t *font)
{
    lv_obj_t *caption_label = make_label(
        s_root, caption, &lv_font_montserrat_12, theme->accent);
    lv_obj_set_width(caption_label, width);
    lv_obj_set_pos(caption_label, x, y);

    lv_obj_t *value = make_label(s_root, "--", font, theme->text);
    lv_obj_set_width(value, width);
    lv_obj_set_pos(value, x, y + 20);
    return value;
}

static void create_loudness_view(const ui_theme_t *theme)
{
    create_root(theme, "LOUDNESS");

    s_loudness_momentary = create_loudness_metric(
        theme, 20, 45, 260, "MOMENTARY  400 ms",
        &lv_font_montserrat_48);
    s_loudness_short = create_loudness_metric(
        theme, 304, 53, 156, "SHORT-TERM  3 s",
        &lv_font_montserrat_28);
    s_loudness_integrated = create_loudness_metric(
        theme, 20, 156, 260, "INTEGRATED",
        &lv_font_montserrat_28);
    s_loudness_true_peak = create_loudness_metric(
        theme, 304, 164, 156, "TRUE PEAK EST.",
        &lv_font_montserrat_28);

    s_loudness_status = make_label(
        s_root, "0:00  |  0 GATED BLOCKS",
        &lv_font_montserrat_14, theme->accent2);
    lv_obj_set_width(s_loudness_status, SCREEN_W - 40);
    lv_obj_set_pos(s_loudness_status, 20, 258);

    lv_obj_t *footer = make_label(
        s_root, "ITU-R BS.1770 mono | K-weighted | 4x peak estimate",
        &lv_font_montserrat_12, theme->text);
    lv_obj_set_width(footer, SCREEN_W - 40);
    lv_obj_set_style_text_opa(footer, LV_OPA_50, 0);
    lv_obj_set_pos(footer, 20, 292);
}

#if AUDIO_DUAL_RANGE
static lv_obj_t *create_diagnostic_value(
    const ui_theme_t *theme, int x, int y)
{
    lv_obj_t *label = make_label(
        s_root, "--", &lv_font_montserrat_28, theme->text);
    lv_obj_set_width(label, 164);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

static void create_range_diagnostics_view(const ui_theme_t *theme)
{
    create_root(theme, "RANGE DIAGNOSTICS");

    s_diag_status = make_label(
        s_root, "WAITING FOR AUDIO", &lv_font_montserrat_14,
        theme->accent2);
    lv_obj_set_width(s_diag_status, 270);
    lv_obj_set_style_text_align(
        s_diag_status, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_diag_status, 194, 8);

    lv_obj_t *hot_header = make_label(
        s_root, "HOT / VINL", &lv_font_montserrat_14, theme->accent);
    lv_obj_set_width(hot_header, 164);
    lv_obj_set_style_text_align(
        hot_header, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(hot_header, 132, 42);

    lv_obj_t *sensitive_header = make_label(
        s_root, "SENSITIVE / VINR", &lv_font_montserrat_14,
        theme->accent);
    lv_obj_set_width(sensitive_header, 164);
    lv_obj_set_style_text_align(
        sensitive_header, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(sensitive_header, 300, 42);

    const char *captions[] = {
        "ADC RMS dBFS", "ADC PEAK dBFS", "INPUT RMS*",
    };
    const int caption_y[] = { 79, 133, 187 };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *caption = make_label(
            s_root, captions[i], &lv_font_montserrat_12, theme->text);
        lv_obj_set_width(caption, 112);
        lv_obj_set_style_text_align(
            caption, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(caption, 10, caption_y[i]);
    }

    s_diag_hot_rms = create_diagnostic_value(theme, 132, 67);
    s_diag_sensitive_rms =
        create_diagnostic_value(theme, 300, 67);
    s_diag_hot_peak = create_diagnostic_value(theme, 132, 121);
    s_diag_sensitive_peak =
        create_diagnostic_value(theme, 300, 121);
    s_diag_hot_input = create_diagnostic_value(theme, 132, 175);
    s_diag_sensitive_input =
        create_diagnostic_value(theme, 300, 175);

    s_diag_comparison = make_label(
        s_root, "S/H -- | MISMATCH --", &lv_font_montserrat_14,
        theme->accent2);
    lv_obj_set_width(s_diag_comparison, SCREEN_W);
    lv_obj_set_style_text_align(
        s_diag_comparison, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_diag_comparison, 0, 237);

    lv_obj_t *target = make_label(
        s_root, "Raw S/H target +29.7 dB | corrected mismatch target 0.0 dB",
        &lv_font_montserrat_12, theme->text);
    lv_obj_set_width(target, SCREEN_W);
    lv_obj_set_style_text_align(target, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(target, LV_OPA_60, 0);
    lv_obj_set_pos(target, 0, 265);

    lv_obj_t *footer = make_label(
        s_root, "* nominal input estimate until 1 kHz calibration",
        &lv_font_montserrat_12, theme->text);
    lv_obj_set_width(footer, SCREEN_W);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(footer, LV_OPA_50, 0);
    lv_obj_set_pos(footer, 0, 294);
}
#endif

static void clear_ui_references(void)
{
    s_root = NULL;
    s_input_control = NULL;
    s_input_control_label = NULL;
    s_average_control = NULL;
    s_average_control_label = NULL;
    s_rms_caption = NULL;
    s_rms_value = NULL;
    s_peak_value = NULL;
    s_voltage_value = NULL;
    s_dbv_value = NULL;
    s_dbu_value = NULL;
    s_loudness_momentary = NULL;
    s_loudness_short = NULL;
    s_loudness_integrated = NULL;
    s_loudness_true_peak = NULL;
    s_loudness_status = NULL;
    s_diag_status = NULL;
    s_diag_hot_rms = NULL;
    s_diag_hot_peak = NULL;
    s_diag_hot_input = NULL;
    s_diag_sensitive_rms = NULL;
    s_diag_sensitive_peak = NULL;
    s_diag_sensitive_input = NULL;
    s_diag_comparison = NULL;
    s_peak_tick = NULL;
    for (int i = 0; i < METER_ZONE_COUNT; i++) {
        s_zone_fill[i] = NULL;
        s_meter_track[i] = NULL;
    }
    memset(s_accent_label, 0, sizeof(s_accent_label));
    s_accent_label_count = 0;
}

static void destroy_ui(void)
{
    if (s_root) lv_obj_delete(s_root);
    clear_ui_references();
}

static void build_ui(void)
{
    s_accent_label_count = 0;
    memset(s_accent_label, 0, sizeof(s_accent_label));
    const ui_theme_t *theme = db_meter_theme();
    if (s_mode == DB_MODE_LOUDNESS) {
        create_loudness_view(theme);
        return;
    }
#if AUDIO_DUAL_RANGE
    if (s_mode == DB_MODE_RANGE_DIAGNOSTICS) {
        create_range_diagnostics_view(theme);
        return;
    }
#endif
    create_level_view(theme);
}

static void db_meter_enter(int variant)
{
    (void)variant;
    audio_set_mode(AUDIO_METER);
    audio_set_viz_mode(VIZ_MONITOR);
    load_options();
#if AUDIO_DUAL_RANGE
    s_control = DB_CONTROL_AVERAGE;
#else
    s_control = DB_CONTROL_INPUT;
#endif

    s_display_rms_db = METER_FLOOR_DB;
    s_peak_hold_db = METER_FLOOR_DB;
    s_window_peak = 0.0f;
    s_peak_hold_until = 0;
    s_last_sample_ms = plat_millis();
    s_last_display_ms = s_last_sample_ms;
    s_last_peak_x = -1;
    s_rebuild_pending = false;
    plat_audio_viz_get(&s_snapshot);
    s_input_source = s_snapshot.input_source;
    s_input_clipped = s_snapshot.input_clipped;
    reset_power_history(&s_snapshot);
    s_rms_text[0] = '\0';
    s_peak_text[0] = '\0';
    s_voltage_text[0] = '\0';
    s_dbv_text[0] = '\0';
    s_dbu_text[0] = '\0';
    build_ui();
    if (s_mode == DB_MODE_LOUDNESS) audio_loudness_reset();
}

static void db_meter_appearance_changed(void)
{
    if (s_root) s_rebuild_pending = true;
}

static void db_meter_exit(void)
{
    save_options_if_due(plat_millis(), true);
    destroy_ui();
    s_rebuild_pending = false;
}

static void update_peak_hold(float peak_db, uint32_t now, float dt_seconds)
{
    if (peak_db >= s_peak_hold_db) {
        s_peak_hold_db = peak_db;
        s_peak_hold_until = now + PEAK_HOLD_MS;
    } else if ((int32_t)(now - s_peak_hold_until) >= 0) {
        s_peak_hold_db -= 18.0f * dt_seconds;
        if (s_peak_hold_db < peak_db) s_peak_hold_db = peak_db;
        if (s_peak_hold_db < METER_FLOOR_DB) {
            s_peak_hold_db = METER_FLOOR_DB;
        }
    }
}

static void update_meter_geometry(float rms_db)
{
    int level_x = db_to_x(rms_db);
    for (int i = 0; i < METER_ZONE_COUNT; i++) {
        int zone_x0 = db_to_x(METER_ZONES[i].lo_db);
        int zone_x1 = db_to_x(METER_ZONES[i].hi_db);
        int width = level_x - zone_x0 + 1;
        int maximum = zone_x1 - zone_x0 + 1;
        if (width < 0) width = 0;
        if (width > maximum) width = maximum;
        if (width != s_last_zone_width[i]) {
            lv_obj_set_width(s_zone_fill[i], width);
            s_last_zone_width[i] = width;
        }
    }

    int peak_x = db_to_x(s_peak_hold_db);
    if (peak_x != s_last_peak_x) {
        lv_obj_set_x(s_peak_tick, METER_X + peak_x);
        s_last_peak_x = peak_x;
    }
}

static void format_voltage(char *out, size_t size, float vrms)
{
    if (vrms < 0.001f) {
        snprintf(out, size, "%.0f uV", vrms * 1000000.0f);
    } else if (vrms < 1.0f) {
        snprintf(out, size, "%.1f mV", vrms * 1000.0f);
    } else {
        snprintf(out, size, "%.3f V", vrms);
    }
}

#if AUDIO_DUAL_RANGE
static float diagnostic_dbfs(float amplitude)
{
    if (!isfinite(amplitude) || amplitude <= 0.000001f) {
        return -120.0f;
    }
    return 20.0f * log10f(fabsf(amplitude));
}

static void format_diagnostic_dbfs(
    char *out, size_t size, float amplitude)
{
    const float db = diagnostic_dbfs(amplitude);
    if (db <= -120.0f) {
        snprintf(out, size, "-INF");
    } else {
        snprintf(out, size, "%.1f", db);
    }
}

static void render_range_diagnostics(uint32_t now)
{
    if (now - s_last_display_ms < METER_DISPLAY_MS) return;
    s_last_display_ms = now;
    plat_audio_viz_get(&s_snapshot);

    const float hot_rms = fabsf(s_snapshot.hot_adc_rms);
    const float hot_peak = fabsf(s_snapshot.hot_adc_peak);
    const float sensitive_rms =
        fabsf(s_snapshot.sensitive_adc_rms);
    const float sensitive_peak =
        fabsf(s_snapshot.sensitive_adc_peak);
    const float hot_input =
        audio_autorange_adc_rms_to_input_vrms(
            AUDIO_AUTORANGE_HOT, hot_rms);
    const float sensitive_input =
        audio_autorange_adc_rms_to_input_vrms(
            AUDIO_AUTORANGE_SENSITIVE, sensitive_rms);

    char text[64];
    const char *active =
        s_snapshot.input_source == AUDIO_INPUT_SOURCE_HOT
            ? "HOT" : "SENSITIVE";
    const bool hot_clipped =
        hot_peak >= AUDIO_DUAL_ADC_CLIP_PEAK;
    const bool sensitive_clipped =
        sensitive_peak >= AUDIO_DUAL_ADC_CLIP_PEAK;
    if (hot_clipped && sensitive_clipped) {
        snprintf(text, sizeof(text), "ACTIVE %s | BOTH CLIP", active);
    } else if (hot_clipped) {
        snprintf(text, sizeof(text), "ACTIVE %s | HOT CLIP", active);
    } else if (sensitive_clipped) {
        snprintf(text, sizeof(text), "ACTIVE %s | SENS CLIP", active);
    } else {
        snprintf(text, sizeof(text), "ACTIVE %s", active);
    }
    lv_label_set_text(s_diag_status, text);

    format_diagnostic_dbfs(text, sizeof(text), hot_rms);
    lv_label_set_text(s_diag_hot_rms, text);
    format_diagnostic_dbfs(text, sizeof(text), hot_peak);
    lv_label_set_text(s_diag_hot_peak, text);
    format_diagnostic_dbfs(text, sizeof(text), sensitive_rms);
    lv_label_set_text(s_diag_sensitive_rms, text);
    format_diagnostic_dbfs(text, sizeof(text), sensitive_peak);
    lv_label_set_text(s_diag_sensitive_peak, text);

    format_voltage(text, sizeof(text), hot_input);
    lv_label_set_text(s_diag_hot_input, text);
    format_voltage(text, sizeof(text), sensitive_input);
    lv_label_set_text(s_diag_sensitive_input, text);

    if (hot_rms > 0.000001f && sensitive_rms > 0.000001f &&
        hot_input > 0.000001f && sensitive_input > 0.000001f) {
        const float raw_ratio_db =
            diagnostic_dbfs(sensitive_rms) -
            diagnostic_dbfs(hot_rms);
        const float mismatch_db =
            20.0f * log10f(sensitive_input / hot_input);
        snprintf(text, sizeof(text),
                 "S/H %+.1f dB | MISMATCH %+.1f dB",
                 raw_ratio_db, mismatch_db);
    } else {
        snprintf(text, sizeof(text), "S/H -- | MISMATCH --");
    }
    lv_label_set_text(s_diag_comparison, text);
}
#endif

static void db_meter_render(void)
{
    if (!s_root) return;

    uint32_t now = plat_millis();
    if (s_rebuild_pending) {
        destroy_ui();
        build_ui();
        s_rebuild_pending = false;
        s_last_display_ms = now;
    }
#if AUDIO_DUAL_RANGE
    if (s_mode == DB_MODE_RANGE_DIAGNOSTICS) {
        render_range_diagnostics(now);
        return;
    }
#endif

    if (s_mode == DB_MODE_LOUDNESS) {
        if (now - s_last_display_ms < METER_DISPLAY_MS) return;
        s_last_display_ms = now;
        plat_audio_viz_get(&s_snapshot);
        const loudness_snapshot_t *loudness = &s_snapshot.loudness;
        char text[48];
        if (loudness->momentary_lufs <= LOUDNESS_FLOOR_LUFS) {
            snprintf(text, sizeof(text), "-- LUFS");
        } else {
            snprintf(text, sizeof(text), "%.1f LUFS",
                     loudness->momentary_lufs);
        }
        lv_label_set_text(s_loudness_momentary, text);
        if (loudness->short_term_lufs <= LOUDNESS_FLOOR_LUFS) {
            snprintf(text, sizeof(text), "-- LUFS");
        } else {
            snprintf(text, sizeof(text), "%.1f LUFS",
                     loudness->short_term_lufs);
        }
        lv_label_set_text(s_loudness_short, text);
        if (loudness->integrated_lufs <= LOUDNESS_FLOOR_LUFS) {
            snprintf(text, sizeof(text), "-- LUFS");
        } else {
            snprintf(text, sizeof(text), "%.1f LUFS",
                     loudness->integrated_lufs);
        }
        lv_label_set_text(s_loudness_integrated, text);
        if (loudness->true_peak_dbtp <= LOUDNESS_FLOOR_LUFS) {
            snprintf(text, sizeof(text), "-- dBTP");
        } else {
            snprintf(text, sizeof(text), "%.1f dBTP",
                     loudness->true_peak_dbtp);
        }
        lv_label_set_text(s_loudness_true_peak, text);
        const unsigned seconds = loudness->elapsed_ms / 1000U;
        snprintf(text, sizeof(text), "%u:%02u  |  %u GATED BLOCKS",
                 seconds / 60U, seconds % 60U,
                 (unsigned)loudness->integrated_blocks);
        lv_label_set_text(s_loudness_status, text);
        return;
    }

    save_options_if_due(now, false);
    uint32_t sample_elapsed_ms = now - s_last_sample_ms;
    if (sample_elapsed_ms < METER_SAMPLE_MS) return;
    s_last_sample_ms = now;

    plat_audio_viz_get(&s_snapshot);
#if AUDIO_DUAL_RANGE
    if (s_input_source != s_snapshot.input_source ||
        s_input_clipped != s_snapshot.input_clipped) {
        s_input_source = s_snapshot.input_source;
        s_input_clipped = s_snapshot.input_clipped;
        refresh_controls();
    }
#endif
    if (sample_elapsed_ms > 250U) {
        reset_power_history(&s_snapshot);
    } else {
        capture_power_bucket(&s_snapshot, now);
    }
    float rms = isfinite(s_snapshot.rms)
        ? clampf(s_snapshot.rms, 0.0f, 1.0f) : 0.0f;
    float peak = isfinite(s_snapshot.sample_peak)
        ? clampf(s_snapshot.sample_peak, 0.0f, 1.0f) : 0.0f;
    if (peak > s_window_peak) s_window_peak = peak;

    uint32_t display_elapsed_ms = now - s_last_display_ms;
    if (display_elapsed_ms < METER_DISPLAY_MS) return;
    if (display_elapsed_ms > 500U) display_elapsed_ms = 500U;
    s_last_display_ms = now;

    float displayed_rms = selected_rms(rms, now);
    audio_level_reading_t displayed;
    if (s_snapshot.uses_gg_input_scale) {
        audio_level_calculate_gg(
            displayed_rms, s_window_peak, &displayed);
    } else {
        audio_level_calculate(
            displayed_rms, s_window_peak, s_input_range, &displayed);
    }
    s_display_rms_db = displayed.rms_dbfs;
    update_peak_hold(displayed.peak_dbfs, now,
                     (float)display_elapsed_ms * 0.001f);
    update_meter_geometry(s_display_rms_db);
    s_window_peak = 0.0f;

    char text[16];
    if (s_display_rms_db <= AUDIO_LEVEL_FLOOR_DB) {
        snprintf(text, sizeof(text), "-INF");
    } else {
        snprintf(text, sizeof(text), "%.1f", s_display_rms_db);
    }
    set_text_if_changed(s_rms_value, s_rms_text, sizeof(s_rms_text), text);

    if (s_peak_hold_db <= AUDIO_LEVEL_FLOOR_DB) {
        snprintf(text, sizeof(text), "-INF dBFS");
    } else {
        snprintf(text, sizeof(text), "%.1f dBFS", s_peak_hold_db);
    }
    set_text_if_changed(s_peak_value, s_peak_text,
                        sizeof(s_peak_text), text);

    format_voltage(text, sizeof(text), displayed.input_vrms);
    set_text_if_changed(s_voltage_value, s_voltage_text,
                        sizeof(s_voltage_text), text);
    snprintf(text, sizeof(text), "%.1f", displayed.input_dbv);
    set_text_if_changed(s_dbv_value, s_dbv_text, sizeof(s_dbv_text), text);
    snprintf(text, sizeof(text), "%.1f", displayed.input_dbu);
    set_text_if_changed(s_dbu_value, s_dbu_text, sizeof(s_dbu_text), text);
}

static bool db_meter_on_event(ui_event_t event)
{
    if (s_mode == DB_MODE_LOUDNESS) {
        if (event == EV_OK) {
            audio_loudness_reset();
            return true;
        }
        return false;
    }
#if AUDIO_DUAL_RANGE
    if (s_mode == DB_MODE_RANGE_DIAGNOSTICS) return false;
#endif
    if (event == EV_LEFT || event == EV_RIGHT) {
#if AUDIO_DUAL_RANGE
        s_control = DB_CONTROL_AVERAGE;
#else
        s_control = event == EV_LEFT
            ? DB_CONTROL_INPUT : DB_CONTROL_AVERAGE;
#endif
        refresh_controls();
        return true;
    }

    int delta;
    if (event == EV_UP) {
        delta = -1;
    } else if (event == EV_DOWN) {
        delta = 1;
    } else {
        return false;
    }

#if AUDIO_DUAL_RANGE
    int next = (int)s_average_mode + delta;
    if (next < 0) next = DB_AVERAGE_COUNT - 1;
    if (next >= DB_AVERAGE_COUNT) next = 0;
    set_average_mode(next);
#else
    if (s_control == DB_CONTROL_INPUT) {
        int next = (int)s_input_range + delta;
        if (next < 0) next = AUDIO_INPUT_RANGE_COUNT - 1;
        if (next >= AUDIO_INPUT_RANGE_COUNT) next = 0;
        set_input_range(next);
    } else {
        int next = (int)s_average_mode + delta;
        if (next < 0) next = DB_AVERAGE_COUNT - 1;
        if (next >= DB_AVERAGE_COUNT) next = 0;
        set_average_mode(next);
    }
#endif
    return true;
}

static int db_meter_mode_count(void)
{
#if AUDIO_DUAL_RANGE
    return 3;
#else
    return 2;
#endif
}

static const char *db_meter_mode_name(int idx)
{
    if (idx == DB_MODE_LEVEL) return "Level Meter";
    if (idx == DB_MODE_LOUDNESS) return "Loudness";
#if AUDIO_DUAL_RANGE
    if (idx == DB_MODE_RANGE_DIAGNOSTICS) return "Range Diagnostics";
#endif
    return "";
}

static int db_meter_mode_index(void)
{
    return (int)s_mode;
}

static void db_meter_mode_set(int idx)
{
    if (idx < DB_MODE_LEVEL || idx >= db_meter_mode_count()) return;
    const db_meter_mode_t next = (db_meter_mode_t)idx;
    if (next == s_mode) return;
    s_mode = next;
    if (s_root) {
        s_rebuild_pending = true;
        if (s_mode == DB_MODE_LOUDNESS) audio_loudness_reset();
    }
}

const gadget_app_t APP_DB_METER = {
    .id = "dbmeter",
    .name = "dB Meter",
    .audio_mode = AUDIO_METER,
    .icon = NULL,
    .on_enter = db_meter_enter,
    .on_exit = db_meter_exit,
    .on_render = db_meter_render,
    .on_event = db_meter_on_event,
    .on_appearance_changed = db_meter_appearance_changed,
    .mode_count = db_meter_mode_count,
    .mode_name = db_meter_mode_name,
    .mode_index = db_meter_mode_index,
    .mode_set = db_meter_mode_set,
    .choice_settings = DB_METER_CHOICE_SETTINGS,
    .choice_setting_count =
        (int)(sizeof(DB_METER_CHOICE_SETTINGS) /
              sizeof(DB_METER_CHOICE_SETTINGS[0])),
};

#ifdef PEDAL_SIM
int db_meter_debug_input_range(void)
{
    return (int)s_input_range;
}

int db_meter_debug_average_mode(void)
{
    return (int)s_average_mode;
}
#endif
