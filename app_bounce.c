#include "gadget_app.h"

#include <math.h>
#include <stdio.h>

#include "music_events.h"
#include "theme.h"

#define BOUNCE_W 32
#define BOUNCE_H 32
#define BOUNCE_SCREEN_W 480
#define BOUNCE_SCREEN_H 320
#define BOUNCE_FLOOR_Y 248.0f
#define BOUNCE_CENTER_MIN_X 40.0f
#define BOUNCE_CENTER_MAX_X 440.0f
#define BOUNCE_MIDI_MIN 40.0f
#define BOUNCE_MIDI_MAX 76.0f
#define BOUNCE_PITCH_LERP 0.20f
#define BOUNCE_JUMP_K 2.60f
#define BOUNCE_TRAILS 8
#define BOUNCE_GRAVITY_COUNT 3

#define Z8 0, 0, 0, 0, 0, 0, 0, 0
#define R_EMPTY Z8, Z8, Z8, Z8

static const uint8_t BOUNCE_IDLE_DATA[BOUNCE_W * BOUNCE_H] = {
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    Z8, 0, 0, 0, 60, 150, 0, 0, 0, 0, 0, 0, 0, 150, 60, 0, 0, Z8,
    Z8, 0, 0, 70, 210, 250, 90, 0, 0, 0, 0, 90, 250, 210, 70, 0, Z8,
    Z8, 0, 80, 230, 255, 255, 220, 120, 120, 120, 220, 255, 255, 230, 80, 0, Z8,
    Z8, 80, 230, 255, 255, 255, 255, 230, 230, 230, 255, 255, 255, 255, 230, 80, Z8,
    0, 0, 0, 80, 230, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 230, 80, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 80, 230, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 230, 80, 0, 0, 0, 0, 0, 0,
    0, 40, 210, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 210, 40, 0, 0, 0, 0, 0,
    0, 110, 245, 255, 255, 255, 255, 90, 90, 255, 255, 255, 90, 90, 255, 255, 255, 245, 110, 0, 0, 0, 0, 0,
    0, 160, 255, 255, 255, 255, 255, 40, 40, 255, 255, 255, 40, 40, 255, 255, 255, 255, 160, 0, 0, 0, 0, 0,
    0, 190, 255, 255, 255, 255, 255, 190, 190, 255, 255, 255, 190, 190, 255, 255, 255, 255, 190, 0, 0, 0, 0, 0,
    0, 210, 255, 255, 255, 255, 255, 255, 255, 255, 230, 230, 255, 255, 255, 255, 255, 255, 210, 0, 0, 0, 0, 0,
    0, 220, 255, 255, 255, 255, 255, 255, 255, 220, 120, 120, 220, 255, 255, 255, 255, 255, 220, 0, 0, 0, 0, 0,
    0, 220, 255, 255, 255, 255, 255, 255, 255, 255, 220, 220, 255, 255, 255, 255, 255, 255, 220, 0, 0, 0, 0, 0,
    0, 210, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 210, 0, 0, 0, 0, 0,
    0, 190, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 190, 0, 0, 0, 0, 0,
    0, 160, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 160, 0, 0, 0, 0, 0,
    0, 110, 245, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 245, 110, 0, 0, 0, 0, 0,
    0, 40, 210, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 210, 40, 0, 0, 0, 0, 0,
    0, 0, 80, 230, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 230, 80, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 80, 230, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 230, 80, 0, 0, 0, 0, 0, 0, 0,
    Z8, 80, 230, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 230, 80, 0, Z8,
    Z8, 0, 80, 230, 255, 255, 255, 255, 255, 255, 255, 255, 230, 80, 0, 0, Z8,
    Z8, 0, 0, 80, 220, 255, 255, 255, 255, 255, 255, 220, 80, 0, 0, 0, Z8,
    Z8, 0, 0, 0, 80, 200, 240, 255, 255, 240, 200, 80, 0, 0, 0, 0, Z8,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
};

static const uint8_t BOUNCE_SQUASH_DATA[BOUNCE_W * BOUNCE_H] = {
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    Z8, 0, 0, 80, 180, 0, 0, 0, 0, 0, 0, 0, 180, 80, 0, 0, Z8,
    Z8, 0, 70, 220, 255, 210, 100, 0, 0, 0, 100, 210, 255, 220, 70, 0, Z8,
    0, 0, 80, 230, 255, 255, 255, 220, 160, 160, 220, 255, 255, 255, 230, 80, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 60, 220, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 220, 60, 0, 0, 0, 0, 0, 0,
    40, 210, 255, 255, 255, 255, 255, 80, 80, 255, 255, 255, 80, 80, 255, 255, 255, 210, 40, 0, 0, 0, 0, 0,
    90, 245, 255, 255, 255, 255, 255, 40, 40, 255, 255, 255, 40, 40, 255, 255, 255, 245, 90, 0, 0, 0, 0, 0,
    130, 255, 255, 255, 255, 255, 255, 200, 200, 255, 255, 255, 200, 200, 255, 255, 255, 255, 130, 0, 0, 0, 0, 0,
    160, 255, 255, 255, 255, 255, 255, 255, 255, 255, 220, 220, 255, 255, 255, 255, 255, 255, 160, 0, 0, 0, 0, 0,
    180, 255, 255, 255, 255, 255, 255, 255, 255, 220, 120, 120, 220, 255, 255, 255, 255, 255, 180, 0, 0, 0, 0, 0,
    190, 255, 255, 255, 255, 255, 255, 255, 255, 255, 220, 220, 255, 255, 255, 255, 255, 255, 190, 0, 0, 0, 0, 0,
    190, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 190, 0, 0, 0, 0, 0,
    180, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 180, 0, 0, 0, 0, 0,
    160, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 160, 0, 0, 0, 0, 0,
    120, 245, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 245, 120, 0, 0, 0, 0, 0,
    70, 220, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 220, 70, 0, 0, 0, 0, 0,
    0, 80, 230, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 230, 80, 0, 0, 0, 0, 0, 0,
    0, 0, 80, 220, 250, 255, 255, 255, 255, 255, 255, 255, 250, 220, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
    R_EMPTY,
};

static const lv_image_dsc_t BOUNCE_IDLE_IMG = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_A8,
        .flags = 0,
        .w = BOUNCE_W,
        .h = BOUNCE_H,
        .stride = BOUNCE_W,
    },
    .data_size = sizeof(BOUNCE_IDLE_DATA),
    .data = BOUNCE_IDLE_DATA,
};

static const lv_image_dsc_t BOUNCE_SQUASH_IMG = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_A8,
        .flags = 0,
        .w = BOUNCE_W,
        .h = BOUNCE_H,
        .stride = BOUNCE_W,
    },
    .data_size = sizeof(BOUNCE_SQUASH_DATA),
    .data = BOUNCE_SQUASH_DATA,
};

static const float GRAVITY[BOUNCE_GRAVITY_COUNT] = { 0.32f, 0.52f, 0.82f };
static const char *GRAVITY_LABELS[BOUNCE_GRAVITY_COUNT] = { "LOW", "MID", "HIGH" };

static lv_obj_t *s_host;
static lv_obj_t *s_sprite;
static lv_obj_t *s_status;
static lv_obj_t *s_ground;
static lv_obj_t *s_trail[BOUNCE_TRAILS];
static float s_x;
static float s_y;
static float s_vy;
static int s_gravity_idx = 1;
static int s_squash_frames;
static uint32_t s_last_onset_seq;
static int s_trail_head;
static bool s_trail_active[BOUNCE_TRAILS];
static float s_trail_x[BOUNCE_TRAILS];
static float s_trail_y[BOUNCE_TRAILS];

static const lv_font_t *font_small(void)
{
#if LV_FONT_UNSCII_8
    return &lv_font_unscii_8;
#else
    return &lv_font_montserrat_12;
#endif
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float target_x_for_pitch(const music_snapshot_t *snap)
{
    if (!snap->pitch_valid || !(snap->f0 > 0.0f) || !isfinite(snap->f0)) {
        return 0.5f * (BOUNCE_SCREEN_W - BOUNCE_W);
    }

    float midi = 69.0f + 12.0f * log2f(snap->f0 / 440.0f);
    midi = clampf(midi, BOUNCE_MIDI_MIN, BOUNCE_MIDI_MAX);
    float ratio = (midi - BOUNCE_MIDI_MIN) / (BOUNCE_MIDI_MAX - BOUNCE_MIDI_MIN);
    float center = BOUNCE_CENTER_MIN_X +
        ratio * (BOUNCE_CENTER_MAX_X - BOUNCE_CENTER_MIN_X);
    return center - 0.5f * BOUNCE_W;
}

static void style_scene(void)
{
    const ui_theme_t *t = theme_get();
    if (s_host) {
        lv_obj_set_style_bg_color(s_host, t->bg, 0);
        lv_obj_set_style_bg_opa(s_host, LV_OPA_COVER, 0);
    }
    if (s_status) {
        lv_obj_set_style_text_color(s_status, t->text, 0);
    }
    if (s_ground) {
        lv_obj_set_style_bg_color(s_ground, t->grid, 0);
    }
    if (s_sprite) {
        lv_obj_set_style_image_recolor(s_sprite, t->accent, 0);
        lv_obj_set_style_image_recolor_opa(s_sprite, LV_OPA_COVER, 0);
    }
    for (int i = 0; i < BOUNCE_TRAILS; i++) {
        if (!s_trail[i]) continue;
        lv_obj_set_style_bg_color(s_trail[i], t->accent2, 0);
    }
}

static void reset_motion(void)
{
    s_x = 0.5f * (BOUNCE_SCREEN_W - BOUNCE_W);
    s_y = BOUNCE_FLOOR_Y;
    s_vy = 0.0f;
    s_squash_frames = 0;
    s_last_onset_seq = 0;
    s_trail_head = 0;
    for (int i = 0; i < BOUNCE_TRAILS; i++) {
        s_trail_active[i] = false;
        s_trail_x[i] = 0.0f;
        s_trail_y[i] = 0.0f;
    }
}

static void drop_trail(void)
{
    s_trail_active[s_trail_head] = true;
    s_trail_x[s_trail_head] = s_x + 0.5f * BOUNCE_W;
    s_trail_y[s_trail_head] = s_y + BOUNCE_H - 4.0f;
    s_trail_head = (s_trail_head + 1) % BOUNCE_TRAILS;
}

static void bounce_enter(int variant)
{
    (void)variant;
    audio_set_mode(AUDIO_TUNER);
    reset_motion();
    music_snapshot_t snap;
    music_snapshot_get(&snap);
    s_last_onset_seq = snap.onset_seq;

    s_host = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_host, BOUNCE_SCREEN_W, BOUNCE_SCREEN_H);
    lv_obj_set_pos(s_host, 0, 0);
    lv_obj_remove_flag(s_host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(s_host, 0, 0);
    lv_obj_set_style_radius(s_host, 0, 0);
    lv_obj_set_style_pad_all(s_host, 0, 0);

    s_status = lv_label_create(s_host);
    lv_obj_set_style_text_font(s_status, font_small(), 0);
    lv_obj_set_pos(s_status, 10, 6);

    s_ground = lv_obj_create(s_host);
    lv_obj_set_size(s_ground, BOUNCE_SCREEN_W, 2);
    lv_obj_set_pos(s_ground, 0, (int)(BOUNCE_FLOOR_Y + BOUNCE_H));
    lv_obj_remove_flag(s_ground, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(s_ground, 0, 0);
    lv_obj_set_style_radius(s_ground, 0, 0);

    for (int i = 0; i < BOUNCE_TRAILS; i++) {
        s_trail[i] = lv_obj_create(s_host);
        lv_obj_set_size(s_trail[i], 10, 4);
        lv_obj_remove_flag(s_trail[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_border_width(s_trail[i], 0, 0);
        lv_obj_set_style_radius(s_trail[i], 2, 0);
        lv_obj_set_style_bg_opa(s_trail[i], LV_OPA_0, 0);
    }

    s_sprite = lv_image_create(s_host);
    lv_image_set_src(s_sprite, &BOUNCE_IDLE_IMG);
    lv_image_set_pivot(s_sprite, BOUNCE_W / 2, BOUNCE_H);
    style_scene();
}

static void bounce_exit(void)
{
    if (s_host) lv_obj_delete(s_host);
    s_host = NULL;
    s_sprite = NULL;
    s_status = NULL;
    s_ground = NULL;
    for (int i = 0; i < BOUNCE_TRAILS; i++) s_trail[i] = NULL;
}

static void update_trails(void)
{
    for (int i = 0; i < BOUNCE_TRAILS; i++) {
        if (!s_trail[i]) continue;
        if (!s_trail_active[i]) {
            lv_obj_set_style_bg_opa(s_trail[i], LV_OPA_0, 0);
            continue;
        }

        int age = (s_trail_head - 1 - i + BOUNCE_TRAILS) % BOUNCE_TRAILS;
        int opa = 70 + (BOUNCE_TRAILS - age) * 14;
        if (opa > LV_OPA_COVER) opa = LV_OPA_COVER;
        lv_obj_set_style_bg_opa(s_trail[i], (lv_opa_t)opa, 0);
        lv_obj_set_pos(s_trail[i], (int)(s_trail_x[i] - 5.0f), (int)s_trail_y[i]);
    }
}

static void bounce_render(void)
{
    if (!s_host || !s_sprite || !s_status) return;

    music_snapshot_t snap;
    music_snapshot_get(&snap);
    style_scene();

    if (snap.onset_seq != s_last_onset_seq) {
        s_last_onset_seq = snap.onset_seq;
        float strength = clampf(snap.onset_strength, 0.0f, 3.0f);
        s_vy = -BOUNCE_JUMP_K * strength;
        s_squash_frames = 2;
    }

    s_x += BOUNCE_PITCH_LERP * (target_x_for_pitch(&snap) - s_x);
    s_vy += GRAVITY[s_gravity_idx];
    s_y += s_vy;
    if (s_y > BOUNCE_FLOOR_Y) {
        if (s_vy > 1.2f) s_squash_frames = 2;
        s_y = BOUNCE_FLOOR_Y;
        s_vy = 0.0f;
    }

    bool squash = s_squash_frames > 0;
    if (s_squash_frames > 0) s_squash_frames--;
    lv_image_set_src(s_sprite, squash ? &BOUNCE_SQUASH_IMG : &BOUNCE_IDLE_IMG);

    float pulse = 1.0f + 0.15f * clampf(snap.level, 0.0f, 1.0f);
    uint32_t scale_x = (uint32_t)(LV_SCALE_NONE * pulse * (squash ? 1.12f : 1.0f));
    uint32_t scale_y = (uint32_t)(LV_SCALE_NONE * pulse * (squash ? 0.84f : 1.0f));
    lv_image_set_scale_x(s_sprite, scale_x);
    lv_image_set_scale_y(s_sprite, scale_y);
    lv_obj_set_pos(s_sprite, (int)s_x, (int)s_y);

    const char *note = snap.pitch_valid ? snap.note_name : "--";
    char status[64];
    if (snap.pitch_valid) {
        snprintf(status, sizeof(status), "%s%d  BPM %.0f  G %s",
                 note, snap.octave, snap.bpm, GRAVITY_LABELS[s_gravity_idx]);
    } else {
        snprintf(status, sizeof(status), "--  BPM %.0f  G %s",
                 snap.bpm, GRAVITY_LABELS[s_gravity_idx]);
    }
    lv_label_set_text(s_status, status);

    update_trails();
}

static bool bounce_on_event(ui_event_t event)
{
    if (event == EV_UP) {
        s_gravity_idx = (s_gravity_idx + BOUNCE_GRAVITY_COUNT - 1) %
            BOUNCE_GRAVITY_COUNT;
        return true;
    }
    if (event == EV_DOWN) {
        s_gravity_idx = (s_gravity_idx + 1) % BOUNCE_GRAVITY_COUNT;
        return true;
    }
    if (event == EV_OK) {
        drop_trail();
        return true;
    }
    if (event == EV_LEFT || event == EV_RIGHT) return true;
    return false;
}

const gadget_app_t APP_BOUNCE = {
    .id = "bounce",
    .name = "Bounce",
    .audio_mode = AUDIO_TUNER,
    .icon = &BOUNCE_IDLE_IMG,
    .on_enter = bounce_enter,
    .on_exit = bounce_exit,
    .on_render = bounce_render,
    .on_event = bounce_on_event,
};

#undef Z8
#undef R_EMPTY
