#include "gadget_app.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "app_slots.h"
#include "audio_playback.h"
#include "music_lobby.h"
#include "storage.h"
#include "theme.h"
#include "wav_decoder.h"

#define MUSIC_OWNER_ID "music"
#define MUSIC_FILL_TARGET 6144u
#define MUSIC_CHUNK_FRAMES 512u
#define MUSIC_BAR_COUNT 16
#define MUSIC_PROGRESS_W 400
#define MUSIC_VOLUME_MAX_STEP 20
#define MUSIC_VOLUME_DEFAULT_STEP 14
#define MUSIC_UI_REFRESH_MS 80u
#define MUSIC_EOF_DRAIN_MS 60u

typedef enum {
    MUSIC_SOURCE_NONE = 0,
    MUSIC_SOURCE_LOBBY,
    MUSIC_SOURCE_WAV,
    MUSIC_SOURCE_ERROR,
} music_source_t;

static lv_obj_t *s_host;
static lv_obj_t *s_title;
static lv_obj_t *s_source_label;
static lv_obj_t *s_volume_label;
static lv_obj_t *s_track_label;
static lv_obj_t *s_meta_label;
static lv_obj_t *s_progress_bg;
static lv_obj_t *s_progress_fill;
static lv_obj_t *s_elapsed_label;
static lv_obj_t *s_duration_label;
static lv_obj_t *s_prev_icon;
static lv_obj_t *s_play_icon;
static lv_obj_t *s_next_icon;
static lv_obj_t *s_bars[MUSIC_BAR_COUNT];

static storage_item_t *s_tracks;
static int s_track_count;
static int s_track;
static int s_volume_step;
static bool s_claimed;
static bool s_source_ready;
static bool s_source_eof;
static music_source_t s_source;
static music_lobby_t s_lobby;
static wav_decoder_t s_wav;
static float s_visual_peak;
static uint32_t s_last_ui_ms;
static uint32_t s_eof_empty_since;
static char s_error[96];

static const ui_theme_t *music_theme(void)
{
    return theme_for_app_color(app_slots_color(&APP_MUSIC));
}

static const lv_font_t *font_small(void)
{
#if LV_FONT_UNSCII_8
    return &lv_font_unscii_8;
#else
    return &lv_font_montserrat_14;
#endif
}

static void style_group(lv_obj_t *obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t *make_block(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    style_group(obj);
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

static bool music_ensure_catalog(void)
{
    if (s_tracks) return true;
    s_tracks = heap_caps_calloc(
        STORAGE_MAX_ITEMS, sizeof(*s_tracks),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return s_tracks != NULL;
}

static void music_scan_catalog(void)
{
    s_track_count = music_ensure_catalog()
        ? storage_scan(STORAGE_MEDIA_MUSIC, s_tracks, STORAGE_MAX_ITEMS)
        : 0;
    if (s_track_count <= 0) {
        s_track_count = 0;
        s_track = 0;
    } else if (s_track >= s_track_count) {
        s_track = s_track_count - 1;
    }
}

static bool has_extension(const char *path, const char *extension)
{
    if (!path || !extension) return false;
    const char *actual = strrchr(path, '.');
    if (!actual) return false;
    while (*actual && *extension) {
        if (tolower((unsigned char)*actual) !=
            tolower((unsigned char)*extension)) return false;
        actual++;
        extension++;
    }
    return *actual == '\0' && *extension == '\0';
}

static const char *unsupported_format_name(const char *path)
{
    const char *extension = path ? strrchr(path, '.') : NULL;
    if (!extension || !extension[1]) return "Audio";
    if (has_extension(path, ".mp3")) return "MP3";
    if (has_extension(path, ".flac")) return "FLAC";
    if (has_extension(path, ".ogg")) return "OGG";
    return extension + 1;
}

static float volume_gain(void)
{
    return (float)s_volume_step / (float)MUSIC_VOLUME_MAX_STEP;
}

static void music_apply_volume(bool persist)
{
    if (s_volume_step < 0) s_volume_step = 0;
    if (s_volume_step > MUSIC_VOLUME_MAX_STEP) {
        s_volume_step = MUSIC_VOLUME_MAX_STEP;
    }
    if (s_claimed) {
        (void)audio_playback_set_master_gain(
            MUSIC_OWNER_ID, volume_gain());
    }
    if (persist) {
        app_slots_set_options(&APP_MUSIC, (uint8_t)(s_volume_step + 1));
    }
}

static void music_load_volume(void)
{
    const uint8_t encoded = app_slots_options(&APP_MUSIC);
    s_volume_step = encoded >= 1u &&
                    encoded <= MUSIC_VOLUME_MAX_STEP + 1u
        ? (int)encoded - 1
        : MUSIC_VOLUME_DEFAULT_STEP;
    music_apply_volume(false);
}

static void music_close_source(void)
{
    wav_decoder_close(&s_wav);
    s_source = MUSIC_SOURCE_NONE;
    s_source_ready = false;
    s_source_eof = false;
    s_eof_empty_since = 0u;
}

static bool music_open_source(void)
{
    music_close_source();
    s_error[0] = '\0';

    if (s_track_count <= 0) {
        music_lobby_reset(&s_lobby);
        s_source = MUSIC_SOURCE_LOBBY;
        s_source_ready = true;
        return true;
    }

    const storage_item_t *item = &s_tracks[s_track];
    if (!has_extension(item->path, ".wav")) {
        (void)snprintf(
            s_error, sizeof(s_error), "%s playback is not available yet",
            unsupported_format_name(item->path));
        s_source = MUSIC_SOURCE_ERROR;
        return false;
    }

    FILE *file = storage_open(item->path, "rb");
    if (!file) {
        (void)snprintf(s_error, sizeof(s_error), "Unable to open WAV file");
        s_source = MUSIC_SOURCE_ERROR;
        return false;
    }
    if (!wav_decoder_open(&s_wav, file)) {
        const wav_decoder_error_t error = s_wav.error;
        (void)snprintf(s_error, sizeof(s_error), "%s",
                       wav_decoder_error_text(error));
        wav_decoder_close(&s_wav);
        s_source = MUSIC_SOURCE_ERROR;
        return false;
    }

    s_source = MUSIC_SOURCE_WAV;
    s_source_ready = true;
    return true;
}

static void measure_chunk(const float *samples, size_t frames)
{
    float peak = 0.0f;
    for (size_t i = 0; i < frames * AUDIO_PLAYBACK_CHANNELS; i++) {
        float magnitude = samples[i] < 0.0f ? -samples[i] : samples[i];
        if (magnitude > peak) peak = magnitude;
    }
    if (peak > s_visual_peak) s_visual_peak = peak;
}

static void music_fill_queue(void)
{
    float chunk[MUSIC_CHUNK_FRAMES * AUDIO_PLAYBACK_CHANNELS];
    if (!s_claimed || !s_source_ready || s_source_eof) return;

    audio_playback_status_t status;
    audio_playback_get_status(&status);
    while (status.queued_music_frames < MUSIC_FILL_TARGET) {
        size_t requested = MUSIC_FILL_TARGET - status.queued_music_frames;
        if (requested > MUSIC_CHUNK_FRAMES) requested = MUSIC_CHUNK_FRAMES;

        size_t produced = 0u;
        if (s_source == MUSIC_SOURCE_LOBBY) {
            produced = music_lobby_generate(&s_lobby, chunk, requested);
        } else if (s_source == MUSIC_SOURCE_WAV) {
            produced = wav_decoder_read(&s_wav, chunk, requested);
            if (produced == 0u) s_source_eof = true;
        }
        if (produced == 0u) break;

        measure_chunk(chunk, produced);
        const size_t written = audio_playback_write(
            MUSIC_OWNER_ID, AUDIO_PLAYBACK_BUS_MUSIC, chunk, produced);
        if (written < produced) break;
        audio_playback_get_status(&status);
    }
}

static uint32_t music_duration_ms(void)
{
    if (s_source == MUSIC_SOURCE_LOBBY) return music_lobby_duration_ms();
    if (s_source == MUSIC_SOURCE_WAV) return wav_decoder_duration_ms(&s_wav);
    return 0u;
}

static uint32_t music_position_ms(void)
{
    audio_playback_status_t status;
    audio_playback_get_status(&status);
    const uint32_t queued_ms = (uint32_t)(
        status.queued_music_frames * 1000u /
        AUDIO_PLAYBACK_SAMPLE_RATE);
    uint32_t position = 0u;
    const uint32_t duration = music_duration_ms();

    if (s_source == MUSIC_SOURCE_LOBBY) {
        position = music_lobby_position_ms(&s_lobby);
        if (duration > 0u) {
            position = position >= queued_ms
                ? position - queued_ms
                : duration - ((queued_ms - position) % duration);
            if (position >= duration) position = 0u;
        }
    } else if (s_source == MUSIC_SOURCE_WAV) {
        position = wav_decoder_position_ms(&s_wav);
        position = position >= queued_ms ? position - queued_ms : 0u;
        if (duration > 0u && position > duration) position = duration;
    }
    return position;
}

static void format_time(uint32_t milliseconds, char out[16])
{
    const uint32_t seconds = milliseconds / 1000u;
    (void)snprintf(out, 16, "%lu:%02lu",
                   (unsigned long)(seconds / 60u),
                   (unsigned long)(seconds % 60u));
}

static void music_style_scene(void)
{
    if (!s_host) return;
    const ui_theme_t *theme = music_theme();

    lv_obj_set_style_bg_color(s_host, theme->bg, 0);
    lv_obj_set_style_bg_opa(s_host, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_title, theme->accent, 0);
    lv_obj_set_style_text_color(s_source_label, theme->text, 0);
    lv_obj_set_style_text_opa(s_source_label, LV_OPA_60, 0);
    lv_obj_set_style_text_color(s_volume_label, theme->text, 0);
    lv_obj_set_style_text_color(s_track_label, theme->text, 0);
    lv_obj_set_style_text_color(s_meta_label,
                                s_source == MUSIC_SOURCE_ERROR
                                    ? theme->accent2 : theme->text, 0);
    lv_obj_set_style_text_opa(s_meta_label, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(s_progress_bg, theme->grid, 0);
    lv_obj_set_style_bg_color(s_progress_fill, theme->accent, 0);
    lv_obj_set_style_text_color(s_elapsed_label, theme->text, 0);
    lv_obj_set_style_text_color(s_duration_label, theme->text, 0);
    lv_obj_set_style_text_opa(s_elapsed_label, LV_OPA_60, 0);
    lv_obj_set_style_text_opa(s_duration_label, LV_OPA_60, 0);
    lv_obj_set_style_text_color(s_prev_icon, theme->text, 0);
    lv_obj_set_style_text_color(s_next_icon, theme->text, 0);
    lv_obj_set_style_text_color(s_play_icon, theme->accent, 0);

    for (int i = 0; i < MUSIC_BAR_COUNT; i++) {
        lv_obj_set_style_bg_color(
            s_bars[i], (i % 5) == 4 ? theme->accent2 : theme->accent, 0);
        lv_obj_set_style_bg_opa(s_bars[i], (lv_opa_t)(45 + i * 6), 0);
    }
}

static const char *music_track_name(void)
{
    if (s_track_count <= 0) return "GG LOBBY";
    return s_tracks[s_track].name;
}

static void music_update_ui(bool force)
{
    if (!s_host) return;
    const uint32_t now = plat_millis();
    if (!force && (uint32_t)(now - s_last_ui_ms) < MUSIC_UI_REFRESH_MS) {
        return;
    }
    s_last_ui_ms = now;

    audio_playback_status_t status;
    audio_playback_get_status(&status);
    const bool playing = status.state == AUDIO_PLAYBACK_PLAYING;
    const uint32_t position = music_position_ms();
    const uint32_t duration = music_duration_ms();
    char elapsed[16];
    char total[16];
    char text[96];

    lv_label_set_text(s_track_label, music_track_name());
    if (s_track_count <= 0) {
        lv_label_set_text(s_source_label, "BUILT-IN");
    } else {
        (void)snprintf(text, sizeof(text), "%d / %d  SD",
                       s_track + 1, s_track_count);
        lv_label_set_text(s_source_label, text);
    }
    (void)snprintf(text, sizeof(text), LV_SYMBOL_VOLUME_MAX "  %d%%",
                   s_volume_step * 5);
    lv_label_set_text(s_volume_label, text);

    if (s_source == MUSIC_SOURCE_ERROR) {
        lv_label_set_text(s_meta_label, s_error);
        lv_label_set_text(s_play_icon, LV_SYMBOL_REFRESH);
    } else if (s_source == MUSIC_SOURCE_LOBBY) {
        lv_label_set_text(s_meta_label,
                          playing ? "8-BIT LOOP" : "PAUSED  8-BIT LOOP");
        lv_label_set_text(s_play_icon,
                          playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    } else {
        (void)snprintf(text, sizeof(text), "%s  %lu Hz  %s",
                       playing ? "WAV" : "PAUSED",
                       (unsigned long)s_wav.sample_rate,
                       s_wav.channels == 1u ? "MONO" : "STEREO");
        lv_label_set_text(s_meta_label, text);
        lv_label_set_text(s_play_icon,
                          playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }

    format_time(position, elapsed);
    format_time(duration, total);
    lv_label_set_text(s_elapsed_label, elapsed);
    lv_label_set_text(s_duration_label, total);
    int progress_width = duration > 0u
        ? (int)((uint64_t)MUSIC_PROGRESS_W * position / duration)
        : 0;
    if (progress_width <= 0) {
        lv_obj_add_flag(s_progress_fill, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(s_progress_fill, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(s_progress_fill, progress_width);
    }

    const float activity = playing ? s_visual_peak : 0.0f;
    for (int i = 0; i < MUSIC_BAR_COUNT; i++) {
        const int pulse = (int)((now / 90u + (uint32_t)i * 5u) % 11u);
        const float shape = 0.48f + (float)((i * 7) % 9) * 0.055f;
        int height = 5 + (int)(activity * shape * (72 + pulse * 2));
        if (height > 94) height = 94;
        lv_obj_set_height(s_bars[i], height);
        lv_obj_set_y(s_bars[i], 158 - height);
    }
    s_visual_peak *= 0.76f;
    if (force) music_style_scene();
}

static void music_start_current(void)
{
    if (!s_claimed) return;
    (void)audio_playback_stop(MUSIC_OWNER_ID);
    s_visual_peak = 0.0f;
    if (!music_open_source()) {
        music_update_ui(true);
        return;
    }
    music_fill_queue();
    (void)audio_playback_play(MUSIC_OWNER_ID);
    music_update_ui(true);
}

static void music_select_relative(int delta)
{
    const bool had_tracks = s_track_count > 0;
    if (s_track_count <= 0) {
        music_scan_catalog();
    }
    if (s_track_count > 0 && had_tracks) {
        s_track = (s_track + delta + s_track_count) % s_track_count;
    }
    music_start_current();
}

static void music_toggle_playback(void)
{
    if (!s_claimed) return;
    audio_playback_status_t status;
    audio_playback_get_status(&status);

    if (s_source == MUSIC_SOURCE_ERROR || !s_source_ready ||
        (s_source_eof && status.queued_music_frames == 0u)) {
        music_start_current();
    } else if (status.state == AUDIO_PLAYBACK_PLAYING) {
        (void)audio_playback_pause(MUSIC_OWNER_ID);
    } else {
        music_fill_queue();
        (void)audio_playback_play(MUSIC_OWNER_ID);
    }
    music_update_ui(true);
}

static void music_create_scene(void)
{
    s_host = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(s_host, 0, 0);
    lv_obj_set_size(s_host, 480, 320);
    style_group(s_host);

    s_title = make_label(s_host, "MUSIC", &lv_font_montserrat_14);
    lv_obj_set_pos(s_title, 18, 13);

    s_volume_label = make_label(s_host, "", &lv_font_montserrat_14);
    lv_obj_set_width(s_volume_label, 130);
    lv_obj_set_style_text_align(s_volume_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_volume_label, 175, 13);

    s_source_label = make_label(s_host, "", font_small());
    lv_obj_set_width(s_source_label, 150);
    lv_obj_set_style_text_align(s_source_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_source_label, 312, 13);

    for (int i = 0; i < MUSIC_BAR_COUNT; i++) {
        s_bars[i] = make_block(s_host, 22 + i * 28, 153, 16, 5);
    }

    s_track_label = make_label(s_host, "GG LOBBY",
                               &lv_font_montserrat_28);
    lv_obj_set_width(s_track_label, 420);
    lv_label_set_long_mode(s_track_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_track_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_track_label, 30, 174);

    s_meta_label = make_label(s_host, "8-BIT LOOP", font_small());
    lv_obj_set_width(s_meta_label, 420);
    lv_label_set_long_mode(s_meta_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_meta_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_meta_label, 30, 211);

    s_progress_bg = make_block(s_host, 40, 239, MUSIC_PROGRESS_W, 3);
    s_progress_fill = make_block(s_host, 40, 239, 1, 3);
    s_elapsed_label = make_label(s_host, "0:00", font_small());
    lv_obj_set_pos(s_elapsed_label, 40, 247);
    s_duration_label = make_label(s_host, "0:00", font_small());
    lv_obj_set_width(s_duration_label, 80);
    lv_obj_set_style_text_align(s_duration_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_duration_label, 360, 247);

    s_prev_icon = make_label(s_host, LV_SYMBOL_PREV,
                             &lv_font_montserrat_28);
    lv_obj_set_pos(s_prev_icon, 158, 278);
    s_play_icon = make_label(s_host, LV_SYMBOL_PLAY,
                             &lv_font_montserrat_28);
    lv_obj_set_pos(s_play_icon, 226, 278);
    s_next_icon = make_label(s_host, LV_SYMBOL_NEXT,
                             &lv_font_montserrat_28);
    lv_obj_set_pos(s_next_icon, 294, 278);
}

static void music_enter(int variant)
{
    (void)variant;
    audio_set_mode(AUDIO_NONE);
    music_create_scene();
    s_claimed = audio_playback_claim(MUSIC_OWNER_ID) == AUDIO_PLAYBACK_OK;
    music_load_volume();
    music_scan_catalog();
    if (s_claimed) {
        music_start_current();
    } else {
        s_source = MUSIC_SOURCE_ERROR;
        (void)snprintf(s_error, sizeof(s_error), "Audio output unavailable");
        music_update_ui(true);
    }
}

static void music_exit(void)
{
    music_close_source();
    if (s_claimed) {
        (void)audio_playback_release(MUSIC_OWNER_ID);
    }
    s_claimed = false;
    if (s_host) lv_obj_delete(s_host);
    s_host = NULL;
    s_title = NULL;
    s_source_label = NULL;
    s_volume_label = NULL;
    s_track_label = NULL;
    s_meta_label = NULL;
    s_progress_bg = NULL;
    s_progress_fill = NULL;
    s_elapsed_label = NULL;
    s_duration_label = NULL;
    s_prev_icon = NULL;
    s_play_icon = NULL;
    s_next_icon = NULL;
    memset(s_bars, 0, sizeof(s_bars));
    audio_set_mode(AUDIO_SPECTRUM);
}

static void music_render(void)
{
    if (!s_host || !s_claimed) return;
    if (!audio_playback_is_available()) {
        s_claimed = false;
        music_close_source();
        s_source = MUSIC_SOURCE_ERROR;
        (void)snprintf(s_error, sizeof(s_error),
                       "Audio output disconnected");
        music_update_ui(true);
        return;
    }
    audio_playback_status_t status;
    audio_playback_get_status(&status);
    if (status.state == AUDIO_PLAYBACK_PLAYING) music_fill_queue();

    audio_playback_get_status(&status);
    if (s_source == MUSIC_SOURCE_WAV && s_source_eof &&
        status.queued_music_frames == 0u) {
        const uint32_t now = plat_millis();
        if (s_eof_empty_since == 0u) s_eof_empty_since = now;
        if ((uint32_t)(now - s_eof_empty_since) >= MUSIC_EOF_DRAIN_MS) {
            if (s_track_count > 0) {
                s_track = (s_track + 1) % s_track_count;
            }
            music_start_current();
        }
    } else {
        s_eof_empty_since = 0u;
    }
    music_update_ui(false);
}

static bool music_on_event(ui_event_t event)
{
    if (event == EV_LEFT) {
        music_select_relative(-1);
        return true;
    }
    if (event == EV_RIGHT) {
        music_select_relative(1);
        return true;
    }
    if (event == EV_OK) {
        music_toggle_playback();
        return true;
    }
    if (event == EV_UP) {
        s_volume_step++;
        music_apply_volume(true);
        music_update_ui(true);
        return true;
    }
    if (event == EV_DOWN) {
        s_volume_step--;
        music_apply_volume(true);
        music_update_ui(true);
        return true;
    }
    return false;
}

static int music_mode_count(void)
{
    return 1;
}

static const char *music_mode_name(int idx)
{
    return idx == 0 ? "Player" : "";
}

static int music_mode_index(void)
{
    return 0;
}

static void music_mode_set(int idx)
{
    (void)idx;
}

static int music_volume_count(void)
{
    return 6;
}

static const char *music_volume_name(int idx)
{
    static const char *const NAMES[] = {
        "0%", "20%", "40%", "60%", "80%", "100%",
    };
    return idx >= 0 && idx < 6 ? NAMES[idx] : "";
}

static int music_volume_index(void)
{
    int index = (s_volume_step + 2) / 4;
    return index > 5 ? 5 : index;
}

static void music_volume_set(int idx)
{
    if (idx < 0) idx = 0;
    if (idx > 5) idx = 5;
    s_volume_step = idx * 4;
    music_apply_volume(true);
    music_update_ui(true);
}

static const app_choice_setting_t MUSIC_CHOICE_SETTINGS[] = {
    {
        .name = "Volume",
        .item_count = music_volume_count,
        .item_name = music_volume_name,
        .item_index = music_volume_index,
        .item_set = music_volume_set,
    },
};

const gadget_app_t APP_MUSIC = {
    .id = "music",
    .name = "Music",
    .audio_mode = AUDIO_NONE,
    .icon = NULL,
    .on_enter = music_enter,
    .on_exit = music_exit,
    .on_render = music_render,
    .on_event = music_on_event,
    .on_appearance_changed = music_style_scene,
    .mode_count = music_mode_count,
    .mode_name = music_mode_name,
    .mode_index = music_mode_index,
    .mode_set = music_mode_set,
    .choice_settings = MUSIC_CHOICE_SETTINGS,
    .choice_setting_count = 1,
    .output_routes = APP_OUTPUT_AUX | APP_OUTPUT_HEADPHONES,
    .required_capabilities = PLAT_CAP_AUDIO_PLAYBACK_OUTPUT,
};

#ifdef PEDAL_SIM
bool music_app_debug_is_lobby(void)
{
    return s_source == MUSIC_SOURCE_LOBBY;
}

bool music_app_debug_is_playing(void)
{
    audio_playback_status_t status;
    audio_playback_get_status(&status);
    return status.state == AUDIO_PLAYBACK_PLAYING;
}

int music_app_debug_volume_step(void)
{
    return s_volume_step;
}

uint32_t music_app_debug_position_ms(void)
{
    return music_position_ms();
}
#endif
