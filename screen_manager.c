/* ============================================================================
 *  screen_manager.c  —  Core-0/LVGL-owned screen state machine
 * ========================================================================== */
#include "lvgl.h"
#include "renderer.h"
#include "app.h"
#include "content_screen.h"
#include "tuner.h"
#include "tuner_screen.h"

typedef enum { SCR_HOME, SCR_MONITOR, SCR_IMAGES, SCR_TUNER } screen_t;
static screen_t s_cur = SCR_HOME;
static screen_t s_prev = SCR_HOME;
static int s_rend;
static int s_theme;
static int s_img;
static int s_sel;
static lv_obj_t *s_host;
static lv_obj_t *s_item[3];
static audio_viz_snapshot_t s_viz_snapshot;

static const char *IMG_F[] = {
    "S:content/img1.bin", "S:content/img2.bin", "S:content/img3.bin"
};
static const char *IMG_N[] = { "img1", "img2", "img3" };
#define IMGN ((int)(sizeof(IMG_F) / sizeof(IMG_F[0])))

static const char *MENU[] = { "Sound Monitor", "Images", "Tuner" };
#define MENUN ((int)(sizeof(MENU) / sizeof(MENU[0])))

static void clean_screen(void)
{
    lv_obj_clean(lv_screen_active());
    s_host = NULL;
    for (int i = 0; i < MENUN; i++) s_item[i] = NULL;
}

static void cleanup_current(void)
{
    switch (s_cur) {
    case SCR_MONITOR:
        renderer_teardown();
        s_host = NULL;
        break;
    case SCR_IMAGES:
        content_screen_destroy();
        break;
    case SCR_TUNER:
        tuner_screen_destroy();
        mute_set(0);
        audio_set_mode(AUDIO_SPECTRUM);
        break;
    case SCR_HOME:
        break;
    }
}

static void highlight_home(void)
{
    for (int i = 0; i < MENUN; i++) {
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

    for (int i = 0; i < MENUN; i++) {
        s_item[i] = lv_label_create(screen);
        lv_label_set_text(s_item[i], MENU[i]);
        lv_obj_set_style_text_font(s_item[i], &lv_font_montserrat_28, 0);
        lv_obj_align(s_item[i], LV_ALIGN_CENTER, 0, -40 + i * 46);
    }
    highlight_home();
    s_cur = SCR_HOME;
}

static void select_monitor_renderer(void)
{
    if (!s_host) return;
    renderer_select(s_rend, s_host, viz_theme_at(s_theme));
    audio_set_viz_mode(s_rend == renderer_find("curve") ? VIZ_MONITOR : VIZ_DECOR);
}

static void build_monitor(void)
{
    audio_set_mode(AUDIO_SPECTRUM);
    s_host = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_host, 480, 320);
    lv_obj_set_pos(s_host, 0, 0);
    lv_obj_remove_flag(s_host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_host, 0, 0);
    lv_obj_set_style_border_width(s_host, 0, 0);
    select_monitor_renderer();
    s_cur = SCR_MONITOR;
}

static void build_images(void)
{
    audio_set_mode(AUDIO_SPECTRUM);
    content_screen_create();
    content_show_image(IMG_F[s_img], IMG_N[s_img]);
    s_cur = SCR_IMAGES;
}

static void enter_monitor(void)
{
    cleanup_current();
    clean_screen();
    build_monitor();
}

static void enter_images(void)
{
    cleanup_current();
    clean_screen();
    build_images();
}

static void go_home(void)
{
    cleanup_current();
    clean_screen();
    audio_set_mode(AUDIO_SPECTRUM);
    build_home();
}

static void enter_tuner(void)
{
    s_prev = s_cur;
    cleanup_current();
    clean_screen();
    mute_set(1);
    audio_set_mode(AUDIO_TUNER);
    tuner_screen_create();
    s_cur = SCR_TUNER;
}

static void exit_tuner(void)
{
    screen_t target = s_prev;
    cleanup_current();
    clean_screen();
    audio_set_mode(AUDIO_SPECTRUM);

    if (target == SCR_MONITOR) build_monitor();
    else if (target == SCR_IMAGES) build_images();
    else build_home();
}

static float s_bpm = 120.0f;

void sm_load_scene(int content, int theme, int renderer)
{
    if (content < 0 || content >= IMGN ||
        theme < 0 || theme >= viz_theme_count() ||
        renderer < 0 || renderer >= renderer_count()) return;

    s_img = content;
    s_theme = theme;
    s_rend = renderer;

    if (s_cur == SCR_MONITOR) select_monitor_renderer();
    else enter_images();
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
    audio_set_mode(AUDIO_SPECTRUM);
    build_home();
}

void sm_on_event(ui_event_t event)
{
    if (event == EV_FOOTSW) {
        if (s_cur == SCR_TUNER) exit_tuner();
        else enter_tuner();
        return;
    }

    switch (s_cur) {
    case SCR_HOME:
        if (event == EV_PREV) {
            s_sel = (s_sel - 1 + MENUN) % MENUN;
            highlight_home();
        } else if (event == EV_NEXT) {
            s_sel = (s_sel + 1) % MENUN;
            highlight_home();
        } else if (event == EV_SELECT) {
            if (s_sel == 0) enter_monitor();
            else if (s_sel == 1) enter_images();
            else enter_tuner();
        }
        break;

    case SCR_MONITOR:
        if (event == EV_PREV && renderer_count() > 0) {
            s_rend = (s_rend + 1) % renderer_count();
            select_monitor_renderer();
        } else if (event == EV_NEXT && viz_theme_count() > 0) {
            s_theme = (s_theme + 1) % viz_theme_count();
            select_monitor_renderer();
        } else if (event == EV_SELECT || event == EV_BACK) {
            go_home();
        }
        break;

    case SCR_IMAGES:
        if (event == EV_PREV) {
            s_img = (s_img - 1 + IMGN) % IMGN;
            content_show_image(IMG_F[s_img], IMG_N[s_img]);
        } else if (event == EV_NEXT) {
            s_img = (s_img + 1) % IMGN;
            content_show_image(IMG_F[s_img], IMG_N[s_img]);
        } else if (event == EV_SELECT || event == EV_BACK) {
            go_home();
        }
        break;

    case SCR_TUNER:
        if (event == EV_SELECT || event == EV_BACK) exit_tuner();
        break;
    }
}

void sm_render(void)
{
    if (s_cur == SCR_MONITOR) {
        audio_viz_snapshot_get(&s_viz_snapshot);
        const viz_frame_t frame = {
            .bars = s_viz_snapshot.bars,
            .peaks = s_viz_snapshot.peaks,
            .n = VIZ_POINTS,
            .level = s_viz_snapshot.level,
        };
        renderer_render(&frame);
    } else if (s_cur == SCR_TUNER) {
        tuner_result_t result;
        tuner_get(&result);
        tuner_screen_update(result.voiced, result.name, result.octave,
                            result.cents, result.f0);
    }
}

int sm_current(void)
{
    return (int)s_cur;
}
