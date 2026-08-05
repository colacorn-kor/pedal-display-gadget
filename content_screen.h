#pragma once

#include "theme.h"

void content_fs_register(void);
void content_screen_apply_theme(const ui_theme_t *theme);
void content_screen_create(void);
void content_screen_destroy(void);
void content_show_image(const char *file, const char *name);
void content_show_gif(const char *file, const char *name);
void content_show_text(const char *text, const char *name);
void content_show_wallpaper(const char *text, const char *name);

#ifdef PEDAL_SIM
bool content_screen_debug_wallpaper_visible(void);
#endif

