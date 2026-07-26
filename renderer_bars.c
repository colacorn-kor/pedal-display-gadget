/* ============================================================================
 * renderer_bars.c - guitar/bass-oriented 12-band spectrum renderer
 * ========================================================================== */
#include "renderer.h"

#include <math.h>
#include <stdint.h>

#include "audio_config.h"

#define SCR_W 480
#define SCR_H 320
#define TOP 34
#define BOT 40
#define LEFT 35
#define RIGHT 7
#define PX LEFT
#define PY TOP
#define PW (SCR_W - LEFT - RIGHT)
#define PH (SCR_H - TOP - BOT)
#define BAND_COUNT 12

typedef struct {
    float center_hz;
    const char *label;
} eq_band_t;

/* Guitar/bass-focused centers derived from common graphic-EQ landmarks. */
static const eq_band_t EQ_BANDS[BAND_COUNT] = {
    {    50.0f, "50"   },
    {   100.0f, "100"  },
    {   200.0f, "200"  },
    {   400.0f, "400"  },
    {   600.0f, "600"  },
    {   800.0f, "800"  },
    {  1200.0f, "1.2k" },
    {  1600.0f, "1.6k" },
    {  3200.0f, "3.2k" },
    {  4500.0f, "4.5k" },
    {  6400.0f, "6.4k" },
    { 10000.0f, "10k"  },
};

static const int DB_MARKS[] = { 0, -12, -24, -36, -48, -60, -72 };
#define DB_MARK_COUNT ((int)(sizeof(DB_MARKS) / sizeof(DB_MARKS[0])))

static const uint32_t MULTI_COLORS[BAND_COUNT] = {
    0x22D3EE, 0x27C9D2, 0x2DD4BF,
    0x34D399, 0x6EE7B7, 0xA3E635,
    0xFACC15, 0xFBBF24, 0xFB923C,
    0xF97316, 0xF43F5E, 0xEC4899,
};

static lv_obj_t *s_root;
static lv_obj_t *s_bar[BAND_COUNT];
static lv_obj_t *s_peak[BAND_COUNT];
static viz_theme_t s_theme;
static float s_band_edges[BAND_COUNT + 1];
static int s_previous_height[BAND_COUNT];
static int s_previous_peak_y[BAND_COUNT];
static uint32_t s_previous_color[BAND_COUNT];
static bool s_previous_peak_hidden[BAND_COUNT];
static float s_slot;
static int s_bar_width;

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

static void style_rect(lv_obj_t *obj, uint32_t color)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    return label;
}

static int db_to_y(float db)
{
    float normalized = (db - VIZ_DB_FLOOR) /
                       (VIZ_DB_TOP - VIZ_DB_FLOOR);
    int y = (int)((1.0f - normalized) * (PH - 1) + 0.5f);
    if (y < 0) return 0;
    if (y >= PH) return PH - 1;
    return y;
}

static int frequency_to_index(float frequency, int count)
{
    float normalized =
        logf(frequency / VIZ_FREQ_LO_HZ) /
        logf(VIZ_FREQ_HI_HZ / VIZ_FREQ_LO_HZ);
    int index = (int)(normalized * (count - 1) + 0.5f);
    if (index < 0) return 0;
    if (index >= count) return count - 1;
    return index;
}

static void calculate_band_edges(void)
{
    s_band_edges[0] = VIZ_FREQ_LO_HZ;
    for (int i = 1; i < BAND_COUNT; i++) {
        s_band_edges[i] =
            sqrtf(EQ_BANDS[i - 1].center_hz * EQ_BANDS[i].center_hz);
    }
    s_band_edges[BAND_COUNT] = VIZ_FREQ_HI_HZ;
}

static uint32_t band_color(int band, float value)
{
    uint32_t base;
    if (s_theme.accent == 0x22D3EE) {
        base = s_theme.accent;
    } else {
        base = MULTI_COLORS[band];
    }

    uint8_t opacity = (uint8_t)(120.0f + value * 135.0f);
    return mix_hex(s_theme.bg, base, opacity);
}

static void create_grid_and_axes(void)
{
    uint32_t axis_color = mix_hex(s_theme.bg, s_theme.line, 128);

    lv_obj_t *title = make_label(s_root, "GUITAR / BASS 12-BAND",
                                 s_theme.accent);
    lv_obj_set_pos(title, PX, 5);

    lv_obj_t *mode = make_label(s_root, "RMS SPECTRUM", axis_color);
    lv_obj_set_width(mode, 130);
    lv_obj_set_style_text_align(mode, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(mode, SCR_W - RIGHT - 130, 5);

    for (int i = 0; i < DB_MARK_COUNT; i++) {
        int y = db_to_y((float)DB_MARKS[i]);
        lv_obj_t *line = lv_obj_create(s_root);
        lv_obj_set_size(line, PW, 1);
        lv_obj_set_pos(line, PX, PY + y);
        style_rect(line, s_theme.grid);

        char text[8];
        lv_snprintf(text, sizeof(text), "%d", DB_MARKS[i]);
        lv_obj_t *label = make_label(s_root, text, axis_color);
        lv_obj_set_width(label, LEFT - 6);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(label, 0, PY + y - 6);
    }

    for (int i = 0; i < BAND_COUNT; i++) {
        int x = PX + (int)((i + 0.5f) * s_slot + 0.5f);
        lv_obj_t *label = make_label(s_root, EQ_BANDS[i].label, axis_color);
        lv_obj_set_width(label, (int)s_slot);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, x - (int)(s_slot * 0.5f),
                       PY + PH + 4);
    }
}

static void bars_create(lv_obj_t *parent, const viz_theme_t *theme)
{
    s_theme = *theme;
    s_slot = (float)PW / BAND_COUNT;
    s_bar_width = (int)(s_slot * 0.70f + 0.5f);
    calculate_band_edges();

    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, SCR_W, SCR_H);
    lv_obj_set_pos(s_root, 0, 0);
    style_rect(s_root, theme->bg);
    create_grid_and_axes();

    for (int band = 0; band < BAND_COUNT; band++) {
        int x = PX + (int)(band * s_slot +
                           0.5f * (s_slot - s_bar_width) + 0.5f);
        s_bar[band] = lv_obj_create(s_root);
        lv_obj_set_size(s_bar[band], s_bar_width, 1);
        lv_obj_set_pos(s_bar[band], x, PY + PH - 1);
        style_rect(s_bar[band], band_color(band, 0.0f));

        s_peak[band] = lv_obj_create(s_root);
        lv_obj_set_size(s_peak[band], s_bar_width, 2);
        lv_obj_set_pos(s_peak[band], x, PY + PH);
        style_rect(s_peak[band], theme->peak);
        lv_obj_add_flag(s_peak[band], LV_OBJ_FLAG_HIDDEN);

        s_previous_height[band] = 1;
        s_previous_peak_y[band] = PY + PH;
        s_previous_color[band] = band_color(band, 0.0f);
        s_previous_peak_hidden[band] = true;
    }
}

static void aggregate_band(const viz_frame_t *frame, int band,
                           float *value, float *peak)
{
    int begin = frequency_to_index(s_band_edges[band], frame->n);
    int end = frequency_to_index(s_band_edges[band + 1], frame->n);
    if (end <= begin) end = begin + 1;
    if (end > frame->n) end = frame->n;

    *value = 0.0f;
    *peak = 0.0f;
    for (int i = begin; i < end; i++) {
        if (isfinite(frame->bars[i]) && frame->bars[i] > *value) {
            *value = frame->bars[i];
        }
        if (frame->peaks && isfinite(frame->peaks[i]) &&
            frame->peaks[i] > *peak) {
            *peak = frame->peaks[i];
        }
    }
    if (*value > 1.0f) *value = 1.0f;
    if (*peak > 1.0f) *peak = 1.0f;
}

static void bars_update(const viz_frame_t *frame)
{
    if (!s_root || !frame || !frame->bars || frame->n <= 0) return;

    for (int band = 0; band < BAND_COUNT; band++) {
        float value;
        float peak;
        aggregate_band(frame, band, &value, &peak);

        int height = (int)(value * PH + 0.5f);
        if (height < 1) height = 1;
        if (height != s_previous_height[band]) {
            lv_obj_set_size(s_bar[band], s_bar_width, height);
            lv_obj_set_y(s_bar[band], PY + PH - height);
            s_previous_height[band] = height;
        }

        uint32_t color = band_color(band, value);
        if (color != s_previous_color[band]) {
            lv_obj_set_style_bg_color(s_bar[band], lv_color_hex(color), 0);
            s_previous_color[band] = color;
        }

        if (peak > 0.001f) {
            int peak_y = PY + (int)((1.0f - peak) * (PH - 1));
            if (peak_y != s_previous_peak_y[band]) {
                lv_obj_set_y(s_peak[band], peak_y);
                s_previous_peak_y[band] = peak_y;
            }
            if (s_previous_peak_hidden[band]) {
                lv_obj_remove_flag(s_peak[band], LV_OBJ_FLAG_HIDDEN);
                s_previous_peak_hidden[band] = false;
            }
        } else if (!s_previous_peak_hidden[band]) {
            lv_obj_add_flag(s_peak[band], LV_OBJ_FLAG_HIDDEN);
            s_previous_peak_hidden[band] = true;
        }
    }
}

static void bars_destroy(void)
{
    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }
    for (int band = 0; band < BAND_COUNT; band++) {
        s_bar[band] = NULL;
        s_peak[band] = NULL;
        s_previous_height[band] = -1;
        s_previous_peak_y[band] = -1;
        s_previous_color[band] = UINT32_MAX;
        s_previous_peak_hidden[band] = true;
    }
}

const renderer_t RENDERER_BARS = {
    "bars", bars_create, bars_update, bars_destroy
};
