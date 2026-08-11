#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GAME_FRAME_WIDTH 160
#define GAME_FRAME_HEIGHT 144

typedef enum {
    GAME_BUTTON_A      = 1u << 0,
    GAME_BUTTON_B      = 1u << 1,
    GAME_BUTTON_SELECT = 1u << 2,
    GAME_BUTTON_START  = 1u << 3,
    GAME_BUTTON_RIGHT  = 1u << 4,
    GAME_BUTTON_LEFT   = 1u << 5,
    GAME_BUTTON_UP     = 1u << 6,
    GAME_BUTTON_DOWN   = 1u << 7,
} game_button_t;

typedef struct {
    char title[17];
    char system[16];
    size_t rom_bytes;
    bool audio_supported;
} game_rom_info_t;

typedef size_t (*game_audio_write_fn)(const float *stereo_interleaved,
                                      size_t frames, void *ctx);

typedef struct {
    game_audio_write_fn write;
    void *ctx;
    uint32_t sample_rate;
} game_audio_sink_t;

bool game_runtime_probe(const char *path, game_rom_info_t *out,
                        char *error, size_t error_size);
bool game_runtime_start(const char *path, const game_audio_sink_t *audio,
                        char *error, size_t error_size);
void game_runtime_stop(void);
bool game_runtime_running(void);
void game_runtime_set_buttons(uint8_t pressed_mask);
void game_runtime_press(game_button_t button);
bool game_runtime_advance(uint32_t now_ms);
const uint8_t *game_runtime_frame(void);
uint32_t game_runtime_frame_sequence(void);
const game_rom_info_t *game_runtime_info(void);
const char *game_runtime_error(void);
