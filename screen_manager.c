/* ============================================================================
 *  screen_manager.c  -- Core-0/LVGL-owned app dispatcher
 * ========================================================================== */
#include "lvgl.h"

#include "app.h"
#include "app_slots.h"
#include "gadget_app.h"
#include "renderer.h"

typedef enum {
    SM_HOME = 0,
    SM_APP_BASE = 1,
} sm_screen_t;

#define POPUP_ITEM_COUNT 4
#define POPUP_EXIT_IDX 0

static int s_active_app = -1;
static int s_sel;
static lv_obj_t *s_item[10];
static lv_obj_t *s_popup;
static lv_obj_t *s_popup_item[POPUP_ITEM_COUNT];
static int s_popup_sel;
static float s_bpm = 120.0f;

static const char *POPUP_LABELS[POPUP_ITEM_COUNT] = {
    "Exit", "Settings", "Help", "About"
};

static void clean_screen(void)
{
    lv_obj_clean(lv_screen_active());
    for (int i = 0; i < (int)(sizeof(s_item) / sizeof(s_item[0])); i++) {
        s_item[i] = 0;
    }
    s_popup = 0;
    for (int i = 0; i < POPUP_ITEM_COUNT; i++) s_popup_item[i] = 0;
    s_popup_sel = 0;
}

static const gadget_app_t *active_app(void)
{
    return app_registry_at(s_active_app);
}

static int app_index(const gadget_app_t *app)
{
    return app ? app_registry_find(app->id) : -1;
}

static int variant_for_app(const gadget_app_t *app)
{
    for (int i = 0; i < app_slots_count(); i++) {
        app_slot_t *slot = app_slots_at(i);
        if (slot && slot->app == app) return slot->variant;
    }
    return 0;
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

static void popup_highlight(void)
{
    for (int i = 0; i < POPUP_ITEM_COUNT; i++) {
        if (s_popup_item[i]) {
            lv_obj_set_style_text_color(
                s_popup_item[i],
                lv_color_hex(i == s_popup_sel ? 0x7FD4A8 : 0xD9DEE5),
                0);
        }
    }
}

static void popup_close(void)
{
    if (s_popup) lv_obj_delete(s_popup);
    s_popup = 0;
    for (int i = 0; i < POPUP_ITEM_COUNT; i++) s_popup_item[i] = 0;
    s_popup_sel = 0;
}

static void popup_open(void)
{
    if (s_popup) return;

    s_popup_sel = POPUP_EXIT_IDX;
    s_popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_popup, 480, 320);
    lv_obj_set_pos(s_popup, 0, 0);
    lv_obj_remove_flag(s_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_popup, 0, 0);
    lv_obj_set_style_border_width(s_popup, 0, 0);
    lv_obj_set_style_pad_all(s_popup, 0, 0);
    lv_obj_set_style_bg_color(s_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_popup, 170, 0);

    lv_obj_t *panel = lv_obj_create(s_popup);
    lv_obj_set_size(panel, 260, 210);
    lv_obj_center(panel);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x2A323C), 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x121821), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "MENU");
    lv_obj_set_style_text_color(title, lv_color_hex(0x6B7480), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    for (int i = 0; i < POPUP_ITEM_COUNT; i++) {
        s_popup_item[i] = lv_label_create(panel);
        lv_label_set_text(s_popup_item[i], POPUP_LABELS[i]);
        lv_obj_set_style_text_font(s_popup_item[i], &lv_font_montserrat_28, 0);
        lv_obj_align(s_popup_item[i], LV_ALIGN_TOP_LEFT, 42, 54 + i * 36);
    }
    popup_highlight();
}

static void popup_move(ui_event_t event)
{
    if (event == EV_UP || event == EV_LEFT) {
        s_popup_sel = (s_popup_sel - 1 + POPUP_ITEM_COUNT) % POPUP_ITEM_COUNT;
    } else if (event == EV_DOWN || event == EV_RIGHT) {
        s_popup_sel = (s_popup_sel + 1) % POPUP_ITEM_COUNT;
    }
    popup_highlight();
}

static void go_home(void)
{
    popup_close();
    exit_active_app();
    clean_screen();
    audio_set_mode(AUDIO_SPECTRUM);
    build_home();
}

static void enter_app(int idx, int variant)
{
    const gadget_app_t *app = app_registry_at(idx);
    if (!app) return;

    popup_close();
    exit_active_app();
    clean_screen();
    s_active_app = idx;
    if (app->on_enter) app->on_enter(variant);
    app_slots_set_last_view(app->id);
}

static void enter_app_ptr(const gadget_app_t *app)
{
    const int idx = app_index(app);
    if (idx >= 0) enter_app(idx, variant_for_app(app));
}

static void footsw_next_app(void)
{
    const gadget_app_t *app = s_active_app < 0
        ? app_slots_first_live()
        : app_slots_next_live(active_app());
    enter_app_ptr(app);
}

static void footsw_quick_app(void)
{
    int idx = app_registry_find(app_slots_quick_app());
    if (idx < 0) idx = app_registry_find("tuner");
    if (idx >= 0 && s_active_app != idx) {
        const gadget_app_t *app = app_registry_at(idx);
        enter_app(idx, variant_for_app(app));
    }
}

static void popup_activate(void)
{
    if (s_popup_sel == POPUP_EXIT_IDX) go_home();
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
    app_slots_init();
    audio_set_mode(AUDIO_SPECTRUM);

    const char *last_view = app_slots_last_view();
    const int last_idx = last_view && last_view[0]
        ? app_registry_find(last_view)
        : -1;
    if (last_idx >= 0) {
        const gadget_app_t *app = app_registry_at(last_idx);
        enter_app(last_idx, variant_for_app(app));
        return;
    }

    const gadget_app_t *first_live = app_slots_first_live();
    if (first_live) {
        enter_app_ptr(first_live);
    } else {
        build_home();
    }
}

void sm_on_event(ui_event_t event)
{
    if (event == EV_HOME_HOLD) {
        go_home();
        return;
    }

    if (s_popup) {
        if (event == EV_UP || event == EV_DOWN ||
            event == EV_LEFT || event == EV_RIGHT) {
            popup_move(event);
        } else if (event == EV_OK) {
            popup_activate();
        } else if (event == EV_HOME) {
            popup_close();
        } else if (event == EV_FOOTSW) {
            popup_close();
            footsw_next_app();
        } else if (event == EV_FOOTSW_HOLD) {
            popup_close();
            footsw_quick_app();
        }
        return;
    }

    if (event == EV_FOOTSW) {
        footsw_next_app();
        return;
    }
    if (event == EV_FOOTSW_HOLD) {
        footsw_quick_app();
        return;
    }

    if (event == EV_HOME) {
        if (s_active_app >= 0) popup_open();
        return;
    }

    if (s_active_app < 0) {
        const int n = app_registry_count();
        if (n <= 0) return;

        if (event == EV_UP || event == EV_LEFT) {
            s_sel = (s_sel - 1 + n) % n;
            highlight_home();
        } else if (event == EV_DOWN || event == EV_RIGHT) {
            s_sel = (s_sel + 1) % n;
            highlight_home();
        } else if (event == EV_OK) {
            enter_app(s_sel, 0);
        }
        return;
    }

    const gadget_app_t *app = active_app();
    if (app && app->on_event && app->on_event(event)) return;
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
