/* ============================================================================
 *  renderer_curve.c  —  stride-aware RGB565 PSRAM canvas renderer
 * ========================================================================== */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "renderer.h"

#define SCR_W 480
#define TOP   22
#define BOT   18
#define LEFT  30
#define RIGHT 6
#define PX    LEFT
#define PY    TOP
#define PW    (SCR_W-LEFT-RIGHT)
#define PH    (320-TOP-BOT)
#define F_LO  20.0f
#define F_HI  20000.0f
#define RGB565(r,g,b) (uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))
#define C565(h) RGB565(((h)>>16)&0xFF,((h)>>8)&0xFF,(h)&0xFF)

static const float GRID_FREQ[] = {
    20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000
};
#define GRID_COUNT ((int)(sizeof(GRID_FREQ)/sizeof(GRID_FREQ[0])))

static const char *TAG = "curve";
static lv_obj_t *s_root;
static lv_obj_t *s_canvas;
static void *s_buf;
static void *s_static_buf;
static uint32_t s_stride;
static viz_theme_t s_theme;
static int s_y_curve[PW];
static int s_y_prev[PW];
static int s_peak_prev[PW];
static uint16_t s_fill_prev[PW];
static int s_frame_valid;
static int64_t s_fps_start_us;
static int s_fps_frames;

static inline int grid_x(float frequency)
{
    return (int)((logf(frequency/F_LO)/logf(F_HI/F_LO))*(PW-1)+0.5f);
}

static uint16_t fill_color(float value)
{
    return value < 0.60f ? C565(s_theme.lo)
         : value < 0.85f ? C565(s_theme.mid) : C565(s_theme.hi);
}

static inline uint16_t *pixel_at(void *buffer, uint32_t stride, int x, int y)
{
    return (uint16_t *)((uint8_t *)buffer + y * stride) + x;
}

static float sample_column(const float *values, int n, float step, int x)
{
    float position = x * step;
    int i = (int)position;
    float fraction = position - i;
    int j = i + 1 < n ? i + 1 : i;
    float value = values[i] * (1.0f - fraction) + values[j] * fraction;
    if (value < 0.0f) {
        value = 0.0f;
    } else if (value > 1.0f) {
        value = 1.0f;
    }
    return value;
}

static int value_to_y(float value)
{
    int y = (int)((1.0f - value) * (PH - 1) + 0.5f);
    if (y < 0) {
        y = 0;
    } else if (y >= PH) {
        y = PH - 1;
    }
    return y;
}

static void draw_static_canvas(void)
{
    uint16_t bg = C565(s_theme.bg);
    uint16_t grid = C565(s_theme.grid);

    for (int y = 0; y < PH; y++) {
        uint16_t *canvas_row = pixel_at(s_buf, s_stride, 0, y);
        uint16_t *static_row = pixel_at(s_static_buf, s_stride, 0, y);
        for (int x = 0; x < PW; x++) {
            canvas_row[x] = bg;
            static_row[x] = bg;
        }
    }

    if (s_theme.show_grid) {
        for (int g = 0; g < GRID_COUNT; g++) {
            int x = grid_x(GRID_FREQ[g]);
            if (x < 0) {
                x = 0;
            } else if (x >= PW) {
                x = PW - 1;
            }
            for (int y = 0; y < PH; y++) {
                *pixel_at(s_buf, s_stride, x, y) = grid;
                *pixel_at(s_static_buf, s_stride, x, y) = grid;
            }
        }
    }
}

static void restore_column(int x, int y0, int y1)
{
    if (y0 < 0) {
        y0 = 0;
    }
    if (y1 >= PH) {
        y1 = PH - 1;
    }
    for (int y = y0; y <= y1; y++) {
        *pixel_at(s_buf, s_stride, x, y) =
            *pixel_at(s_static_buf, s_stride, x, y);
    }
}

static void draw_dynamic_column(int x, int curve_y, int peak_y, uint16_t fill,
                                uint16_t line, uint16_t peak)
{
    for (int y = PH - 1; y >= curve_y; y--) {
        *pixel_at(s_buf, s_stride, x, y) = fill;
    }
    *pixel_at(s_buf, s_stride, x, curve_y) = line;
    if (peak_y >= 0 && peak_y < PH) {
        *pixel_at(s_buf, s_stride, x, peak_y) = peak;
    }
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
    s_fps_start_us = 0;
    s_fps_frames = 0;
    for (int x = 0; x < PW; x++) {
        s_y_prev[x] = -1;
        s_peak_prev[x] = -1;
        s_fill_prev[x] = 0;
    }

    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, SCR_W, 320);
    lv_obj_set_pos(s_root, 0, 0);
    rectangle_style(s_root, theme->bg);

    if (theme->show_axis) {
        lv_obj_t *title = lv_label_create(s_root);
        lv_label_set_text(title, "MONITOR");
        lv_obj_set_style_text_color(title, lv_color_hex(theme->accent), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
        lv_obj_set_pos(title, PX, 3);
    }

    size_t bytes = LV_CANVAS_BUF_SIZE(PW, PH, 16, LV_DRAW_BUF_STRIDE_ALIGN);
    s_buf = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_buf) {
        ESP_LOGE(TAG, "PSRAM canvas allocation failed (%u bytes)", (unsigned)bytes);
        s_canvas = NULL;
        return;
    }
    s_static_buf = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_static_buf) {
        ESP_LOGE(TAG, "PSRAM static canvas allocation failed (%u bytes)",
                 (unsigned)bytes);
        heap_caps_free(s_buf);
        s_buf = NULL;
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
        return;
    }
    s_stride = draw_buf->header.stride;
    draw_static_canvas();
    lv_obj_invalidate(s_canvas);
}

static void curve_update(const viz_frame_t *frame)
{
    if (!s_canvas || !s_buf || !s_static_buf || !frame ||
        !frame->bars || frame->n < 2 || s_stride == 0) {
        return;
    }

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

    uint16_t line = C565(s_theme.line);
    uint16_t peak = C565(s_theme.peak);

    float step = (float)(frame->n - 1) / (float)(PW - 1);
    int dirty_x0 = PW;
    int dirty_y0 = PH;
    int dirty_x1 = -1;
    int dirty_y1 = -1;

    for (int x = 0; x < PW; x++) {
        float value = sample_column(frame->bars, frame->n, step, x);
        int curve_y = value_to_y(value);
        s_y_curve[x] = curve_y;
        uint16_t color = fill_color(value);

        int peak_y = -1;
        if (frame->peaks) {
            float peak_value = sample_column(frame->peaks, frame->n, step, x);
            if (peak_value > 0.001f) {
                peak_y = value_to_y(peak_value);
            }
        }

        int changed = !s_frame_valid ||
                      s_y_prev[x] != curve_y ||
                      s_peak_prev[x] != peak_y ||
                      s_fill_prev[x] != color;
        if (!changed) {
            continue;
        }

        int y0 = s_frame_valid ? s_y_prev[x] : 0;
        if (curve_y < y0) {
            y0 = curve_y;
        }
        if (s_frame_valid && s_peak_prev[x] >= 0 && s_peak_prev[x] < y0) {
            y0 = s_peak_prev[x];
        }
        if (peak_y >= 0 && peak_y < y0) {
            y0 = peak_y;
        }

        restore_column(x, y0, PH - 1);
        draw_dynamic_column(x, curve_y, peak_y, color, line, peak);

        if (x < dirty_x0) {
            dirty_x0 = x;
        }
        if (x > dirty_x1) {
            dirty_x1 = x;
        }
        if (y0 < dirty_y0) {
            dirty_y0 = y0;
        }
        dirty_y1 = PH - 1;

        s_y_prev[x] = curve_y;
        s_peak_prev[x] = peak_y;
        s_fill_prev[x] = color;
    }

    if (dirty_x1 >= dirty_x0 && dirty_y1 >= dirty_y0) {
        lv_area_t dirty = {
            .x1 = PX + dirty_x0,
            .y1 = PY + dirty_y0,
            .x2 = PX + dirty_x1,
            .y2 = PY + dirty_y1,
        };
        lv_obj_invalidate_area(s_canvas, &dirty);
        log_curve_fps(dirty_x1 - dirty_x0 + 1, dirty_y1 - dirty_y0 + 1);
    } else {
        log_curve_fps(0, 0);
    }
    s_frame_valid = 1;
}

static void curve_destroy(void)
{
    if (s_root) {
        lv_obj_delete(s_root);  /* canvas stops referencing its external buffer */
        s_root = NULL;
        s_canvas = NULL;
    }
    if (s_buf) {
        heap_caps_free(s_buf);
        s_buf = NULL;
    }
    if (s_static_buf) {
        heap_caps_free(s_static_buf);
        s_static_buf = NULL;
    }
    s_stride = 0;
    s_frame_valid = 0;
}

const renderer_t RENDERER_CURVE = {
    "curve", curve_create, curve_update, curve_destroy
};
