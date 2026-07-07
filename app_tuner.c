#include "gadget_app.h"

#include "tuner.h"
#include "tuner_screen.h"

static int s_variant;

static void tuner_enter(int variant)
{
    s_variant = variant;
    mute_set(1);
    audio_set_mode(AUDIO_TUNER);
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

const gadget_app_t APP_TUNER = {
    .id = "tuner",
    .name = "Tuner",
    .audio_mode = AUDIO_TUNER,
    .icon = NULL,
    .on_enter = tuner_enter,
    .on_exit = tuner_exit,
    .on_render = tuner_render,
    .on_event = tuner_on_event,
    .variant_count = 2,
};
