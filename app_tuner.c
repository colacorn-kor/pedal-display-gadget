#include "gadget_app.h"

#include "app_slots.h"
#include "theme.h"
#include "tuner.h"
#include "tuner_screen.h"

static int s_variant;

static void tuner_enter(int variant)
{
    s_variant = variant;
    mute_set(1);
    audio_set_mode(AUDIO_TUNER);
    tuner_screen_apply_theme(
        theme_for_app_color(app_slots_color(&APP_TUNER)));
    tuner_screen_create();
}

static void tuner_exit(void)
{
    tuner_screen_destroy();
    mute_set(0);
    audio_set_mode(AUDIO_SPECTRUM);
}

static void tuner_render(void)
{
    tuner_result_t result;
    tuner_get(&result);
    tuner_screen_update(result.voiced, result.name, result.octave,
                        result.cents, result.f0);
}

static bool tuner_on_event(ui_event_t event)
{
    (void)event;
    (void)s_variant;
    return false;
}

static void tuner_appearance_changed(void)
{
    tuner_screen_apply_theme(
        theme_for_app_color(app_slots_color(&APP_TUNER)));
}

static int tuner_mode_count(void)
{
    return 1;
}

static const char *tuner_mode_name(int idx)
{
    return idx == 0 ? "Standard" : "";
}

static int tuner_mode_index(void)
{
    return 0;
}

static void tuner_mode_set(int idx)
{
    (void)idx;
}

const gadget_app_t APP_TUNER = {
    .id = "tuner",
    .name = "Tuner",
    .audio_mode = AUDIO_TUNER,
    .icon = NULL,
    .on_enter = tuner_enter,
    .on_exit = tuner_exit,
    .on_render = tuner_render,
    .on_event = tuner_on_event,
    .on_appearance_changed = tuner_appearance_changed,
    .mode_count = tuner_mode_count,
    .mode_name = tuner_mode_name,
    .mode_index = tuner_mode_index,
    .mode_set = tuner_mode_set,
    .variant_count = 2,
};
