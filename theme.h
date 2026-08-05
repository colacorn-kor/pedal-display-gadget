#pragma once

#include "lvgl.h"

typedef struct {
    char name[16];
    lv_color_t bg;
    lv_color_t surface;
    lv_color_t text;
    lv_color_t accent;
    lv_color_t accent2;
    lv_color_t grid;
} ui_theme_t;

typedef enum {
    UI_THEME_MODE_DARK = 0,
    UI_THEME_MODE_LIGHT,
    UI_THEME_MODE_COUNT,
} ui_theme_mode_t;

typedef enum {
    UI_THEME_COLOR_BLUE = 0,
    UI_THEME_COLOR_GREEN,
    UI_THEME_COLOR_YELLOW,
    UI_THEME_COLOR_RED,
    UI_THEME_COLOR_COUNT,
} ui_theme_color_t;

typedef enum {
    APP_COLOR_DEFAULT = 0,
    APP_COLOR_BLUE,
    APP_COLOR_GREEN,
    APP_COLOR_YELLOW,
    APP_COLOR_RED,
    APP_COLOR_COUNT,
} app_color_t;

void theme_init(void);
const ui_theme_t *theme_get(void);
const ui_theme_t *theme_at(int idx);
int theme_count(void);
int theme_index(void);
void theme_set_index(int idx);
ui_theme_mode_t theme_mode(void);
ui_theme_color_t theme_color(void);
void theme_set_mode(ui_theme_mode_t mode);
void theme_set_color(ui_theme_color_t color);
int theme_mode_count(void);
int theme_color_count(void);
const char *theme_mode_name(int idx);
const char *theme_color_name(int idx);
int theme_index_for(ui_theme_mode_t mode, ui_theme_color_t color);
const ui_theme_t *theme_at_mode_color(ui_theme_mode_t mode,
                                      ui_theme_color_t color);
void theme_on_change(void (*cb)(void));

const ui_theme_t *theme_for_app_color(app_color_t color);
const char *theme_app_color_name(int idx);
