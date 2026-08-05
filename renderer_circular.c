/* ============================================================================
 * renderer_circular.c - mirrored radial spectrum for music backgrounds
 * ========================================================================== */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "renderer.h"

#define SCR_W 480
#define SCR_H 320
#define CANVAS_W 240
#define CANVAS_H 240
#define CANVAS_X ((SCR_W - CANVAS_W) / 2)
#define CANVAS_Y ((SCR_H - CANVAS_H) / 2)
#define CENTER_X (CANVAS_W / 2)
#define CENTER_Y (CANVAS_H / 2)
#define MIRROR_RADIUS_MAX (CANVAS_W / 2 - 1)
#define SEGMENT_COUNT 72
#define HALF_SEGMENTS (SEGMENT_COUNT / 2)
#define MIRROR_POINT_COUNT (HALF_SEGMENTS + 1)
#define INNER_RADIUS 57.0f
#define MIN_BAR_LENGTH 7.0f
#define MAX_BAR_LENGTH 45.0f
#define BAR_START_RADIUS (INNER_RADIUS + 5.0f)
#define DRAW_PADDING 4
#define CIRCULAR_FRAME_US 60000
#define REDRAW_EPSILON 0.002f
#define PI_F 3.14159265358979323846f
#define RGB565(r,g,b) \
    (uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))
#define C565(h) \
    RGB565(((h)>>16)&0xFF,((h)>>8)&0xFF,(h)&0xFF)

typedef struct {
    float sine[SEGMENT_COUNT];
    float cosine[SEGMENT_COUNT];
    float smooth[MIRROR_POINT_COUNT];
    float target[MIRROR_POINT_COUNT];
    int sample_first[MIRROR_POINT_COUNT];
    float sample_fraction[MIRROR_POINT_COUNT];
    int sample_count;
} circular_work_t;

static const char *TAG = "circular";
static lv_obj_t *s_root;
static lv_obj_t *s_canvas;
static lv_obj_t *s_title;
static lv_obj_t *s_subtitle;
static void *s_buffer;
static circular_work_t *s_work;
static uint32_t s_stride;
static viz_theme_t s_theme;
static int64_t s_last_draw_us;
static int64_t s_fps_start_us;
static int s_fps_frames;
static int s_previous_radius;
static uint16_t s_background_color;
static uint16_t s_grid_color;
static uint16_t s_core_color;
static uint16_t s_glow_color;
static uint16_t s_line_color;

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

static inline uint16_t *pixel_at(int x, int y)
{
    return (uint16_t *)((uint8_t *)s_buffer + y * s_stride) + x;
}

static inline void put_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) return;
    *pixel_at(x, y) = color;
}

static void draw_line_thin(int x0, int y0, int x1, int y1,
                           uint16_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static void draw_line(int x0, int y0, int x1, int y1,
                      uint16_t color, int thickness)
{
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    const int offset_x = dx < dy ? 1 : 0;
    const int offset_y = dx < dy ? 0 : 1;
    const int radius = thickness / 2;

    for (int offset = -radius; offset <= radius; offset++) {
        draw_line_thin(x0 + offset * offset_x,
                       y0 + offset * offset_y,
                       x1 + offset * offset_x,
                       y1 + offset * offset_y, color);
    }
}

static void draw_ring(float radius, uint16_t color, int thickness)
{
    int previous_x = CENTER_X +
        (int)(s_work->cosine[SEGMENT_COUNT - 1] * radius + 0.5f);
    int previous_y = CENTER_Y +
        (int)(s_work->sine[SEGMENT_COUNT - 1] * radius + 0.5f);

    for (int i = 0; i < SEGMENT_COUNT; i++) {
        int x = CENTER_X + (int)(s_work->cosine[i] * radius + 0.5f);
        int y = CENTER_Y + (int)(s_work->sine[i] * radius + 0.5f);
        draw_line(previous_x, previous_y, x, y, color, thickness);
        previous_x = x;
        previous_y = y;
    }
}

static void clear_canvas_radius(int radius)
{
    if (radius < 0 || radius > CENTER_X) radius = CENTER_X;
    int x0 = CENTER_X - radius;
    int x1 = CENTER_X + radius;
    int y0 = CENTER_Y - radius;
    int y1 = CENTER_Y + radius;
    if (x0 < 0) x0 = 0;
    if (x1 >= CANVAS_W) x1 = CANVAS_W - 1;
    if (y0 < 0) y0 = 0;
    if (y1 >= CANVAS_H) y1 = CANVAS_H - 1;

    const uint32_t pair = (uint32_t)s_background_color |
                          ((uint32_t)s_background_color << 16);
    for (int y = y0; y <= y1; y++) {
        uint16_t *row = pixel_at(x0, y);
        int count = x1 - x0 + 1;
        if (((uintptr_t)row & 0x3U) != 0 && count > 0) {
            *row++ = s_background_color;
            count--;
        }
        uint32_t *pairs = (uint32_t *)row;
        while (count >= 2) {
            *pairs++ = pair;
            count -= 2;
        }
        if (count > 0) *(uint16_t *)pairs = s_background_color;
    }
}

static void mirror_canvas_radius(int radius)
{
    if (radius < 0 || radius > MIRROR_RADIUS_MAX) {
        radius = MIRROR_RADIUS_MAX;
    }
    int y0 = CENTER_Y - radius;
    int y1 = CENTER_Y + radius;
    if (y0 < 0) y0 = 0;
    if (y1 >= CANVAS_H) y1 = CANVAS_H - 1;

    for (int y = y0; y <= y1; y++) {
        for (int source_x = CENTER_X;
             source_x <= CENTER_X + radius;
             source_x++) {
            const int mirror_x = CANVAS_W - 1 - source_x;
            *pixel_at(mirror_x, y) = *pixel_at(source_x, y);
        }
    }
}

static int mirror_point_for_segment(int segment)
{
    return segment <= HALF_SEGMENTS
        ? segment : SEGMENT_COUNT - segment;
}

static float spectrum_position_for_point(int point)
{
    return 1.0f - (float)point / (float)HALF_SEGMENTS;
}

static void prepare_sample_map(int count)
{
    if (s_work->sample_count == count) return;
    for (int point = 0; point < MIRROR_POINT_COUNT; point++) {
        const float position =
            spectrum_position_for_point(point) * (float)(count - 1);
        const int first = (int)position;
        s_work->sample_first[point] = first;
        s_work->sample_fraction[point] = position - (float)first;
    }
    s_work->sample_count = count;
}

static bool prepare_targets(const viz_frame_t *frame)
{
    prepare_sample_map(frame->n);
    bool changed = s_last_draw_us == 0;
    for (int point = 0; point < MIRROR_POINT_COUNT; point++) {
        const int first = s_work->sample_first[point];
        const int second = first + 1 < frame->n ? first + 1 : first;
        const float fraction = s_work->sample_fraction[point];
        const float target = clamp_unit(
            frame->bars[first] * (1.0f - fraction) +
            frame->bars[second] * fraction);
        s_work->target[point] = target;
        if (fabsf(target - s_work->smooth[point]) > REDRAW_EPSILON) {
            changed = true;
        }
    }
    return changed;
}

static int draw_frame(void)
{
    const int minimum_radius = (int)(INNER_RADIUS + DRAW_PADDING + 0.5f);
    clear_canvas_radius(s_previous_radius > minimum_radius
        ? s_previous_radius : minimum_radius);
    draw_ring(INNER_RADIUS - 12.0f, s_grid_color, 1);
    draw_ring(INNER_RADIUS - 3.0f, s_core_color, 3);
    draw_ring(INNER_RADIUS + 2.0f, s_grid_color, 1);

    int maximum_length = (int)MIN_BAR_LENGTH;
    for (int point = 0; point < MIRROR_POINT_COUNT; point++) {
        const float target = s_work->target[point];
        const float alpha = target > s_work->smooth[point] ? 0.58f : 0.16f;
        s_work->smooth[point] +=
            (target - s_work->smooth[point]) * alpha;
    }

    for (int i = 0; i < SEGMENT_COUNT; i++) {
        const int point = mirror_point_for_segment(i);
        const float length = MIN_BAR_LENGTH + s_work->smooth[point] *
                             (MAX_BAR_LENGTH - MIN_BAR_LENGTH);
        const int integer_length = (int)(length + 0.5f);
        if (integer_length > maximum_length) maximum_length = integer_length;
        const float r0 = BAR_START_RADIUS;
        const float r1 = r0 + (float)integer_length;
        int x0 = CENTER_X + (int)(s_work->cosine[i] * r0 + 0.5f);
        int y0 = CENTER_Y + (int)(s_work->sine[i] * r0 + 0.5f);
        int x1 = CENTER_X + (int)(s_work->cosine[i] * r1 + 0.5f);
        int y1 = CENTER_Y + (int)(s_work->sine[i] * r1 + 0.5f);

        draw_line(x0, y0, x1, y1, s_glow_color, 5);
        draw_line(x0, y0, x1, y1, s_line_color, 3);
    }
    return (int)(BAR_START_RADIUS + (float)maximum_length +
                 DRAW_PADDING + 0.5f);
}

static void invalidate_canvas_radius(int radius)
{
    lv_area_t canvas_area;
    lv_obj_get_coords(s_canvas, &canvas_area);
    lv_area_t dirty = {
        .x1 = canvas_area.x1 + CENTER_X - radius - 1,
        .y1 = canvas_area.y1 + CENTER_Y - radius,
        .x2 = canvas_area.x1 + CENTER_X + radius,
        .y2 = canvas_area.y1 + CENTER_Y + radius,
    };
    lv_obj_invalidate_area(s_canvas, &dirty);
}

static void log_fps(int dirty_radius)
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
        ESP_LOGI(TAG, "fps=%d dirty=%dx%d", fps,
                 dirty_radius * 2 + 1, dirty_radius * 2 + 1);
        s_fps_start_us = now;
        s_fps_frames = 0;
    }
}

static void style_root(lv_obj_t *obj, uint32_t color)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
}

static lv_obj_t *make_center_label(lv_obj_t *parent, const char *text,
                                   const lv_font_t *font, uint32_t color,
                                   int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, 150);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, (SCR_W - 150) / 2, y);
    return label;
}

static void circular_create(lv_obj_t *parent, const viz_theme_t *theme)
{
    s_theme = *theme;
    s_stride = 0;
    s_last_draw_us = 0;
    s_fps_start_us = 0;
    s_fps_frames = 0;
    s_previous_radius = 0;
    s_background_color = C565(s_theme.bg);
    s_grid_color = C565(mix_hex(s_theme.bg, s_theme.grid, 148));
    s_core_color = C565(mix_hex(s_theme.bg, s_theme.accent, 150));
    s_glow_color = C565(mix_hex(s_theme.bg, s_theme.accent, 90));
    s_line_color = C565(s_theme.line);

    s_work = heap_caps_calloc(1, sizeof(*s_work),
                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_work) {
        ESP_LOGE(TAG, "PSRAM work buffer allocation failed (%u bytes)",
                 (unsigned)sizeof(*s_work));
        return;
    }
    for (int i = 0; i < SEGMENT_COUNT; i++) {
        float angle = -0.5f * PI_F +
                      2.0f * PI_F * (float)i / (float)SEGMENT_COUNT;
        s_work->cosine[i] = cosf(angle);
        s_work->sine[i] = sinf(angle);
    }

    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, SCR_W, SCR_H);
    lv_obj_set_pos(s_root, 0, 0);
    style_root(s_root, theme->bg);

    size_t bytes = LV_CANVAS_BUF_SIZE(
        CANVAS_W, CANVAS_H, 16, LV_DRAW_BUF_STRIDE_ALIGN);
    s_buffer = heap_caps_calloc(1, bytes,
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_buffer) {
        ESP_LOGE(TAG, "PSRAM canvas allocation failed (%u bytes)",
                 (unsigned)bytes);
        heap_caps_free(s_work);
        s_work = NULL;
        return;
    }

    s_canvas = lv_canvas_create(s_root);
    if (!s_canvas) {
        ESP_LOGE(TAG, "canvas object creation failed");
        heap_caps_free(s_buffer);
        s_buffer = NULL;
        heap_caps_free(s_work);
        s_work = NULL;
        return;
    }
    lv_canvas_set_buffer(s_canvas, s_buffer, CANVAS_W, CANVAS_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(s_canvas, CANVAS_X, CANVAS_Y);
    lv_draw_buf_t *draw_buffer = lv_canvas_get_draw_buf(s_canvas);
    if (!draw_buffer || !draw_buffer->data) {
        ESP_LOGE(TAG, "canvas draw buffer unavailable");
        lv_obj_delete(s_canvas);
        s_canvas = NULL;
        heap_caps_free(s_buffer);
        s_buffer = NULL;
        heap_caps_free(s_work);
        s_work = NULL;
        return;
    }
    s_stride = draw_buffer->header.stride;
    clear_canvas_radius(CENTER_X);
    draw_ring(INNER_RADIUS - 3.0f, C565(s_theme.grid), 1);
    mirror_canvas_radius(MIRROR_RADIUS_MAX);

    s_title = make_center_label(s_root, "SOUND",
                                &lv_font_montserrat_14,
                                theme->line, 143);
    s_subtitle = make_center_label(s_root, "SPECTRUM",
                                   &lv_font_montserrat_12,
                                   theme->accent, 163);
    lv_obj_invalidate(s_canvas);
}

static void circular_update(const viz_frame_t *frame)
{
    if (!s_canvas || !s_buffer || !s_work || s_stride == 0 ||
        !frame || !frame->bars || frame->n < 2) {
        return;
    }

    int64_t now = esp_timer_get_time();
    if (s_last_draw_us != 0 &&
        now - s_last_draw_us < CIRCULAR_FRAME_US) {
        return;
    }
    if (!prepare_targets(frame)) {
        s_last_draw_us = now;
        return;
    }
    s_last_draw_us = now;

    const int current_radius = draw_frame();
    const int dirty_radius = current_radius > s_previous_radius
        ? current_radius : s_previous_radius;
    mirror_canvas_radius(dirty_radius);
    s_previous_radius = current_radius;
    invalidate_canvas_radius(dirty_radius);
    renderer_perf_note_redraw();
    log_fps(dirty_radius);
}

static void circular_destroy(void)
{
    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
        s_canvas = NULL;
        s_title = NULL;
        s_subtitle = NULL;
    }
    if (s_buffer) {
        heap_caps_free(s_buffer);
        s_buffer = NULL;
    }
    if (s_work) {
        heap_caps_free(s_work);
        s_work = NULL;
    }
    s_stride = 0;
    s_previous_radius = 0;
}

#ifdef PEDAL_SIM
float renderer_circular_debug_position(int segment)
{
    if (segment < 0 || segment >= SEGMENT_COUNT) return -1.0f;
    return spectrum_position_for_point(mirror_point_for_segment(segment));
}

uint32_t renderer_circular_debug_mirror_mismatches(void)
{
    if (!s_buffer || s_stride == 0) return UINT32_MAX;
    uint32_t mismatches = 0;
    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = 0; x < CANVAS_W / 2; x++) {
            if (*pixel_at(x, y) != *pixel_at(CANVAS_W - 1 - x, y)) {
                mismatches++;
            }
        }
    }
    return mismatches;
}
#endif

const renderer_t RENDERER_CIRCULAR = {
    "circular", circular_create, circular_update, circular_destroy
};
