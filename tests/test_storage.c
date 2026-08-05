#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "storage.h"
#include "storage_backend.h"

static bool s_mounted;
static int s_open_calls;

static const char *const IMAGE_FILES[] = {
    "Zebra.JPG", "alpha.png", "notes.txt", ".hidden.gif", "Anim.GIF",
    "alpha.jpg",
};
static const char *const MUSIC_FILES[] = {
    "song.WAV", "cover.jpg", "track.flac",
};
static const char *const GAME_FILES[] = {
    "mario.NES", "zelda.gb", "new.gba", "dual.nds",
};

bool storage_backend_mount(void)
{
    s_mounted = true;
    return true;
}

bool storage_backend_ready(void)
{
    return s_mounted;
}

const char *storage_backend_status(void)
{
    return s_mounted ? "ready" : "not ready";
}

static const char *const *files_for_directory(
    const char *directory, int *count)
{
    if (strcmp(directory, "GG/images") == 0) {
        *count = (int)(sizeof(IMAGE_FILES) / sizeof(IMAGE_FILES[0]));
        return IMAGE_FILES;
    }
    if (strcmp(directory, "GG/music") == 0) {
        *count = (int)(sizeof(MUSIC_FILES) / sizeof(MUSIC_FILES[0]));
        return MUSIC_FILES;
    }
    if (strcmp(directory, "GG/games") == 0) {
        *count = (int)(sizeof(GAME_FILES) / sizeof(GAME_FILES[0]));
        return GAME_FILES;
    }
    *count = 0;
    return NULL;
}

int storage_backend_list(const char *relative_dir,
                         storage_backend_visitor_t visitor,
                         void *ctx)
{
    int count = 0;
    const char *const *files = files_for_directory(relative_dir, &count);
    for (int i = 0; i < count; i++) {
        if (!visitor(files[i], ctx)) return i + 1;
    }
    return count;
}

FILE *storage_backend_open(const char *relative_path, const char *mode)
{
    (void)relative_path;
    (void)mode;
    s_open_calls++;
    return (FILE *)(uintptr_t)1;
}

static int fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    storage_item_t items[STORAGE_MAX_ITEMS];

    if (!storage_init() || !storage_ready()) return fail("mount");
    if (strcmp(storage_status(), "ready") != 0) return fail("status");

    int count = storage_scan(
        STORAGE_MEDIA_IMAGE, items, STORAGE_MAX_ITEMS);
    if (count != 4) return fail("image filter");
    if (strcmp(items[0].path, "GG/images/alpha.jpg") != 0 ||
        strcmp(items[1].path, "GG/images/alpha.png") != 0 ||
        strcmp(items[2].name, "Anim") != 0 ||
        strcmp(items[3].name, "Zebra") != 0) {
        return fail("case-insensitive image sort");
    }
    if (strcmp(items[1].path, "GG/images/alpha.png") != 0) {
        return fail("relative image path");
    }

    count = storage_scan(STORAGE_MEDIA_MUSIC, items, STORAGE_MAX_ITEMS);
    if (count != 2) return fail("music filter");

    count = storage_scan(STORAGE_MEDIA_GAME, items, STORAGE_MAX_ITEMS);
    if (count != 2) return fail("Game content filter");
    if (storage_media_accepts_filename(STORAGE_MEDIA_GAME, "game.gba") ||
        storage_media_accepts_filename(STORAGE_MEDIA_GAME, "game.nds")) {
        return fail("unsupported game accepted");
    }

    if (storage_open("../secret", "rb") != NULL || s_open_calls != 0) {
        return fail("path traversal");
    }
    if (!storage_open("GG/images/alpha.png", "rb") ||
        s_open_calls != 1) {
        return fail("safe open");
    }

    printf("PASS: SD media catalogs, sorting, and path safety\n");
    return 0;
}
