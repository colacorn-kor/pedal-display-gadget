#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "storage.h"
#include "storage_backend.h"

static bool s_mounted;
static bool s_mount_allowed = true;
static bool s_list_failed;
static int s_open_calls;
static char s_too_long_image[STORAGE_PATH_MAX + 40];

static const char *IMAGE_FILES[] = {
    "Zebra.JPG", "alpha.png", "notes.txt", ".hidden.gif", "Anim.GIF",
    "alpha.jpg", "Image10.bmp", "Image2.bmp", s_too_long_image,
};
static const char *const MUSIC_FILES[] = {
    "song.WAV", "cover.jpg", "track.flac",
};
static const char *const GAME_FILES[] = {
    "mario.NES", "zelda.gb", "new.gba", "dual.nds",
};

bool storage_backend_mount(void)
{
    s_mounted = s_mount_allowed;
    return s_mounted;
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
    if (s_list_failed) return -1;
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
    memset(s_too_long_image, 'x', sizeof(s_too_long_image));
    memcpy(s_too_long_image + sizeof(s_too_long_image) - 5, ".png", 5);

    if (!storage_init() || !storage_ready()) return fail("mount");
    if (strcmp(storage_status(), "ready") != 0) return fail("status");

    storage_scan_result_t scan = storage_scan_ex(
        STORAGE_MEDIA_IMAGE, items, STORAGE_MAX_ITEMS);
    int count = scan.count;
    if (scan.status != STORAGE_SCAN_OK || count != 6 ||
        scan.accepted_count != 7 || scan.skipped_too_long != 1 ||
        scan.truncated) {
        return fail("image scan details");
    }
    if (strcmp(items[0].path, "GG/images/alpha.jpg") != 0 ||
        strcmp(items[1].path, "GG/images/alpha.png") != 0 ||
        strcmp(items[2].name, "Anim") != 0 ||
        strcmp(items[3].name, "Image2") != 0 ||
        strcmp(items[4].name, "Image10") != 0 ||
        strcmp(items[5].name, "Zebra") != 0) {
        return fail("case-insensitive natural image sort");
    }
    if (strcmp(items[1].path, "GG/images/alpha.png") != 0) {
        return fail("relative image path");
    }

    scan = storage_scan_ex(STORAGE_MEDIA_IMAGE, items, 3);
    if (scan.status != STORAGE_SCAN_OK || scan.count != 3 ||
        scan.accepted_count != 7 || scan.skipped_too_long != 1 ||
        !scan.truncated ||
        strcmp(items[0].path, "GG/images/alpha.jpg") != 0 ||
        strcmp(items[1].path, "GG/images/alpha.png") != 0 ||
        strcmp(items[2].path, "GG/images/Anim.GIF") != 0) {
        return fail("catalog truncation");
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

    s_list_failed = true;
    scan = storage_scan_ex(STORAGE_MEDIA_IMAGE, items, STORAGE_MAX_ITEMS);
    if (scan.status != STORAGE_SCAN_IO_ERROR || scan.count != 0) {
        return fail("list error status");
    }
    s_list_failed = false;
    s_mounted = false;
    s_mount_allowed = false;
    scan = storage_scan_ex(STORAGE_MEDIA_IMAGE, items, STORAGE_MAX_ITEMS);
    if (scan.status != STORAGE_SCAN_UNAVAILABLE || scan.count != 0) {
        return fail("mount error status");
    }

    printf("PASS: SD catalogs, natural sorting, limits, and path safety\n");
    return 0;
}
