#include "gadget_app.h"

#include "platform.h"
#include "renderer.h"

typedef struct {
    const char *name;
    const char *renderer;
    int theme;
} monitor_preset_t;

static const monitor_preset_t PRESETS[] = {
    { "Spectrum (Blue)",  "curve",    1 },
    { "Spectrum (Green)", "curve",    0 },
    { "Bar (Multi)",   "bars",     0 },
    { "Bar (Blue)",    "bars",     1 },
    { "Talk (Blue)",   "reactive", 1 },
    { "Talk (Green)",  "reactive", 0 },
};

static int s_renderer;
static int s_theme = 1;
static lv_obj_t *s_host;
static audio_viz_snapshot_t s_viz_snapshot;

static void monitor_select_renderer(void)
{
    if (!s_host) return;
    renderer_select(s_renderer, s_host, viz_theme_at(s_theme));
    audio_set_viz_mode(s_renderer == renderer_find("curve") ? VIZ_MONITOR : VIZ_DECOR);
}

void monitor_app_set_scene(int theme, int renderer)
{
    s_theme = theme;
    s_renderer = renderer;
}

void monitor_app_refresh(void)
{
    monitor_select_renderer();
}

int monitor_app_preset_count(void)
{
    return (int)(sizeof(PRESETS) / sizeof(PRESETS[0]));
}

const char *monitor_app_preset_name(int idx)
{
    return idx >= 0 && idx < monitor_app_preset_count()
        ? PRESETS[idx].name
        : "";
}

int monitor_app_preset_index(void)
{
    for (int i = 0; i < monitor_app_preset_count(); i++) {
        if (renderer_find(PRESETS[i].renderer) == s_renderer &&
            PRESETS[i].theme == s_theme) {
            return i;
        }
    }
    return 0;
}

void monitor_app_set_preset(int idx)
{
    if (idx < 0 || idx >= monitor_app_preset_count()) return;

    const int renderer = renderer_find(PRESETS[idx].renderer);
    if (renderer < 0) return;
    s_renderer = renderer;
    s_theme = PRESETS[idx].theme;
    monitor_select_renderer();
}

static void monitor_enter(int variant)
{
    (void)variant;
    audio_set_mode(AUDIO_SPECTRUM);

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
    s_host = 0;
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
    (void)event;
    return false;
}

const gadget_app_t APP_MONITOR = {
    .id = "monitor",
    .name = "Sound Monitor",
    .audio_mode = AUDIO_SPECTRUM,
    .icon = NULL,
    .on_enter = monitor_enter,
    .on_exit = monitor_exit,
    .on_render = monitor_render,
    .on_event = monitor_on_event,
};
