#pragma once

void tuner_screen_create(void);
void tuner_screen_update(int voiced, const char *name, int octave,
                         float cents, float f0);
void tuner_screen_destroy(void);

