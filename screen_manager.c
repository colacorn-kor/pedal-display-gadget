/* ============================================================================
 *  screen_manager.c  -- Core-0/LVGL-owned app dispatcher
 * ========================================================================== */
#include "lvgl.h"

#include "app.h"
#include "gadget_app.h"
#include "renderer.h"

typedef enum {
    SM_HOME = 0,
    SM_APP_BASE = 1,
} sm_screen_t;

static int s_active_app = -1;
static int s_prev_app = -1;
static int s_prev_was_home = 1;
static int s_sel;
static lv_obj_t *s_item[10];
static float s_bpm = 120.0f;

static void clean_screen(void)
{
    lv_obj_clean(lv_screen_active());
    for (int i = 0; i < (int)(sizeof(s_item) / sizeof(s_item[0])); i++) {
        s_item[i] = 0;
    }
}

static const gadget_app_t *active_app(void)
{
    return app_registry_at(s_active_app);
}

static int tuner_idx(void)
{
    return app_registry_find("tuner");
}

static void exit_active_app(void)
{
    const gadget_app_t *app = active_app();
    if (app && app->on_exit) app->on_exit();
    s_active_app = -1;
}

static void highlight_home(void)
{
    const int n = app_registry_count();
    for (int i = 0; i < n; i++) {
        if (s_item[i]) {
            lv_obj_set_style_text_color(
                s_item[i], lv_color_hex(i == s_sel ? 0x7FD4A8 : 0x6B7480), 0);
        }
    }
}

static void build_home(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0E1116), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "PEDAL DISPLAY");
    lv_obj_set_style_text_color(title, lv_color_hex(0x4A525C), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);

    const int n = app_registry_count();
    if (s_sel >= n) s_sel = n > 0 ? n - 1 : 0;

    for (int i = 0; i < n && i < (int)(sizeof(s_item) / sizeof(s_item[0])); i++) {
        s_item[i] = lv_label_create(screen);
        lv_label_set_text(s_item[i], app_registry_name(i));
        lv_obj_set_style_text_font(s_item[i], &lv_font_montserrat_28, 0);
        lv_obj_align(s_item[i], LV_ALIGN_CENTER, 0, -40 + i * 46);
    }
    highlight_home();
}

static void go_home(void)
{
    exit_active_app();
    clean_screen();
    audio_set_mode(AUDIO_SPECTRUM);
    build_home();
}

static void enter_app(int idx, int variant)
{
    const gadget_app_t *app = app_registry_at(idx);
    if (!app) return;

    exit_active_app();
    clean_screen();
    s_active_app = idx;
    if (app->on_enter) app->on_enter(variant);
}

static void enter_tuner_bridge(void)
{
    const int tuner = tuner_idx();
    if (tuner < 0) return;

    s_prev_app = s_active_app;
    s_prev_was_home = s_active_app < 0;
    enter_app(tuner, 0);
}

static void exit_tuner_bridge(void)
{
    const int target = s_prev_app;
    const int was_home = s_prev_was_home;

    exit_active_app();
    clean_screen();
    audio_set_mode(AUDIO_SPECTRUM);

    if (!was_home && app_registry_at(target)) {
        s_active_app = target;
        const gadget_app_t *app = active_app();
        if (app && app->on_enter) app->on_enter(0);
    } else {
        build_home();
    }
}

void sm_load_scene(int content, int theme, int renderer)
{
    if (content < 0 || content >= images_app_count() ||
        theme < 0 || theme >= viz_theme_count() ||
        renderer < 0 || renderer >= renderer_count()) return;

    images_app_set_content(content);
    monitor_app_set_scene(theme, renderer);

    const int monitor = app_registry_find("monitor");
    const int images = app_registry_find("images");
    if (s_active_app == monitor) monitor_app_refresh();
    else enter_app(images, 0);
}

void sm_load_scene_named(int content, int theme, const char *renderer_name)
{
    int renderer = renderer_name ? renderer_find(renderer_name) : -1;
    if (renderer >= 0) sm_load_scene(content, theme, renderer);
}

void sm_set_tempo(float bpm)
{
    if (bpm >= 30.0f && bpm <= 300.0f) s_bpm = bpm;
}

float sm_get_tempo(void)
{
    return s_bpm;
}

void sm_init(void)
{
    renderers_init();
    apps_init();
    audio_set_mode(AUDIO_SPECTRUM);
    build_home();
}

void sm_on_event(ui_event_t event)
{
    if (event == EV_FOOTSW) {
        if (s_active_app == tuner_idx()) exit_tuner_bridge();
        else enter_tuner_bridge();
        return;
    }

    if (s_active_app < 0) {
        const int n = app_registry_count();
        if (n <= 0) return;

        if (event == EV_PREV) {
            s_sel = (s_sel - 1 + n) % n;
            highlight_home();
        } else if (event == EV_NEXT) {
            s_sel = (s_sel + 1) % n;
            highlight_home();
        } else if (event == EV_SELECT) {
            if (s_sel == tuner_idx()) enter_tuner_bridge();
            else enter_app(s_sel, 0);
        }
        return;
    }

    const gadget_app_t *app = active_app();
    if (app && app->on_event && app->on_event(event)) return;

    if (event == EV_SELECT || event == EV_BACK) {
        if (s_active_app == tuner_idx()) exit_tuner_bridge();
        else go_home();
    }
}

void sm_render(void)
{
    const gadget_app_t *app = active_app();
    if (app && app->on_render) app->on_render();
}

int sm_current(void)
{
    return s_active_app < 0 ? SM_HOME : SM_APP_BASE + s_active_app;
}
