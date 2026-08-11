#include "game_runtime.h"

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "game_core.h"
#include "storage.h"

#define GAME_FRAME_US 16743u
#define GAME_MAX_CATCHUP_FRAMES 3
#define GAME_PULSE_FRAMES 6

typedef struct {
    const game_core_t *core;
    game_core_instance_t *instance;
    uint8_t *rom;
    size_t rom_size;
    game_rom_info_t info;
    char path[STORAGE_PATH_MAX];
    char save_path[STORAGE_PATH_MAX];
    char error[96];
    uint32_t last_ms;
    uint32_t accumulator_us;
    uint8_t held_buttons;
    uint8_t pulse_frames[8];
    bool clock_started;
    bool failed;
} game_runtime_state_t;

static game_runtime_state_t s_runtime;

static const game_core_t *const CORES[] = {
    &GAME_CORE_PEANUT_GB,
};

static void copy_error(char *target, size_t target_size, const char *message)
{
    if (!target || target_size == 0) return;
    (void)snprintf(target, target_size, "%s",
                   message && message[0] ? message : "Game error");
}

static const game_core_t *core_for_path(const char *path)
{
    for (size_t i = 0; i < sizeof(CORES) / sizeof(CORES[0]); i++) {
        if (CORES[i]->accepts_path(path)) return CORES[i];
    }
    return NULL;
}

static bool open_and_size(const char *path, FILE **out, size_t *size,
                          char *error, size_t error_size)
{
    FILE *file = storage_open(path, "rb");
    if (!file) {
        copy_error(error, error_size, "ROM file could not be opened");
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        copy_error(error, error_size, "ROM size could not be read");
        return false;
    }
    const long end = ftell(file);
    if (end <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        copy_error(error, error_size, "ROM file is empty or unreadable");
        return false;
    }
    *out = file;
    *size = (size_t)end;
    return true;
}

bool game_runtime_probe(const char *path, game_rom_info_t *out,
                        char *error, size_t error_size)
{
    if (out) memset(out, 0, sizeof(*out));
    const game_core_t *core = core_for_path(path);
    if (!core) {
        copy_error(error, error_size, "No installed core supports this file");
        return false;
    }

    FILE *file = NULL;
    size_t file_size = 0;
    if (!open_and_size(path, &file, &file_size, error, error_size)) {
        return false;
    }
    const bool valid = core->probe(
        file, file_size, out, error, error_size);
    fclose(file);
    return valid;
}

static bool make_save_path(const char *rom_path, char *out, size_t out_size)
{
    const int copied = snprintf(out, out_size, "%s", rom_path);
    if (copied < 0 || copied >= (int)out_size) return false;
    char *slash = strrchr(out, '/');
    char *dot = strrchr(out, '.');
    if (!dot || (slash && dot < slash)) return false;
    const size_t prefix = (size_t)(dot - out);
    return snprintf(out + prefix, out_size - prefix, ".sav") == 4;
}

static void load_save(void)
{
    uint8_t *data = s_runtime.core->save_data(s_runtime.instance);
    const size_t size = s_runtime.core->save_size(s_runtime.instance);
    if (!data || size == 0 || !s_runtime.save_path[0]) return;

    FILE *file = storage_open(s_runtime.save_path, "rb");
    if (!file) return;
    const size_t read = fread(data, 1, size, file);
    const int extra = fgetc(file);
    fclose(file);
    if (read != size || extra != EOF) memset(data, 0, size);
}

static void save_game(void)
{
    uint8_t *data = s_runtime.core->save_data(s_runtime.instance);
    const size_t size = s_runtime.core->save_size(s_runtime.instance);
    if (!data || size == 0 || !s_runtime.save_path[0]) return;

    FILE *file = storage_open(s_runtime.save_path, "wb");
    if (!file) return;
    (void)fwrite(data, 1, size, file);
    (void)fclose(file);
}

bool game_runtime_start(const char *path, const game_audio_sink_t *audio,
                        char *error, size_t error_size)
{
    game_runtime_stop();

    game_rom_info_t info;
    if (!game_runtime_probe(path, &info, error, error_size)) return false;
    const game_core_t *core = core_for_path(path);

    FILE *file = NULL;
    size_t file_size = 0;
    if (!open_and_size(path, &file, &file_size, error, error_size)) {
        return false;
    }
    uint8_t *rom = heap_caps_malloc(
        file_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rom) {
        fclose(file);
        copy_error(error, error_size, "Not enough memory for this ROM");
        return false;
    }
    const bool loaded = fread(rom, 1, file_size, file) == file_size;
    fclose(file);
    if (!loaded) {
        heap_caps_free(rom);
        copy_error(error, error_size, "ROM read did not complete");
        return false;
    }

    game_core_instance_t *instance = core->create(
        rom, file_size, audio, error, error_size);
    if (!instance) {
        heap_caps_free(rom);
        return false;
    }

    memset(&s_runtime, 0, sizeof(s_runtime));
    s_runtime.core = core;
    s_runtime.instance = instance;
    s_runtime.rom = rom;
    s_runtime.rom_size = file_size;
    s_runtime.info = info;
    (void)snprintf(s_runtime.path, sizeof(s_runtime.path), "%s", path);
    (void)make_save_path(path, s_runtime.save_path,
                         sizeof(s_runtime.save_path));
    s_runtime.accumulator_us = GAME_FRAME_US;
    load_save();
    return true;
}

void game_runtime_stop(void)
{
    if (s_runtime.core && s_runtime.instance) {
        save_game();
        s_runtime.core->destroy(s_runtime.instance);
    }
    heap_caps_free(s_runtime.rom);
    memset(&s_runtime, 0, sizeof(s_runtime));
}

bool game_runtime_running(void)
{
    return s_runtime.instance != NULL && !s_runtime.failed;
}

void game_runtime_set_buttons(uint8_t pressed_mask)
{
    s_runtime.held_buttons = pressed_mask;
}

void game_runtime_press(game_button_t button)
{
    const uint8_t value = (uint8_t)button;
    for (int bit = 0; bit < 8; bit++) {
        if (value & (1u << bit)) s_runtime.pulse_frames[bit] = GAME_PULSE_FRAMES;
    }
}

static uint8_t current_buttons(void)
{
    uint8_t buttons = s_runtime.held_buttons;
    for (int bit = 0; bit < 8; bit++) {
        if (s_runtime.pulse_frames[bit] > 0) buttons |= (uint8_t)(1u << bit);
    }
    return buttons;
}

static void age_pulses(void)
{
    for (int bit = 0; bit < 8; bit++) {
        if (s_runtime.pulse_frames[bit] > 0) s_runtime.pulse_frames[bit]--;
    }
}

bool game_runtime_advance(uint32_t now_ms)
{
    if (!s_runtime.instance || s_runtime.failed) return false;
    if (!s_runtime.clock_started) {
        s_runtime.clock_started = true;
        s_runtime.last_ms = now_ms;
    } else {
        uint32_t elapsed_ms = now_ms - s_runtime.last_ms;
        s_runtime.last_ms = now_ms;
        if (elapsed_ms > 100u) elapsed_ms = 100u;
        s_runtime.accumulator_us += elapsed_ms * 1000u;
    }

    int frames = 0;
    while (s_runtime.accumulator_us >= GAME_FRAME_US &&
           frames < GAME_MAX_CATCHUP_FRAMES) {
        s_runtime.core->set_buttons(
            s_runtime.instance, current_buttons());
        if (!s_runtime.core->run_frame(s_runtime.instance)) {
            s_runtime.failed = true;
            copy_error(s_runtime.error, sizeof(s_runtime.error),
                       s_runtime.core->error(s_runtime.instance));
            return false;
        }
        age_pulses();
        s_runtime.accumulator_us -= GAME_FRAME_US;
        frames++;
    }
    if (frames == GAME_MAX_CATCHUP_FRAMES &&
        s_runtime.accumulator_us > GAME_FRAME_US * 2u) {
        s_runtime.accumulator_us = GAME_FRAME_US;
    }
    return true;
}

const uint8_t *game_runtime_frame(void)
{
    return s_runtime.core && s_runtime.instance
        ? s_runtime.core->frame(s_runtime.instance) : NULL;
}

uint32_t game_runtime_frame_sequence(void)
{
    return s_runtime.core && s_runtime.instance
        ? s_runtime.core->frame_sequence(s_runtime.instance) : 0;
}

const game_rom_info_t *game_runtime_info(void)
{
    return s_runtime.instance ? &s_runtime.info : NULL;
}

const char *game_runtime_error(void)
{
    return s_runtime.error[0] ? s_runtime.error : "Game runtime stopped";
}
