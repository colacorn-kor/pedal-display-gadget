/* ============================================================================
 *  screen_manager.c  -- Core-0/LVGL-owned app dispatcher and launcher
 * ========================================================================== */
#include "lvgl.h"

#include "app.h"
#include "app_slots.h"
#include "gadget_app.h"
#include "platform.h"
#include "renderer.h"
#include "theme.h"

typedef enum {
    SM_HOME = 0,
    SM_APP_BASE = 1,
} sm_screen_t;

typedef enum {
    MODE_LIVE,
    MODE_POPUP,
    MODE_LAUNCHER,
    MODE_REORDER,
} sm_mode_t;

typedef enum {
    POPUP_APP_MENU,
    POPUP_SETTINGS,
    POPUP_GLOBAL_THEME,
    POPUP_MONITOR_THEME,
    POPUP_INFO,
} popup_page_t;

typedef enum {
    POPUP_FROM_APP,
    POPUP_FROM_LAUNCHER,
} popup_origin_t;

enum {
    ROW_LIVE = 0,
    ROW_STASH = 1,
    ROW_ACTION = 2,
};

enum {
    ACTION_REORDER = 0,
    ACTION_SETTINGS = 1,
    ACTION_COUNT = 2,
};

#define POPUP_ITEM_MAX 6
#define TILE_VISIBLE 4
#define TILE_W 88
#define TILE_H 88
#define TILE_GAP 12
#define TILE_X0 24
#define LIVE_LABEL_Y 40
#define LIVE_TILE_Y 58
#define STASH_LABEL_Y 196
#define STASH_TILE_Y 210
#define LAST_VIEW_SAVE_DELAY_MS 1000U

static int s_active_app = -1;
static sm_mode_t s_mode = MODE_LIVE;
static lv_obj_t *s_popup;
static lv_obj_t *s_popup_item[POPUP_ITEM_MAX];
static int s_popup_item_count;
static int s_popup_sel;
static popup_page_t s_popup_page = POPUP_APP_MENU;
static popup_origin_t s_popup_origin = POPUP_FROM_APP;
static sm_mode_t s_popup_return_mode = MODE_LIVE;
static float s_bpm = 120.0f;
static bool s_last_view_save_pending;
static uint32_t s_last_view_save_since;

static int s_cursor_row = ROW_LIVE;
static int s_cursor_col;
static int s_scroll_live;
static int s_scroll_stash;
static const gadget_app_t *s_return_app;

static bool s_reorder_picked;
static int s_reorder_slot = -1;
static chain_t s_reorder_chain[APP_SLOT_MAX];
static uint8_t s_reorder_order[APP_SLOT_MAX];

static const lv_font_t *font_small(void)
{
#if LV_FONT_UNSCII_8
    return &lv_font_unscii_8;
#else
    return &lv_font_montserrat_12;
#endif
}

static const lv_font_t *font_pixel(void)
{
#if LV_FONT_UNSCII_16
    return &lv_font_unscii_16;
#else
    return &lv_font_montserrat_14;
#endif
}

static const ui_theme_t *ui(void)
{
    return theme_get();
}

static void clean_screen(void)
{
    lv_obj_clean(lv_screen_active());
    s_popup = 0;
    for (int i = 0; i < POPUP_ITEM_MAX; i++) {
        s_popup_item[i] = 0;
    }
    s_popup_item_count = 0;
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

static bool app_is_live(const gadget_app_t *app)
{
    for (int i = 0; i < app_slots_count(); i++) {
        app_slot_t *slot = app_slots_at(i);
        if (slot && slot->app == app) return slot->chain == CHAIN_LIVE;
    }
    return false;
}

static void exit_active_app(void)
{
    const gadget_app_t *app = active_app();
    if (app && app->on_exit) app->on_exit();
    s_active_app = -1;
}

static chain_t chain_for_row(int row)
{
    return row == ROW_STASH ? CHAIN_STASH : CHAIN_LIVE;
}

static int slot_sort_cmp(int a, int b)
{
    app_slot_t *sa = app_slots_at(a);
    app_slot_t *sb = app_slots_at(b);
    if (!sa || !sb) return a - b;
    if (sa->order != sb->order) return (int)sa->order - (int)sb->order;
    return a - b;
}

static int collect_chain(chain_t chain, int out[APP_SLOT_MAX])
{
    int n = 0;
    for (int i = 0; i < app_slots_count(); i++) {
        app_slot_t *slot = app_slots_at(i);
        if (slot && slot->app && slot->chain == chain) out[n++] = i;
    }

    for (int i = 1; i < n; i++) {
        const int v = out[i];
        int j = i - 1;
        while (j >= 0 && slot_sort_cmp(out[j], v) > 0) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = v;
    }
    return n;
}

static int chain_count(chain_t chain)
{
    int idx[APP_SLOT_MAX];
    return collect_chain(chain, idx);
}

static int slot_index_at(chain_t chain, int pos)
{
    int idx[APP_SLOT_MAX];
    const int n = collect_chain(chain, idx);
    return pos >= 0 && pos < n ? idx[pos] : -1;
}

static int row_count(int row)
{
    if (row == ROW_ACTION) return ACTION_COUNT;
    return chain_count(chain_for_row(row));
}

static int max_launcher_row(void)
{
    return ROW_ACTION;
}

static void normalize_chain(chain_t chain)
{
    int idx[APP_SLOT_MAX];
    const int n = collect_chain(chain, idx);
    for (int i = 0; i < n; i++) {
        app_slot_t *slot = app_slots_at(idx[i]);
        if (slot) slot->order = (uint8_t)i;
    }
}

static void normalize_all_slots(void)
{
    normalize_chain(CHAIN_LIVE);
    normalize_chain(CHAIN_STASH);
}

static void capture_reorder_snapshot(void)
{
    for (int i = 0; i < APP_SLOT_MAX; i++) {
        s_reorder_chain[i] = CHAIN_LIVE;
        s_reorder_order[i] = 0;
    }
    for (int i = 0; i < app_slots_count(); i++) {
        app_slot_t *slot = app_slots_at(i);
        if (!slot) continue;
        s_reorder_chain[i] = slot->chain;
        s_reorder_order[i] = slot->order;
    }
}

static void restore_reorder_snapshot(void)
{
    for (int i = 0; i < app_slots_count(); i++) {
        app_slot_t *slot = app_slots_at(i);
        if (!slot) continue;
        slot->chain = s_reorder_chain[i];
        slot->order = s_reorder_order[i];
    }
    normalize_all_slots();
}

static void clamp_scroll_for_row(int row)
{
    if (row == ROW_ACTION) return;

    int *scroll = row == ROW_STASH ? &s_scroll_stash : &s_scroll_live;
    const int count = row_count(row);
    const int max_scroll = count > TILE_VISIBLE ? count - TILE_VISIBLE : 0;
    if (*scroll > max_scroll) *scroll = max_scroll;
    if (*scroll < 0) *scroll = 0;
    if (s_cursor_row != row) return;
    if (s_cursor_col < *scroll) *scroll = s_cursor_col;
    if (s_cursor_col >= *scroll + TILE_VISIBLE) {
        *scroll = s_cursor_col - TILE_VISIBLE + 1;
    }
}

static void clamp_cursor(void)
{
    const int max_row = max_launcher_row();
    if (s_cursor_row > max_row) s_cursor_row = max_row;
    if (s_cursor_row < ROW_LIVE) s_cursor_row = ROW_LIVE;

    const int count = row_count(s_cursor_row);
    if (count <= 0) {
        s_cursor_col = 0;
    } else if (s_cursor_col >= count) {
        s_cursor_col = count - 1;
    } else if (s_cursor_col < 0) {
        s_cursor_col = 0;
    }

    clamp_scroll_for_row(ROW_LIVE);
    clamp_scroll_for_row(ROW_STASH);
}

static int selected_slot_index(void)
{
    if (s_cursor_row == ROW_ACTION) return -1;
    return slot_index_at(chain_for_row(s_cursor_row), s_cursor_col);
}

static void draw_section_label(const char *label, int y, bool selected)
{
    const ui_theme_t *t = ui();
    lv_obj_t *obj = lv_label_create(lv_screen_active());
    lv_label_set_text(obj, label);
    lv_obj_set_style_text_font(obj, font_small(), 0);
    lv_obj_set_style_text_color(obj, selected ? t->accent : t->text, 0);
    lv_obj_set_pos(obj, TILE_X0, y);

    lv_obj_t *line = lv_obj_create(lv_screen_active());
    lv_obj_set_size(line, 360, 1);
    lv_obj_set_pos(line, TILE_X0 + 56, y + 5);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_style_bg_color(line, t->grid, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
}

static void draw_fallback_icon(lv_obj_t *tile, const gadget_app_t *app)
{
    char initial[2] = { '?', 0 };
    if (app && app->name && app->name[0]) initial[0] = app->name[0];

    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, initial);
    lv_obj_set_style_text_font(label, font_pixel(), 0);
    lv_obj_set_style_text_color(label, ui()->text, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -10);
}

static void draw_tile_cursor(lv_obj_t *tile, bool picked)
{
    lv_obj_t *cursor = lv_label_create(tile);
    lv_label_set_text(
        cursor,
        s_mode == MODE_REORDER ? (picked ? "><" : "<>") : LV_SYMBOL_UP);
    lv_obj_set_style_text_font(
        cursor,
        s_mode == MODE_REORDER ? font_small() : &lv_font_montserrat_14,
        0);
    lv_obj_set_style_text_color(cursor, ui()->accent2, 0);
    if (s_mode != MODE_REORDER) {
        lv_obj_set_style_transform_rotation(cursor, 3150, 0);
    }
    lv_obj_align(cursor, LV_ALIGN_BOTTOM_RIGHT, picked ? 1 : 2, 2);
}

static void draw_tile(int slot_idx, int row, int visible_idx, int y)
{
    const ui_theme_t *t = ui();
    app_slot_t *slot = app_slots_at(slot_idx);
    if (!slot || !slot->app) return;

    const bool selected = s_cursor_row == row && s_cursor_col ==
        (visible_idx + (row == ROW_STASH ? s_scroll_stash : s_scroll_live));
    const bool picked = s_reorder_picked && s_reorder_slot == slot_idx;
    const bool subdued = s_mode == MODE_REORDER && s_reorder_picked && !picked;
    const bool disabled = slot->app->needs_codec;
    const int x = TILE_X0 + visible_idx * (TILE_W + TILE_GAP);
    const int y_offset = picked ? -6 : 0;

    lv_obj_t *tile = lv_obj_create(lv_screen_active());
    lv_obj_set_size(tile, TILE_W, TILE_H);
    lv_obj_set_pos(tile, x, y + y_offset);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(tile, 6, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_set_style_bg_color(tile, picked ? t->accent : t->surface, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        tile,
        picked ? t->accent2 : (selected ? t->accent : t->grid),
        0);
    lv_obj_set_style_border_width(tile, selected || picked ? 3 : 1, 0);
    lv_obj_set_style_transform_width(tile, selected ? 4 : 0, 0);
    lv_obj_set_style_transform_height(tile, selected ? 4 : 0, 0);
    lv_obj_set_style_outline_color(tile, selected ? t->accent : t->grid, 0);
    lv_obj_set_style_outline_width(tile, selected ? 1 : 0, 0);
    lv_obj_set_style_outline_pad(tile, selected ? 1 : 0, 0);
    if (picked) {
        lv_obj_set_style_shadow_width(tile, 10, 0);
        lv_obj_set_style_shadow_opa(tile, LV_OPA_40, 0);
        lv_obj_set_style_shadow_color(tile, t->accent, 0);
    }

    lv_opa_t opa = LV_OPA_COVER;
    if (slot->chain == CHAIN_STASH) opa = LV_OPA_60;
    if (subdued) opa = LV_OPA_60;
    if (disabled) opa = LV_OPA_40;
    if (selected && !subdued && !disabled) opa = LV_OPA_COVER;
    lv_obj_set_style_opa(tile, opa, 0);

    if (slot->app->icon) {
        lv_obj_t *icon = lv_image_create(tile);
        lv_image_set_src(icon, slot->app->icon);
        lv_obj_set_style_image_recolor(icon, picked ? t->surface : t->text, 0);
        lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
        lv_obj_align(icon, LV_ALIGN_CENTER, 0, -14);
    } else {
        draw_fallback_icon(tile, slot->app);
    }

    lv_obj_t *name = lv_label_create(tile);
    lv_label_set_text(name, slot->app->name);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, TILE_W - 10);
    lv_obj_set_style_text_font(name, font_small(), 0);
    lv_obj_set_style_text_color(name, picked ? t->surface : t->text, 0);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, -8);

    if (selected) draw_tile_cursor(tile, picked);
}

static void draw_chain_row(int row, int y)
{
    const chain_t chain = chain_for_row(row);
    int idx[APP_SLOT_MAX];
    const int n = collect_chain(chain, idx);
    const int scroll = row == ROW_STASH ? s_scroll_stash : s_scroll_live;
    const int end = scroll + TILE_VISIBLE < n ? scroll + TILE_VISIBLE : n;

    for (int i = scroll; i < end; i++) {
        draw_tile(idx[i], row, i - scroll, y);
    }
}

static void draw_actions(void)
{
    const ui_theme_t *t = ui();
    const char *labels[ACTION_COUNT] = { "[REORDER]", "[SETTINGS]" };
    const int xs[ACTION_COUNT] = { 294, 394 };

    for (int i = 0; i < ACTION_COUNT; i++) {
        const bool selected = s_cursor_row == ROW_ACTION && s_cursor_col == i;
        lv_obj_t *label = lv_label_create(lv_screen_active());
        lv_label_set_text(label, labels[i]);
        lv_obj_set_style_text_font(label, font_small(), 0);
        lv_obj_set_style_text_color(
            label,
            selected ? t->accent : t->text,
            0);
        lv_obj_set_pos(label, xs[i], 306);

        if (selected) {
            lv_obj_t *cursor = lv_label_create(lv_screen_active());
            lv_label_set_text(
                cursor,
                s_mode == MODE_REORDER ? "<>" : LV_SYMBOL_DOWN);
            lv_obj_set_style_text_font(
                cursor,
                s_mode == MODE_REORDER ? font_small() : &lv_font_montserrat_14,
                0);
            lv_obj_set_style_text_color(cursor, t->accent2, 0);
            if (s_mode != MODE_REORDER) {
                lv_obj_set_style_transform_rotation(cursor, 3150, 0);
            }
            lv_obj_set_pos(cursor, xs[i] - 12, 294);
        }
    }
}

static void build_launcher(void)
{
    clamp_cursor();
    clean_screen();

    const ui_theme_t *t = ui();
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, t->bg, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "GUI");
    lv_obj_set_style_text_font(title, font_pixel(), 0);
    lv_obj_set_style_text_color(title, t->accent, 0);
    lv_obj_set_pos(title, TILE_X0, 8);

    draw_section_label("LIVE", LIVE_LABEL_Y,
                       s_cursor_row == ROW_LIVE || (s_reorder_picked &&
                       app_slots_at(s_reorder_slot) &&
                       app_slots_at(s_reorder_slot)->chain == CHAIN_LIVE));
    draw_chain_row(ROW_LIVE, LIVE_TILE_Y);

    draw_section_label("STASH", STASH_LABEL_Y,
                       s_cursor_row == ROW_STASH || (s_reorder_picked &&
                       app_slots_at(s_reorder_slot) &&
                       app_slots_at(s_reorder_slot)->chain == CHAIN_STASH));
    draw_chain_row(ROW_STASH, STASH_TILE_Y);

    draw_actions();
    if (s_mode == MODE_REORDER) {
        lv_obj_t *mode = lv_label_create(screen);
        lv_label_set_text(mode, "REORDER");
        lv_obj_set_style_text_font(mode, font_small(), 0);
        lv_obj_set_style_text_color(mode, t->accent, 0);
        lv_obj_set_pos(mode, TILE_X0, 306);
    }
}

static void popup_build(void);

static void on_theme_changed(void)
{
    if (s_mode == MODE_LAUNCHER || s_mode == MODE_REORDER) {
        build_launcher();
    } else if (s_mode == MODE_POPUP) {
        if (s_popup_return_mode == MODE_LAUNCHER) {
            s_mode = MODE_LAUNCHER;
            build_launcher();
            s_mode = MODE_POPUP;
        }
        popup_build();
    }
}

static void popup_clear_objects(void)
{
    if (s_popup) lv_obj_delete(s_popup);
    s_popup = 0;
    for (int i = 0; i < POPUP_ITEM_MAX; i++) {
        s_popup_item[i] = 0;
    }
    s_popup_item_count = 0;
}

static void popup_highlight(void)
{
    const ui_theme_t *t = ui();
    for (int i = 0; i < s_popup_item_count; i++) {
        if (!s_popup_item[i]) continue;
        lv_obj_set_style_text_color(
            s_popup_item[i],
            i == s_popup_sel ? t->accent : t->text,
            0);
    }
}

static void popup_close(void)
{
    popup_clear_objects();
    s_popup_sel = 0;
    if (s_mode == MODE_POPUP) s_mode = s_popup_return_mode;
}

static void popup_set_page(popup_page_t page)
{
    s_popup_page = page;
    s_popup_sel = page == POPUP_MONITOR_THEME
        ? monitor_app_preset_index()
        : 0;
    popup_build();
}

static void popup_build(void)
{
    static const char *const app_menu[] = { "Exit", "Settings" };
    static const char *const settings[] = { "Theme", "Info" };
    const char *const *labels = 0;
    const char *title_text = "MENU";
    int panel_w = 280;
    int panel_h = 176;

    popup_clear_objects();

    if (s_popup_page == POPUP_APP_MENU) {
        labels = app_menu;
        s_popup_item_count = 2;
    } else if (s_popup_page == POPUP_SETTINGS) {
        labels = settings;
        title_text = "SETTINGS";
        s_popup_item_count = 2;
    } else if (s_popup_page == POPUP_GLOBAL_THEME) {
        title_text = "Theme";
        panel_h = 150;
    } else if (s_popup_page == POPUP_MONITOR_THEME) {
        title_text = "Theme";
        panel_w = 310;
        panel_h = 286;
        s_popup_item_count = monitor_app_preset_count();
        if (s_popup_item_count > POPUP_ITEM_MAX) {
            s_popup_item_count = POPUP_ITEM_MAX;
        }
    } else {
        title_text = "Info";
        panel_h = 160;
    }

    const ui_theme_t *t = ui();
    s_popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_popup, 480, 320);
    lv_obj_set_pos(s_popup, 0, 0);
    lv_obj_remove_flag(s_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_popup, 0, 0);
    lv_obj_set_style_border_width(s_popup, 0, 0);
    lv_obj_set_style_pad_all(s_popup, 0, 0);
    lv_obj_set_style_bg_color(s_popup, t->bg, 0);
    lv_obj_set_style_bg_opa(s_popup, 190, 0);

    lv_obj_t *panel = lv_obj_create(s_popup);
    lv_obj_set_size(panel, panel_w, panel_h);
    lv_obj_center(panel);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, t->grid, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, t->surface, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, t->accent, 0);
    lv_obj_set_style_text_font(title, font_small(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    if (s_popup_page == POPUP_GLOBAL_THEME) {
        lv_obj_t *selector = lv_label_create(panel);
        lv_label_set_text_fmt(
            selector,
            LV_SYMBOL_LEFT "   %s   " LV_SYMBOL_RIGHT,
            theme_get()->name);
        lv_obj_set_style_text_font(selector, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(selector, t->text, 0);
        lv_obj_align(selector, LV_ALIGN_CENTER, 0, 14);
    } else if (s_popup_page == POPUP_INFO) {
        const gadget_app_t *app = active_app();
        lv_obj_t *info = lv_label_create(panel);
        lv_label_set_text_fmt(
            info,
            "Pedal Display Gadget\n%s",
            app ? app->name : "Launcher");
        lv_obj_set_style_text_font(info, font_small(), 0);
        lv_obj_set_style_text_color(info, t->text, 0);
        lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(info, LV_ALIGN_CENTER, 0, 14);
    } else {
        for (int i = 0; i < s_popup_item_count; i++) {
            const char *label = s_popup_page == POPUP_MONITOR_THEME
                ? monitor_app_preset_name(i)
                : labels[i];
            s_popup_item[i] = lv_label_create(panel);
            lv_label_set_text(s_popup_item[i], label);
            lv_obj_set_style_text_font(
                s_popup_item[i],
                s_popup_page == POPUP_MONITOR_THEME
                    ? font_small()
                    : font_pixel(),
                0);
            lv_obj_align(
                s_popup_item[i],
                LV_ALIGN_TOP_LEFT,
                s_popup_page == POPUP_MONITOR_THEME ? 42 : 54,
                54 + i * (s_popup_page == POPUP_MONITOR_THEME ? 34 : 42));
        }
        popup_highlight();
    }
}

static void popup_open_app_menu(void)
{
    if (s_popup) return;
    s_popup_origin = POPUP_FROM_APP;
    s_popup_return_mode = MODE_LIVE;
    s_popup_page = POPUP_APP_MENU;
    s_popup_sel = 0;
    s_mode = MODE_POPUP;
    popup_build();
}

static void popup_open_launcher_settings(void)
{
    if (s_popup) return;
    s_popup_origin = POPUP_FROM_LAUNCHER;
    s_popup_return_mode = MODE_LAUNCHER;
    s_popup_page = POPUP_SETTINGS;
    s_popup_sel = 0;
    s_mode = MODE_POPUP;
    popup_build();
}

static void popup_move(int delta)
{
    if (s_popup_item_count <= 0) return;
    s_popup_sel =
        (s_popup_sel + delta + s_popup_item_count) % s_popup_item_count;
    popup_highlight();
}

static void popup_adjust_theme(int delta)
{
    if (s_popup_page != POPUP_GLOBAL_THEME || theme_count() <= 0) return;
    theme_set_index(
        (theme_index() + delta + theme_count()) % theme_count());
}

static void enter_app(int idx, int variant)
{
    const gadget_app_t *app = app_registry_at(idx);
    if (!app) return;

    popup_close();
    exit_active_app();
    clean_screen();
    s_mode = MODE_LIVE;
    s_active_app = idx;
    s_return_app = app;
    if (app->on_enter) app->on_enter(variant);
    s_last_view_save_pending = true;
    s_last_view_save_since = lv_tick_get();
}

static void enter_app_ptr(const gadget_app_t *app)
{
    const int idx = app_index(app);
    if (idx >= 0) enter_app(idx, variant_for_app(app));
}

static void enter_launcher(void)
{
    const gadget_app_t *app = active_app();
    if (app) s_return_app = app;

    popup_close();
    exit_active_app();
    s_mode = MODE_LAUNCHER;
    s_reorder_picked = false;
    s_reorder_slot = -1;
    s_last_view_save_pending = false;
    app_slots_set_last_view("");
    build_launcher();
}

static void return_to_live(void)
{
    const gadget_app_t *target = s_return_app;
    if (!target || !app_is_live(target)) target = app_slots_first_live();
    if (target) {
        enter_app_ptr(target);
    } else {
        build_launcher();
    }
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
    if (s_popup_page == POPUP_APP_MENU) {
        if (s_popup_sel == 0) {
            enter_launcher();
        } else {
            popup_set_page(POPUP_SETTINGS);
        }
    } else if (s_popup_page == POPUP_SETTINGS) {
        if (s_popup_sel == 0) {
            const bool monitor_theme =
                s_popup_origin == POPUP_FROM_APP &&
                app_registry_find("monitor") == s_active_app;
            popup_set_page(
                monitor_theme ? POPUP_MONITOR_THEME : POPUP_GLOBAL_THEME);
        } else {
            popup_set_page(POPUP_INFO);
        }
    } else if (s_popup_page == POPUP_MONITOR_THEME) {
        monitor_app_set_preset(s_popup_sel);
        popup_set_page(POPUP_SETTINGS);
    } else if (s_popup_page == POPUP_GLOBAL_THEME ||
               s_popup_page == POPUP_INFO) {
        popup_set_page(POPUP_SETTINGS);
    }
}

static void popup_back(void)
{
    if (s_popup_page == POPUP_APP_MENU) {
        popup_close();
    } else if (s_popup_page == POPUP_SETTINGS) {
        if (s_popup_origin == POPUP_FROM_APP) {
            popup_set_page(POPUP_APP_MENU);
        } else {
            popup_close();
        }
    } else {
        popup_set_page(POPUP_SETTINGS);
    }
}

static void move_cursor_vertical(int delta)
{
    const int max_row = max_launcher_row();
    s_cursor_row += delta;
    if (s_cursor_row < ROW_LIVE) s_cursor_row = max_row;
    if (s_cursor_row > max_row) s_cursor_row = ROW_LIVE;
    clamp_cursor();
}

static void move_cursor_horizontal(int delta)
{
    if (s_cursor_row == ROW_ACTION) {
        s_cursor_col =
            (s_cursor_col + delta + ACTION_COUNT) % ACTION_COUNT;
        return;
    }

    const int count = row_count(s_cursor_row);
    if (count <= 0) return;
    s_cursor_col = (s_cursor_col + delta + count) % count;
    clamp_cursor();
}

static void enter_reorder(void)
{
    s_mode = MODE_REORDER;
    s_reorder_picked = false;
    s_reorder_slot = -1;
    capture_reorder_snapshot();
    clamp_cursor();
    build_launcher();
}

static void exit_reorder(void)
{
    if (s_reorder_picked) restore_reorder_snapshot();
    s_reorder_picked = false;
    s_reorder_slot = -1;
    s_mode = MODE_LAUNCHER;
    clamp_cursor();
    build_launcher();
}

static void launcher_activate(void)
{
    if (s_cursor_row == ROW_ACTION) {
        if (s_cursor_col == ACTION_REORDER) {
            enter_reorder();
        } else if (s_cursor_col == ACTION_SETTINGS) {
            popup_open_launcher_settings();
        }
        return;
    }

    const int slot_idx = selected_slot_index();
    app_slot_t *slot = app_slots_at(slot_idx);
    if (!slot || !slot->app || slot->app->needs_codec) return;
    enter_app_ptr(slot->app);
}

static void reorder_move_horizontal(int delta)
{
    app_slot_t *picked = app_slots_at(s_reorder_slot);
    if (!picked) return;

    normalize_chain(picked->chain);
    int idx[APP_SLOT_MAX];
    const int n = collect_chain(picked->chain, idx);
    int pos = -1;
    for (int i = 0; i < n; i++) {
        if (idx[i] == s_reorder_slot) pos = i;
    }
    if (pos < 0) return;

    const int next = pos + delta;
    if (next < 0 || next >= n) return;
    app_slot_t *other = app_slots_at(idx[next]);
    if (!other) return;

    const uint8_t tmp = picked->order;
    picked->order = other->order;
    other->order = tmp;
    s_cursor_col = next;
    clamp_cursor();
    build_launcher();
}

static void reorder_move_vertical(int delta)
{
    app_slot_t *picked = app_slots_at(s_reorder_slot);
    if (!picked) return;

    const chain_t next_chain = delta < 0 ? CHAIN_LIVE : CHAIN_STASH;
    if (picked->chain == next_chain) return;

    const int next_pos = chain_count(next_chain);
    picked->chain = next_chain;
    picked->order = (uint8_t)next_pos;
    normalize_all_slots();
    s_cursor_row = next_chain == CHAIN_LIVE ? ROW_LIVE : ROW_STASH;
    s_cursor_col = next_pos;
    clamp_cursor();
    build_launcher();
}

static void reorder_pick_or_drop(void)
{
    if (!s_reorder_picked) {
        if (s_cursor_row == ROW_ACTION) {
            if (s_cursor_col == ACTION_REORDER) {
                exit_reorder();
            } else {
                exit_reorder();
                popup_open_launcher_settings();
            }
            return;
        }

        const int slot_idx = selected_slot_index();
        if (slot_idx < 0) return;
        s_reorder_slot = slot_idx;
        s_reorder_picked = true;
        build_launcher();
        return;
    }

    normalize_all_slots();
    app_slots_save();
    capture_reorder_snapshot();
    s_reorder_picked = false;
    s_reorder_slot = -1;
    clamp_cursor();
    build_launcher();
}

static void handle_launcher_event(ui_event_t event)
{
    if (event == EV_HOME || event == EV_FOOTSW) {
        return_to_live();
    } else if (event == EV_FOOTSW_HOLD) {
        footsw_quick_app();
    } else if (event == EV_UP) {
        move_cursor_vertical(-1);
        build_launcher();
    } else if (event == EV_DOWN) {
        move_cursor_vertical(1);
        build_launcher();
    } else if (event == EV_LEFT) {
        move_cursor_horizontal(-1);
        if (s_mode == MODE_LAUNCHER) build_launcher();
    } else if (event == EV_RIGHT) {
        move_cursor_horizontal(1);
        if (s_mode == MODE_LAUNCHER) build_launcher();
    } else if (event == EV_OK) {
        launcher_activate();
    }
}

static void handle_reorder_event(ui_event_t event)
{
    if (event == EV_HOME) {
        exit_reorder();
    } else if (event == EV_FOOTSW) {
        exit_reorder();
        return_to_live();
    } else if (event == EV_FOOTSW_HOLD) {
        exit_reorder();
        footsw_quick_app();
    } else if (event == EV_OK) {
        reorder_pick_or_drop();
    } else if (!s_reorder_picked && event == EV_UP) {
        move_cursor_vertical(-1);
        build_launcher();
    } else if (!s_reorder_picked && event == EV_DOWN) {
        move_cursor_vertical(1);
        build_launcher();
    } else if (!s_reorder_picked && event == EV_LEFT) {
        move_cursor_horizontal(-1);
        build_launcher();
    } else if (!s_reorder_picked && event == EV_RIGHT) {
        move_cursor_horizontal(1);
        build_launcher();
    } else if (s_reorder_picked && event == EV_LEFT) {
        reorder_move_horizontal(-1);
    } else if (s_reorder_picked && event == EV_RIGHT) {
        reorder_move_horizontal(1);
    } else if (s_reorder_picked && event == EV_UP) {
        reorder_move_vertical(-1);
    } else if (s_reorder_picked && event == EV_DOWN) {
        reorder_move_vertical(1);
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
    if (s_active_app == monitor) {
        monitor_app_refresh();
    } else {
        enter_app(images, 0);
    }
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
    plat_init();
    renderers_init();
    apps_init();
    app_slots_init();
    theme_init();
    theme_on_change(on_theme_changed);
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

    if (last_view && last_view[0]) {
        enter_app_ptr(app_slots_first_live());
    } else {
        enter_launcher();
    }
}

void sm_on_event(ui_event_t event)
{
    if (event == EV_HOME_HOLD) {
        enter_launcher();
        return;
    }

    if (s_mode == MODE_POPUP) {
        if (event == EV_UP) {
            popup_move(-1);
        } else if (event == EV_DOWN) {
            popup_move(1);
        } else if (event == EV_LEFT) {
            popup_adjust_theme(-1);
        } else if (event == EV_RIGHT) {
            popup_adjust_theme(1);
        } else if (event == EV_OK) {
            popup_activate();
        } else if (event == EV_HOME) {
            popup_back();
        } else if (event == EV_FOOTSW) {
            popup_close();
            footsw_next_app();
        } else if (event == EV_FOOTSW_HOLD) {
            popup_close();
            footsw_quick_app();
        }
        return;
    }

    if (s_mode == MODE_LAUNCHER) {
        handle_launcher_event(event);
        return;
    }

    if (s_mode == MODE_REORDER) {
        handle_reorder_event(event);
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
        if (s_active_app >= 0) popup_open_app_menu();
        return;
    }

    const gadget_app_t *app = active_app();
    if (app && app->on_event && app->on_event(event)) return;
}

void sm_render(void)
{
    const gadget_app_t *app = active_app();
    if (app && s_last_view_save_pending &&
        (uint32_t)(lv_tick_get() - s_last_view_save_since) >=
            LAST_VIEW_SAVE_DELAY_MS) {
        s_last_view_save_pending = false;
        app_slots_set_last_view(app->id);
    }
    if (s_popup) return;
    if (app && app->on_render) app->on_render();
}

int sm_current(void)
{
    return s_active_app < 0 ? SM_HOME : SM_APP_BASE + s_active_app;
}
