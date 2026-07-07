#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app.h"
#include "lvgl.h"

typedef enum {
    APP_INPUT_BUTTONS = 1u << 0,
    APP_INPUT_FOOTSWITCH = 1u << 1,
    APP_INPUT_MIDI = 1u << 2,
} app_input_source_t;

typedef enum {
    APP_OUTPUT_DISPLAY = 1u << 0,
    APP_OUTPUT_AUX = 1u << 1,
    APP_OUTPUT_HEADPHONES = 1u << 2,
} app_output_route_t;

typedef struct gadget_app gadget_app_t;

typedef void (*app_enter_fn)(int variant);
typedef void (*app_exit_fn)(void);
typedef void (*app_render_fn)(void);
typedef bool (*app_event_fn)(ui_event_t event);

struct gadget_app {
    const char *id;
    const char *name;
    audio_mode_t audio_mode;
    const lv_img_dsc_t *icon;

    app_enter_fn on_enter;
    app_exit_fn on_exit;
    app_render_fn on_render;
    app_event_fn on_event;

    app_input_source_t input_sources; /* Phase 2 reserved; zero means default inputs. */
    app_output_route_t output_routes; /* Phase 2 reserved; no enum value exists for main output. */
    int variant_count;      /* Phase 2 reserved. */
    bool needs_codec;       /* Phase 2 reserved. */
};

void app_registry_register(const gadget_app_t *app);
int app_registry_count(void);
const gadget_app_t *app_registry_at(int idx);
int app_registry_find(const char *id);
const char *app_registry_name(int idx);
void apps_init(void);

extern const gadget_app_t APP_MONITOR;
extern const gadget_app_t APP_IMAGES;
extern const gadget_app_t APP_TUNER;

void monitor_app_set_scene(int theme, int renderer);
void monitor_app_refresh(void);
void images_app_set_content(int content);
int images_app_count(void);
