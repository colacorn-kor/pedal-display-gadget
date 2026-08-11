#include "storage_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform_sim.h"
#include "storage_sim_test.h"

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define SIM_STORAGE_PATH_MAX 1024

#ifndef PEDAL_SIM_SD_ROOT
#define PEDAL_SIM_SD_ROOT "sim/sdcard"
#endif

static bool s_ready;
static bool s_root_configured;
static bool s_smoke_root;
static char s_root_utf8[SIM_STORAGE_PATH_MAX];
static char s_status[SIM_STORAGE_PATH_MAX + 32] = "PC SD folder unavailable";

#ifdef _WIN32
static wchar_t s_root_wide[SIM_STORAGE_PATH_MAX];

static bool utf8_to_wide(const char *text, wchar_t *out, int capacity)
{
    if (!text || !out || capacity <= 0) return false;
    return MultiByteToWideChar(
               CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, out, capacity) > 0;
}

static bool wide_to_utf8(const wchar_t *text, char *out, int capacity)
{
    if (!text || !out || capacity <= 0) return false;
    return WideCharToMultiByte(
               CP_UTF8, 0, text, -1, out, capacity, NULL, NULL) > 0;
}

static bool make_wide_path(const char *relative, wchar_t *out, int capacity)
{
    wchar_t relative_wide[SIM_STORAGE_PATH_MAX];
    if (!utf8_to_wide(relative, relative_wide, SIM_STORAGE_PATH_MAX)) {
        return false;
    }
    return swprintf_s(
               out, (size_t)capacity, L"%ls/%ls",
               s_root_wide, relative_wide) >= 0;
}
#else
static bool make_path(const char *relative, char *out, size_t capacity)
{
    const int length = snprintf(
        out, capacity, "%s/%s", s_root_utf8, relative);
    return length >= 0 && length < (int)capacity;
}
#endif

static const char *const SMOKE_GALLERY_FILES[] = {
    "GG/images/02 Second.bmp",
    "GG/images/10 Tenth.bmp",
    "GG/images/broken.png",
    "GG/images/This is an intentionally very long Gallery filename used to verify that the footer keeps its layout stable and ends with an ellipsis.bmp",
};
static const char *const SMOKE_GAME_FILE = "GG/games/GG Test.gb";

static const unsigned char SMOKE_BMP[] = {
    'B', 'M', 58, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,
    40, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 24, 0,
    0, 0, 0, 0, 4, 0, 0, 0, 0x13, 0x0b, 0, 0,
    0x13, 0x0b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x40, 0x90, 0xe0, 0,
};

static const char *smoke_filename(const char *path)
{
    const char *slash = path ? strrchr(path, '/') : NULL;
    return slash ? slash + 1 : (path ? path : "");
}

static void smoke_cleanup(void)
{
    if (!s_smoke_root) return;
#ifdef _WIN32
    for (size_t i = 0;
         i < sizeof(SMOKE_GALLERY_FILES) / sizeof(SMOKE_GALLERY_FILES[0]);
         i++) {
        wchar_t path[SIM_STORAGE_PATH_MAX];
        if (make_wide_path(SMOKE_GALLERY_FILES[i], path,
                           SIM_STORAGE_PATH_MAX)) {
            (void)_wremove(path);
        }
    }
    wchar_t game_path[SIM_STORAGE_PATH_MAX];
    if (make_wide_path(SMOKE_GAME_FILE, game_path, SIM_STORAGE_PATH_MAX)) {
        (void)_wremove(game_path);
    }
    wchar_t path[SIM_STORAGE_PATH_MAX];
    if (make_wide_path("GG/images", path, SIM_STORAGE_PATH_MAX)) {
        (void)RemoveDirectoryW(path);
    }
    if (make_wide_path("GG/music", path, SIM_STORAGE_PATH_MAX)) {
        (void)RemoveDirectoryW(path);
    }
    if (make_wide_path("GG/games", path, SIM_STORAGE_PATH_MAX)) {
        (void)RemoveDirectoryW(path);
    }
    if (make_wide_path("GG", path, SIM_STORAGE_PATH_MAX)) {
        (void)RemoveDirectoryW(path);
    }
    (void)RemoveDirectoryW(s_root_wide);
#else
    for (size_t i = 0;
         i < sizeof(SMOKE_GALLERY_FILES) / sizeof(SMOKE_GALLERY_FILES[0]);
         i++) {
        char path[SIM_STORAGE_PATH_MAX];
        if (make_path(SMOKE_GALLERY_FILES[i], path, sizeof(path))) {
            (void)remove(path);
        }
    }
    char game_path[SIM_STORAGE_PATH_MAX];
    if (make_path(SMOKE_GAME_FILE, game_path, sizeof(game_path))) {
        (void)remove(game_path);
    }
    char path[SIM_STORAGE_PATH_MAX];
    if (make_path("GG/images", path, sizeof(path))) (void)rmdir(path);
    if (make_path("GG/music", path, sizeof(path))) (void)rmdir(path);
    if (make_path("GG/games", path, sizeof(path))) (void)rmdir(path);
    if (make_path("GG", path, sizeof(path))) (void)rmdir(path);
    (void)rmdir(s_root_utf8);
#endif
}

static bool create_smoke_directory(const char *relative)
{
#ifdef _WIN32
    wchar_t path[SIM_STORAGE_PATH_MAX];
    if (!make_wide_path(relative, path, SIM_STORAGE_PATH_MAX)) return false;
    return CreateDirectoryW(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    char path[SIM_STORAGE_PATH_MAX];
    if (!make_path(relative, path, sizeof(path))) return false;
    return mkdir(path, 0700) == 0 || errno == EEXIST;
#endif
}

static bool configure_smoke_root(void)
{
#ifdef _WIN32
    wchar_t temp[MAX_PATH];
    const DWORD temp_length = GetTempPathW(MAX_PATH, temp);
    if (temp_length == 0 || temp_length >= MAX_PATH ||
        swprintf_s(s_root_wide, SIM_STORAGE_PATH_MAX,
                   L"%lsGG-pedal-sim-%lu-%llu", temp,
                   (unsigned long)GetCurrentProcessId(),
                   (unsigned long long)GetTickCount64()) < 0 ||
        !CreateDirectoryW(s_root_wide, NULL)) {
        return false;
    }
    if (!wide_to_utf8(s_root_wide, s_root_utf8,
                      (int)sizeof(s_root_utf8))) {
        return false;
    }
#else
    const int length = snprintf(s_root_utf8, sizeof(s_root_utf8),
                                "/tmp/GG-pedal-sim-%ld", (long)getpid());
    if (length < 0 || length >= (int)sizeof(s_root_utf8) ||
        mkdir(s_root_utf8, 0700) != 0) {
        return false;
    }
#endif
    s_smoke_root = true;
    if (!create_smoke_directory("GG") ||
        !create_smoke_directory("GG/images") ||
        !create_smoke_directory("GG/music") ||
        !create_smoke_directory("GG/games")) {
        smoke_cleanup();
        return false;
    }
    (void)atexit(smoke_cleanup);
    return true;
}

static bool configure_root(void)
{
    if (s_root_configured) return true;

    if (plat_sim_is_smoke_test()) {
        if (!configure_smoke_root()) return false;
        s_root_configured = true;
        return true;
    }

    const char *configured = getenv("GG_SD_ROOT");
    if (!configured || !*configured) configured = PEDAL_SIM_SD_ROOT;
    const int length = snprintf(
        s_root_utf8, sizeof(s_root_utf8), "%s", configured);
    if (length < 0 || length >= (int)sizeof(s_root_utf8)) return false;

#ifdef _WIN32
    if (!utf8_to_wide(s_root_utf8, s_root_wide, SIM_STORAGE_PATH_MAX)) {
        return false;
    }
#endif
    s_root_configured = true;
    return true;
}

static bool write_smoke_file(const char *relative,
                             const unsigned char *data, size_t size)
{
#ifdef _WIN32
    wchar_t path[SIM_STORAGE_PATH_MAX];
    if (!make_wide_path(relative, path, SIM_STORAGE_PATH_MAX)) return false;
    FILE *file = _wfopen(path, L"wb");
#else
    char path[SIM_STORAGE_PATH_MAX];
    if (!make_path(relative, path, sizeof(path))) return false;
    FILE *file = fopen(path, "wb");
#endif
    if (!file) return false;
    const bool written = fwrite(data, 1, size, file) == size;
    const bool closed = fclose(file) == 0;
    return written && closed;
}

bool sim_storage_populate_smoke_gallery(void)
{
    if (!s_root_configured && !configure_root()) return false;
    if (!s_smoke_root) return false;
    static const unsigned char BROKEN_PNG[] = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 1, 2, 3, 4,
    };
    return write_smoke_file(SMOKE_GALLERY_FILES[0],
                            SMOKE_BMP, sizeof(SMOKE_BMP)) &&
           write_smoke_file(SMOKE_GALLERY_FILES[1],
                            SMOKE_BMP, sizeof(SMOKE_BMP)) &&
           write_smoke_file(SMOKE_GALLERY_FILES[2],
                            BROKEN_PNG, sizeof(BROKEN_PNG)) &&
           write_smoke_file(SMOKE_GALLERY_FILES[3],
                            SMOKE_BMP, sizeof(SMOKE_BMP));
}

bool sim_storage_populate_smoke_game(void)
{
    if (!s_root_configured && !configure_root()) return false;
    if (!s_smoke_root) return false;

    static unsigned char rom[32768];
    memset(rom, 0, sizeof(rom));
    rom[0x100] = 0xc3;
    rom[0x101] = 0x50;
    rom[0x102] = 0x01;
    memcpy(&rom[0x134], "GG TEST", 7);
    rom[0x147] = 0x00;
    rom[0x148] = 0x00;
    rom[0x149] = 0x00;
    rom[0x150] = 0xc3;
    rom[0x151] = 0x50;
    rom[0x152] = 0x01;
    unsigned char checksum = 0;
    for (size_t i = 0x134; i < 0x14d; i++) {
        checksum = (unsigned char)(checksum - rom[i] - 1u);
    }
    rom[0x14d] = checksum;
    return write_smoke_file(SMOKE_GAME_FILE, rom, sizeof(rom));
}

const char *sim_storage_smoke_long_filename(void)
{
    return smoke_filename(SMOKE_GALLERY_FILES[3]);
}

bool storage_backend_mount(void)
{
    if (!configure_root()) return false;

#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(s_root_wide);
    s_ready = attributes != INVALID_FILE_ATTRIBUTES &&
              (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    s_ready = stat(s_root_utf8, &info) == 0 && S_ISDIR(info.st_mode);
#endif

    if (s_ready) {
        (void)snprintf(
            s_status, sizeof(s_status), "PC SD: %s", s_root_utf8);
    } else {
        (void)snprintf(
            s_status, sizeof(s_status), "PC SD folder unavailable");
    }
    return s_ready;
}

bool storage_backend_ready(void)
{
    return s_ready;
}

const char *storage_backend_status(void)
{
    return s_status;
}

int storage_backend_list(const char *relative_dir,
                         storage_backend_visitor_t visitor,
                         void *ctx)
{
    if (!s_ready || !relative_dir || !visitor) return -1;

#ifdef _WIN32
    wchar_t directory[SIM_STORAGE_PATH_MAX];
    wchar_t pattern[SIM_STORAGE_PATH_MAX];
    if (!make_wide_path(relative_dir, directory, SIM_STORAGE_PATH_MAX) ||
        swprintf_s(
            pattern, SIM_STORAGE_PATH_MAX, L"%ls/*", directory) < 0) {
        return -1;
    }

    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) return -1;

    int count = 0;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
        char filename[SIM_STORAGE_PATH_MAX];
        if (!wide_to_utf8(
                data.cFileName, filename, (int)sizeof(filename))) {
            continue;
        }
        count++;
        if (!visitor(filename, ctx)) break;
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return count;
#else
    char directory[SIM_STORAGE_PATH_MAX];
    if (!make_path(relative_dir, directory, sizeof(directory))) return -1;

    DIR *dir = opendir(directory);
    if (!dir) return -1;

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char path[SIM_STORAGE_PATH_MAX];
        const int length = snprintf(
            path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (length < 0 || length >= (int)sizeof(path)) continue;
        struct stat info;
        if (stat(path, &info) != 0 || S_ISDIR(info.st_mode)) continue;
        count++;
        if (!visitor(entry->d_name, ctx)) break;
    }
    closedir(dir);
    return count;
#endif
}

FILE *storage_backend_open(const char *relative_path, const char *mode)
{
    if (!s_ready || !relative_path || !mode) return NULL;

#ifdef _WIN32
    wchar_t full[SIM_STORAGE_PATH_MAX];
    wchar_t wide_mode[16];
    if (!make_wide_path(relative_path, full, SIM_STORAGE_PATH_MAX) ||
        !utf8_to_wide(mode, wide_mode, (int)(sizeof(wide_mode) /
                                             sizeof(wide_mode[0])))) {
        return NULL;
    }
    return _wfopen(full, wide_mode);
#else
    char full[SIM_STORAGE_PATH_MAX];
    if (!make_path(relative_path, full, sizeof(full))) return NULL;
    return fopen(full, mode);
#endif
}
