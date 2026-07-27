#pragma once

#include "theme.h"

void tuner_screen_apply_theme(const ui_theme_t *theme);
void tuner_screen_create(void);
void tuner_screen_update(int voiced, const char *name, int octave,
                         float cents, float f0);
void tuner_screen_destroy(void);

