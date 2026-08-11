#include "gadget_app.h"

#include <stdio.h>
#include <string.h>

#include "app_slots.h"
#include "audio_effects.h"
#include "audio_playback.h"
#include "bounce_game.h"
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

#define OBSTACLE_COUNT 2
#define OBSTACLE_W 24
#define OBSTACLE_H 40
#define OBSTACLE_Y (GROUND_Y - OBSTACLE_H)

#define GROUND_MARK_COUNT 8
#define GROUND_MARK_SPACING 70
#define GROUND_MARK_W 28
#define CLOUD_COUNT 3

#define GAME_FRAME_MS 40U
#define SCORE_REFRESH_MS 200U
#define GAME_CLEAR_TIME_MS 3000U
#define GAME_OVER_CLEAR_MS 750U
#define GAME_REFERENCE_FPS 60.0f
#define GAME_GRAVITY 2160.0f
#define GAME_JUMP_VELOCITY (-720.0f)
#define GAME_DROP_VELOCITY 180.0f
#define GAME_START_SPEED 345.6f
#define GAME_MAX_SPEED 720.0f
#define GAME_ACCELERATION 3.6f
#define GAME_SCORE_COEFFICIENT 0.025f
#define GAME_GAP_COEFFICIENT 0.6f
#define GAME_OBSTACLE_MIN_GAP 120.0f
#define GAME_MAX_GAP_COEFFICIENT 1.5f
#define GAME_AUDIO_JUMP_THRESHOLD 0.35f
#define GAME_AUDIO_REARM_THRESHOLD 0.25f
#define GAME_MAX_DT 0.080f

#define CAT_PRIMARY_COUNT 9
#define CAT_SECONDARY_COUNT 3
#define CAT_DETAIL_COUNT 5

typedef struct {
    lv_obj_t *root;
    lv_obj_t *trunk;
    lv_obj_t *left_stem;
    lv_obj_t *left_arm;
    lv_obj_t *right_stem;
    lv_obj_t *right_arm;
    float x;
    bool active;
    bool passed;
} obstacle_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *parts[3];
    float x;
} cloud_t;

static lv_obj_t *s_host;
static lv_obj_t *s_cat;
static lv_obj_t *s_cat_shadow;
static lv_obj_t *s_cat_primary[CAT_PRIMARY_COUNT];
static lv_obj_t *s_cat_secondary[CAT_SECONDARY_COUNT];
static lv_obj_t *s_cat_detail[CAT_DETAIL_COUNT];
static lv_obj_t *s_leg_back;
static lv_obj_t *s_leg_front;
static lv_obj_t *s_tail_tip;
static lv_obj_t *s_ground;
static lv_obj_t *s_ground_mark[GROUND_MARK_COUNT];
static lv_obj_t *s_title_label;
static lv_obj_t *s_score_label;
static lv_obj_t *s_best_label;
static lv_obj_t *s_game_over_label;
static obstacle_t s_obstacles[OBSTACLE_COUNT];
static cloud_t s_clouds[CLOUD_COUNT];

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
static uint32_t s_game_over_ms;
static int s_last_cat_y = -1;
static int s_last_run_phase = -1;
static bool s_game_over;
static bool s_running;
static bool s_audio_level_armed;
static char s_score_text[24];
static char s_best_text[24];
static const gadget_app_t *s_appearance_owner;
static const char *s_embedded_title;
static const char *s_audio_owner_id;
static uint32_t s_effect_suppress_until;
static bool s_audio_claimed;

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

    s_cat = lv_obj_create(s_host);
    lv_obj_set_size(s_cat, CAT_W, CAT_H);
    style_group(s_cat);

    s_cat_primary[primary] = make_block(s_cat, 8, 15, 25, 16);
    lv_obj_set_style_radius(s_cat_primary[primary++], 3, 0);
    s_cat_primary[primary] = make_block(s_cat, 25, 7, 18, 19);
    lv_obj_set_style_radius(s_cat_primary[primary++], 3, 0);
    s_cat_primary[primary++] = make_block(s_cat, 26, 1, 7, 9);
    s_cat_primary[primary++] = make_block(s_cat, 37, 1, 6, 9);
    s_cat_primary[primary++] = make_block(s_cat, 1, 16, 9, 5);
    s_cat_primary[primary++] = make_block(s_cat, 0, 10, 5, 10);
    s_tail_tip = make_block(s_cat, 2, 6, 7, 5);
    s_cat_primary[primary++] = s_tail_tip;
    s_leg_back = make_block(s_cat, 12, 29, 6, 9);
    s_cat_primary[primary++] = s_leg_back;
    s_leg_front = make_block(s_cat, 27, 29, 6, 9);
    s_cat_primary[primary++] = s_leg_front;

    s_cat_secondary[0] = make_block(s_cat, 32, 17, 11, 7);
    s_cat_secondary[1] = make_block(s_cat, 28, 3, 3, 5);
    s_cat_secondary[2] = make_block(s_cat, 39, 3, 2, 5);

    s_cat_detail[detail++] = make_block(s_cat, 31, 11, 3, 3);
    s_cat_detail[detail++] = make_block(s_cat, 40, 18, 3, 2);
    s_cat_detail[detail++] = make_block(s_cat, 38, 22, 4, 1);
    s_cat_detail[detail++] = make_block(s_cat, 37, 24, 7, 1);
    s_cat_detail[detail++] = make_block(s_cat, 36, 27, 8, 1);
}

static void create_obstacle(obstacle_t *obstacle)
{
    obstacle->root = lv_obj_create(s_host);
    lv_obj_set_size(obstacle->root, OBSTACLE_W, OBSTACLE_H);
    style_group(obstacle->root);

    obstacle->trunk = make_block(obstacle->root, 9, 0, 7, OBSTACLE_H);
    obstacle->left_stem = make_block(obstacle->root, 2, 8, 5, 15);
    obstacle->left_arm = make_block(obstacle->root, 5, 17, 7, 6);
    obstacle->right_stem = make_block(obstacle->root, 19, 14, 4, 14);
    obstacle->right_arm = make_block(obstacle->root, 13, 22, 8, 6);
    obstacle->active = false;
    obstacle->passed = false;
    obstacle->x = 0.0f;
    lv_obj_add_flag(obstacle->root, LV_OBJ_FLAG_HIDDEN);
}

static void create_cloud(cloud_t *cloud, int index)
{
    cloud->root = lv_obj_create(s_host);
    lv_obj_set_size(cloud->root, 48, 18);
    lv_obj_set_pos(cloud->root, 120 + index * 170, 48 + index * 28);
    style_group(cloud->root);
    cloud->parts[0] = make_block(cloud->root, 0, 10, 48, 6);
    cloud->parts[1] = make_block(cloud->root, 10, 4, 17, 10);
    cloud->parts[2] = make_block(cloud->root, 25, 1, 16, 13);
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_radius(cloud->parts[i], LV_RADIUS_CIRCLE, 0);
    }
    cloud->x = (float)(120 + index * 170);
}

static void style_scene(void)
{
    const gadget_app_t *owner = s_appearance_owner
        ? s_appearance_owner : &APP_GAME;
    const ui_theme_t *theme =
        theme_for_app_color(app_slots_color(owner));
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
    for (int i = 0; i < CLOUD_COUNT; i++) {
        for (int part = 0; part < 3; part++) {
            if (s_clouds[i].parts[part]) {
                lv_obj_set_style_bg_color(
                    s_clouds[i].parts[part], theme->grid, 0);
                lv_obj_set_style_bg_opa(
                    s_clouds[i].parts[part], LV_OPA_30, 0);
            }
        }
    }
    if (s_cat_shadow) {
        lv_obj_set_style_bg_color(s_cat_shadow, theme->text, 0);
    }
    for (int i = 0; i < CAT_PRIMARY_COUNT; i++) {
        if (s_cat_primary[i]) {
            lv_obj_set_style_bg_color(s_cat_primary[i], cat_color, 0);
        }
    }
    for (int i = 0; i < CAT_SECONDARY_COUNT; i++) {
        if (s_cat_secondary[i]) {
            lv_obj_set_style_bg_color(s_cat_secondary[i], theme->surface, 0);
        }
    }
    for (int i = 0; i < CAT_DETAIL_COUNT; i++) {
        if (s_cat_detail[i]) {
            lv_obj_set_style_bg_color(s_cat_detail[i], theme->bg, 0);
        }
    }
    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        obstacle_t *obstacle = &s_obstacles[i];
        if (!obstacle->root) continue;
        lv_obj_set_style_bg_color(obstacle->trunk, theme->text, 0);
        lv_obj_set_style_bg_color(obstacle->left_stem, theme->text, 0);
        lv_obj_set_style_bg_color(obstacle->left_arm, theme->text, 0);
        lv_obj_set_style_bg_color(obstacle->right_stem, theme->text, 0);
        lv_obj_set_style_bg_color(obstacle->right_arm, theme->text, 0);
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
    const float speed_per_frame = s_speed / GAME_REFERENCE_FPS;
    const float minimum = OBSTACLE_W * speed_per_frame +
                          GAME_OBSTACLE_MIN_GAP * GAME_GAP_COEFFICIENT;
    const float maximum = minimum * GAME_MAX_GAP_COEFFICIENT;
    const uint32_t span = (uint32_t)(maximum - minimum + 1.0f);
    return minimum + (float)(next_random() % (span > 0 ? span : 1U));
}

static bool cat_is_grounded(void)
{
    return s_cat_y >= CAT_GROUND_Y - 0.5f;
}

static void set_obstacle_active(obstacle_t *obstacle, bool active)
{
    obstacle->active = active;
    if (!obstacle->root) return;
    if (active) {
        lv_obj_remove_flag(obstacle->root, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obstacle->root, LV_OBJ_FLAG_HIDDEN);
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
    s_spawn_distance =
        GAME_START_SPEED * ((float)GAME_CLEAR_TIME_MS * 0.001f);
    s_ground_offset = 0.0f;
    s_score = 0;
    s_game_over = false;
    s_running = false;
    s_audio_level_armed = true;
    s_game_over_ms = 0;
    s_last_onset_seq = onset_seq;
    s_last_cat_y = -1;
    s_last_run_phase = -1;
    s_score_text[0] = '\0';
    s_best_text[0] = '\0';

    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        set_obstacle_active(&s_obstacles[i], false);
    }
    for (int i = 0; i < CLOUD_COUNT; i++) {
        s_clouds[i].x = (float)(120 + i * 170);
        if (s_clouds[i].root) {
            lv_obj_set_pos(
                s_clouds[i].root, (int)s_clouds[i].x, 48 + i * 28);
        }
    }
    if (s_game_over_label) {
        lv_obj_add_flag(s_game_over_label, LV_OBJ_FLAG_HIDDEN);
    }
    style_scene();
    update_score_labels(plat_millis(), true);
}

static void play_effect(audio_effect_t effect)
{
    if (!s_audio_claimed || !s_audio_owner_id) return;
    if (audio_effects_play(s_audio_owner_id, effect)) {
        s_effect_suppress_until = plat_millis() + 120u;
    }
}

static bool jump(float onset_strength, bool with_effect)
{
    if (!cat_is_grounded() || s_game_over) return false;

    (void)onset_strength;
    s_running = true;
    s_cat_vy = GAME_JUMP_VELOCITY - s_speed * 0.1f;
    if (with_effect) play_effect(AUDIO_EFFECT_JUMP);
    return true;
}

static void restart_and_jump(float onset_strength, uint32_t onset_seq,
                             bool with_effect)
{
    reset_game(onset_seq);
    (void)jump(onset_strength, with_effect);
}

static void spawn_obstacle(void)
{
    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        obstacle_t *obstacle = &s_obstacles[i];
        if (obstacle->active) continue;
        obstacle->x = SCREEN_W + 8.0f;
        obstacle->passed = false;
        set_obstacle_active(obstacle, true);
        lv_obj_set_pos(
            obstacle->root, (int)obstacle->x, OBSTACLE_Y);
        s_spawn_distance = next_spawn_gap();
        return;
    }
    s_spawn_distance = 60.0f;
}

static bool collision_with(const obstacle_t *obstacle)
{
    if (!obstacle->active) return false;

    const float cat_left = CAT_X + 7.0f;
    const float cat_right = CAT_X + CAT_W - 5.0f;
    const float cat_top = s_cat_y + 5.0f;
    const float cat_bottom = s_cat_y + CAT_H - 2.0f;
    const float obstacle_left = obstacle->x + 4.0f;
    const float obstacle_right = obstacle->x + OBSTACLE_W - 3.0f;
    const float obstacle_top = OBSTACLE_Y + 3.0f;
    const float obstacle_bottom = GROUND_Y;

    return cat_right > obstacle_left && cat_left < obstacle_right &&
           cat_bottom > obstacle_top && cat_top < obstacle_bottom;
}

static void end_game(void)
{
    if (s_game_over) return;
    s_game_over = true;
    s_running = false;
    s_game_over_ms = plat_millis();
    s_cat_vy = 0.0f;
    if (s_score > s_best) s_best = s_score;

    char text[48];
    snprintf(text, sizeof(text), "GAME OVER\nSCORE %05lu",
             (unsigned long)s_score);
    lv_label_set_text(s_game_over_label, text);
    lv_obj_remove_flag(s_game_over_label, LV_OBJ_FLAG_HIDDEN);
    update_score_labels(plat_millis(), true);
    style_scene();
    play_effect(AUDIO_EFFECT_HIT);
}

static void update_cat_pose(uint32_t now)
{
    int cat_y = (int)(s_cat_y + 0.5f);
    if (cat_y != s_last_cat_y) {
        lv_obj_set_pos(s_cat, CAT_X, cat_y);
        s_last_cat_y = cat_y;
    }

    const bool grounded = cat_is_grounded();
    int run_phase = grounded && s_running && !s_game_over
        ? (int)((now / 120U) & 1U) : 2;
    if (run_phase != s_last_run_phase) {
        if (run_phase == 0) {
            lv_obj_set_y(s_leg_back, 29);
            lv_obj_set_y(s_leg_front, 31);
            lv_obj_set_y(s_tail_tip, 6);
        } else if (run_phase == 1) {
            lv_obj_set_y(s_leg_back, 31);
            lv_obj_set_y(s_leg_front, 29);
            lv_obj_set_y(s_tail_tip, 8);
        } else {
            lv_obj_set_y(s_leg_back, 27);
            lv_obj_set_y(s_leg_front, 27);
            lv_obj_set_y(s_tail_tip, 7);
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

static void update_clouds(float distance)
{
    for (int i = 0; i < CLOUD_COUNT; i++) {
        cloud_t *cloud = &s_clouds[i];
        if (!cloud->root) continue;
        cloud->x -= distance * 0.04f;
        if (cloud->x < -48.0f) {
            cloud->x = SCREEN_W + 30.0f +
                       (float)(next_random() % 150U);
            lv_obj_set_y(cloud->root, 38 + (int)(next_random() % 76U));
        }
        lv_obj_set_x(cloud->root, (int)(cloud->x + 0.5f));
    }
}

static void update_game(float dt, uint32_t now)
{
    s_speed = clampf(s_speed + GAME_ACCELERATION * dt,
                     GAME_START_SPEED, GAME_MAX_SPEED);
    const float travel = s_speed * dt;
    s_distance += travel;
    s_score = (uint32_t)(s_distance * GAME_SCORE_COEFFICIENT + 0.5f);
    s_spawn_distance -= travel;
    update_ground(travel);
    update_clouds(travel);

    s_cat_vy += GAME_GRAVITY * dt;
    s_cat_y += s_cat_vy * dt;
    if (s_cat_y >= CAT_GROUND_Y) {
        s_cat_y = CAT_GROUND_Y;
        s_cat_vy = 0.0f;
    }

    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        obstacle_t *obstacle = &s_obstacles[i];
        if (!obstacle->active) continue;
        obstacle->x -= travel;
        if (!obstacle->passed &&
            obstacle->x + OBSTACLE_W < CAT_X + 7.0f) {
            obstacle->passed = true;
            play_effect(AUDIO_EFFECT_SCORE);
        }
        if (obstacle->x < -OBSTACLE_W) {
            set_obstacle_active(obstacle, false);
            continue;
        }
        lv_obj_set_x(obstacle->root, (int)(obstacle->x + 0.5f));
        if (collision_with(obstacle)) {
            end_game();
            break;
        }
    }

    if (!s_game_over && s_spawn_distance <= 0.0f) spawn_obstacle();
    update_cat_pose(now);
    update_score_labels(now, false);
}

static void bounce_enter(int variant)
{
    (void)variant;
    if (!s_appearance_owner) s_appearance_owner = &APP_GAME;
    if (!s_embedded_title) s_embedded_title = "";
    audio_set_mode(AUDIO_SPECTRUM);
    audio_set_viz_mode(VIZ_DECOR);

    music_snapshot_t snapshot;
    plat_music_get(&snapshot);

    s_host = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_host, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_host, 0, 0);
    style_group(s_host);

    if (s_embedded_title[0]) {
        s_title_label = make_label(
            s_host, s_embedded_title, &lv_font_montserrat_14);
        lv_obj_set_pos(s_title_label, 12, 9);
    }

    s_best_label = make_label(s_host, "BEST 00000", font_small());
    lv_obj_set_width(s_best_label, 130);
    lv_obj_set_style_text_align(s_best_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_best_label, 196, 10);

    s_score_label = make_label(s_host, "SCORE 00000", font_small());
    lv_obj_set_width(s_score_label, 138);
    lv_obj_set_style_text_align(s_score_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_score_label, 330, 10);

    for (int i = 0; i < CLOUD_COUNT; i++) create_cloud(&s_clouds[i], i);

    s_ground = make_block(s_host, 0, GROUND_Y, SCREEN_W, 2);
    for (int i = 0; i < GROUND_MARK_COUNT; i++) {
        s_ground_mark[i] = make_block(
            s_host, i * GROUND_MARK_SPACING, GROUND_Y + 10,
            GROUND_MARK_W, 2);
    }

    create_cat();
    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        create_obstacle(&s_obstacles[i]);
    }

    s_game_over_label = make_label(
        s_host, "GAME OVER", &lv_font_montserrat_28);
    lv_obj_set_width(s_game_over_label, 250);
    lv_obj_set_style_text_align(s_game_over_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_game_over_label, 115, 92);
    lv_obj_add_flag(s_game_over_label, LV_OBJ_FLAG_HIDDEN);

    s_rng = plat_millis() ^ 0x9e3779b9U;
    s_audio_owner_id = s_appearance_owner->id;
    s_audio_claimed = audio_playback_claim(s_audio_owner_id) ==
                      AUDIO_PLAYBACK_OK;
    if (s_audio_claimed) {
        (void)audio_playback_set_bus_gain(
            s_audio_owner_id, AUDIO_PLAYBACK_BUS_EFFECTS, 0.72f);
        (void)audio_playback_play(s_audio_owner_id);
    }
    s_effect_suppress_until = 0u;
    s_last_frame_ms = plat_millis();
    s_last_score_ms = s_last_frame_ms;
    reset_game(snapshot.onset_seq);
    update_cat_pose(s_last_frame_ms);
}

static void bounce_exit(void)
{
    if (s_audio_claimed && s_audio_owner_id) {
        (void)audio_playback_release(s_audio_owner_id);
    }
    s_audio_claimed = false;
    s_audio_owner_id = NULL;
    if (s_host) lv_obj_delete(s_host);
    s_host = NULL;
    s_cat = NULL;
    s_cat_shadow = NULL;
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
    memset(s_ground_mark, 0, sizeof(s_ground_mark));
    memset(s_obstacles, 0, sizeof(s_obstacles));
    memset(s_clouds, 0, sizeof(s_clouds));
    s_appearance_owner = NULL;
    s_embedded_title = NULL;
}

static void bounce_render(void)
{
    if (!s_host || !s_cat) return;

    music_snapshot_t snapshot;
    plat_music_get(&snapshot);

    const uint32_t now = plat_millis();
    if (snapshot.level <= GAME_AUDIO_REARM_THRESHOLD) {
        s_audio_level_armed = true;
    }
    if (snapshot.onset_seq != s_last_onset_seq) {
        s_last_onset_seq = snapshot.onset_seq;
        if ((int32_t)(now - s_effect_suppress_until) >= 0) {
            if (s_game_over) {
                if ((uint32_t)(now - s_game_over_ms) >=
                    GAME_OVER_CLEAR_MS) {
                    restart_and_jump(snapshot.onset_strength,
                                     snapshot.onset_seq, false);
                }
            } else {
                (void)jump(snapshot.onset_strength, false);
            }
        }
    }
    if (s_audio_level_armed &&
        snapshot.level >= GAME_AUDIO_JUMP_THRESHOLD) {
        s_audio_level_armed = false;
        if ((int32_t)(now - s_effect_suppress_until) >= 0) {
            if (s_game_over) {
                if ((uint32_t)(now - s_game_over_ms) >=
                    GAME_OVER_CLEAR_MS) {
                    restart_and_jump(1.0f, snapshot.onset_seq, false);
                }
            } else {
                (void)jump(1.0f, false);
            }
        }
    }

    uint32_t elapsed_ms = now - s_last_frame_ms;
    if (elapsed_ms < GAME_FRAME_MS) return;
    s_last_frame_ms = now;
    float dt = clampf((float)elapsed_ms * 0.001f, 0.0f, GAME_MAX_DT);

    if (!s_game_over && s_running) {
        update_game(dt, now);
    } else {
        update_cat_pose(now);
    }
}

static bool bounce_on_event(ui_event_t event)
{
    if (event == EV_OK || event == EV_UP) {
        if (s_game_over) {
            if ((uint32_t)(plat_millis() - s_game_over_ms) >=
                GAME_OVER_CLEAR_MS) {
                restart_and_jump(1.5f, s_last_onset_seq, true);
            }
        } else {
            (void)jump(1.5f, true);
        }
        return true;
    }
    if (event == EV_DOWN) {
        if (!s_game_over && !cat_is_grounded() &&
            s_cat_vy < GAME_DROP_VELOCITY) {
            s_cat_vy = GAME_DROP_VELOCITY;
        }
        return true;
    }
    if (event == EV_LEFT || event == EV_RIGHT) {
        return true;
    }
    return false;
}

void bounce_game_enter(const gadget_app_t *appearance_owner,
                       const char *title)
{
    s_appearance_owner = appearance_owner ? appearance_owner : &APP_GAME;
    s_embedded_title = title ? title : "";
    bounce_enter(0);
}

void bounce_game_exit(void)
{
    bounce_exit();
}

void bounce_game_render(void)
{
    bounce_render();
}

bool bounce_game_on_event(ui_event_t event)
{
    return bounce_on_event(event);
}

void bounce_game_apply_appearance(void)
{
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

#endif
