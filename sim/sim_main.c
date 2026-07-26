#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"

#include "app.h"
#include "app_slots.h"
#include "content_screen.h"
#include "gadget_app.h"
#include "platform.h"
#include "platform_sim.h"
#include "theme.h"
#include "tuner.h"

#define SMOKE_APP_SCREEN_BASE 1

static bool run_frames_for(uint32_t duration_ms)
{
    const uint32_t start = SDL_GetTicks();

    do {
        ui_event_t ev;
        while (plat_input_poll(&ev)) {
            sm_on_event(ev);
        }
        sm_render();

        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms == LV_NO_TIMER_READY || wait_ms > 8) wait_ms = 8;
        if (wait_ms == 0) wait_ms = 1;
        SDL_Delay(wait_ms);
    } while (!plat_sim_should_quit() &&
             SDL_GetTicks() - start < duration_ms);

    return !plat_sim_should_quit();
}

static bool smoke_expect_app(const char *id)
{
    const int idx = app_registry_find(id);
    const int actual = sm_current();
    const int expected = idx < 0 ? -1 : SMOKE_APP_SCREEN_BASE + idx;

    if (idx >= 0 && actual == expected) return true;
    fprintf(stderr,
            "SMOKE FAIL: expected app '%s' (screen %d), got screen %d\n",
            id, expected, actual);
    return false;
}

static bool smoke_send(ui_event_t event, const char *label)
{
    if (!plat_sim_post_event(event)) {
        fprintf(stderr, "SMOKE FAIL: event queue full at %s\n", label);
        return false;
    }
    if (!run_frames_for(40)) {
        fprintf(stderr, "SMOKE FAIL: simulator quit at %s\n", label);
        return false;
    }
    return true;
}

static bool smoke_visualizer_has_signal(void)
{
    audio_viz_snapshot_t snapshot;
    bool nonzero_bar = false;

    plat_audio_viz_get(&snapshot);
    for (int i = 0; i < VIZ_POINTS; i++) {
        if (snapshot.bars[i] > 0.001f) {
            nonzero_bar = true;
            break;
        }
    }
    if (snapshot.level > 0.001f && nonzero_bar) return true;

    fprintf(stderr,
            "SMOKE FAIL: synthetic visualizer is silent (level %.4f)\n",
            snapshot.level);
    return false;
}

static bool smoke_tuner_is_voiced(tuner_result_t *result)
{
    tuner_get(result);
    if (result->voiced && isfinite(result->f0) && result->f0 > 30.0f &&
        result->f0 < 1300.0f) {
        return true;
    }
    fprintf(stderr,
            "SMOKE FAIL: tuner did not lock (voiced %d, f0 %.2f, clarity %.3f)\n",
            result->voiced, result->f0, result->clarity);
    return false;
}

static bool run_smoke_test(void)
{
    tuner_result_t tuner;
    const int initial_theme = theme_index();

    if (sm_current() != 0) {
        fprintf(stderr, "SMOKE FAIL: startup did not open launcher\n");
        return false;
    }

    if (!smoke_send(EV_DOWN, "live row -> empty stash row") ||
        !smoke_send(EV_DOWN, "empty stash row -> action row") ||
        !smoke_send(EV_RIGHT, "reorder -> settings") ||
        !smoke_send(EV_OK, "open launcher settings") ||
        !smoke_send(EV_OK, "settings -> global theme") ||
        !smoke_send(EV_RIGHT, "cycle global theme")) {
        return false;
    }
    if (theme_index() != (initial_theme + 1) % theme_count()) {
        fprintf(stderr, "SMOKE FAIL: global theme selector did not advance\n");
        return false;
    }
    if (!smoke_send(EV_HOME, "theme -> settings") ||
        !smoke_send(EV_HOME, "settings -> launcher") ||
        !smoke_send(EV_LEFT, "settings -> reorder") ||
        !smoke_send(EV_OK, "enter reorder at action row") ||
        !smoke_send(EV_OK, "exit reorder at action row") ||
        !smoke_send(EV_UP, "action row -> empty stash row") ||
        !smoke_send(EV_UP, "empty stash row -> live row")) {
        return false;
    }

    if (!smoke_send(EV_OK, "launcher -> monitor") ||
        !smoke_expect_app("monitor")) return false;
    if (audio_get_mode() != AUDIO_SPECTRUM) {
        fprintf(stderr, "SMOKE FAIL: monitor did not select spectrum mode\n");
        return false;
    }
    if (app_slots_last_view()[0] != '\0') {
        fprintf(stderr, "SMOKE FAIL: last view was saved during rapid switch\n");
        return false;
    }
    if (!run_frames_for(1050) ||
        strcmp(app_slots_last_view(), "monitor") != 0) {
        fprintf(stderr, "SMOKE FAIL: stable app view was not persisted\n");
        return false;
    }

    const int initial_preset = monitor_app_preset_index();
    if (!smoke_send(EV_UP, "monitor direct up is inert") ||
        monitor_app_preset_index() != initial_preset) {
        fprintf(stderr, "SMOKE FAIL: monitor theme changed outside settings\n");
        return false;
    }
    if (!smoke_send(EV_HOME, "monitor -> app menu") ||
        !smoke_send(EV_DOWN, "app menu -> settings") ||
        !smoke_send(EV_OK, "open app settings") ||
        !smoke_send(EV_OK, "app settings -> monitor theme") ||
        !smoke_send(EV_DOWN, "select next monitor preset") ||
        !smoke_send(EV_OK, "apply monitor preset")) {
        return false;
    }
    if (monitor_app_preset_index() !=
        (initial_preset + 1) % monitor_app_preset_count()) {
        fprintf(stderr, "SMOKE FAIL: monitor preset was not applied\n");
        return false;
    }
    if (!smoke_send(EV_HOME, "app settings -> app menu") ||
        !smoke_send(EV_HOME, "close app menu") ||
        !smoke_expect_app("monitor")) {
        return false;
    }

    if (!run_frames_for(250) || !smoke_visualizer_has_signal()) return false;

    if (!smoke_send(EV_HOME_HOLD, "monitor -> launcher") ||
        sm_current() != 0) {
        fprintf(stderr, "SMOKE FAIL: HOME hold did not open launcher\n");
        return false;
    }

    if (!smoke_send(EV_RIGHT, "select images") ||
        !smoke_send(EV_OK, "launcher -> images") ||
        !smoke_expect_app("images")) return false;

    if (!smoke_send(EV_FOOTSW, "images -> tuner") ||
        !smoke_expect_app("tuner")) return false;
    if (audio_get_mode() != AUDIO_TUNER || mute_get() != 1) {
        fprintf(stderr,
                "SMOKE FAIL: tuner resource state is mode %d, mute %d\n",
                (int)audio_get_mode(), mute_get());
        return false;
    }
    if (!run_frames_for(500) || !smoke_tuner_is_voiced(&tuner)) return false;

    if (!smoke_send(EV_FOOTSW, "tuner -> bounce") ||
        !smoke_expect_app("bounce")) return false;
    if (mute_get() != 0) {
        fprintf(stderr, "SMOKE FAIL: tuner exit did not release mute\n");
        return false;
    }

    if (!smoke_send(EV_FOOTSW_HOLD, "bounce -> quick tuner") ||
        !smoke_expect_app("tuner") || mute_get() != 1) {
        fprintf(stderr, "SMOKE FAIL: quick app did not enter muted tuner\n");
        return false;
    }

    if (!smoke_send(EV_HOME_HOLD, "tuner -> launcher") ||
        sm_current() != 0 || mute_get() != 0) {
        fprintf(stderr,
                "SMOKE FAIL: launcher return did not clean up tuner\n");
        return false;
    }

    printf("SMOKE PASS: three-row launcher, settings themes, reorder, "
           "monitor viz, images, live cycle, tuner %.2f Hz (%s%d), "
           "quick app, cleanup\n",
           tuner.f0, tuner.name, tuner.octave);
    return true;
}

static bool open_preview(const char *preview)
{
    if (strcmp(preview, "bars") == 0 ||
        strcmp(preview, "circular") == 0) {
        sm_on_event(EV_OK);
        monitor_app_set_preset(
            strcmp(preview, "bars") == 0 ? 2 : 4);
        return smoke_expect_app("monitor");
    }

    if (strcmp(preview, "dbmeter") == 0) {
        int idx = app_registry_find("dbmeter");
        if (idx < 0) return false;
        for (int i = 0; i < idx; i++) sm_on_event(EV_RIGHT);
        sm_on_event(EV_OK);
        return smoke_expect_app("dbmeter");
    }

    fprintf(stderr, "Unknown preview: %s\n", preview);
    return false;
}

int main(int argc, char **argv)
{
    if (!plat_sim_configure(argc, argv)) return 1;
    if (plat_sim_should_exit_after_args()) return 0;

    lv_init();

    lv_display_t *display = lv_sdl_window_create(480, 320);
    if (!display) return 1;
    lv_sdl_window_set_title(display, "Pedal Display Gadget Simulator");
    lv_sdl_window_set_resizeable(display, false);

    content_fs_register();
    sm_init();

    const char *preview = plat_sim_preview();
    if (preview && !open_preview(preview)) return 1;

    if (plat_sim_is_smoke_test() && !preview) {
        return run_smoke_test() ? 0 : 1;
    }

    while (!plat_sim_should_quit()) {
        ui_event_t ev;
        while (plat_input_poll(&ev)) {
            sm_on_event(ev);
        }
        sm_render();

        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms == LV_NO_TIMER_READY || wait_ms > 16) wait_ms = 16;
        if (wait_ms == 0) wait_ms = 1;
        SDL_Delay(wait_ms);
    }

    return 0;
}
