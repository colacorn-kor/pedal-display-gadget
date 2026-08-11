#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app.h"
#include "music_events.h"

typedef uint32_t platform_capability_mask_t;

typedef enum {
    PLAT_CAP_DISPLAY = 1u << 0,
    PLAT_CAP_AUDIO_ANALYSIS_INPUT = 1u << 1,
    PLAT_CAP_AUDIO_PLAYBACK_OUTPUT = 1u << 2,
    PLAT_CAP_MEDIA_STORAGE = 1u << 3,
    PLAT_CAP_MIDI_INPUT = 1u << 4,
    PLAT_CAP_MIDI_OUTPUT = 1u << 5,
    PLAT_CAP_GAME_RUNTIME = 1u << 6,
} platform_capability_t;

typedef uint32_t platform_game_input_mask_t;

typedef enum {
    PLAT_GAME_INPUT_UP     = 1u << 0,
    PLAT_GAME_INPUT_DOWN   = 1u << 1,
    PLAT_GAME_INPUT_LEFT   = 1u << 2,
    PLAT_GAME_INPUT_RIGHT  = 1u << 3,
    PLAT_GAME_INPUT_OK     = 1u << 4,
    PLAT_GAME_INPUT_A      = 1u << 5,
    PLAT_GAME_INPUT_B      = 1u << 6,
    PLAT_GAME_INPUT_START  = 1u << 7,
    PLAT_GAME_INPUT_SELECT = 1u << 8,
} platform_game_input_t;

typedef struct {
    bool input_available;
    bool output_available;
    char input_name[64];
    char output_name[64];
    uint32_t received_messages;
    uint32_t dropped_messages;
    uint32_t sent_messages;
} platform_midi_status_t;

void plat_init(void);
platform_capability_mask_t plat_capabilities(void);

static inline bool plat_has_capabilities(platform_capability_mask_t required)
{
    return (plat_capabilities() & required) == required;
}

uint32_t plat_millis(void);
uint64_t plat_micros(void);
bool plat_input_poll(ui_event_t *ev);
platform_game_input_mask_t plat_game_input_state(void);
void plat_game_input_publish(platform_game_input_mask_t held_mask);
void plat_nvs_load(void *blob, size_t n, bool *found);
void plat_nvs_save(const void *blob, size_t n);
void plat_audio_viz_get(audio_viz_snapshot_t *out);
void plat_music_get(music_snapshot_t *out);
void plat_midi_get_status(platform_midi_status_t *out);
bool plat_midi_send_short(uint8_t status, uint8_t data1, uint8_t data2);
void plat_lvgl_lock(void);
void plat_lvgl_unlock(void);
