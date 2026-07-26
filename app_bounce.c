#include "gadget_app.h"

#include <stdio.h>
#include <string.h>

#include "music_events.h"
#include "platform.h"
#include "theme.h"

#define SCREEN_W 480
#define SCREEN_H 320
#define GROUND_Y 258

#define CAT_X 76
#define CAT_W 44
#define CAT_H 38
#define CAT_GROUND_Y ((float)(GROUND_Y - CAT_H))

#define CUP_COUNT 2
#define CUP_W 26
#define CUP_H 36
#define CUP_Y (GROUND_Y - CUP_H)

#define GROUND_MARK_COUNT 8
#define GROUND_MARK_SPACING 70
#define GROUND_MARK_W 28

#define GAME_FRAME_MS 40U
#define SCORE_REFRESH_MS 200U
#define GAME_GRAVITY 1500.0f
#define GAME_JUMP_VELOCITY (-590.0f)
#define GAME_START_SPEED 155.0f
#define GAME_MAX_SPEED 270.0f
#define GAME_MAX_DT 0.080f

#define CAT_PRIMARY_COUNT 8
#define CAT_SECONDARY_COUNT 1
#define CAT_DETAIL_COUNT 5
#define NYAN_DETAIL_COUNT 5
#define RAINBOW_COUNT 6

enum {
    BOUNCE_THEME_CLASSIC = 0,
    BOUNCE_THEME_NYAN,
    BOUNCE_THEME_COUNT,
};

static const char *const BOUNCE_THEME_NAMES[BOUNCE_THEME_COUNT] = {
    "Classic Cat",
    "Nyan Cat",
};

static const uint32_t RAINBOW_COLORS[RAINBOW_COUNT] = {
    0xff4f5e,
    0xff9f43,
    0xffdf5d,
    0x50d56f,
    0x4d9eff,
    0xa66cff,
};

typedef struct {
    lv_obj_t *root;
    lv_obj_t *body;
    lv_obj_t *rim;
    lv_obj_t *lip;
    lv_obj_t *stripe;
    float x;
    bool active;
} cup_t;

static lv_obj_t *s_host;
static lv_obj_t *s_cat;
static lv_obj_t *s_cat_shadow;
static lv_obj_t *s_cat_primary[CAT_PRIMARY_COUNT];
static lv_obj_t *s_cat_secondary[CAT_SECONDARY_COUNT];
static lv_obj_t *s_cat_detail[CAT_DETAIL_COUNT];
static lv_obj_t *s_cat_body;
static lv_obj_t *s_nyan_detail[NYAN_DETAIL_COUNT];
static lv_obj_t *s_rainbow[RAINBOW_COUNT];
static lv_obj_t *s_leg_back;
static lv_obj_t *s_leg_front;
static lv_obj_t *s_tail_tip;
static lv_obj_t *s_ground;
static lv_obj_t *s_ground_mark[GROUND_MARK_COUNT];
static lv_obj_t *s_title_label;
static lv_obj_t *s_score_label;
static lv_obj_t *s_best_label;
static lv_obj_t *s_game_over_label;
static cup_t s_cups[CUP_COUNT];

static float s_cat_y;
static float s_cat_vy;
static float s_speed;
static float s_distance;
static float s_spawn_distance;
static float s_ground_offset;
static uint32_t s_score;
static uint32_t s_best;
static uint32_t s_rng;
static uint32_t s_last_frame_ms;
static uint32_t s_last_score_ms;
static uint32_t s_last_onset_seq;
static int s_theme_idx = -1;
static int s_local_theme = BOUNCE_THEME_CLASSIC;
static int s_last_cat_y = -1;
static int s_last_run_phase = -1;
static bool s_game_over;
static char s_score_text[24];
static char s_best_text[24];

static const lv_font_t *font_small(void)
{
#if LV_FONT_UNSCII_8
    return &lv_font_unscii_8;
#else
    return &lv_font_montserrat_12;
#endif
}

static float clampf(float value, float lo, float hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static void style_group(lv_obj_t *obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_0, 0);
}

static void style_block(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
}

static lv_obj_t *make_block(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, x, y);
    style_block(obj, lv_color_hex(0xffffff));
    return obj;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    return label;
}

static void create_cat(void)
{
    int primary = 0;
    int detail = 0;

    s_cat_shadow = make_block(s_host, CAT_X + 4, GROUND_Y + 4, 36, 4);
    lv_obj_set_style_radius(s_cat_shadow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_cat_shadow, LV_OPA_30, 0);

    for (int i = 0; i < RAINBOW_COUNT; i++) {
        s_rainbow[i] = make_block(
            s_host, CAT_X - 54, (int)CAT_GROUND_Y + 13 + i * 3, 62, 3);
    }

    s_cat = lv_obj_create(s_host);
    lv_obj_set_size(s_cat, CAT_W, CAT_H);
    style_group(s_cat);

    s_cat_body = make_block(s_cat, 6, 14, 27, 18);
    s_cat_primary[primary++] = s_cat_body;
    s_nyan_detail[0] = make_block(s_cat, 9, 17, 21, 12);
    s_nyan_detail[1] = make_block(s_cat, 12, 20, 2, 2);
    s_nyan_detail[2] = make_block(s_cat, 18, 24, 2, 2);
    s_nyan_detail[3] = make_block(s_cat, 24, 19, 2, 2);
    s_nyan_detail[4] = make_block(s_cat, 25, 27, 2, 2);
    s_cat_primary[primary++] = make_block(s_cat, 24, 6, 18, 20);
    s_cat_primary[primary++] = make_block(s_cat, 25, 1, 7, 10);
    s_cat_primary[primary++] = make_block(s_cat, 35, 1, 7, 10);
    s_cat_primary[primary++] = make_block(s_cat, 0, 16, 10, 5);
    s_tail_tip = make_block(s_cat, 0, 10, 5, 10);
    s_cat_primary[primary++] = s_tail_tip;
    s_leg_back = make_block(s_cat, 9, 29, 6, 9);
    s_cat_primary[primary++] = s_leg_back;
    s_leg_front = make_block(s_cat, 27, 29, 6, 9);
    s_cat_primary[primary++] = s_leg_front;

    s_cat_secondary[0] = make_block(s_cat, 29, 17, 12, 7);

    s_cat_detail[detail++] = make_block(s_cat, 31, 11, 3, 3);
    s_cat_detail[detail++] = make_block(s_cat, 39, 19, 3, 3);
    s_cat_detail[detail++] = make_block(s_cat, 38, 24, 6, 1);
    s_cat_detail[detail++] = make_block(s_cat, 37, 27, 7, 1);
    s_cat_detail[detail++] = make_block(s_cat, 19, 19, 4, 4);
}

static void create_cup(cup_t *cup)
{
    cup->root = lv_obj_create(s_host);
    lv_obj_set_size(cup->root, CUP_W, CUP_H);
    style_group(cup->root);

    cup->lip = make_block(cup->root, 2, 0, CUP_W - 4, 3);
    cup->rim = make_block(cup->root, 0, 3, CUP_W, 5);
    cup->body = make_block(cup->root, 4, 8, CUP_W - 8, CUP_H - 8);
    cup->stripe = make_block(cup->root, 4, 17, CUP_W - 8, 4);
    cup->active = false;
    cup->x = 0.0f;
    lv_obj_add_flag(cup->root, LV_OBJ_FLAG_HIDDEN);
}

static void style_scene(void)
{
    const ui_theme_t *theme = theme_get();
    const bool nyan = s_local_theme == BOUNCE_THEME_NYAN;
    const lv_color_t cat_color =
        s_game_over ? theme->accent2 : theme->accent;

    if (s_host) {
        lv_obj_set_style_bg_color(s_host, theme->bg, 0);
        lv_obj_set_style_bg_opa(s_host, LV_OPA_COVER, 0);
    }
    if (s_ground) lv_obj_set_style_bg_color(s_ground, theme->grid, 0);
    for (int i = 0; i < GROUND_MARK_COUNT; i++) {
        if (s_ground_mark[i]) {
            lv_obj_set_style_bg_color(s_ground_mark[i], theme->grid, 0);
        }
    }
    if (s_cat_shadow) {
        lv_obj_set_style_bg_color(s_cat_shadow, theme->text, 0);
    }
    for (int i = 0; i < CAT_PRIMARY_COUNT; i++) {
        if (s_cat_primary[i]) {
            lv_obj_set_style_bg_color(
                s_cat_primary[i],
                nyan ? lv_color_hex(0x999999) : cat_color, 0);
        }
    }
    if (nyan && s_cat_body) {
        lv_obj_set_style_bg_color(s_cat_body, lv_color_hex(0xf0c779), 0);
    }
    for (int i = 0; i < CAT_SECONDARY_COUNT; i++) {
        if (s_cat_secondary[i]) {
            lv_obj_set_style_bg_color(
                s_cat_secondary[i],
                nyan ? lv_color_hex(0xd9d9d9) : theme->surface, 0);
        }
    }
    for (int i = 0; i < CAT_DETAIL_COUNT; i++) {
        if (s_cat_detail[i]) {
            lv_obj_set_style_bg_color(
                s_cat_detail[i],
                nyan ? lv_color_hex(0x333333) : theme->bg, 0);
        }
    }
    for (int i = 0; i < NYAN_DETAIL_COUNT; i++) {
        if (!s_nyan_detail[i]) continue;
        if (nyan) {
            lv_obj_remove_flag(s_nyan_detail[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_nyan_detail[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_nyan_detail[0]) {
        lv_obj_set_style_bg_color(s_nyan_detail[0], lv_color_hex(0xe96fa8), 0);
    }
    static const uint32_t sprinkle_colors[NYAN_DETAIL_COUNT - 1] = {
        0xfff06a, 0x65d8ff, 0xffffff, 0xff5e72,
    };
    for (int i = 1; i < NYAN_DETAIL_COUNT; i++) {
        if (s_nyan_detail[i]) {
            lv_obj_set_style_bg_color(
                s_nyan_detail[i], lv_color_hex(sprinkle_colors[i - 1]), 0);
        }
    }
    for (int i = 0; i < RAINBOW_COUNT; i++) {
        if (!s_rainbow[i]) continue;
        lv_obj_set_style_bg_color(
            s_rainbow[i], lv_color_hex(RAINBOW_COLORS[i]), 0);
        lv_obj_set_style_bg_opa(
            s_rainbow[i], s_game_over ? LV_OPA_40 : LV_OPA_COVER, 0);
        if (nyan) {
            lv_obj_remove_flag(s_rainbow[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_rainbow[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    for (int i = 0; i < CUP_COUNT; i++) {
        if (!s_cups[i].root) continue;
        lv_obj_set_style_bg_color(s_cups[i].body, theme->text, 0);
        lv_obj_set_style_bg_color(s_cups[i].rim, theme->text, 0);
        lv_obj_set_style_bg_color(s_cups[i].lip, theme->text, 0);
        lv_obj_set_style_bg_color(s_cups[i].stripe, theme->accent2, 0);
    }
    if (s_score_label) {
        lv_obj_set_style_text_color(s_score_label, theme->text, 0);
    }
    if (s_title_label) {
        lv_obj_set_style_text_color(s_title_label, theme->accent, 0);
    }
    if (s_best_label) {
        lv_obj_set_style_text_color(s_best_label, theme->text, 0);
        lv_obj_set_style_text_opa(s_best_label, LV_OPA_50, 0);
    }
    if (s_game_over_label) {
        lv_obj_set_style_text_color(s_game_over_label, theme->accent2, 0);
    }
}

static uint32_t next_random(void)
{
    s_rng = s_rng * 1664525U + 1013904223U;
    return s_rng;
}

static float next_spawn_gap(void)
{
    float speed_margin = (s_speed - GAME_START_SPEED) * 0.55f;
    return 250.0f + (float)(next_random() % 150U) + speed_margin;
}

static bool cat_is_grounded(void)
{
    return s_cat_y >= CAT_GROUND_Y - 0.5f;
}

static void set_cup_active(cup_t *cup, bool active)
{
    cup->active = active;
    if (!cup->root) return;
    if (active) {
        lv_obj_remove_flag(cup->root, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(cup->root, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_score_labels(uint32_t now, bool force)
{
    if (!force && (uint32_t)(now - s_last_score_ms) < SCORE_REFRESH_MS) {
        return;
    }
    s_last_score_ms = now;

    char text[24];
    snprintf(text, sizeof(text), "SCORE %05lu", (unsigned long)s_score);
    if (strcmp(text, s_score_text) != 0) {
        snprintf(s_score_text, sizeof(s_score_text), "%s", text);
        lv_label_set_text(s_score_label, s_score_text);
    }

    snprintf(text, sizeof(text), "BEST %05lu", (unsigned long)s_best);
    if (strcmp(text, s_best_text) != 0) {
        snprintf(s_best_text, sizeof(s_best_text), "%s", text);
        lv_label_set_text(s_best_label, s_best_text);
    }
}

static void reset_game(uint32_t onset_seq)
{
    s_cat_y = CAT_GROUND_Y;
    s_cat_vy = 0.0f;
    s_speed = GAME_START_SPEED;
    s_distance = 0.0f;
    s_spawn_distance = 330.0f;
    s_ground_offset = 0.0f;
    s_score = 0;
    s_game_over = false;
    s_last_onset_seq = onset_seq;
    s_last_cat_y = -1;
    s_last_run_phase = -1;
    s_score_text[0] = '\0';
    s_best_text[0] = '\0';

    for (int i = 0; i < CUP_COUNT; i++) {
        set_cup_active(&s_cups[i], false);
    }
    if (s_game_over_label) {
        lv_obj_add_flag(s_game_over_label, LV_OBJ_FLAG_HIDDEN);
    }
    style_scene();
    update_score_labels(plat_millis(), true);
}

static void jump(float onset_strength)
{
    if (!cat_is_grounded() || s_game_over) return;

    float strength = clampf((onset_strength - 1.0f) * 0.25f, 0.0f, 1.0f);
    s_cat_vy = GAME_JUMP_VELOCITY * (0.94f + 0.10f * strength);
}

static void restart_and_jump(float onset_strength, uint32_t onset_seq)
{
    reset_game(onset_seq);
    jump(onset_strength);
}

static void spawn_cup(void)
{
    for (int i = 0; i < CUP_COUNT; i++) {
        if (s_cups[i].active) continue;
        s_cups[i].x = SCREEN_W + 8.0f;
        set_cup_active(&s_cups[i], true);
        lv_obj_set_pos(s_cups[i].root, (int)s_cups[i].x, CUP_Y);
        s_spawn_distance = next_spawn_gap();
        return;
    }
    s_spawn_distance = 60.0f;
}

static bool collision_with(const cup_t *cup)
{
    if (!cup->active) return false;

    const float cat_left = CAT_X + 7.0f;
    const float cat_right = CAT_X + CAT_W - 5.0f;
    const float cat_top = s_cat_y + 5.0f;
    const float cat_bottom = s_cat_y + CAT_H - 2.0f;
    const float cup_left = cup->x + 3.0f;
    const float cup_right = cup->x + CUP_W - 3.0f;
    const float cup_top = CUP_Y + 4.0f;
    const float cup_bottom = GROUND_Y;

    return cat_right > cup_left && cat_left < cup_right &&
           cat_bottom > cup_top && cat_top < cup_bottom;
}

static void end_game(void)
{
    if (s_game_over) return;
    s_game_over = true;
    s_cat_vy = 0.0f;
    if (s_score > s_best) s_best = s_score;

    char text[48];
    snprintf(text, sizeof(text), "GAME OVER\nSCORE %05lu",
             (unsigned long)s_score);
    lv_label_set_text(s_game_over_label, text);
    lv_obj_remove_flag(s_game_over_label, LV_OBJ_FLAG_HIDDEN);
    update_score_labels(plat_millis(), true);
    style_scene();
}

static void update_cat_pose(uint32_t now)
{
    int cat_y = (int)(s_cat_y + 0.5f);
    if (cat_y != s_last_cat_y) {
        lv_obj_set_pos(s_cat, CAT_X, cat_y);
        for (int i = 0; i < RAINBOW_COUNT; i++) {
            if (s_rainbow[i]) lv_obj_set_y(s_rainbow[i], cat_y + 13 + i * 3);
        }
        s_last_cat_y = cat_y;
    }

    const bool grounded = cat_is_grounded();
    int run_phase = grounded && !s_game_over ? (int)((now / 120U) & 1U) : 2;
    if (run_phase != s_last_run_phase) {
        if (run_phase == 0) {
            lv_obj_set_y(s_leg_back, 29);
            lv_obj_set_y(s_leg_front, 31);
            lv_obj_set_y(s_tail_tip, 10);
        } else if (run_phase == 1) {
            lv_obj_set_y(s_leg_back, 31);
            lv_obj_set_y(s_leg_front, 29);
            lv_obj_set_y(s_tail_tip, 12);
        } else {
            lv_obj_set_y(s_leg_back, 27);
            lv_obj_set_y(s_leg_front, 27);
            lv_obj_set_y(s_tail_tip, 11);
        }
        for (int i = 0; i < RAINBOW_COUNT; i++) {
            if (s_rainbow[i]) {
                lv_obj_set_x(s_rainbow[i], CAT_X - 54 - (run_phase & 1) * 3);
            }
        }
        s_last_run_phase = run_phase;
    }

    float height = CAT_GROUND_Y - s_cat_y;
    int shadow_width = (int)clampf(36.0f - height * 0.18f, 18.0f, 36.0f);
    lv_obj_set_width(s_cat_shadow, shadow_width);
    lv_obj_set_x(s_cat_shadow, CAT_X + (CAT_W - shadow_width) / 2);
    lv_obj_set_style_bg_opa(
        s_cat_shadow,
        (lv_opa_t)clampf(80.0f - height * 0.45f, 20.0f, 80.0f), 0);
}

static void update_ground(float distance)
{
    s_ground_offset -= distance;
    while (s_ground_offset <= -GROUND_MARK_SPACING) {
        s_ground_offset += GROUND_MARK_SPACING;
    }
    for (int i = 0; i < GROUND_MARK_COUNT; i++) {
        int x = (int)s_ground_offset + i * GROUND_MARK_SPACING;
        lv_obj_set_x(s_ground_mark[i], x);
    }
}

static void update_game(float dt, uint32_t now)
{
    s_speed = clampf(GAME_START_SPEED + (float)s_score * 0.45f,
                     GAME_START_SPEED, GAME_MAX_SPEED);
    const float travel = s_speed * dt;
    s_distance += travel;
    s_score = (uint32_t)(s_distance / 11.0f);
    s_spawn_distance -= travel;
    update_ground(travel);

    s_cat_vy += GAME_GRAVITY * dt;
    s_cat_y += s_cat_vy * dt;
    if (s_cat_y >= CAT_GROUND_Y) {
        s_cat_y = CAT_GROUND_Y;
        s_cat_vy = 0.0f;
    }

    for (int i = 0; i < CUP_COUNT; i++) {
        cup_t *cup = &s_cups[i];
        if (!cup->active) continue;
        cup->x -= travel;
        if (cup->x < -CUP_W) {
            set_cup_active(cup, false);
            continue;
        }
        lv_obj_set_x(cup->root, (int)(cup->x + 0.5f));
        if (collision_with(cup)) {
            end_game();
            break;
        }
    }

    if (!s_game_over && s_spawn_distance <= 0.0f) spawn_cup();
    update_cat_pose(now);
    update_score_labels(now, false);
}

static void bounce_enter(int variant)
{
    (void)variant;
    audio_set_mode(AUDIO_SPECTRUM);
    audio_set_viz_mode(VIZ_DECOR);

    music_snapshot_t snapshot;
    plat_music_get(&snapshot);

    s_host = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_host, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_host, 0, 0);
    style_group(s_host);

    s_title_label = make_label(
        s_host, "CAT RUN", &lv_font_montserrat_14);
    lv_obj_set_pos(s_title_label, 12, 9);

    s_best_label = make_label(s_host, "BEST 00000", font_small());
    lv_obj_set_width(s_best_label, 130);
    lv_obj_set_style_text_align(s_best_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_best_label, 196, 10);

    s_score_label = make_label(s_host, "SCORE 00000", font_small());
    lv_obj_set_width(s_score_label, 138);
    lv_obj_set_style_text_align(s_score_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_score_label, 330, 10);

    s_ground = make_block(s_host, 0, GROUND_Y, SCREEN_W, 2);
    for (int i = 0; i < GROUND_MARK_COUNT; i++) {
        s_ground_mark[i] = make_block(
            s_host, i * GROUND_MARK_SPACING, GROUND_Y + 10,
            GROUND_MARK_W, 2);
    }

    create_cat();
    for (int i = 0; i < CUP_COUNT; i++) create_cup(&s_cups[i]);

    s_game_over_label = make_label(
        s_host, "GAME OVER", &lv_font_montserrat_28);
    lv_obj_set_width(s_game_over_label, 250);
    lv_obj_set_style_text_align(s_game_over_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_game_over_label, 115, 92);
    lv_obj_add_flag(s_game_over_label, LV_OBJ_FLAG_HIDDEN);

    s_rng = plat_millis() ^ 0x9e3779b9U;
    s_last_frame_ms = plat_millis();
    s_last_score_ms = s_last_frame_ms;
    s_theme_idx = theme_index();
    reset_game(snapshot.onset_seq);
    update_cat_pose(s_last_frame_ms);
}

static void bounce_exit(void)
{
    if (s_host) lv_obj_delete(s_host);
    s_host = NULL;
    s_cat = NULL;
    s_cat_shadow = NULL;
    s_cat_body = NULL;
    s_leg_back = NULL;
    s_leg_front = NULL;
    s_tail_tip = NULL;
    s_ground = NULL;
    s_title_label = NULL;
    s_score_label = NULL;
    s_best_label = NULL;
    s_game_over_label = NULL;
    memset(s_cat_primary, 0, sizeof(s_cat_primary));
    memset(s_cat_secondary, 0, sizeof(s_cat_secondary));
    memset(s_cat_detail, 0, sizeof(s_cat_detail));
    memset(s_nyan_detail, 0, sizeof(s_nyan_detail));
    memset(s_rainbow, 0, sizeof(s_rainbow));
    memset(s_ground_mark, 0, sizeof(s_ground_mark));
    memset(s_cups, 0, sizeof(s_cups));
}

static void bounce_render(void)
{
    if (!s_host || !s_cat) return;

    music_snapshot_t snapshot;
    plat_music_get(&snapshot);

    const int theme_idx = theme_index();
    if (theme_idx != s_theme_idx) {
        s_theme_idx = theme_idx;
        style_scene();
    }

    if (snapshot.onset_seq != s_last_onset_seq) {
        s_last_onset_seq = snapshot.onset_seq;
        if (s_game_over) {
            restart_and_jump(snapshot.onset_strength, snapshot.onset_seq);
        } else {
            jump(snapshot.onset_strength);
        }
    }

    uint32_t now = plat_millis();
    uint32_t elapsed_ms = now - s_last_frame_ms;
    if (elapsed_ms < GAME_FRAME_MS) return;
    s_last_frame_ms = now;
    float dt = clampf((float)elapsed_ms * 0.001f, 0.0f, GAME_MAX_DT);

    if (!s_game_over) {
        update_game(dt, now);
    } else {
        update_cat_pose(now);
    }
}

static bool bounce_on_event(ui_event_t event)
{
    if (event == EV_OK || event == EV_UP) {
        if (s_game_over) {
            restart_and_jump(1.5f, s_last_onset_seq);
        } else {
            jump(1.5f);
        }
        return true;
    }
    if (event == EV_DOWN || event == EV_LEFT || event == EV_RIGHT) {
        return true;
    }
    return false;
}

static int bounce_theme_count(void)
{
    return BOUNCE_THEME_COUNT;
}

static const char *bounce_theme_name(int idx)
{
    return idx >= 0 && idx < BOUNCE_THEME_COUNT
        ? BOUNCE_THEME_NAMES[idx]
        : "";
}

static int bounce_theme_index(void)
{
    return s_local_theme;
}

static void bounce_theme_set(int idx)
{
    if (idx < 0 || idx >= BOUNCE_THEME_COUNT) return;
    s_local_theme = idx;
    style_scene();
}

#ifdef PEDAL_SIM
int bounce_app_debug_cat_y(void)
{
    return (int)(s_cat_y + 0.5f);
}

bool bounce_app_debug_game_over(void)
{
    return s_game_over;
}

int bounce_app_debug_theme_index(void)
{
    return s_local_theme;
}
#endif

const gadget_app_t APP_BOUNCE = {
    .id = "bounce",
    .name = "Bounce",
    .audio_mode = AUDIO_SPECTRUM,
    .icon = NULL,
    .on_enter = bounce_enter,
    .on_exit = bounce_exit,
    .on_render = bounce_render,
    .on_event = bounce_on_event,
    .local_theme_count = bounce_theme_count,
    .local_theme_name = bounce_theme_name,
    .local_theme_index = bounce_theme_index,
    .local_theme_set = bounce_theme_set,
};
