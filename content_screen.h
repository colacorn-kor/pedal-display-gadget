#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "theme.h"

typedef enum {
    CONTENT_VIEW_NONE = 0,
    CONTENT_VIEW_IMAGE,
    CONTENT_VIEW_GIF,
    CONTENT_VIEW_WALLPAPER,
    CONTENT_VIEW_STATUS,
    CONTENT_VIEW_ERROR,
} content_view_t;

void content_fs_register(void);
void content_screen_apply_theme(const ui_theme_t *theme);
void content_screen_create(void);
void content_screen_destroy(void);
void content_screen_set_chrome(const char *title, const char *counter,
                               const char *name, const char *detail,
                               bool navigation);
void content_screen_set_chrome_visible(bool visible);
bool content_image_get_info(const char *file, bool gif,
                            uint32_t *width, uint32_t *height);
void content_show_image(const char *file, const char *name);
void content_show_gif(const char *file, const char *name);
void content_show_text(const char *text, const char *name);
void content_show_wallpaper(const char *text, const char *name);
void content_show_status(const char *symbol, const char *status,
                         const char *detail, bool error);

#ifdef PEDAL_SIM
bool content_screen_debug_wallpaper_visible(void);
content_view_t content_screen_debug_view(void);
const char *content_screen_debug_name(void);
const char *content_screen_debug_counter(void);
const char *content_screen_debug_status(void);
bool content_screen_debug_chrome_visible(void);
#endif

