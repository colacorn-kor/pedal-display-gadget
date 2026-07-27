#include "gadget_app.h"

#include "app_slots.h"
#include "platform.h"
#include "renderer.h"
#include "theme.h"

typedef enum {
    MONITOR_MODE_CURVE = 0,
    MONITOR_MODE_12_BAND,
    MONITOR_MODE_CIRCULAR,
    MONITOR_MODE_REFERENCE,
    MONITOR_MODE_COUNT,
} monitor_mode_t;

static const char *const MODE_NAMES[MONITOR_MODE_COUNT] = {
    "Curve",
    "12-Band",
    "Circular",
    "Reference",
};

static const char *const MODE_RENDERERS[MONITOR_MODE_COUNT] = {
    "curve",
    "bars",
    "circular",
    "curve",
};

#define MONITOR_OPTIONS_VALID          0x80u
#define MONITOR_OPTIONS_TILT_MASK      0x07u
#define MONITOR_OPTIONS_SMOOTH_SHIFT   3u
#define MONITOR_OPTIONS_SMOOTH_MASK    0x03u
#define MONITOR_DEFAULT_TILT_INDEX     3
#define MONITOR_DEFAULT_SMOOTH_INDEX   1

static const int CURVE_TILT_TENTHS[] = { 0, 15, 30, 45, 60 };
#define CURVE_TILT_COUNT \
    ((int)(sizeof(CURVE_TILT_TENTHS) / sizeof(CURVE_TILT_TENTHS[0])))
#define CURVE_SMOOTH_COUNT 3

static monitor_mode_t s_mode = MONITOR_MODE_CURVE;
static int s_curve_tilt = MONITOR_DEFAULT_TILT_INDEX;
static int s_curve_smoothing = MONITOR_DEFAULT_SMOOTH_INDEX;
static int s_renderer = -1;
static lv_obj_t *s_host;
static viz_theme_t s_viz_theme;
static audio_viz_snapshot_t s_viz_snapshot;

static void monitor_load_options(void)
{
    const uint8_t options = app_slots_options(&APP_MONITOR);
    if (!(options & MONITOR_OPTIONS_VALID)) {
        s_curve_tilt = MONITOR_DEFAULT_TILT_INDEX;
        s_curve_smoothing = MONITOR_DEFAULT_SMOOTH_INDEX;
        return;
    }

    s_curve_tilt = options & MONITOR_OPTIONS_TILT_MASK;
    s_curve_smoothing =
        (options >> MONITOR_OPTIONS_SMOOTH_SHIFT) &
        MONITOR_OPTIONS_SMOOTH_MASK;
    if (s_curve_tilt >= CURVE_TILT_COUNT) {
        s_curve_tilt = MONITOR_DEFAULT_TILT_INDEX;
    }
    if (s_curve_smoothing >= CURVE_SMOOTH_COUNT) {
        s_curve_smoothing = MONITOR_DEFAULT_SMOOTH_INDEX;
    }
}

static void monitor_save_options(void)
{
    const uint8_t options = MONITOR_OPTIONS_VALID |
        (uint8_t)s_curve_tilt |
        (uint8_t)(s_curve_smoothing << MONITOR_OPTIONS_SMOOTH_SHIFT);
    app_slots_set_options(&APP_MONITOR, options);
}

static uint32_t color_hex(lv_color_t color)
{
    return lv_color_to_u32(color) & 0xffffffU;
}

static uint32_t mix_hex(uint32_t base, uint32_t overlay, uint8_t opacity)
{
    const uint32_t inverse = 255U - opacity;
    const uint32_t red = ((((base >> 16) & 0xffU) * inverse) +
                          (((overlay >> 16) & 0xffU) * opacity) + 127U) /
                         255U;
    const uint32_t green = ((((base >> 8) & 0xffU) * inverse) +
                            (((overlay >> 8) & 0xffU) * opacity) + 127U) /
                           255U;
    const uint32_t blue = (((base & 0xffU) * inverse) +
                           ((overlay & 0xffU) * opacity) + 127U) /
                          255U;
    return (red << 16) | (green << 8) | blue;
}

static void build_viz_theme(void)
{
    const ui_theme_t *ui =
        theme_for_app_color(app_slots_color(&APP_MONITOR));
    const uint32_t bg = color_hex(ui->bg);
    const uint32_t accent = color_hex(ui->accent);
    const uint32_t accent2 = color_hex(ui->accent2);

    s_viz_theme = (viz_theme_t) {
        .bg = bg,
        .grid = color_hex(ui->grid),
        .accent = accent,
        .line = color_hex(ui->text),
        .peak = accent2,
        .lo = mix_hex(bg, accent, 70),
        .mid = mix_hex(bg, accent2, 84),
        .hi = mix_hex(bg, accent2, 150),
        .show_grid = 1,
        .show_axis = 1,
    };
}

static void monitor_select_renderer(void)
{
    if (!s_host) return;

    const bool reference = s_mode == MONITOR_MODE_REFERENCE;
    const int tilt_tenths = reference
        ? 0
        : (s_mode == MONITOR_MODE_CURVE
            ? CURVE_TILT_TENTHS[s_curve_tilt]
            : (s_mode == MONITOR_MODE_CIRCULAR ? 30 : 45));
    renderer_curve_configure(
        reference ? CURVE_DISPLAY_REFERENCE : CURVE_DISPLAY_VISUAL,
        tilt_tenths,
        reference ? 0 : s_curve_smoothing);

    s_renderer = renderer_find(MODE_RENDERERS[s_mode]);
    if (s_renderer < 0) return;
    build_viz_theme();
    renderer_select(s_renderer, s_host, &s_viz_theme);
    audio_set_viz_mode(
        s_mode == MONITOR_MODE_CIRCULAR ? VIZ_DECOR : VIZ_MONITOR);
    audio_set_viz_tilt_tenths(tilt_tenths);
}

void monitor_app_set_scene(int theme, int renderer)
{
    const renderer_t *selected = renderer_at(renderer);
    if (selected) {
        for (int i = 0; i < MONITOR_MODE_COUNT; i++) {
            if (renderer_find(MODE_RENDERERS[i]) == renderer) {
                s_mode = (monitor_mode_t)i;
                break;
            }
        }
    }

    app_color_t color = APP_COLOR_DEFAULT;
    if (theme == 0) color = APP_COLOR_GREEN;
    else if (theme == 1) color = APP_COLOR_BLUE;
    else if (theme == 2) color = APP_COLOR_WHITE;
    app_slots_set_color_runtime(&APP_MONITOR, color);
    app_slots_set_mode_runtime(&APP_MONITOR, (uint8_t)s_mode);
}

void monitor_app_refresh(void)
{
    monitor_select_renderer();
}

static int monitor_mode_count(void)
{
    return MONITOR_MODE_COUNT;
}

static const char *monitor_mode_name(int idx)
{
    return idx >= 0 && idx < MONITOR_MODE_COUNT
        ? MODE_NAMES[idx]
        : "";
}

static int monitor_mode_index(void)
{
    return s_mode;
}

static void monitor_mode_set(int idx)
{
    if (idx < 0 || idx >= MONITOR_MODE_COUNT) return;
    s_mode = (monitor_mode_t)idx;
    monitor_select_renderer();
}

static void monitor_enter(int variant)
{
    (void)variant;
    audio_set_mode(AUDIO_SPECTRUM);
    monitor_load_options();

    s_host = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_host, 480, 320);
    lv_obj_set_pos(s_host, 0, 0);
    lv_obj_remove_flag(s_host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_host, 0, 0);
    lv_obj_set_style_border_width(s_host, 0, 0);

    monitor_select_renderer();
}

static void monitor_exit(void)
{
    renderer_teardown();
    s_host = NULL;
}

static void monitor_render(void)
{
    plat_audio_viz_get(&s_viz_snapshot);
    const viz_frame_t frame = {
        .bars = s_viz_snapshot.bars,
        .peaks = s_viz_snapshot.peaks,
        .n = VIZ_POINTS,
        .level = s_viz_snapshot.level,
    };
    renderer_render(&frame);
}

static bool monitor_on_event(ui_event_t event)
{
    if (s_mode != MONITOR_MODE_CURVE) return false;

    int next_tilt = s_curve_tilt;
    int next_smoothing = s_curve_smoothing;
    if (event == EV_UP) next_tilt++;
    else if (event == EV_DOWN) next_tilt--;
    else if (event == EV_RIGHT) next_smoothing++;
    else if (event == EV_LEFT) next_smoothing--;
    else return false;

    if (next_tilt < 0) next_tilt = 0;
    if (next_tilt >= CURVE_TILT_COUNT) next_tilt = CURVE_TILT_COUNT - 1;
    if (next_smoothing < 0) next_smoothing = 0;
    if (next_smoothing >= CURVE_SMOOTH_COUNT) {
        next_smoothing = CURVE_SMOOTH_COUNT - 1;
    }
    if (next_tilt == s_curve_tilt &&
        next_smoothing == s_curve_smoothing) {
        return true;
    }

    s_curve_tilt = next_tilt;
    s_curve_smoothing = next_smoothing;
    monitor_save_options();
    renderer_curve_configure(
        CURVE_DISPLAY_VISUAL,
        CURVE_TILT_TENTHS[s_curve_tilt],
        s_curve_smoothing);
    audio_set_viz_tilt_tenths(CURVE_TILT_TENTHS[s_curve_tilt]);
    return true;
}

#ifdef PEDAL_SIM
int monitor_app_debug_tilt_index(void)
{
    return s_curve_tilt;
}

int monitor_app_debug_smoothing_index(void)
{
    return s_curve_smoothing;
}
#endif

const gadget_app_t APP_MONITOR = {
    .id = "monitor",
    .name = "Sound Monitor",
    .audio_mode = AUDIO_SPECTRUM,
    .icon = NULL,
    .on_enter = monitor_enter,
    .on_exit = monitor_exit,
    .on_render = monitor_render,
    .on_event = monitor_on_event,
    .on_appearance_changed = monitor_app_refresh,
    .mode_count = monitor_mode_count,
    .mode_name = monitor_mode_name,
    .mode_index = monitor_mode_index,
    .mode_set = monitor_mode_set,
};
