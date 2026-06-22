/* ============================================================================
 *  renderer_curve.c  —  stride-aware RGB565 PSRAM canvas renderer
 * ========================================================================== */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
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
static viz_theme_t s_theme;
static int s_y_curve[PW];

static inline int grid_x(float frequency)
{
    return (int)((logf(frequency/F_LO)/logf(F_HI/F_LO))*(PW-1)+0.5f);
}

static uint16_t fill_color(float value)
{
    return value < 0.60f ? C565(s_theme.lo)
         : value < 0.85f ? C565(s_theme.mid) : C565(s_theme.hi);
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

    s_canvas = lv_canvas_create(s_root);
    lv_canvas_set_buffer(s_canvas, s_buf, PW, PH, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(s_canvas, PX, PY);
}

static void curve_update(const viz_frame_t *frame)
{
    if (!s_canvas || !s_buf || !frame || !frame->bars || frame->n < 2) return;

    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(s_canvas);
    if (!draw_buf || !draw_buf->data) return;
    uint8_t *data = draw_buf->data;
    uint32_t stride = draw_buf->header.stride;

    uint16_t bg = C565(s_theme.bg);
    uint16_t grid = C565(s_theme.grid);
    uint16_t line = C565(s_theme.line);
    uint16_t peak = C565(s_theme.peak);

    for (int y = 0; y < PH; y++) {
        uint16_t *row = (uint16_t *)(data + y * stride);
        for (int x = 0; x < PW; x++) row[x] = bg;
    }

    float step = (float)(frame->n - 1) / (float)(PW - 1);
    for (int x = 0; x < PW; x++) {
        float position = x * step;
        int i = (int)position;
        float fraction = position - i;
        int j = i + 1 < frame->n ? i + 1 : i;
        float value = frame->bars[i] * (1.0f - fraction) +
                      frame->bars[j] * fraction;
        if (value < 0.0f) value = 0.0f;
        else if (value > 1.0f) value = 1.0f;

        int curve_y = (int)((1.0f - value) * (PH - 1) + 0.5f);
        s_y_curve[x] = curve_y;
        uint16_t color = fill_color(value);
        for (int y = PH - 1; y >= curve_y; y--) {
            ((uint16_t *)(data + y * stride))[x] = color;
        }
    }

    if (s_theme.show_grid) {
        for (int g = 0; g < GRID_COUNT; g++) {
            int x = grid_x(GRID_FREQ[g]);
            if (x < 0) x = 0;
            if (x >= PW) x = PW - 1;
            for (int y = 0; y < PH; y++) {
                ((uint16_t *)(data + y * stride))[x] = grid;
            }
        }
    }

    for (int x = 0; x < PW; x++) {
        ((uint16_t *)(data + s_y_curve[x] * stride))[x] = line;
        if (frame->peaks) {
            float position = x * step;
            int i = (int)position;
            float fraction = position - i;
            int j = i + 1 < frame->n ? i + 1 : i;
            float value = frame->peaks[i] * (1.0f - fraction) +
                          frame->peaks[j] * fraction;
            if (value > 1.0f) value = 1.0f;
            if (value > 0.001f) {
                int y = (int)((1.0f - value) * (PH - 1) + 0.5f);
                if (y >= 0 && y < PH) ((uint16_t *)(data + y * stride))[x] = peak;
            }
        }
    }
    lv_obj_invalidate(s_canvas);
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
}

const renderer_t RENDERER_CURVE = {
    "curve", curve_create, curve_update, curve_destroy
};
