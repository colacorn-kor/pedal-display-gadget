#include "game_core.h"

#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"

#define ENABLE_SOUND 0
#define PEANUT_GB_12_COLOUR 0
#define PEANUT_GB_HIGH_LCD_ACCURACY 0
#include "third_party/peanut_gb/peanut_gb.h"

#define GB_HEADER_SIZE 0x150u
#define GB_HEADER_TITLE 0x134u
#define GB_HEADER_CGB_FLAG 0x143u
#define GB_HEADER_CART_TYPE 0x147u
#define GB_HEADER_ROM_SIZE 0x148u
#define GB_HEADER_RAM_SIZE 0x149u
#define GB_HEADER_CHECKSUM 0x14du

struct game_core_instance {
    struct gb_s gb;
    const uint8_t *rom;
    size_t rom_size;
    uint8_t *ram;
    size_t ram_size;
    uint8_t *frame;
    uint32_t frame_sequence;
    jmp_buf recovery;
    bool recovery_ready;
    bool failed;
    char error_text[64];
};

static void set_error(char *error, size_t error_size, const char *message)
{
    if (!error || error_size == 0) return;
    (void)snprintf(error, error_size, "%s", message ? message : "Game error");
}

static int ascii_tolower(int value)
{
    return tolower((unsigned char)value);
}

static bool ends_with_gb(const char *path)
{
    if (!path) return false;
    const size_t length = strlen(path);
    return length >= 3 && path[length - 3] == '.' &&
           ascii_tolower(path[length - 2]) == 'g' &&
           ascii_tolower(path[length - 1]) == 'b';
}

static bool cartridge_supported(uint8_t type)
{
    static const bool supported[32] = {
        true,  true,  true,  true,  false, true,  true,  false,
        true,  true,  false, true,  true,  true,  false, true,
        true,  true,  true,  true,  false, false, false, false,
        false, true,  true,  true,  true,  true,  true,  false,
    };
    return type < sizeof(supported) / sizeof(supported[0]) &&
           supported[type];
}

static bool peanut_probe(FILE *file, size_t file_size, game_rom_info_t *out,
                         char *error, size_t error_size)
{
    uint8_t header[GB_HEADER_SIZE];
    if (!file || fseek(file, 0, SEEK_SET) != 0 ||
        fread(header, 1, sizeof(header), file) != sizeof(header)) {
        set_error(error, error_size, "ROM header read failed");
        return false;
    }

    const uint8_t size_code = header[GB_HEADER_ROM_SIZE];
    if (size_code > 8u) {
        set_error(error, error_size, "Unsupported Game Boy ROM size");
        return false;
    }
    const size_t declared_size = (size_t)32768u << size_code;
    if (file_size != declared_size) {
        set_error(error, error_size, "Game Boy ROM size does not match header");
        return false;
    }
    if (!cartridge_supported(header[GB_HEADER_CART_TYPE])) {
        set_error(error, error_size, "Unsupported Game Boy cartridge type");
        return false;
    }
    if (header[GB_HEADER_RAM_SIZE] > 5u) {
        set_error(error, error_size, "Unsupported Game Boy save RAM size");
        return false;
    }
    if (header[GB_HEADER_CGB_FLAG] == 0xc0u) {
        set_error(error, error_size, "Game Boy Color-only ROM is not supported");
        return false;
    }

    uint8_t checksum = 0;
    for (size_t i = GB_HEADER_TITLE; i < GB_HEADER_CHECKSUM; i++) {
        checksum = (uint8_t)(checksum - header[i] - 1u);
    }
    if (checksum != header[GB_HEADER_CHECKSUM]) {
        set_error(error, error_size, "Invalid Game Boy header checksum");
        return false;
    }

    if (out) {
        memset(out, 0, sizeof(*out));
        size_t title_length = 0;
        for (size_t i = GB_HEADER_TITLE;
             i <= GB_HEADER_CGB_FLAG && title_length < sizeof(out->title) - 1;
             i++) {
            const uint8_t ch = header[i];
            if (ch == 0 || ch < 0x20u || ch > 0x7eu) break;
            out->title[title_length++] = (char)ch;
        }
        if (title_length == 0) {
            (void)snprintf(out->title, sizeof(out->title), "Untitled");
        }
        (void)snprintf(out->system, sizeof(out->system), "GAME BOY");
        out->rom_bytes = file_size;
        out->audio_supported = false;
    }
    return true;
}

static uint8_t rom_read(struct gb_s *gb, const uint_fast32_t address)
{
    struct game_core_instance *instance = gb->direct.priv;
    if (instance && address < instance->rom_size) {
        return instance->rom[address];
    }
    if (instance) {
        instance->failed = true;
        (void)snprintf(instance->error_text, sizeof(instance->error_text),
                       "ROM read outside cartridge at %lu",
                       (unsigned long)address);
        if (instance->recovery_ready) longjmp(instance->recovery, 1);
    }
    return 0xffu;
}

static uint8_t ram_read(struct gb_s *gb, const uint_fast32_t address)
{
    struct game_core_instance *instance = gb->direct.priv;
    return instance && instance->ram && address < instance->ram_size
        ? instance->ram[address] : 0xffu;
}

static void ram_write(struct gb_s *gb, const uint_fast32_t address,
                      const uint8_t value)
{
    struct game_core_instance *instance = gb->direct.priv;
    if (instance && instance->ram && address < instance->ram_size) {
        instance->ram[address] = value;
    }
}

static void core_error(struct gb_s *gb, const enum gb_error_e error,
                       const uint16_t address)
{
    struct game_core_instance *instance = gb->direct.priv;
    if (!instance) abort();
    instance->failed = true;
    (void)snprintf(instance->error_text, sizeof(instance->error_text),
                   "Game Boy core error %d at %04X", (int)error, address);
    if (instance->recovery_ready) longjmp(instance->recovery, 1);
    abort();
}

static void draw_line(struct gb_s *gb, const uint8_t *pixels,
                      const uint_fast8_t line)
{
    struct game_core_instance *instance = gb->direct.priv;
    if (!instance || !instance->frame || !pixels || line >= GAME_FRAME_HEIGHT) {
        return;
    }
    uint8_t *target = instance->frame + (size_t)line * GAME_FRAME_WIDTH;
    for (int x = 0; x < GAME_FRAME_WIDTH; x++) target[x] = pixels[x] & 0x03u;
    if (line == GAME_FRAME_HEIGHT - 1) instance->frame_sequence++;
}

static game_core_instance_t *peanut_create(
    const uint8_t *rom, size_t rom_size, const game_audio_sink_t *audio,
    char *error, size_t error_size)
{
    (void)audio;
    struct game_core_instance *instance = calloc(1, sizeof(*instance));
    if (!instance) {
        set_error(error, error_size, "Game Boy core memory unavailable");
        return NULL;
    }
    instance->rom = rom;
    instance->rom_size = rom_size;
    instance->frame = heap_caps_calloc(
        GAME_FRAME_WIDTH * GAME_FRAME_HEIGHT, sizeof(instance->frame[0]),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!instance->frame) {
        set_error(error, error_size, "Game Boy frame memory unavailable");
        free(instance);
        return NULL;
    }

    const enum gb_init_error_e init = gb_init(
        &instance->gb, rom_read, ram_read, ram_write, core_error, instance);
    if (init != GB_INIT_NO_ERROR) {
        set_error(error, error_size,
                  init == GB_INIT_INVALID_CHECKSUM
                      ? "Invalid Game Boy header checksum"
                      : "Unsupported Game Boy cartridge");
        heap_caps_free(instance->frame);
        free(instance);
        return NULL;
    }

    if (gb_get_save_size_s(&instance->gb, &instance->ram_size) != 0) {
        set_error(error, error_size, "Unsupported Game Boy save RAM");
        heap_caps_free(instance->frame);
        free(instance);
        return NULL;
    }
    if (instance->ram_size > 0) {
        instance->ram = heap_caps_calloc(
            instance->ram_size, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!instance->ram) {
            set_error(error, error_size, "Game Boy save memory unavailable");
            heap_caps_free(instance->frame);
            free(instance);
            return NULL;
        }
    }

    gb_init_lcd(&instance->gb, draw_line);
    instance->gb.direct.frame_skip = true;
    instance->gb.direct.joypad = 0xffu;
    return instance;
}

static void peanut_destroy(game_core_instance_t *opaque)
{
    struct game_core_instance *instance = opaque;
    if (!instance) return;
    heap_caps_free(instance->ram);
    heap_caps_free(instance->frame);
    free(instance);
}

static void peanut_set_buttons(game_core_instance_t *opaque,
                               uint8_t pressed_mask)
{
    struct game_core_instance *instance = opaque;
    if (instance) instance->gb.direct.joypad = (uint8_t)~pressed_mask;
}

static bool peanut_run_frame(game_core_instance_t *opaque)
{
    struct game_core_instance *instance = opaque;
    if (!instance || instance->failed) return false;
    if (setjmp(instance->recovery) != 0) {
        instance->recovery_ready = false;
        return false;
    }
    instance->recovery_ready = true;
    gb_run_frame(&instance->gb);
    instance->recovery_ready = false;
    return !instance->failed;
}

static const uint8_t *peanut_frame(const game_core_instance_t *opaque)
{
    const struct game_core_instance *instance = opaque;
    return instance ? instance->frame : NULL;
}

static uint32_t peanut_frame_sequence(const game_core_instance_t *opaque)
{
    const struct game_core_instance *instance = opaque;
    return instance ? instance->frame_sequence : 0;
}

static uint8_t *peanut_save_data(game_core_instance_t *opaque)
{
    struct game_core_instance *instance = opaque;
    return instance ? instance->ram : NULL;
}

static size_t peanut_save_size(const game_core_instance_t *opaque)
{
    const struct game_core_instance *instance = opaque;
    return instance ? instance->ram_size : 0;
}

static const char *peanut_error(const game_core_instance_t *opaque)
{
    const struct game_core_instance *instance = opaque;
    return instance && instance->error_text[0]
        ? instance->error_text : "Game Boy core stopped";
}

const game_core_t GAME_CORE_PEANUT_GB = {
    .id = "peanut-gb",
    .system = "GAME BOY",
    .accepts_path = ends_with_gb,
    .probe = peanut_probe,
    .create = peanut_create,
    .destroy = peanut_destroy,
    .set_buttons = peanut_set_buttons,
    .run_frame = peanut_run_frame,
    .frame = peanut_frame,
    .frame_sequence = peanut_frame_sequence,
    .save_data = peanut_save_data,
    .save_size = peanut_save_size,
    .error = peanut_error,
};
