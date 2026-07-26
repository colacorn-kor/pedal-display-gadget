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
#define CANVAS_W 292
#define CANVAS_H 292
#define CANVAS_X ((SCR_W - CANVAS_W) / 2)
#define CANVAS_Y ((SCR_H - CANVAS_H) / 2)
#define CENTER_X (CANVAS_W / 2)
#define CENTER_Y (CANVAS_H / 2)
#define SEGMENT_COUNT 96
#define HALF_SEGMENTS (SEGMENT_COUNT / 2)
#define INNER_RADIUS 74.0f
#define MIN_BAR_LENGTH 8.0f
#define MAX_BAR_LENGTH 60.0f
#define CIRCULAR_FRAME_US 40000
#define PI_F 3.14159265358979323846f
#define RGB565(r,g,b) \
    (uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))
#define C565(h) \
    RGB565(((h)>>16)&0xFF,((h)>>8)&0xFF,(h)&0xFF)

typedef struct {
    float sine[SEGMENT_COUNT];
    float cosine[SEGMENT_COUNT];
    float smooth[SEGMENT_COUNT];
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

static void put_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) return;
    *pixel_at(x, y) = color;
}

static void draw_line(int x0, int y0, int x1, int y1,
                      uint16_t color, int thickness)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    int radius = thickness / 2;

    for (;;) {
        for (int oy = -radius; oy <= radius; oy++) {
            for (int ox = -radius; ox <= radius; ox++) {
                if (ox * ox + oy * oy <= radius * radius + 1) {
                    put_pixel(x0 + ox, y0 + oy, color);
                }
            }
        }
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

static void clear_canvas(void)
{
    uint16_t background = C565(s_theme.bg);
    for (int y = 0; y < CANVAS_H; y++) {
        uint16_t *row = pixel_at(0, y);
        for (int x = 0; x < CANVAS_W; x++) row[x] = background;
    }
}

static float sample_spectrum(const viz_frame_t *frame, int segment)
{
    int mirrored = segment < HALF_SEGMENTS
        ? segment : SEGMENT_COUNT - 1 - segment;
    float normalized = (float)mirrored / (float)(HALF_SEGMENTS - 1);
    float position = normalized * (frame->n - 1);
    int first = (int)position;
    int second = first + 1 < frame->n ? first + 1 : first;
    float fraction = position - first;
    return clamp_unit(frame->bars[first] * (1.0f - fraction) +
                      frame->bars[second] * fraction);
}

static void draw_frame(const viz_frame_t *frame)
{
    uint16_t grid = C565(mix_hex(s_theme.bg, s_theme.grid, 148));
    uint16_t core = C565(mix_hex(s_theme.bg, s_theme.accent, 150));
    uint16_t glow = C565(mix_hex(s_theme.bg, s_theme.accent, 90));
    uint16_t line = C565(s_theme.line);
    float pulse = clamp_unit(frame->level);
    float inner = INNER_RADIUS + pulse * 2.5f;

    clear_canvas();
    draw_ring(inner - 12.0f, grid, 1);
    draw_ring(inner - 3.0f, core, 2);
    draw_ring(inner + 2.0f, grid, 1);

    for (int i = 0; i < SEGMENT_COUNT; i++) {
        float target = sample_spectrum(frame, i);
        float alpha = target > s_work->smooth[i] ? 0.58f : 0.16f;
        s_work->smooth[i] += (target - s_work->smooth[i]) * alpha;
        float length = MIN_BAR_LENGTH +
                       s_work->smooth[i] * (MAX_BAR_LENGTH - MIN_BAR_LENGTH);
        float r0 = inner + 5.0f;
        float r1 = r0 + length;
        int x0 = CENTER_X + (int)(s_work->cosine[i] * r0 + 0.5f);
        int y0 = CENTER_Y + (int)(s_work->sine[i] * r0 + 0.5f);
        int x1 = CENTER_X + (int)(s_work->cosine[i] * r1 + 0.5f);
        int y1 = CENTER_Y + (int)(s_work->sine[i] * r1 + 0.5f);

        draw_line(x0, y0, x1, y1, glow, 5);
        draw_line(x0, y0, x1, y1, line, 2);
    }
}

static void log_fps(void)
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
        ESP_LOGI(TAG, "fps=%d dirty=%dx%d", fps, CANVAS_W, CANVAS_H);
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
    clear_canvas();
    draw_ring(INNER_RADIUS - 3.0f, C565(s_theme.grid), 1);

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
    s_last_draw_us = now;

    draw_frame(frame);
    lv_obj_invalidate(s_canvas);
    log_fps();
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
}

const renderer_t RENDERER_CIRCULAR = {
    "circular", circular_create, circular_update, circular_destroy
};
