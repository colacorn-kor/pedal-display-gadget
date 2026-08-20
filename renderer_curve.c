/* ============================================================================
 *  renderer_curve.c - music-oriented log spectrum canvas renderer
 * ========================================================================== */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "renderer.h"

#define SCR_W 480
#define SCR_H 320
#define TOP   25
#define BOT   23
#define LEFT  38
#define RIGHT 8
#define PX    LEFT
#define PY    TOP
#define PW    (SCR_W - LEFT - RIGHT)
#define PH    (SCR_H - TOP - BOT)
#define CURVE_FRAME_US 30000
#define CURVE_SMOOTHING_LEVELS 3
#define RGB565(r,g,b) (uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))
#define C565(h) RGB565(((h)>>16)&0xFF,((h)>>8)&0xFF,(h)&0xFF)

typedef struct {
    float frequency;
    const char *label;
    uint8_t major;
} frequency_mark_t;

typedef struct {
    float low_hz;
    float high_hz;
    uint32_t color;
    const char *label;
} frequency_zone_t;

typedef struct {
    float smooth[VIZ_POINTS];
    float underlay[VIZ_POINTS];
    int y_curve[PW];
    int y_prev[PW];
    int y_underlay[PW];
    int y_underlay_prev[PW];
} curve_work_t;

static const frequency_mark_t FREQUENCY_MARKS[] = {
    {    20.0f, "20",  1 },
    {    30.0f, NULL,  0 },
    {    50.0f, "50",  1 },
    {    70.0f, NULL,  0 },
    {   100.0f, "100", 1 },
    {   200.0f, "200", 1 },
    {   300.0f, NULL,  0 },
    {   500.0f, "500", 1 },
    {   700.0f, NULL,  0 },
    {  1000.0f, "1k",  1 },
    {  2000.0f, "2k",  1 },
    {  3000.0f, NULL,  0 },
    {  5000.0f, "5k",  1 },
    {  7000.0f, NULL,  0 },
    { 10000.0f, "10k", 1 },
    { 20000.0f, "20k", 1 },
};
#define FREQUENCY_MARK_COUNT \
    ((int)(sizeof(FREQUENCY_MARKS) / sizeof(FREQUENCY_MARKS[0])))

static const int DB_MARKS[] = { 0, -12, -24, -36, -48, -60, -72 };
#define DB_MARK_COUNT ((int)(sizeof(DB_MARKS) / sizeof(DB_MARKS[0])))

static const frequency_zone_t FREQUENCY_ZONES[] = {
    {    20.0f,    60.0f, 0xD94A64, "Sub\nBass" },
    {    60.0f,   250.0f, 0xEA6257, "Bass"      },
    {   250.0f,   500.0f, 0xF6C945, "Low\nMid"  },
    {   500.0f,  2000.0f, 0x5BCB77, "Mid"       },
    {  2000.0f,  8000.0f, 0x43BCCD, "High\nMid" },
    {  8000.0f, 20000.0f, 0x8E6CEF, "High"      },
};
#define FREQUENCY_ZONE_COUNT \
    ((int)(sizeof(FREQUENCY_ZONES) / sizeof(FREQUENCY_ZONES[0])))
#define FREQUENCY_ZONE_OPACITY 24

static const char *TAG = "curve";
static lv_obj_t *s_root;
static lv_obj_t *s_canvas;
static lv_obj_t *s_title_label;
static lv_obj_t *s_profile_label;
static void *s_buf;
static void *s_static_buf;
static curve_work_t *s_work;
static uint32_t s_stride;
static viz_theme_t s_theme;
static int s_frame_valid;
static int64_t s_last_draw_us;
static int64_t s_fps_start_us;
static int s_fps_frames;
static curve_display_mode_t s_display_mode = CURVE_DISPLAY_VISUAL;
static int s_smoothing_level = 1;
static spectrum_weighting_t s_weighting = SPECTRUM_WEIGHT_FLAT;

static float clamp_unit(float value)
{
    if (!isfinite(value) || value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
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

static uint16_t mix_565(uint16_t base, uint16_t overlay, uint8_t opacity)
{
    unsigned inverse = 255U - opacity;
    unsigned base_r = (base >> 11) & 0x1fU;
    unsigned base_g = (base >> 5) & 0x3fU;
    unsigned base_b = base & 0x1fU;
    unsigned over_r = (overlay >> 11) & 0x1fU;
    unsigned over_g = (overlay >> 5) & 0x3fU;
    unsigned over_b = overlay & 0x1fU;
    unsigned red = (base_r * inverse + over_r * opacity + 127U) / 255U;
    unsigned green = (base_g * inverse + over_g * opacity + 127U) / 255U;
    unsigned blue = (base_b * inverse + over_b * opacity + 127U) / 255U;
    return (uint16_t)((red << 11) | (green << 5) | blue);
}

static inline uint16_t *pixel_at(void *buffer, uint32_t stride, int x, int y)
{
    return (uint16_t *)((uint8_t *)buffer + y * stride) + x;
}

static int frequency_to_x(float frequency)
{
    float position = logf(frequency / VIZ_FREQ_LO_HZ) /
                     logf(VIZ_FREQ_HI_HZ / VIZ_FREQ_LO_HZ);
    int x = (int)(position * (PW - 1) + 0.5f);
    if (x < 0) return 0;
    if (x >= PW) return PW - 1;
    return x;
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

static int value_to_y(float value)
{
    return db_to_y(VIZ_DB_FLOOR +
                   clamp_unit(value) * (VIZ_DB_TOP - VIZ_DB_FLOOR));
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    return label;
}

static void update_header_labels(void)
{
    if (!s_title_label || !s_profile_label) return;

    if (s_display_mode == CURVE_DISPLAY_REFERENCE) {
        char text[48];
        lv_label_set_text(s_title_label, "REFERENCE");
        lv_snprintf(text, sizeof(text), "-72..0 dBFS   %s",
                    spectrum_weighting_name(s_weighting));
        lv_label_set_text(s_profile_label, text);
        return;
    }

    static const char *const smoothing_names[CURVE_SMOOTHING_LEVELS] = {
        "DETAIL", "BALANCED", "SIMPLE",
    };
    lv_label_set_text(s_title_label, "CURVE");
    if (s_weighting != SPECTRUM_WEIGHT_FLAT) {
        char text[48];
        lv_snprintf(text, sizeof(text), "%s / %s",
                    smoothing_names[s_smoothing_level],
                    spectrum_weighting_name(s_weighting));
        lv_label_set_text(s_profile_label, text);
    } else {
        lv_label_set_text(s_profile_label, smoothing_names[s_smoothing_level]);
    }
}

void renderer_curve_configure(curve_display_mode_t mode,
                              int smoothing_level,
                              spectrum_weighting_t weighting)
{
    s_display_mode = mode == CURVE_DISPLAY_REFERENCE
        ? CURVE_DISPLAY_REFERENCE : CURVE_DISPLAY_VISUAL;
    if (smoothing_level < 0) smoothing_level = 0;
    if (smoothing_level >= CURVE_SMOOTHING_LEVELS) {
        smoothing_level = CURVE_SMOOTHING_LEVELS - 1;
    }
    s_smoothing_level = smoothing_level;
    s_weighting = weighting >= SPECTRUM_WEIGHT_FLAT &&
                  weighting < SPECTRUM_WEIGHT_COUNT
        ? weighting : SPECTRUM_WEIGHT_FLAT;
    s_frame_valid = 0;
    update_header_labels();
}

static void create_axes(void)
{
    if (!s_theme.show_axis) return;

    uint32_t axis_color = mix_hex(s_theme.bg, s_theme.line, 118);
    s_title_label = create_label(s_root, "", s_theme.accent);
    lv_obj_set_pos(s_title_label, PX, 3);

    s_profile_label = create_label(s_root, "", axis_color);
    lv_obj_set_width(s_profile_label, 240);
    lv_obj_set_style_text_align(s_profile_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_profile_label, SCR_W - RIGHT - 240, 3);
    update_header_labels();

    for (int i = 0; i < DB_MARK_COUNT; i++) {
        char text[8];
        lv_snprintf(text, sizeof(text), "%d", DB_MARKS[i]);
        lv_obj_t *label = create_label(s_root, text, axis_color);
        lv_obj_set_width(label, LEFT - 8);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(label, 1, PY + db_to_y((float)DB_MARKS[i]) - 6);
    }

    for (int i = 0; i < FREQUENCY_MARK_COUNT; i++) {
        const frequency_mark_t *mark = &FREQUENCY_MARKS[i];
        if (!mark->label) continue;

        int x = frequency_to_x(mark->frequency);
        lv_obj_t *label = create_label(s_root, mark->label, axis_color);
        if (i == 0) {
            lv_obj_set_width(label, 34);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_pos(label, PX, PY + PH + 1);
        } else if (i == FREQUENCY_MARK_COUNT - 1) {
            lv_obj_set_width(label, 34);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_set_pos(label, PX + x - 34, PY + PH + 1);
        } else {
            lv_obj_set_width(label, 38);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(label, PX + x - 19, PY + PH + 1);
        }
    }
}

static void create_zone_labels(void)
{
    for (int i = 0; i < FREQUENCY_ZONE_COUNT; i++) {
        const frequency_zone_t *zone = &FREQUENCY_ZONES[i];
        if (!zone->label) continue;

        const int x0 = frequency_to_x(zone->low_hz);
        const int x1 = i == FREQUENCY_ZONE_COUNT - 1
            ? PW : frequency_to_x(zone->high_hz);
        lv_obj_t *label = create_label(
            s_root, zone->label,
            mix_hex(s_theme.bg, zone->color, 176));
        lv_obj_set_height(label, 30);
        lv_obj_set_width(label, x1 - x0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, PX + x0, PY + 5);
    }
}

static void draw_static_canvas(void)
{
    uint16_t background = C565(s_theme.bg);
    uint16_t grid = C565(s_theme.grid);
    uint16_t minor_grid = mix_565(background, grid, 108);

    for (int y = 0; y < PH; y++) {
        uint16_t *canvas_row = pixel_at(s_buf, s_stride, 0, y);
        uint16_t *static_row = pixel_at(s_static_buf, s_stride, 0, y);
        for (int x = 0; x < PW; x++) {
            canvas_row[x] = background;
            static_row[x] = background;
        }
    }

    for (int i = 0; i < FREQUENCY_ZONE_COUNT; i++) {
        const frequency_zone_t *zone = &FREQUENCY_ZONES[i];
        const int x0 = i == 0 ? 0 : frequency_to_x(zone->low_hz);
        const int x1 = i == FREQUENCY_ZONE_COUNT - 1
            ? PW : frequency_to_x(zone->high_hz);
        const uint16_t zone_color = mix_565(
            background, C565(zone->color), FREQUENCY_ZONE_OPACITY);
        for (int y = 0; y < PH; y++) {
            uint16_t *canvas_row = pixel_at(s_buf, s_stride, 0, y);
            uint16_t *static_row = pixel_at(s_static_buf, s_stride, 0, y);
            for (int x = x0; x < x1; x++) {
                canvas_row[x] = zone_color;
                static_row[x] = zone_color;
            }
        }
    }

    if (!s_theme.show_grid) return;

    for (int i = 0; i < DB_MARK_COUNT; i++) {
        int y = db_to_y((float)DB_MARKS[i]);
        for (int x = 0; x < PW; x++) {
            *pixel_at(s_buf, s_stride, x, y) = grid;
            *pixel_at(s_static_buf, s_stride, x, y) = grid;
        }
    }

    for (int i = 0; i < FREQUENCY_MARK_COUNT; i++) {
        const frequency_mark_t *mark = &FREQUENCY_MARKS[i];
        int x = frequency_to_x(mark->frequency);
        uint16_t color = mark->major ? grid : minor_grid;
        for (int y = 0; y < PH; y++) {
            *pixel_at(s_buf, s_stride, x, y) = color;
            *pixel_at(s_static_buf, s_stride, x, y) = color;
        }
    }
}

static void smooth_bands(const float *input, int count, float *output)
{
    static const int balanced_weights[] = { 1, 4, 6, 4, 1 };
    static const int simple_weights[] = {
        1, 8, 28, 56, 70, 56, 28, 8, 1,
    };
    if (s_display_mode == CURVE_DISPLAY_REFERENCE ||
        s_smoothing_level == 0) {
        for (int i = 0; i < count; i++) {
            output[i] = clamp_unit(input[i]);
        }
        return;
    }

    const int *weights = s_smoothing_level == 1
        ? balanced_weights : simple_weights;
    const int tap_count = s_smoothing_level == 1 ? 5 : 9;
    const int radius = tap_count / 2;
    for (int i = 0; i < count; i++) {
        float sum = 0.0f;
        int weight_sum = 0;
        for (int tap = -radius; tap <= radius; tap++) {
            int source = i + tap;
            if (source < 0) source = 0;
            if (source >= count) source = count - 1;
            int weight = weights[tap + radius];
            sum += clamp_unit(input[source]) * weight;
            weight_sum += weight;
        }
        output[i] = sum / (float)weight_sum;
    }
}

static float sample_column(const float *values, int count, float step, int x)
{
    float position = x * step;
    int first = (int)position;
    float fraction = position - first;
    int second = first + 1 < count ? first + 1 : first;
    return values[first] * (1.0f - fraction) + values[second] * fraction;
}

static void restore_column(int x, int y0)
{
    if (y0 < 0) y0 = 0;
    if (y0 >= PH) return;
    for (int y = y0; y < PH; y++) {
        *pixel_at(s_buf, s_stride, x, y) =
            *pixel_at(s_static_buf, s_stride, x, y);
    }
}

static void draw_fill_column(int x, int curve_y, uint16_t accent)
{
    int span = PH - 1 - curve_y;
    if (span < 1) span = 1;

    for (int y = curve_y; y < PH; y++) {
        int remaining = PH - 1 - y;
        uint8_t opacity = (uint8_t)(14 + 72 * remaining / span);
        uint16_t base = *pixel_at(s_static_buf, s_stride, x, y);
        *pixel_at(s_buf, s_stride, x, y) =
            mix_565(base, accent, opacity);
    }
}

static void draw_trace_segment(int x, int first_y, int second_y,
                               uint16_t color, uint16_t glow,
                               uint8_t glow_opacity)
{
    if (first_y < 0 || second_y < 0) return;
    int y0 = first_y < second_y ? first_y : second_y;
    int y1 = first_y > second_y ? first_y : second_y;
    if (y0 < 0) y0 = 0;
    if (y1 >= PH) y1 = PH - 1;

    for (int y = y0; y <= y1; y++) {
        if (y > 0) {
            uint16_t *soft = pixel_at(s_buf, s_stride, x, y - 1);
            *soft = mix_565(*soft, glow, glow_opacity);
        }
        if (y + 1 < PH) {
            uint16_t *soft = pixel_at(s_buf, s_stride, x, y + 1);
            *soft = mix_565(*soft, glow, glow_opacity);
        }
        *pixel_at(s_buf, s_stride, x, y) = color;
    }
}

static int minimum_visible_y(int current, int adjacent)
{
    int result = current;
    if (adjacent >= 0 && adjacent < result) result = adjacent;
    return result;
}

static void log_curve_fps(int dirty_w, int dirty_h)
{
    int64_t now = esp_timer_get_time();
    if (s_fps_start_us == 0) {
        s_fps_start_us = now;
        s_fps_frames = 0;
    }

    s_fps_frames++;
    int64_t elapsed = now - s_fps_start_us;
    if (elapsed >= 1000000) {
        int fps = (int)((int64_t)s_fps_frames * 1000000 / elapsed);
        ESP_LOGI(TAG, "fps=%d dirty=%dx%d", fps, dirty_w, dirty_h);
        s_fps_start_us = now;
        s_fps_frames = 0;
    }
}

static void rectangle_style(lv_obj_t *obj, uint32_t color)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
}

static void curve_create(lv_obj_t *parent, const viz_theme_t *theme)
{
    s_theme = *theme;
    s_frame_valid = 0;
    s_stride = 0;
    s_last_draw_us = 0;
    s_fps_start_us = 0;
    s_fps_frames = 0;
    s_work = heap_caps_calloc(1, sizeof(*s_work),
                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_work) {
        ESP_LOGE(TAG, "PSRAM work buffer allocation failed (%u bytes)",
                 (unsigned)sizeof(*s_work));
        return;
    }
    for (int x = 0; x < PW; x++) {
        s_work->y_prev[x] = -1;
        s_work->y_underlay_prev[x] = -1;
    }

    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, SCR_W, SCR_H);
    lv_obj_set_pos(s_root, 0, 0);
    rectangle_style(s_root, theme->bg);
    create_axes();

    size_t bytes = LV_CANVAS_BUF_SIZE(PW, PH, 16, LV_DRAW_BUF_STRIDE_ALIGN);
    s_buf = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_buf) {
        ESP_LOGE(TAG, "PSRAM canvas allocation failed (%u bytes)",
                 (unsigned)bytes);
        s_canvas = NULL;
        heap_caps_free(s_work);
        s_work = NULL;
        return;
    }
    s_static_buf = heap_caps_calloc(1, bytes,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_static_buf) {
        ESP_LOGE(TAG, "PSRAM static canvas allocation failed (%u bytes)",
                 (unsigned)bytes);
        heap_caps_free(s_buf);
        s_buf = NULL;
        heap_caps_free(s_work);
        s_work = NULL;
        s_canvas = NULL;
        return;
    }

    s_canvas = lv_canvas_create(s_root);
    if (!s_canvas) {
        ESP_LOGE(TAG, "canvas object creation failed");
        heap_caps_free(s_static_buf);
        s_static_buf = NULL;
        heap_caps_free(s_buf);
        s_buf = NULL;
        heap_caps_free(s_work);
        s_work = NULL;
        return;
    }
    lv_canvas_set_buffer(s_canvas, s_buf, PW, PH, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(s_canvas, PX, PY);

    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(s_canvas);
    if (!draw_buf || !draw_buf->data) {
        ESP_LOGE(TAG, "canvas draw buffer unavailable");
        lv_obj_delete(s_canvas);
        s_canvas = NULL;
        heap_caps_free(s_static_buf);
        s_static_buf = NULL;
        heap_caps_free(s_buf);
        s_buf = NULL;
        heap_caps_free(s_work);
        s_work = NULL;
        return;
    }
    s_stride = draw_buf->header.stride;
    draw_static_canvas();
    create_zone_labels();
    lv_obj_invalidate(s_canvas);
}

static void curve_update(const viz_frame_t *frame)
{
    if (!s_canvas || !s_buf || !s_static_buf || !s_work || !frame ||
        !frame->bars || frame->n < 2 || s_stride == 0) {
        return;
    }

    int64_t now = esp_timer_get_time();
    if (s_frame_valid && now - s_last_draw_us < CURVE_FRAME_US) return;
    s_last_draw_us = now;

    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(s_canvas);
    if (!draw_buf || !draw_buf->data) return;
    uint32_t stride = draw_buf->header.stride;
    if (stride != s_stride) {
        ESP_LOGW(TAG, "canvas stride changed from %u to %u",
                 (unsigned)s_stride, (unsigned)stride);
        s_stride = stride;
        draw_static_canvas();
        s_frame_valid = 0;
        lv_obj_invalidate(s_canvas);
        return;
    }

    int count = frame->n < VIZ_POINTS ? frame->n : VIZ_POINTS;
    smooth_bands(frame->bars, count, s_work->smooth);
    if (frame->underlay) {
        smooth_bands(frame->underlay, count, s_work->underlay);
    }

    float step = (float)(count - 1) / (float)(PW - 1);
    for (int x = 0; x < PW; x++) {
        s_work->y_curve[x] =
            value_to_y(sample_column(s_work->smooth, count, step, x));
        s_work->y_underlay[x] = frame->underlay
            ? value_to_y(sample_column(s_work->underlay, count, step, x))
            : -1;
    }

    uint16_t accent = C565(s_theme.accent);
    uint16_t line = C565(s_theme.line);
    uint16_t compare = C565(s_theme.peak);
    int dirty_x0 = PW;
    int dirty_y0 = PH;
    int dirty_x1 = -1;

    for (int x = 0; x < PW; x++) {
        int adjacent_changed = x > 0 &&
            s_work->y_prev[x - 1] != s_work->y_curve[x - 1];
        int underlay_adjacent_changed = x > 0 &&
            s_work->y_underlay_prev[x - 1] !=
                s_work->y_underlay[x - 1];
        int changed = !s_frame_valid ||
                      s_work->y_prev[x] != s_work->y_curve[x] ||
                      adjacent_changed ||
                      s_work->y_underlay_prev[x] !=
                          s_work->y_underlay[x] ||
                      underlay_adjacent_changed;
        if (!changed) continue;

        int old_adjacent = x > 0
            ? s_work->y_prev[x - 1] : s_work->y_prev[x];
        int new_adjacent = x > 0
            ? s_work->y_curve[x - 1] : s_work->y_curve[x];
        int old_underlay_adjacent = x > 0
            ? s_work->y_underlay_prev[x - 1]
            : s_work->y_underlay_prev[x];
        int new_underlay_adjacent = x > 0
            ? s_work->y_underlay[x - 1] : s_work->y_underlay[x];
        int old_top = s_frame_valid
            ? minimum_visible_y(s_work->y_prev[x], old_adjacent)
            : 0;
        int new_top = minimum_visible_y(s_work->y_curve[x], new_adjacent);
        if (s_work->y_underlay_prev[x] >= 0) {
            int underlay_top = minimum_visible_y(
                s_work->y_underlay_prev[x], old_underlay_adjacent);
            if (underlay_top < old_top) old_top = underlay_top;
        }
        if (s_work->y_underlay[x] >= 0) {
            int underlay_top = minimum_visible_y(
                s_work->y_underlay[x], new_underlay_adjacent);
            if (underlay_top < new_top) new_top = underlay_top;
        }
        int y0 = old_top < new_top ? old_top : new_top;
        restore_column(x, y0 - 2);

        draw_fill_column(x, s_work->y_curve[x], accent);
        draw_trace_segment(x, new_adjacent, s_work->y_curve[x],
                           line, accent, 118);
        draw_trace_segment(x, new_underlay_adjacent,
                           s_work->y_underlay[x], compare, compare, 72);

        if (x < dirty_x0) dirty_x0 = x;
        if (x > dirty_x1) dirty_x1 = x;
        if (y0 - 2 < dirty_y0) dirty_y0 = y0 - 2;
    }

    for (int x = 0; x < PW; x++) {
        s_work->y_prev[x] = s_work->y_curve[x];
        s_work->y_underlay_prev[x] = s_work->y_underlay[x];
    }

    if (dirty_x1 >= dirty_x0) {
        if (dirty_y0 < 0) dirty_y0 = 0;
        lv_area_t dirty = {
            .x1 = PX + dirty_x0,
            .y1 = PY + dirty_y0,
            .x2 = PX + dirty_x1,
            .y2 = PY + PH - 1,
        };
        lv_obj_invalidate_area(s_canvas, &dirty);
        log_curve_fps(dirty_x1 - dirty_x0 + 1, PH - dirty_y0);
    } else {
        log_curve_fps(0, 0);
    }
    s_frame_valid = 1;
}

static void curve_destroy(void)
{
    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
        s_canvas = NULL;
        s_title_label = NULL;
        s_profile_label = NULL;
    }
    if (s_buf) {
        heap_caps_free(s_buf);
        s_buf = NULL;
    }
    if (s_static_buf) {
        heap_caps_free(s_static_buf);
        s_static_buf = NULL;
    }
    if (s_work) {
        heap_caps_free(s_work);
        s_work = NULL;
    }
    s_stride = 0;
    s_frame_valid = 0;
}

const renderer_t RENDERER_CURVE = {
    "curve", curve_create, curve_update, curve_destroy
};
