#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app.h"
#include "music_events.h"

void plat_init(void);
uint32_t plat_millis(void);
bool plat_input_poll(ui_event_t *ev);
void plat_nvs_load(void *blob, size_t n, bool *found);
void plat_nvs_save(const void *blob, size_t n);
void plat_audio_viz_get(audio_viz_snapshot_t *out);
void plat_music_get(music_snapshot_t *out);
void plat_lvgl_lock(void);
void plat_lvgl_unlock(void);
