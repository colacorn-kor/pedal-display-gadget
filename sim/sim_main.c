#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"

#include "app.h"
#include "app_slots.h"
#include "audio_level.h"
#include "content_screen.h"
#include "gadget_app.h"
#include "platform.h"
#include "platform_sim.h"
#include "renderer.h"
#include "sim_audio.h"
#include "theme.h"
#include "tuner.h"

#define SMOKE_APP_SCREEN_BASE 1
#define RENDER_BENCH_WARMUP_MS 350U
#define RENDER_BENCH_DURATION_MS 1800U
#define RENDER_BENCH_RESPONSE_LIMIT_MS 120.0

static bool smoke_expect_app(const char *id);

static bool run_one_frame(void)
{
    ui_event_t ev;
    while (plat_input_poll(&ev)) {
        sm_on_event(ev);
    }
    sm_render();

    uint32_t wait_ms = lv_timer_handler();
    if (wait_ms == LV_NO_TIMER_READY || wait_ms > 8) wait_ms = 8;
    if (wait_ms == 0) wait_ms = 1;
    SDL_Delay(wait_ms);
    return !plat_sim_should_quit();
}

static bool run_frames_for(uint32_t duration_ms)
{
    const uint32_t start = SDL_GetTicks();

    do {
        if (!run_one_frame()) return false;
    } while (!plat_sim_should_quit() &&
             SDL_GetTicks() - start < duration_ms);

    return !plat_sim_should_quit();
}

static bool run_renderer_benchmark_frames(uint32_t duration_ms)
{
    const uint32_t start = SDL_GetTicks();
    do {
        const uint32_t phase_ms = (SDL_GetTicks() - start) % 800U;
        const float phase = (float)phase_ms / 800.0f;
        const float triangle = phase < 0.5f
            ? phase * 2.0f : (1.0f - phase) * 2.0f;
        sim_audio_set_mouse_x(triangle * 479.0f);
        if (!run_one_frame()) return false;
    } while (SDL_GetTicks() - start < duration_ms);
    return true;
}

static double performance_elapsed_ms(uint64_t start)
{
    return (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
           (double)SDL_GetPerformanceFrequency();
}

static bool wait_for_popup_state(bool expected, double *elapsed_ms)
{
    const uint64_t start = SDL_GetPerformanceCounter();
    while (sm_debug_popup_open() != expected &&
           performance_elapsed_ms(start) <
               RENDER_BENCH_RESPONSE_LIMIT_MS) {
        if (!run_one_frame()) return false;
    }
    if (elapsed_ms) *elapsed_ms = performance_elapsed_ms(start);
    return sm_debug_popup_open() == expected;
}

static bool benchmark_renderer_mode(const gadget_app_t *monitor,
                                    int mode, const char *name,
                                    double minimum_redraw_hz)
{
    monitor->mode_set(mode);
    app_slots_set_mode_runtime(monitor, (uint8_t)mode);
    if (!run_renderer_benchmark_frames(RENDER_BENCH_WARMUP_MS)) return false;

    renderer_perf_reset();
    const uint64_t wall_start = SDL_GetPerformanceCounter();
    if (!run_renderer_benchmark_frames(RENDER_BENCH_DURATION_MS)) return false;
    const double wall_ms = performance_elapsed_ms(wall_start);
    renderer_perf_stats_t stats;
    renderer_perf_get(&stats);

    double open_ms = 0.0;
    double close_ms = 0.0;
    if (!plat_sim_post_event(EV_HOME) ||
        !wait_for_popup_state(true, &open_ms) ||
        !plat_sim_post_event(EV_HOME) ||
        !wait_for_popup_state(false, &close_ms)) {
        fprintf(stderr,
                "RENDER BENCH FAIL: %s HOME response exceeded %.0fms\n",
                name, RENDER_BENCH_RESPONSE_LIMIT_MS);
        return false;
    }

    const double redraw_hz = wall_ms > 0.0
        ? (double)stats.redraws * 1000.0 / wall_ms : 0.0;
    const double redraw_avg_us = stats.redraws > 0
        ? (double)stats.redraw_us_total / (double)stats.redraws : 0.0;
    printf("RENDER BENCH %s: wall=%.0fms calls=%u "
           "redraws=%u rate=%.1fHz redraw_avg=%.1fus max=%uus "
           "HOME=%.1f/%.1fms\n",
           name, wall_ms, (unsigned)stats.update_calls,
           (unsigned)stats.redraws, redraw_hz, redraw_avg_us,
           (unsigned)stats.update_us_max, open_ms, close_ms);

    if (!stats.name || strcmp(stats.name, name) != 0 ||
        redraw_hz < minimum_redraw_hz ||
        stats.update_us_max > 50000U ||
        open_ms > RENDER_BENCH_RESPONSE_LIMIT_MS ||
        close_ms > RENDER_BENCH_RESPONSE_LIMIT_MS) {
        fprintf(stderr, "RENDER BENCH FAIL: %s acceptance limits\n", name);
        return false;
    }
    return true;
}

static bool run_renderer_benchmark(void)
{
    if (fabsf(renderer_circular_debug_position(0) - 1.0f) > 0.0001f ||
        fabsf(renderer_circular_debug_position(36)) > 0.0001f) {
        fprintf(stderr,
                "RENDER BENCH FAIL: circular top/bottom frequency mapping\n");
        return false;
    }
    for (int segment = 1; segment < 36; segment++) {
        if (fabsf(renderer_circular_debug_position(segment) -
                  renderer_circular_debug_position(72 - segment)) >
            0.0001f) {
            fprintf(stderr,
                    "RENDER BENCH FAIL: circular mirror at segment %d\n",
                    segment);
            return false;
        }
    }

    sm_on_event(EV_OK);
    const gadget_app_t *monitor =
        app_registry_at(app_registry_find("monitor"));
    if (!monitor || !monitor->mode_set || !smoke_expect_app("monitor")) {
        fprintf(stderr, "RENDER BENCH FAIL: monitor unavailable\n");
        return false;
    }
    if (!benchmark_renderer_mode(monitor, 1, "bars", 10.0) ||
        !benchmark_renderer_mode(monitor, 2, "circular", 8.0)) {
        return false;
    }
    const uint32_t mirror_mismatches =
        renderer_circular_debug_mirror_mismatches();
    if (mirror_mismatches != 0) {
        fprintf(stderr,
                "RENDER BENCH FAIL: circular has %u mirrored pixel "
                "mismatches\n",
                (unsigned)mirror_mismatches);
        return false;
    }
    printf("RENDER BENCH PASS: 12-Band/Circular redraw and HOME response\n");
    return true;
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
    const ui_theme_mode_t initial_global_mode = theme_mode();
    const ui_theme_color_t initial_global_color = theme_color();

    if (sm_current() != 0) {
        fprintf(stderr, "SMOKE FAIL: startup did not open launcher\n");
        return false;
    }

    if (!smoke_send(EV_DOWN, "live row -> empty stash row") ||
        !smoke_send(EV_DOWN, "empty stash row -> action row") ||
        !smoke_send(EV_RIGHT, "reorder -> settings") ||
        !smoke_send(EV_OK, "open launcher settings") ||
        !smoke_send(EV_OK, "settings -> theme") ||
        !smoke_send(EV_OK, "theme -> mode") ||
        !smoke_send(EV_DOWN, "cycle global light mode") ||
        !smoke_send(EV_OK, "apply global light mode")) {
        return false;
    }
    if (theme_mode() !=
        (ui_theme_mode_t)((initial_global_mode + 1) % theme_mode_count()) ||
        theme_color() != initial_global_color) {
        fprintf(stderr, "SMOKE FAIL: global theme mode did not advance\n");
        return false;
    }
    if (!smoke_send(EV_DOWN, "theme mode -> color") ||
        !smoke_send(EV_OK, "theme -> color") ||
        !smoke_send(EV_DOWN, "cycle global color") ||
        !smoke_send(EV_OK, "apply global color")) {
        return false;
    }
    if (theme_color() !=
        (ui_theme_color_t)((initial_global_color + 1) %
                           theme_color_count())) {
        fprintf(stderr, "SMOKE FAIL: global theme color did not advance\n");
        return false;
    }
    if (!smoke_send(EV_HOME, "theme -> settings") ||
        !smoke_send(EV_DOWN, "launcher Theme -> About") ||
        !smoke_send(EV_OK, "open launcher About") ||
        !smoke_send(EV_HOME, "About -> launcher settings") ||
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
    const int app_popup_palette = theme_index();
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

    const gadget_app_t *monitor =
        app_registry_at(app_registry_find("monitor"));
    const int initial_mode = monitor && monitor->mode_index
        ? monitor->mode_index()
        : -1;
    const int initial_smoothing = monitor_app_debug_smoothing_index();
    if (!monitor ||
        app_slots_color(monitor) != APP_COLOR_DEFAULT ||
        theme_for_app_color(app_slots_color(monitor)) != theme_get()) {
        fprintf(stderr,
                "SMOKE FAIL: monitor Default color did not inherit UI theme\n");
        return false;
    }
    if (!monitor->mode_index || monitor->mode_index() != initial_mode ||
        !smoke_send(EV_RIGHT, "Curve simplification up") ||
        monitor_app_debug_smoothing_index() !=
            (initial_smoothing < 2
                ? initial_smoothing + 1 : initial_smoothing) ||
        !smoke_send(EV_LEFT, "Curve simplification restore") ||
        monitor_app_debug_smoothing_index() != initial_smoothing) {
        fprintf(stderr,
                "SMOKE FAIL: Curve simplification controls were not stable\n");
        return false;
    }
    if (!smoke_send(EV_HOME, "monitor -> app menu") ||
        !smoke_send(EV_OK, "open app settings") ||
        !smoke_send(EV_OK, "app settings -> app theme") ||
        !smoke_send(EV_DOWN, "app theme mode -> color") ||
        !smoke_send(EV_OK, "app theme -> monitor color") ||
        !smoke_send(EV_DOWN, "monitor Default -> Blue") ||
        !smoke_send(EV_OK, "apply monitor Blue color")) {
        return false;
    }
    if (theme_index() != app_popup_palette ||
        app_slots_color(monitor) != APP_COLOR_BLUE ||
        theme_for_app_color(app_slots_color(monitor)) !=
            theme_at_mode_color(theme_mode(), UI_THEME_COLOR_BLUE)) {
        fprintf(stderr,
                "SMOKE FAIL: monitor color changed popup palette or was "
                "not saved independently\n");
        return false;
    }
    if (!smoke_send(EV_OK, "app theme -> monitor mode") ||
        !smoke_send(EV_DOWN, "Curve -> 12-Band") ||
        !smoke_send(EV_DOWN, "12-Band -> Circular") ||
        !smoke_send(EV_DOWN, "Circular -> Reference") ||
        !smoke_send(EV_OK, "apply Reference mode")) {
        return false;
    }
    if (monitor->mode_index() != 3 ||
        app_slots_mode(monitor) != 3) {
        fprintf(stderr,
                "SMOKE FAIL: monitor Reference mode was not saved\n");
        return false;
    }
    if (!smoke_send(EV_HOME, "app theme -> monitor settings") ||
        !smoke_send(EV_DOWN, "monitor theme -> weighting") ||
        !smoke_send(EV_OK, "open monitor weighting") ||
        !smoke_send(EV_DOWN, "monitor Flat -> A-weighted") ||
        !smoke_send(EV_DOWN, "A-weighted -> Flat(Loudness)") ||
        !smoke_send(EV_DOWN, "Flat(Loudness) -> A-weighted(Loudness)") ||
        !smoke_send(EV_OK, "apply monitor A-weighted(Loudness)") ||
        monitor_app_debug_weighting_index() != 3) {
        fprintf(stderr,
                "SMOKE FAIL: monitor A-weighted(Loudness) was not applied\n");
        return false;
    }
    if (!smoke_send(EV_DOWN, "monitor theme -> weighting") ||
        !smoke_send(EV_OK, "reopen monitor weighting") ||
        !smoke_send(EV_UP, "A-weighted(Loudness) -> Flat(Loudness)") ||
        !smoke_send(EV_UP, "Flat(Loudness) -> A-weighted") ||
        !smoke_send(EV_UP, "A-weighted -> Flat") ||
        !smoke_send(EV_OK, "restore monitor Flat") ||
        monitor_app_debug_weighting_index() != 0) {
        fprintf(stderr,
                "SMOKE FAIL: monitor Flat weighting was not restored\n");
        return false;
    }
    if (!run_frames_for(150)) return false;
    if (!smoke_send(EV_HOME, "app settings -> app menu") ||
        !smoke_send(EV_DOWN, "app Settings -> Info") ||
        !smoke_send(EV_OK, "open app Info") ||
        !smoke_send(EV_HOME, "Info -> app menu") ||
        !smoke_send(EV_HOME, "close app menu") ||
        !smoke_expect_app("monitor")) {
        return false;
    }

    if (!smoke_send(EV_DOWN, "direct Flat -> A-weighted") ||
        monitor_app_debug_weighting_index() != 1 ||
        !smoke_send(EV_UP, "direct A-weighted -> Flat") ||
        monitor_app_debug_weighting_index() != 0) {
        fprintf(stderr,
                "SMOKE FAIL: direct monitor Weighting controls failed\n");
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
    if (!content_screen_debug_wallpaper_visible()) {
        fprintf(stderr,
                "SMOKE FAIL: empty Gallery did not show GG wallpaper\n");
        return false;
    }

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
    if (audio_get_mode() != AUDIO_SPECTRUM) {
        fprintf(stderr, "SMOKE FAIL: bounce did not select spectrum mode\n");
        return false;
    }
    const int bounce_popup_palette = theme_index();
    const gadget_app_t *bounce =
        app_registry_at(app_registry_find("bounce"));
    if (app_slots_color(bounce) != APP_COLOR_DEFAULT ||
        theme_for_app_color(app_slots_color(bounce)) != theme_get()) {
        fprintf(stderr,
                "SMOKE FAIL: bounce Default color did not inherit UI theme\n");
        return false;
    }
    if (!smoke_send(EV_HOME, "bounce -> app menu") ||
        !smoke_send(EV_OK, "open bounce settings") ||
        !smoke_send(EV_OK, "bounce settings -> theme") ||
        !smoke_send(EV_OK, "open bounce mode") ||
        !smoke_send(EV_OK, "apply Classic Cat mode")) {
        return false;
    }
    if (bounce_app_debug_mode_index() != 0 ||
        app_slots_mode(bounce) != 0 ||
        theme_index() != bounce_popup_palette) {
        fprintf(stderr,
                "SMOKE FAIL: bounce mode, saved selection, and popup "
                "palette are inconsistent\n");
        return false;
    }
    if (!smoke_send(EV_HOME, "bounce theme -> settings") ||
        !smoke_send(EV_HOME, "bounce settings -> app menu") ||
        !smoke_send(EV_HOME, "close bounce app menu")) {
        return false;
    }
    const int cat_ground_y = bounce_app_debug_cat_y();
    plat_sim_trigger_onset();
    if (!run_frames_for(160) ||
        bounce_app_debug_cat_y() >= cat_ground_y) {
        fprintf(stderr, "SMOKE FAIL: audio onset did not jump the cat\n");
        return false;
    }
    if (!run_frames_for(5200) || !bounce_app_debug_game_over()) {
        fprintf(stderr,
                "SMOKE FAIL: cat runner did not collide with a cup\n");
        return false;
    }
    plat_sim_trigger_onset();
    if (!run_frames_for(160) || bounce_app_debug_game_over() ||
        bounce_app_debug_cat_y() >= cat_ground_y) {
        fprintf(stderr,
                "SMOKE FAIL: audio onset did not restart the cat runner\n");
        return false;
    }

    if (!smoke_send(EV_FOOTSW, "bounce -> db meter") ||
        !smoke_expect_app("dbmeter")) {
        return false;
    }
    if (db_meter_debug_input_range() != AUDIO_INPUT_LINE ||
        db_meter_debug_average_mode() != 0) {
        fprintf(stderr, "SMOKE FAIL: dB meter defaults are invalid\n");
        return false;
    }
    if (!smoke_send(EV_DOWN, "db meter LINE -> INST") ||
        !smoke_send(EV_RIGHT, "db meter select Window") ||
        !smoke_send(EV_DOWN, "db meter LIVE -> AVG 1s") ||
        db_meter_debug_input_range() != AUDIO_INPUT_INST ||
        db_meter_debug_average_mode() != 1) {
        fprintf(stderr, "SMOKE FAIL: dB meter direct controls did not update\n");
        return false;
    }
    if (!smoke_send(EV_HOME, "db meter -> app menu") ||
        !smoke_send(EV_OK, "open db meter settings") ||
        !smoke_send(EV_DOWN, "db meter Theme -> Input") ||
        !smoke_send(EV_OK, "open db meter Input") ||
        !smoke_send(EV_UP, "db meter INST -> LINE") ||
        !smoke_send(EV_OK, "apply db meter LINE") ||
        !smoke_send(EV_DOWN, "db meter Theme -> Input") ||
        !smoke_send(EV_DOWN, "db meter Input -> Window") ||
        !smoke_send(EV_OK, "open db meter Window") ||
        !smoke_send(EV_DOWN, "db meter AVG 1s -> AVG 3s") ||
        !smoke_send(EV_OK, "apply db meter AVG 3s") ||
        db_meter_debug_input_range() != AUDIO_INPUT_LINE ||
        db_meter_debug_average_mode() != 2) {
        fprintf(stderr, "SMOKE FAIL: dB meter Settings did not update\n");
        return false;
    }
    if (!smoke_send(EV_HOME, "db meter settings -> app menu") ||
        !smoke_send(EV_HOME, "close db meter app menu") ||
        !smoke_send(EV_LEFT, "db meter select Input") ||
        !smoke_send(EV_DOWN, "db meter LINE -> INST after menu") ||
        !smoke_send(EV_RIGHT, "db meter select Window after menu") ||
        !smoke_send(EV_UP, "db meter AVG 3s -> AVG 1s after menu") ||
        db_meter_debug_input_range() != AUDIO_INPUT_INST ||
        db_meter_debug_average_mode() != 1) {
        fprintf(stderr,
                "SMOKE FAIL: dB meter direct and menu states diverged\n");
        return false;
    }
    audio_viz_snapshot_t meter_before;
    audio_viz_snapshot_t meter_after;
    plat_audio_viz_get(&meter_before);
    if (!run_frames_for(1100)) return false;
    plat_audio_viz_get(&meter_after);
    if (meter_after.meter_sample_total <= meter_before.meter_sample_total ||
        meter_after.meter_energy_total <= meter_before.meter_energy_total) {
        fprintf(stderr,
                "SMOKE FAIL: dB meter cumulative power did not advance\n");
        return false;
    }

    if (!smoke_send(EV_FOOTSW_HOLD, "db meter -> quick tuner") ||
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

    printf("SMOKE PASS: three-row launcher, Theme/About, app Theme "
           "Mode/Color, four monitor weightings with direct controls, reorder, "
           "monitor viz, Gallery GG fallback, live cycle, tuner %.2f Hz (%s%d), "
           "Curve controls, Reference mode, Classic Cat runner, "
           "input-voltage meter controls/settings, quick app, cleanup\n",
           tuner.f0, tuner.name, tuner.octave);
    return true;
}

static bool open_preview(const char *preview)
{
    if (strcmp(preview, "monitor-menu") == 0 ||
        strcmp(preview, "monitor-settings") == 0 ||
        strcmp(preview, "monitor-color") == 0 ||
        strcmp(preview, "monitor-mode") == 0 ||
        strcmp(preview, "monitor-weighting") == 0) {
        sm_on_event(EV_OK);
        sm_on_event(EV_HOME);
        if (strcmp(preview, "monitor-menu") == 0) return true;

        sm_on_event(EV_OK);
        if (strcmp(preview, "monitor-settings") == 0) return true;
        if (strcmp(preview, "monitor-weighting") == 0) {
            sm_on_event(EV_DOWN);
            sm_on_event(EV_OK);
            return true;
        }

        sm_on_event(EV_OK);
        if (strcmp(preview, "monitor-color") == 0) {
            sm_on_event(EV_DOWN);
            sm_on_event(EV_OK);
        } else {
            sm_on_event(EV_OK);
        }
        return true;
    }

    if (strcmp(preview, "curve") == 0 ||
        strcmp(preview, "reference") == 0 ||
        strcmp(preview, "bars") == 0 ||
        strcmp(preview, "circular") == 0) {
        sm_on_event(EV_OK);
        const gadget_app_t *monitor =
            app_registry_at(app_registry_find("monitor"));
        int mode = 0;
        if (strcmp(preview, "bars") == 0) mode = 1;
        else if (strcmp(preview, "circular") == 0) mode = 2;
        else if (strcmp(preview, "reference") == 0) mode = 3;
        if (monitor && monitor->mode_set) monitor->mode_set(mode);
        app_slots_set_mode_runtime(monitor, (uint8_t)mode);
        return smoke_expect_app("monitor");
    }

    if (strcmp(preview, "dbmeter") == 0) {
        int idx = app_registry_find("dbmeter");
        if (idx < 0) return false;
        for (int i = 0; i < idx; i++) sm_on_event(EV_RIGHT);
        sm_on_event(EV_OK);
        return smoke_expect_app("dbmeter");
    }

    if (strcmp(preview, "gallery") == 0) {
        int idx = app_registry_find("images");
        if (idx < 0) return false;
        for (int i = 0; i < idx; i++) sm_on_event(EV_RIGHT);
        sm_on_event(EV_OK);
        return smoke_expect_app("images");
    }

    if (strcmp(preview, "bounce") == 0) {
        int idx = app_registry_find("bounce");
        if (idx < 0) return false;
        for (int i = 0; i < idx; i++) sm_on_event(EV_RIGHT);
        sm_on_event(EV_OK);
        return smoke_expect_app("bounce");
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

    if (plat_sim_is_renderer_benchmark() && !preview) {
        return run_renderer_benchmark() ? 0 : 1;
    }
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
