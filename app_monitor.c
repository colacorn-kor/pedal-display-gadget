#include "gadget_app.h"

#include "renderer.h"

static int s_renderer;
static int s_theme;
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
    audio_viz_snapshot_get(&s_viz_snapshot);
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
    if (event == EV_PREV && renderer_count() > 0) {
        s_renderer = (s_renderer + 1) % renderer_count();
        monitor_select_renderer();
        return true;
    }
    if (event == EV_NEXT && viz_theme_count() > 0) {
        s_theme = (s_theme + 1) % viz_theme_count();
        monitor_select_renderer();
        return true;
    }
    return false;
}

const gadget_app_t APP_MONITOR = {
    .id = "monitor",
    .name = "Sound Monitor",
    .audio_mode = AUDIO_SPECTRUM,
    .on_enter = monitor_enter,
    .on_exit = monitor_exit,
    .on_render = monitor_render,
    .on_event = monitor_on_event,
};
