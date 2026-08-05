#include "theme.h"

#include "app_slots.h"

static const ui_theme_t THEMES[] = {
    {
        .name = "DARK BLUE",
        .bg = LV_COLOR_MAKE(0x10, 0x14, 0x18),
        .surface = LV_COLOR_MAKE(0x1C, 0x23, 0x2B),
        .text = LV_COLOR_MAKE(0xE8, 0xEC, 0xF1),
        .accent = LV_COLOR_MAKE(0x4F, 0xC3, 0xF7),
        .accent2 = LV_COLOR_MAKE(0xFF, 0xB7, 0x4D),
        .grid = LV_COLOR_MAKE(0x2A, 0x33, 0x3D),
    },
    {
        .name = "DARK GREEN",
        .bg = LV_COLOR_MAKE(0x10, 0x14, 0x18),
        .surface = LV_COLOR_MAKE(0x1C, 0x23, 0x2B),
        .text = LV_COLOR_MAKE(0xE8, 0xEC, 0xF1),
        .accent = LV_COLOR_MAKE(0x50, 0xD8, 0x90),
        .accent2 = LV_COLOR_MAKE(0xF2, 0xC1, 0x4E),
        .grid = LV_COLOR_MAKE(0x2A, 0x33, 0x3D),
    },
    {
        .name = "DARK YELLOW",
        .bg = LV_COLOR_MAKE(0x10, 0x14, 0x18),
        .surface = LV_COLOR_MAKE(0x1C, 0x23, 0x2B),
        .text = LV_COLOR_MAKE(0xE8, 0xEC, 0xF1),
        .accent = LV_COLOR_MAKE(0xF4, 0xC9, 0x5D),
        .accent2 = LV_COLOR_MAKE(0x5D, 0xC8, 0xE8),
        .grid = LV_COLOR_MAKE(0x2A, 0x33, 0x3D),
    },
    {
        .name = "DARK RED",
        .bg = LV_COLOR_MAKE(0x10, 0x14, 0x18),
        .surface = LV_COLOR_MAKE(0x1C, 0x23, 0x2B),
        .text = LV_COLOR_MAKE(0xE8, 0xEC, 0xF1),
        .accent = LV_COLOR_MAKE(0xFF, 0x6B, 0x6B),
        .accent2 = LV_COLOR_MAKE(0x61, 0xC0, 0xBF),
        .grid = LV_COLOR_MAKE(0x2A, 0x33, 0x3D),
    },
    {
        .name = "LIGHT BLUE",
        .bg = LV_COLOR_MAKE(0xF2, 0xF4, 0xF6),
        .surface = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .text = LV_COLOR_MAKE(0x22, 0x26, 0x2B),
        .accent = LV_COLOR_MAKE(0x2F, 0x6F, 0xED),
        .accent2 = LV_COLOR_MAKE(0xE4, 0x57, 0x2E),
        .grid = LV_COLOR_MAKE(0xD5, 0xDA, 0xE0),
    },
    {
        .name = "LIGHT GREEN",
        .bg = LV_COLOR_MAKE(0xF2, 0xF4, 0xF6),
        .surface = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .text = LV_COLOR_MAKE(0x22, 0x26, 0x2B),
        .accent = LV_COLOR_MAKE(0x16, 0x84, 0x5B),
        .accent2 = LV_COLOR_MAKE(0xC1, 0x7A, 0x16),
        .grid = LV_COLOR_MAKE(0xD5, 0xDA, 0xE0),
    },
    {
        .name = "LIGHT YELLOW",
        .bg = LV_COLOR_MAKE(0xF2, 0xF4, 0xF6),
        .surface = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .text = LV_COLOR_MAKE(0x22, 0x26, 0x2B),
        .accent = LV_COLOR_MAKE(0xB5, 0x7A, 0x00),
        .accent2 = LV_COLOR_MAKE(0x2E, 0x7E, 0xA1),
        .grid = LV_COLOR_MAKE(0xD5, 0xDA, 0xE0),
    },
    {
        .name = "LIGHT RED",
        .bg = LV_COLOR_MAKE(0xF2, 0xF4, 0xF6),
        .surface = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
        .text = LV_COLOR_MAKE(0x22, 0x26, 0x2B),
        .accent = LV_COLOR_MAKE(0xC7, 0x3E, 0x4D),
        .accent2 = LV_COLOR_MAKE(0x1D, 0x7F, 0x7A),
        .grid = LV_COLOR_MAKE(0xD5, 0xDA, 0xE0),
    },
};

_Static_assert(sizeof(THEMES) / sizeof(THEMES[0]) ==
                   UI_THEME_MODE_COUNT * UI_THEME_COLOR_COUNT,
               "theme table must cover every mode/color combination");

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

const ui_theme_t *theme_at(int idx)
{
    return &THEMES[clamp_index(idx)];
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

int theme_index_for(ui_theme_mode_t mode, ui_theme_color_t color)
{
    if (mode < UI_THEME_MODE_DARK || mode >= UI_THEME_MODE_COUNT) {
        mode = UI_THEME_MODE_DARK;
    }
    if (color < UI_THEME_COLOR_BLUE || color >= UI_THEME_COLOR_COUNT) {
        color = UI_THEME_COLOR_BLUE;
    }
    return (int)mode * UI_THEME_COLOR_COUNT + (int)color;
}

const ui_theme_t *theme_at_mode_color(ui_theme_mode_t mode,
                                      ui_theme_color_t color)
{
    return theme_at(theme_index_for(mode, color));
}

ui_theme_mode_t theme_mode(void)
{
    return (ui_theme_mode_t)(s_theme_idx / UI_THEME_COLOR_COUNT);
}

ui_theme_color_t theme_color(void)
{
    return (ui_theme_color_t)(s_theme_idx % UI_THEME_COLOR_COUNT);
}

void theme_set_mode(ui_theme_mode_t mode)
{
    theme_set_index(theme_index_for(mode, theme_color()));
}

void theme_set_color(ui_theme_color_t color)
{
    theme_set_index(theme_index_for(theme_mode(), color));
}

int theme_mode_count(void)
{
    return UI_THEME_MODE_COUNT;
}

int theme_color_count(void)
{
    return UI_THEME_COLOR_COUNT;
}

const char *theme_mode_name(int idx)
{
    static const char *const NAMES[UI_THEME_MODE_COUNT] = {
        "Dark", "Light",
    };
    return idx >= 0 && idx < UI_THEME_MODE_COUNT ? NAMES[idx] : "";
}

const char *theme_color_name(int idx)
{
    static const char *const NAMES[UI_THEME_COLOR_COUNT] = {
        "Blue", "Green", "Yellow", "Red",
    };
    return idx >= 0 && idx < UI_THEME_COLOR_COUNT ? NAMES[idx] : "";
}

void theme_on_change(void (*cb)(void))
{
    s_on_change = cb;
}

const ui_theme_t *theme_for_app_color(app_color_t color)
{
    if (color == APP_COLOR_DEFAULT) return theme_get();

    ui_theme_color_t fixed_color;
    switch (color) {
    case APP_COLOR_BLUE:   fixed_color = UI_THEME_COLOR_BLUE; break;
    case APP_COLOR_GREEN:  fixed_color = UI_THEME_COLOR_GREEN; break;
    case APP_COLOR_YELLOW: fixed_color = UI_THEME_COLOR_YELLOW; break;
    case APP_COLOR_RED:    fixed_color = UI_THEME_COLOR_RED; break;
    default: return theme_get();
    }
    return theme_at_mode_color(theme_mode(), fixed_color);
}

const char *theme_app_color_name(int idx)
{
    static const char *const NAMES[APP_COLOR_COUNT] = {
        "Default", "Blue", "Green", "Yellow", "Red",
    };
    return idx >= 0 && idx < APP_COLOR_COUNT ? NAMES[idx] : "";
}
