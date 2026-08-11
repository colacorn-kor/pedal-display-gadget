#include "gadget_app.h"

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "app_slots.h"
#include "audio_playback.h"
#include "bounce_game.h"
#include "game_runtime.h"
#include "storage.h"
#include "theme.h"

#define GAME_TILE_VISIBLE 4
#define GAME_TILE_MIN_COUNT 4
#define GAME_TILE_SIZE 96
#define GAME_TILE_GAP 12
#define GAME_TILE_X0 30
#define GAME_TILE_Y 104
#define GAME_PLAYER_W 320
#define GAME_PLAYER_H 288
#define GAME_PLAYER_X 80
#define GAME_PLAYER_Y 16
#define GAME_ACTION_COUNT 5
#define GAME_AUDIO_OWNER "game"

typedef enum {
    GAME_VIEW_LOBBY = 0,
    GAME_VIEW_BUILTIN,
    GAME_VIEW_EXTERNAL,
} game_view_t;

typedef enum {
    GAME_ACTION_A = 0,
    GAME_ACTION_B,
    GAME_ACTION_START,
    GAME_ACTION_SELECT,
    GAME_ACTION_BACK,
} game_action_t;

static lv_obj_t *s_host;
static lv_obj_t *s_heading;
static lv_obj_t *s_name;
static lv_obj_t *s_source;
static lv_obj_t *s_tiles[GAME_TILE_VISIBLE];
static lv_obj_t *s_tile_labels[GAME_TILE_VISIBLE];
static storage_item_t *s_detected_games;
static game_rom_info_t *s_detected_info;
static int s_detected_count;
static int s_selected;
static int s_scroll;
static game_view_t s_view;

static lv_obj_t *s_player_root;
static lv_obj_t *s_player_canvas;
static lv_obj_t *s_player_title;
static lv_obj_t *s_player_system;
static lv_obj_t *s_player_status;
static lv_obj_t *s_action_labels[GAME_ACTION_COUNT];
static void *s_player_buffer;
static uint32_t s_player_stride;
static uint32_t s_player_frame_sequence;
static game_action_t s_action;
static bool s_audio_claimed;

static void release_game_audio(void);

static const char *const ACTION_NAMES[GAME_ACTION_COUNT] = {
    "A", "B", "START", "SELECT", "BACK",
};

static const lv_font_t *font_small(void)
{
#if LV_FONT_UNSCII_8
    return &lv_font_unscii_8;
#else
    return &lv_font_montserrat_12;
#endif
}

static const ui_theme_t *game_theme(void)
{
    return theme_for_app_color(app_slots_color(&APP_GAME));
}

static void style_group(lv_obj_t *obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    return label;
}

static int lobby_tile_count(void)
{
    const int needed = s_detected_count + 1;
    return needed > GAME_TILE_MIN_COUNT ? needed : GAME_TILE_MIN_COUNT;
}

static bool selected_is_external(void)
{
    return s_selected >= 0 && s_selected < s_detected_count;
}

static void clamp_scroll(void)
{
    const int count = lobby_tile_count();
    if (s_selected < 0) s_selected = 0;
    if (s_selected >= count) s_selected = count - 1;
    if (s_selected < s_scroll) s_scroll = s_selected;
    if (s_selected >= s_scroll + GAME_TILE_VISIBLE) {
        s_scroll = s_selected - GAME_TILE_VISIBLE + 1;
    }
    const int maximum = count > GAME_TILE_VISIBLE
        ? count - GAME_TILE_VISIBLE : 0;
    if (s_scroll > maximum) s_scroll = maximum;
    if (s_scroll < 0) s_scroll = 0;
}

static void scan_games(void)
{
    if (!s_detected_games) {
        s_detected_games = heap_caps_calloc(
            STORAGE_MAX_ITEMS, sizeof(*s_detected_games),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!s_detected_info) {
        s_detected_info = heap_caps_calloc(
            STORAGE_MAX_ITEMS, sizeof(*s_detected_info),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!s_detected_games || !s_detected_info) {
        s_detected_count = 0;
        return;
    }

    const int scanned = storage_scan(
        STORAGE_MEDIA_GAME, s_detected_games, STORAGE_MAX_ITEMS);
    int playable = 0;
    for (int i = 0; i < scanned; i++) {
        game_rom_info_t info;
        if (!game_runtime_probe(
                s_detected_games[i].path, &info, NULL, 0)) {
            continue;
        }
        if (playable != i) s_detected_games[playable] = s_detected_games[i];
        s_detected_info[playable] = info;
        playable++;
    }
    s_detected_count = playable;
}

static void style_lobby(void)
{
    if (!s_host) return;
    const ui_theme_t *theme = game_theme();

    lv_obj_set_style_bg_color(s_host, theme->bg, 0);
    lv_obj_set_style_bg_opa(s_host, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_host, 0, 0);
    lv_obj_set_style_text_color(s_host, theme->text, 0);
    lv_obj_set_style_text_color(s_heading, theme->accent, 0);
    lv_obj_set_style_text_color(s_name, theme->text, 0);
    lv_obj_set_style_text_color(s_source, theme->accent, 0);

    for (int visible = 0; visible < GAME_TILE_VISIBLE; visible++) {
        lv_obj_t *tile = s_tiles[visible];
        if (!tile) continue;
        const int index = s_scroll + visible;
        const bool selected = index == s_selected;
        lv_obj_set_style_bg_color(tile, theme->surface, 0);
        lv_obj_set_style_bg_opa(tile, selected ? LV_OPA_COVER : LV_OPA_70, 0);
        lv_obj_set_style_border_color(
            tile, selected ? theme->accent : theme->grid, 0);
        lv_obj_set_style_border_width(tile, selected ? 4 : 1, 0);
        lv_obj_set_style_outline_width(tile, selected ? 2 : 0, 0);
        lv_obj_set_style_outline_color(tile, theme->accent, 0);
        lv_obj_set_style_outline_opa(tile, selected ? LV_OPA_30 : LV_OPA_0, 0);
        lv_obj_set_style_text_color(s_tile_labels[visible], theme->text, 0);
    }
}

static void update_lobby(void)
{
    if (!s_host) return;
    clamp_scroll();
    if (selected_is_external()) {
        lv_label_set_text(s_name, s_detected_info[s_selected].title);
        lv_label_set_text(s_source, s_detected_info[s_selected].system);
    } else {
        lv_label_set_text(s_name, "");
        lv_label_set_text(s_source, "");
    }

    for (int visible = 0; visible < GAME_TILE_VISIBLE; visible++) {
        const int index = s_scroll + visible;
        lv_label_set_text(
            s_tile_labels[visible],
            index < s_detected_count ? s_detected_info[index].title : "");
    }
    style_lobby();
}

static void create_lobby(void)
{
    audio_set_mode(AUDIO_SPECTRUM);
    audio_set_viz_mode(VIZ_DECOR);
    s_view = GAME_VIEW_LOBBY;

    s_host = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(s_host, 0, 0);
    lv_obj_set_size(s_host, 480, 320);
    style_group(s_host);

    s_heading = make_label(s_host, "GAME", &lv_font_montserrat_14);
    lv_obj_set_pos(s_heading, 20, 18);

    s_name = make_label(s_host, "", &lv_font_montserrat_28);
    lv_obj_set_width(s_name, 320);
    lv_label_set_long_mode(s_name, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_name, 20, 55);

    s_source = make_label(s_host, "", font_small());
    lv_obj_set_width(s_source, 120);
    lv_obj_set_style_text_align(s_source, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_source, 340, 64);

    for (int visible = 0; visible < GAME_TILE_VISIBLE; visible++) {
        lv_obj_t *tile = lv_obj_create(s_host);
        lv_obj_set_pos(tile,
                       GAME_TILE_X0 + visible *
                           (GAME_TILE_SIZE + GAME_TILE_GAP),
                       GAME_TILE_Y);
        lv_obj_set_size(tile, GAME_TILE_SIZE, GAME_TILE_SIZE);
        style_group(tile);
        s_tiles[visible] = tile;

        s_tile_labels[visible] = make_label(tile, "", font_small());
        lv_obj_set_size(s_tile_labels[visible], GAME_TILE_SIZE - 16, 28);
        lv_obj_set_pos(s_tile_labels[visible], 8, GAME_TILE_SIZE - 34);
        lv_obj_set_style_text_align(
            s_tile_labels[visible], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(s_tile_labels[visible], LV_LABEL_LONG_DOT);
    }

    update_lobby();
}

static void destroy_lobby(void)
{
    if (s_host) lv_obj_delete(s_host);
    s_host = NULL;
    s_heading = NULL;
    s_name = NULL;
    s_source = NULL;
    memset(s_tiles, 0, sizeof(s_tiles));
    memset(s_tile_labels, 0, sizeof(s_tile_labels));
}

static uint16_t mix_color(lv_color_t foreground, lv_color_t background,
                          uint8_t opacity)
{
    return lv_color_to_u16(lv_color_mix(foreground, background, opacity));
}

static void player_palette(uint16_t palette[4])
{
    const ui_theme_t *theme = game_theme();
    if (theme_mode() == UI_THEME_MODE_LIGHT) {
        palette[0] = lv_color_to_u16(theme->bg);
        palette[1] = mix_color(theme->accent, theme->bg, 72);
        palette[2] = mix_color(theme->accent, theme->bg, 176);
        palette[3] = lv_color_to_u16(theme->text);
    } else {
        palette[0] = lv_color_to_u16(theme->text);
        palette[1] = mix_color(theme->accent, theme->text, 80);
        palette[2] = mix_color(theme->accent, theme->bg, 176);
        palette[3] = lv_color_to_u16(theme->bg);
    }
}

static void draw_player_frame(void)
{
    if (!s_player_canvas || !s_player_buffer || s_player_stride == 0) return;
    const uint8_t *source = game_runtime_frame();
    if (!source) return;
    uint16_t palette[4];
    player_palette(palette);

    for (int y = 0; y < GAME_FRAME_HEIGHT; y++) {
        uint16_t *row0 = (uint16_t *)(
            (uint8_t *)s_player_buffer + (size_t)(y * 2) * s_player_stride);
        uint16_t *row1 = (uint16_t *)(
            (uint8_t *)s_player_buffer + (size_t)(y * 2 + 1) * s_player_stride);
        for (int x = 0; x < GAME_FRAME_WIDTH; x++) {
            const uint16_t color = palette[source[y * GAME_FRAME_WIDTH + x]];
            row0[x * 2] = color;
            row0[x * 2 + 1] = color;
            row1[x * 2] = color;
            row1[x * 2 + 1] = color;
        }
    }
    lv_obj_invalidate(s_player_canvas);
}

static void style_player(void)
{
    if (!s_player_root) return;
    const ui_theme_t *theme = game_theme();
    lv_obj_set_style_bg_color(s_player_root, theme->bg, 0);
    lv_obj_set_style_bg_opa(s_player_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_player_root, 0, 0);
    lv_obj_set_style_text_color(s_player_root, theme->text, 0);
    lv_obj_set_style_text_color(s_player_system, theme->accent, 0);
    lv_obj_set_style_text_color(s_player_title, theme->text, 0);
    lv_obj_set_style_text_color(s_player_status, theme->accent, 0);
    for (int i = 0; i < GAME_ACTION_COUNT; i++) {
        lv_obj_set_style_text_color(
            s_action_labels[i], i == s_action ? theme->accent : theme->grid, 0);
    }
    draw_player_frame();
}

static void create_player(const game_rom_info_t *info, const char *error)
{
    const char *display_error = error;
    s_player_root = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(s_player_root, 0, 0);
    lv_obj_set_size(s_player_root, 480, 320);
    style_group(s_player_root);

    s_player_system = make_label(
        s_player_root, info ? info->system : "GAME BOY", font_small());
    lv_obj_set_width(s_player_system, 68);
    lv_obj_set_pos(s_player_system, 6, 18);

    s_player_title = make_label(
        s_player_root, info ? info->title : "GAME", font_small());
    lv_obj_set_width(s_player_title, 68);
    lv_label_set_long_mode(s_player_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(s_player_title, 6, 55);

    const size_t bytes = LV_CANVAS_BUF_SIZE(
        GAME_PLAYER_W, GAME_PLAYER_H, 16, LV_DRAW_BUF_STRIDE_ALIGN);
    s_player_buffer = heap_caps_calloc(
        1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_player_buffer) {
        s_player_canvas = lv_canvas_create(s_player_root);
        if (s_player_canvas) {
            lv_canvas_set_buffer(s_player_canvas, s_player_buffer,
                                 GAME_PLAYER_W, GAME_PLAYER_H,
                                 LV_COLOR_FORMAT_RGB565);
            lv_obj_set_pos(s_player_canvas, GAME_PLAYER_X, GAME_PLAYER_Y);
            lv_draw_buf_t *draw = lv_canvas_get_draw_buf(s_player_canvas);
            if (draw && draw->data) s_player_stride = draw->header.stride;
        }
    }
    if (!s_player_canvas || s_player_stride == 0) {
        display_error = "Display memory unavailable";
        game_runtime_stop();
        release_game_audio();
    }

    s_player_status = make_label(
        s_player_root, display_error ? display_error : "",
        &lv_font_montserrat_14);
    lv_obj_set_width(s_player_status, GAME_PLAYER_W - 32);
    lv_obj_set_style_text_align(s_player_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_player_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(s_player_status, GAME_PLAYER_X + 16, 138);
    if (!display_error) lv_obj_add_flag(s_player_status, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < GAME_ACTION_COUNT; i++) {
        s_action_labels[i] = make_label(
            s_player_root, ACTION_NAMES[i], font_small());
        lv_obj_set_width(s_action_labels[i], 66);
        lv_obj_set_style_text_align(
            s_action_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(s_action_labels[i], 407, 45 + i * 47);
    }
    style_player();
}

static void release_game_audio(void)
{
    if (!s_audio_claimed) return;
    (void)audio_playback_stop(GAME_AUDIO_OWNER);
    (void)audio_playback_release(GAME_AUDIO_OWNER);
    s_audio_claimed = false;
}

static void destroy_player(void)
{
    game_runtime_stop();
    release_game_audio();
    if (s_player_root) lv_obj_delete(s_player_root);
    s_player_root = NULL;
    s_player_canvas = NULL;
    s_player_title = NULL;
    s_player_system = NULL;
    s_player_status = NULL;
    memset(s_action_labels, 0, sizeof(s_action_labels));
    heap_caps_free(s_player_buffer);
    s_player_buffer = NULL;
    s_player_stride = 0;
    s_player_frame_sequence = 0;
}

static size_t write_game_audio(const float *samples, size_t frames, void *ctx)
{
    (void)ctx;
    return audio_playback_write(
        GAME_AUDIO_OWNER, AUDIO_PLAYBACK_BUS_MUSIC, samples, frames);
}

static void start_builtin(void)
{
    destroy_lobby();
    s_view = GAME_VIEW_BUILTIN;
    bounce_game_enter(&APP_GAME, NULL);
}

static void start_external(void)
{
    const int index = s_selected;
    const game_rom_info_t info = s_detected_info[index];
    destroy_lobby();
    s_view = GAME_VIEW_EXTERNAL;
    s_action = GAME_ACTION_A;

    game_audio_sink_t sink = {
        .write = write_game_audio,
        .sample_rate = AUDIO_PLAYBACK_SAMPLE_RATE,
    };
    char error[96] = {0};
    if (info.audio_supported && audio_playback_is_available() &&
        audio_playback_claim(GAME_AUDIO_OWNER) == AUDIO_PLAYBACK_OK) {
        s_audio_claimed = true;
        (void)audio_playback_play(GAME_AUDIO_OWNER);
    }
    const bool started = game_runtime_start(
        s_detected_games[index].path, &sink, error, sizeof(error));
    if (!started) release_game_audio();
    create_player(&info, started ? NULL : error);
}

static void return_to_lobby(void)
{
    if (s_view == GAME_VIEW_BUILTIN) {
        bounce_game_exit();
    } else if (s_view == GAME_VIEW_EXTERNAL) {
        destroy_player();
    }
    create_lobby();
}

static uint8_t selected_action_button(void)
{
    switch (s_action) {
    case GAME_ACTION_A: return GAME_BUTTON_A;
    case GAME_ACTION_B: return GAME_BUTTON_B;
    case GAME_ACTION_START: return GAME_BUTTON_START;
    case GAME_ACTION_SELECT: return GAME_BUTTON_SELECT;
    default: return 0;
    }
}

static uint8_t held_game_buttons(void)
{
    const platform_game_input_mask_t held = plat_game_input_state();
    uint8_t buttons = 0;
    if (held & PLAT_GAME_INPUT_UP) buttons |= GAME_BUTTON_UP;
    if (held & PLAT_GAME_INPUT_DOWN) buttons |= GAME_BUTTON_DOWN;
    if (held & PLAT_GAME_INPUT_LEFT) buttons |= GAME_BUTTON_LEFT;
    if (held & PLAT_GAME_INPUT_RIGHT) buttons |= GAME_BUTTON_RIGHT;
    if (held & PLAT_GAME_INPUT_A) buttons |= GAME_BUTTON_A;
    if (held & PLAT_GAME_INPUT_B) buttons |= GAME_BUTTON_B;
    if (held & PLAT_GAME_INPUT_START) buttons |= GAME_BUTTON_START;
    if (held & PLAT_GAME_INPUT_SELECT) buttons |= GAME_BUTTON_SELECT;
    if ((held & PLAT_GAME_INPUT_OK) && s_action != GAME_ACTION_BACK) {
        buttons |= selected_action_button();
    }
    return buttons;
}

static void game_enter(int variant)
{
    (void)variant;
    s_selected = 0;
    s_scroll = 0;
    scan_games();
    create_lobby();
}

static void game_exit(void)
{
    if (s_view == GAME_VIEW_BUILTIN) {
        bounce_game_exit();
    } else if (s_view == GAME_VIEW_EXTERNAL) {
        destroy_player();
    } else {
        destroy_lobby();
    }
    s_view = GAME_VIEW_LOBBY;
}

static void game_render(void)
{
    if (s_view == GAME_VIEW_BUILTIN) {
        bounce_game_render();
        return;
    }
    if (s_view != GAME_VIEW_EXTERNAL || !game_runtime_running()) return;

    game_runtime_set_buttons(held_game_buttons());
    if (!game_runtime_advance(plat_millis())) {
        lv_label_set_text(s_player_status, game_runtime_error());
        lv_obj_remove_flag(s_player_status, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const uint32_t sequence = game_runtime_frame_sequence();
    if (sequence != s_player_frame_sequence) {
        s_player_frame_sequence = sequence;
        draw_player_frame();
    }
}

static bool game_on_event(ui_event_t event)
{
    if (s_view == GAME_VIEW_BUILTIN) {
        if (event == EV_HOME) {
            return_to_lobby();
            return true;
        }
        return bounce_game_on_event(event);
    }

    if (s_view == GAME_VIEW_EXTERNAL) {
        if (event == EV_HOME) {
            s_action = (game_action_t)((s_action + 1) % GAME_ACTION_COUNT);
            style_player();
            return true;
        }
        if (event == EV_OK) {
            if (s_action == GAME_ACTION_BACK) return_to_lobby();
            else game_runtime_press((game_button_t)selected_action_button());
            return true;
        }
        if (event == EV_UP) game_runtime_press(GAME_BUTTON_UP);
        else if (event == EV_DOWN) game_runtime_press(GAME_BUTTON_DOWN);
        else if (event == EV_LEFT) game_runtime_press(GAME_BUTTON_LEFT);
        else if (event == EV_RIGHT) game_runtime_press(GAME_BUTTON_RIGHT);
        else return false;
        return true;
    }

    if (event == EV_LEFT || event == EV_RIGHT) {
        const int count = lobby_tile_count();
        const int delta = event == EV_LEFT ? -1 : 1;
        s_selected = (s_selected + delta + count) % count;
        update_lobby();
        return true;
    }
    if (event == EV_OK) {
        if (selected_is_external()) start_external();
        else start_builtin();
        return true;
    }
    if (event == EV_UP || event == EV_DOWN) return true;
    return false;
}

static void game_appearance_changed(void)
{
    if (s_view == GAME_VIEW_BUILTIN) {
        bounce_game_apply_appearance();
    } else if (s_view == GAME_VIEW_EXTERNAL) {
        style_player();
    } else {
        style_lobby();
    }
}

const gadget_app_t APP_GAME = {
    .id = "game",
    .name = "Game",
    .audio_mode = AUDIO_SPECTRUM,
    .icon = NULL,
    .on_enter = game_enter,
    .on_exit = game_exit,
    .on_render = game_render,
    .on_event = game_on_event,
    .on_appearance_changed = game_appearance_changed,
    .input_sources = APP_INPUT_BUTTONS,
    .output_routes = APP_OUTPUT_DISPLAY |
                     APP_OUTPUT_AUX |
                     APP_OUTPUT_HEADPHONES,
};

#ifdef PEDAL_SIM
bool game_app_debug_is_lobby(void)
{
    return s_view == GAME_VIEW_LOBBY && s_host != NULL;
}

bool game_app_debug_is_builtin(void)
{
    return s_view == GAME_VIEW_BUILTIN;
}

bool game_app_debug_is_external(void)
{
    return s_view == GAME_VIEW_EXTERNAL && game_runtime_running();
}

int game_app_debug_selected_tile(void)
{
    return s_selected;
}

int game_app_debug_tile_count(void)
{
    return lobby_tile_count();
}

int game_app_debug_detected_files(void)
{
    return s_detected_count;
}

uint32_t game_app_debug_frame_sequence(void)
{
    return game_runtime_frame_sequence();
}

const char *game_app_debug_lobby_name(void)
{
    return s_name ? lv_label_get_text(s_name) : "";
}

const char *game_app_debug_lobby_source(void)
{
    return s_source ? lv_label_get_text(s_source) : "";
}
#endif
