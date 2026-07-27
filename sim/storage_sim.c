#include "storage_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define SIM_STORAGE_PATH_MAX 1024

#ifndef PEDAL_SIM_SD_ROOT
#define PEDAL_SIM_SD_ROOT "sim/sdcard"
#endif

static bool s_ready;
static bool s_root_configured;
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

static bool configure_root(void)
{
    if (s_root_configured) return true;

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
