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

void theme_init(void);
const ui_theme_t *theme_get(void);
int theme_count(void);
int theme_index(void);
void theme_set_index(int idx);
void theme_on_change(void (*cb)(void));
