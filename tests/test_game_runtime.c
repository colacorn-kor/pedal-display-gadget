#include <stdio.h>
#include <string.h>

#include "game_runtime.h"
#include "storage.h"

#define TEST_ROM_SIZE 32768u

static uint8_t s_rom[TEST_ROM_SIZE];
static bool s_bad_checksum;

static FILE *file_with_data(const uint8_t *data, size_t size)
{
    FILE *file = tmpfile();
    if (!file) return NULL;
    if (fwrite(data, 1, size, file) != size) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    return file;
}

FILE *storage_open(const char *relative_path, const char *mode)
{
    if (!relative_path || !mode || strcmp(mode, "rb") != 0) return NULL;
    if (strcmp(relative_path, "GG/games/GG Test.gb") != 0 &&
        strcmp(relative_path, "GG/games/GG Test.gbc") != 0) {
        return NULL;
    }
    if (!s_bad_checksum) return file_with_data(s_rom, sizeof(s_rom));
    uint8_t broken[TEST_ROM_SIZE];
    memcpy(broken, s_rom, sizeof(broken));
    broken[0x14d] ^= 0xffu;
    return file_with_data(broken, sizeof(broken));
}

static void build_test_rom(void)
{
    memset(s_rom, 0, sizeof(s_rom));
    s_rom[0x100] = 0xc3;
    s_rom[0x101] = 0x50;
    s_rom[0x102] = 0x01;
    memcpy(&s_rom[0x134], "GG TEST", 7);
    s_rom[0x147] = 0x00;
    s_rom[0x148] = 0x00;
    s_rom[0x149] = 0x00;
    s_rom[0x150] = 0xc3;
    s_rom[0x151] = 0x50;
    s_rom[0x152] = 0x01;

    uint8_t checksum = 0;
    for (size_t i = 0x134; i < 0x14d; i++) {
        checksum = (uint8_t)(checksum - s_rom[i] - 1u);
    }
    s_rom[0x14d] = checksum;
}

static int fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    game_runtime_stop();
    return 1;
}

int main(void)
{
    build_test_rom();
    game_rom_info_t info;
    char error[96];

    if (!game_runtime_probe("GG/games/GG Test.gb", &info,
                            error, sizeof(error)) ||
        strcmp(info.title, "GG TEST") != 0 ||
        strcmp(info.system, "GAME BOY") != 0 ||
        info.rom_bytes != TEST_ROM_SIZE || info.audio_supported) {
        return fail("valid DMG ROM probe");
    }
    if (game_runtime_probe("GG/games/GG Test.gbc", &info,
                           error, sizeof(error))) {
        return fail("unsupported extension rejection");
    }
    s_bad_checksum = true;
    if (game_runtime_probe("GG/games/GG Test.gb", &info,
                           error, sizeof(error))) {
        return fail("invalid checksum rejection");
    }
    s_bad_checksum = false;

    if (!game_runtime_start("GG/games/GG Test.gb", NULL,
                            error, sizeof(error))) {
        fprintf(stderr, "runtime start error: %s\n", error);
        return fail("runtime start");
    }
    game_runtime_press(GAME_BUTTON_START);
    for (uint32_t now = 0; now <= 150; now += 17) {
        if (!game_runtime_advance(now)) return fail("runtime frame advance");
    }
    if (!game_runtime_running() || !game_runtime_frame() ||
        game_runtime_frame_sequence() == 0) {
        return fail("runtime frame publication");
    }
    game_runtime_stop();
    if (game_runtime_running()) return fail("runtime stop");

    puts("game runtime tests passed");
    return 0;
}
