#pragma once

#include "lvgl.h"

typedef struct {
    char name[12];
    lv_color_t bg;
    lv_color_t surface;
    lv_color_t text;
    lv_color_t accent;
    lv_color_t accent2;
    lv_color_t grid;
} ui_theme_t;

typedef enum {
    APP_COLOR_DEFAULT = 0,
    APP_COLOR_BLUE,
    APP_COLOR_WHITE,
    APP_COLOR_GREEN,
    APP_COLOR_COUNT,
} app_color_t;

void theme_init(void);
const ui_theme_t *theme_get(void);
const ui_theme_t *theme_at(int idx);
int theme_count(void);
int theme_index(void);
void theme_set_index(int idx);
void theme_on_change(void (*cb)(void));

const ui_theme_t *theme_for_app_color(app_color_t color);
const char *theme_app_color_name(int idx);
