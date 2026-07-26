#include "gadget_app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

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
#define RMS_AVERAGE_TAU_SECONDS 0.400f

typedef struct {
    float lo_db;
    float hi_db;
    uint32_t color;
} meter_zone_t;

static const meter_zone_t METER_ZONES[METER_ZONE_COUNT] = {
    { -72.0f, -18.0f, 0x2DD4BF },
    { -18.0f,  -6.0f, 0xFBBF24 },
    {  -6.0f,   0.0f, 0xF43F5E },
};

static const int SCALE_DB[] = { -72, -60, -48, -36, -24, -12, 0 };
#define SCALE_COUNT ((int)(sizeof(SCALE_DB) / sizeof(SCALE_DB[0])))

static lv_obj_t *s_root;
static lv_obj_t *s_rms_value;
static lv_obj_t *s_peak_value;
static lv_obj_t *s_voltage_value;
static lv_obj_t *s_dbv_value;
static lv_obj_t *s_dbu_value;
static lv_obj_t *s_zone_fill[METER_ZONE_COUNT];
static lv_obj_t *s_peak_tick;
static audio_viz_snapshot_t s_snapshot;
static float s_display_rms_db = METER_FLOOR_DB;
static float s_peak_hold_db = METER_FLOOR_DB;
static float s_average_power;
static float s_window_peak;
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

static void set_text_if_changed(lv_obj_t *label, char *previous,
                                size_t previous_size, const char *text)
{
    if (!label || strcmp(previous, text) == 0) return;
    snprintf(previous, previous_size, "%s", text);
    lv_label_set_text(label, previous);
}

static void create_meter(const ui_theme_t *theme)
{
    const uint32_t background = lv_color_to_u32(theme->bg) & 0xffffffU;

    for (int i = 0; i < METER_ZONE_COUNT; i++) {
        int x0 = db_to_x(METER_ZONES[i].lo_db);
        int x1 = db_to_x(METER_ZONES[i].hi_db);
        int width = x1 - x0 + 1;

        lv_obj_t *track = lv_obj_create(s_root);
        lv_obj_set_size(track, width, METER_H);
        lv_obj_set_pos(track, METER_X + x0, METER_Y);
        style_rect(track, mix_hex(background, METER_ZONES[i].color, 52));

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
    lv_obj_set_width(caption_label, 130);
    lv_obj_set_style_text_align(caption_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(caption_label, x, 220);

    *value = make_label(s_root, "--", &lv_font_montserrat_28, theme->text);
    lv_obj_set_width(*value, 130);
    lv_obj_set_style_text_align(*value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(*value, x, 239);
}

static void db_meter_enter(int variant)
{
    (void)variant;
    const ui_theme_t *theme = theme_get();
    audio_set_mode(AUDIO_SPECTRUM);
    audio_set_viz_mode(VIZ_MONITOR);

    s_display_rms_db = METER_FLOOR_DB;
    s_peak_hold_db = METER_FLOOR_DB;
    s_average_power = 0.0f;
    s_window_peak = 0.0f;
    s_peak_hold_until = 0;
    s_last_sample_ms = plat_millis();
    s_last_display_ms = s_last_sample_ms;
    s_last_peak_x = -1;
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_rms_text[0] = '\0';
    s_peak_text[0] = '\0';
    s_voltage_text[0] = '\0';
    s_dbv_text[0] = '\0';
    s_dbu_text[0] = '\0';

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_root, 0, 0);
    style_rect(s_root, lv_color_to_u32(theme->bg) & 0xffffffU);

    lv_obj_t *title = make_label(s_root, "DECIBEL METER",
                                 &lv_font_montserrat_14, theme->accent);
    lv_obj_set_pos(title, 18, 9);

    lv_obj_t *reference = make_label(
        s_root, "ADC PIN NOMINAL  |  3.0 Vpp FS",
        &lv_font_montserrat_12, theme->text);
    lv_obj_set_width(reference, 260);
    lv_obj_set_style_text_align(reference, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(reference, 202, 10);

    lv_obj_t *rms_caption = make_label(s_root, "RMS LEVEL",
                                       &lv_font_montserrat_12,
                                       theme->text);
    lv_obj_set_pos(rms_caption, 20, 42);
    s_rms_value = make_label(s_root, "-72.0", &lv_font_montserrat_48,
                             theme->text);
    lv_obj_set_width(s_rms_value, 235);
    lv_obj_set_pos(s_rms_value, 16, 56);
    lv_obj_t *rms_unit = make_label(s_root, "dBFS",
                                    &lv_font_montserrat_14, theme->accent);
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

    create_meter(theme);
    create_metric_column(theme, 20, "ADC V RMS", &s_voltage_value);
    create_metric_column(theme, 175, "dBV RMS", &s_dbv_value);
    create_metric_column(theme, 330, "dBu RMS", &s_dbu_value);

    lv_obj_t *footer = make_label(
        s_root, "Voltage excludes analog front-end gain until calibration",
        &lv_font_montserrat_12, theme->grid);
    lv_obj_set_width(footer, SCREEN_W);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(footer, 0, 294);
}

static void db_meter_exit(void)
{
    if (s_root) lv_obj_delete(s_root);
    s_root = NULL;
    s_rms_value = NULL;
    s_peak_value = NULL;
    s_voltage_value = NULL;
    s_dbv_value = NULL;
    s_dbu_value = NULL;
    s_peak_tick = NULL;
    for (int i = 0; i < METER_ZONE_COUNT; i++) s_zone_fill[i] = NULL;
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

static void db_meter_render(void)
{
    if (!s_root) return;

    uint32_t now = plat_millis();
    uint32_t sample_elapsed_ms = now - s_last_sample_ms;
    if (sample_elapsed_ms < METER_SAMPLE_MS) return;
    if (sample_elapsed_ms > 250U) sample_elapsed_ms = 250U;
    s_last_sample_ms = now;

    plat_audio_viz_get(&s_snapshot);
    float rms = isfinite(s_snapshot.rms)
        ? clampf(s_snapshot.rms, 0.0f, 1.0f) : 0.0f;
    float peak = isfinite(s_snapshot.sample_peak)
        ? clampf(s_snapshot.sample_peak, 0.0f, 1.0f) : 0.0f;
    float sample_dt = (float)sample_elapsed_ms * 0.001f;
    float alpha = sample_dt / (RMS_AVERAGE_TAU_SECONDS + sample_dt);
    s_average_power += (rms * rms - s_average_power) * alpha;
    if (peak > s_window_peak) s_window_peak = peak;

    uint32_t display_elapsed_ms = now - s_last_display_ms;
    if (display_elapsed_ms < METER_DISPLAY_MS) return;
    if (display_elapsed_ms > 500U) display_elapsed_ms = 500U;
    s_last_display_ms = now;

    float displayed_rms = sqrtf(fmaxf(s_average_power, 0.0f));
    audio_level_reading_t displayed;
    audio_level_calculate(displayed_rms, s_window_peak, &displayed);
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

    format_voltage(text, sizeof(text), displayed.adc_vrms);
    set_text_if_changed(s_voltage_value, s_voltage_text,
                        sizeof(s_voltage_text), text);
    snprintf(text, sizeof(text), "%.1f", displayed.dbv);
    set_text_if_changed(s_dbv_value, s_dbv_text, sizeof(s_dbv_text), text);
    snprintf(text, sizeof(text), "%.1f", displayed.dbu);
    set_text_if_changed(s_dbu_value, s_dbu_text, sizeof(s_dbu_text), text);
}

static bool db_meter_on_event(ui_event_t event)
{
    (void)event;
    return false;
}

const gadget_app_t APP_DB_METER = {
    .id = "dbmeter",
    .name = "dB Meter",
    .audio_mode = AUDIO_SPECTRUM,
    .icon = NULL,
    .on_enter = db_meter_enter,
    .on_exit = db_meter_exit,
    .on_render = db_meter_render,
    .on_event = db_meter_on_event,
};
