#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app.h"
#include "lvgl.h"
#include "platform.h"

typedef enum {
    APP_INPUT_BUTTONS = 1u << 0,
    APP_INPUT_FOOTSWITCH = 1u << 1,
    APP_INPUT_MIDI = 1u << 2,
} app_input_source_t;

typedef enum {
    APP_OUTPUT_DISPLAY = 1u << 0,
    APP_OUTPUT_AUX = 1u << 1,
    APP_OUTPUT_HEADPHONES = 1u << 2,
} app_output_route_t;

typedef struct gadget_app gadget_app_t;

typedef void (*app_enter_fn)(int variant);
typedef void (*app_exit_fn)(void);
typedef void (*app_render_fn)(void);
typedef bool (*app_event_fn)(ui_event_t event);
typedef void (*app_appearance_fn)(void);
typedef int (*app_mode_count_fn)(void);
typedef const char *(*app_mode_name_fn)(int idx);
typedef int (*app_mode_index_fn)(void);
typedef void (*app_mode_set_fn)(int idx);

typedef struct {
    const char *name;
    app_mode_count_fn item_count;
    app_mode_name_fn item_name;
    app_mode_index_fn item_index;
    app_mode_set_fn item_set;
} app_choice_setting_t;

struct gadget_app {
    const char *id;
    const char *name;
    audio_mode_t audio_mode;
    const lv_img_dsc_t *icon;

    app_enter_fn on_enter;
    app_exit_fn on_exit;
    app_render_fn on_render;
    app_event_fn on_event;

    app_appearance_fn on_appearance_changed;
    app_mode_count_fn mode_count;
    app_mode_name_fn mode_name;
    app_mode_index_fn mode_index;
    app_mode_set_fn mode_set;
    const app_choice_setting_t *choice_settings;
    int choice_setting_count;

    app_input_source_t input_sources; /* Phase 2 reserved; zero means default inputs. */
    app_output_route_t output_routes; /* Phase 2 reserved; no enum value exists for main output. */
    int variant_count;      /* Phase 2 reserved. */
    platform_capability_mask_t required_capabilities;
};

void app_registry_register(const gadget_app_t *app);
int app_registry_count(void);
const gadget_app_t *app_registry_at(int idx);
int app_registry_find(const char *id);
const char *app_registry_name(int idx);
bool app_registry_is_available(const gadget_app_t *app);
void apps_init(void);

extern const gadget_app_t APP_MONITOR;
extern const gadget_app_t APP_IMAGES;
extern const gadget_app_t APP_TUNER;
extern const gadget_app_t APP_DB_METER;
extern const gadget_app_t APP_MUSIC;
extern const gadget_app_t APP_GAME;
extern const gadget_app_t APP_METRONOME;
extern const gadget_app_t APP_OSCILLOSCOPE;
extern const gadget_app_t APP_MIDI_MONITOR;

void monitor_app_set_scene(int theme, int renderer);
void monitor_app_refresh(void);
void images_app_set_content(int content);
int images_app_count(void);

#ifdef PEDAL_SIM
int monitor_app_debug_smoothing_index(void);
int monitor_app_debug_weighting_index(void);
int bounce_app_debug_cat_y(void);
bool bounce_app_debug_game_over(void);
int db_meter_debug_input_range(void);
int db_meter_debug_average_mode(void);
bool music_app_debug_is_lobby(void);
bool music_app_debug_is_playing(void);
int music_app_debug_volume_step(void);
uint32_t music_app_debug_position_ms(void);
bool game_app_debug_is_lobby(void);
bool game_app_debug_is_builtin(void);
bool game_app_debug_is_external(void);
int game_app_debug_selected_tile(void);
int game_app_debug_tile_count(void);
int game_app_debug_detected_files(void);
uint32_t game_app_debug_frame_sequence(void);
const char *game_app_debug_lobby_name(void);
const char *game_app_debug_lobby_source(void);
int images_app_debug_state(void);
int images_app_debug_count(void);
int images_app_debug_index(void);
const char *images_app_debug_path(void);
bool metronome_app_debug_running(void);
int metronome_app_debug_bpm(void);
int metronome_app_debug_meter(void);
int metronome_app_debug_subdivisions(void);
uint32_t metronome_app_debug_tick_count(void);
bool metronome_app_debug_audio_claimed(void);
int oscilloscope_app_debug_time_index(void);
int oscilloscope_app_debug_scale_index(void);
bool oscilloscope_app_debug_held(void);
uint32_t oscilloscope_app_debug_drawn_sequence(void);
bool midi_monitor_debug_paused(void);
int midi_monitor_debug_channel(void);
uint32_t midi_monitor_debug_total(void);
int midi_monitor_debug_visible_count(void);
#endif
