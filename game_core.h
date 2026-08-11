#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "game_runtime.h"

typedef struct game_core_instance game_core_instance_t;

typedef struct {
    const char *id;
    const char *system;
    bool (*accepts_path)(const char *path);
    bool (*probe)(FILE *file, size_t file_size, game_rom_info_t *out,
                  char *error, size_t error_size);
    game_core_instance_t *(*create)(const uint8_t *rom, size_t rom_size,
                                    const game_audio_sink_t *audio,
                                    char *error, size_t error_size);
    void (*destroy)(game_core_instance_t *instance);
    void (*set_buttons)(game_core_instance_t *instance,
                        uint8_t pressed_mask);
    bool (*run_frame)(game_core_instance_t *instance);
    const uint8_t *(*frame)(const game_core_instance_t *instance);
    uint32_t (*frame_sequence)(const game_core_instance_t *instance);
    uint8_t *(*save_data)(game_core_instance_t *instance);
    size_t (*save_size)(const game_core_instance_t *instance);
    const char *(*error)(const game_core_instance_t *instance);
} game_core_t;

extern const game_core_t GAME_CORE_PEANUT_GB;
