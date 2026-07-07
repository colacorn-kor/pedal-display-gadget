#include "theme.h"

#include "app_slots.h"

static const ui_theme_t THEMES[] = {
    {
        .name = "CHARCOAL",
        .bg = LV_COLOR_MAKE(0x10, 0x14, 0x18),
        .surface = LV_COLOR_MAKE(0x1C, 0x23, 0x2B),
        .text = LV_COLOR_MAKE(0xE8, 0xEC, 0xF1),
        .accent = LV_COLOR_MAKE(0x4F, 0xC3, 0xF7),
        .accent2 = LV_COLOR_MAKE(0xFF, 0xB7, 0x4D),
        .grid = LV_COLOR_MAKE(0x2A, 0x33, 0x3D),
    },
    {
        .name = "IVORY",
        .bg = LV_COLOR_MAKE(0xF2, 0xEF, 0xE8),
        .surface = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .text = LV_COLOR_MAKE(0x22, 0x26, 0x2B),
        .accent = LV_COLOR_MAKE(0x2F, 0x6F, 0xED),
        .accent2 = LV_COLOR_MAKE(0xE4, 0x57, 0x2E),
        .grid = LV_COLOR_MAKE(0xD8, 0xD2, 0xC4),
    },
    {
        .name = "CRT",
        .bg = LV_COLOR_MAKE(0x06, 0x10, 0x06),
        .surface = LV_COLOR_MAKE(0x0A, 0x1A, 0x0A),
        .text = LV_COLOR_MAKE(0xB7, 0xF5, 0xB7),
        .accent = LV_COLOR_MAKE(0x39, 0xFF, 0x14),
        .accent2 = LV_COLOR_MAKE(0xF5, 0xF5, 0x3C),
        .grid = LV_COLOR_MAKE(0x12, 0x33, 0x12),
    },
};

static int s_theme_idx;
static void (*s_on_change)(void);

static int clamp_index(int idx)
{
    const int n = theme_count();
    if (idx < 0) return 0;
    if (idx >= n) return n - 1;
    return idx;
}

void theme_init(void)
{
    s_theme_idx = clamp_index(app_slots_theme());
    if ((uint8_t)s_theme_idx != app_slots_theme()) {
        app_slots_set_theme((uint8_t)s_theme_idx);
    }
}

const ui_theme_t *theme_get(void)
{
    return &THEMES[s_theme_idx];
}

int theme_count(void)
{
    return (int)(sizeof(THEMES) / sizeof(THEMES[0]));
}

int theme_index(void)
{
    return s_theme_idx;
}

void theme_set_index(int idx)
{
    const int next = clamp_index(idx);
    if (next == s_theme_idx) return;
    s_theme_idx = next;
    app_slots_set_theme((uint8_t)s_theme_idx);
    if (s_on_change) s_on_change();
}

void theme_on_change(void (*cb)(void))
{
    s_on_change = cb;
}
