#include "storage.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "storage_backend.h"

typedef struct {
    const char *directory;
    const char *const *extensions;
    int extension_count;
} media_spec_t;

static const char *const IMAGE_EXTENSIONS[] = {
    "bmp", "gif", "jpeg", "jpg", "png", "bin",
};
static const char *const MUSIC_EXTENSIONS[] = {
    "flac", "mp3", "ogg", "wav",
};
static const char *const GAME_EXTENSIONS[] = {
    "bin", "col", "gb", "gbc", "gen", "gg", "gw", "lnx", "md", "nes",
    "pce", "sfc", "sg", "smc", "sms", "wad", "zip",
};

static const media_spec_t MEDIA_SPECS[] = {
    {
        .directory = "GG/images",
        .extensions = IMAGE_EXTENSIONS,
        .extension_count =
            (int)(sizeof(IMAGE_EXTENSIONS) / sizeof(IMAGE_EXTENSIONS[0])),
    },
    {
        .directory = "GG/music",
        .extensions = MUSIC_EXTENSIONS,
        .extension_count =
            (int)(sizeof(MUSIC_EXTENSIONS) / sizeof(MUSIC_EXTENSIONS[0])),
    },
    {
        .directory = "GG/games",
        .extensions = GAME_EXTENSIONS,
        .extension_count =
            (int)(sizeof(GAME_EXTENSIONS) / sizeof(GAME_EXTENSIONS[0])),
    },
};

static int ascii_casecmp(const char *a, const char *b)
{
    while (*a && *b) {
        const int ca = tolower((unsigned char)*a);
        const int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static bool valid_kind(storage_media_kind_t kind)
{
    return kind >= STORAGE_MEDIA_IMAGE && kind <= STORAGE_MEDIA_GAME;
}

static bool safe_relative_path(const char *path)
{
    if (!path || !*path || *path == '/' || *path == '\\') return false;

    const char *component = path;
    for (const char *cursor = path;; cursor++) {
        if (*cursor == '\\' || *cursor == ':') return false;
        if (*cursor == '/' || *cursor == '\0') {
            const size_t length = (size_t)(cursor - component);
            if (length == 0 ||
                (length == 1 && component[0] == '.') ||
                (length == 2 && component[0] == '.' && component[1] == '.')) {
                return false;
            }
            if (*cursor == '\0') break;
            component = cursor + 1;
        }
    }
    return true;
}

bool storage_media_accepts_filename(storage_media_kind_t kind,
                                    const char *filename)
{
    if (!valid_kind(kind) || !filename || !*filename ||
        filename[0] == '.' || strchr(filename, '/') ||
        strchr(filename, '\\')) {
        return false;
    }

    const char *extension = strrchr(filename, '.');
    if (!extension || extension == filename || extension[1] == '\0') {
        return false;
    }
    extension++;

    const media_spec_t *spec = &MEDIA_SPECS[(int)kind];
    for (int i = 0; i < spec->extension_count; i++) {
        if (ascii_casecmp(extension, spec->extensions[i]) == 0) return true;
    }
    return false;
}

bool storage_init(void)
{
    return storage_backend_mount();
}

bool storage_ready(void)
{
    return storage_backend_ready();
}

const char *storage_status(void)
{
    return storage_backend_status();
}

FILE *storage_open(const char *relative_path, const char *mode)
{
    if (!safe_relative_path(relative_path) || !mode || !*mode) return NULL;
    if (!storage_backend_ready() && !storage_backend_mount()) return NULL;
    return storage_backend_open(relative_path, mode);
}

typedef struct {
    const media_spec_t *spec;
    storage_media_kind_t kind;
    storage_item_t *items;
    int capacity;
    int count;
} scan_context_t;

static bool collect_file(const char *filename, void *opaque)
{
    scan_context_t *ctx = (scan_context_t *)opaque;
    if (!storage_media_accepts_filename(ctx->kind, filename)) return true;
    if (ctx->count >= ctx->capacity) return false;

    storage_item_t *item = &ctx->items[ctx->count];
    const int path_length = snprintf(
        item->path, sizeof(item->path), "%s/%s",
        ctx->spec->directory, filename);
    if (path_length < 0 || path_length >= (int)sizeof(item->path)) return true;

    const char *extension = strrchr(filename, '.');
    size_t name_length = extension ? (size_t)(extension - filename)
                                   : strlen(filename);
    if (name_length >= sizeof(item->name)) {
        name_length = sizeof(item->name) - 1;
    }
    memcpy(item->name, filename, name_length);
    item->name[name_length] = '\0';
    ctx->count++;
    return true;
}

static int compare_items(const void *left, const void *right)
{
    const storage_item_t *a = (const storage_item_t *)left;
    const storage_item_t *b = (const storage_item_t *)right;
    return ascii_casecmp(a->path, b->path);
}

int storage_scan(storage_media_kind_t kind, storage_item_t *items, int capacity)
{
    if (!valid_kind(kind) || !items || capacity <= 0) return 0;
    if (capacity > STORAGE_MAX_ITEMS) capacity = STORAGE_MAX_ITEMS;
    if (!storage_backend_ready() && !storage_backend_mount()) return 0;

    scan_context_t ctx = {
        .spec = &MEDIA_SPECS[(int)kind],
        .kind = kind,
        .items = items,
        .capacity = capacity,
        .count = 0,
    };
    if (storage_backend_list(ctx.spec->directory, collect_file, &ctx) < 0) {
        return 0;
    }

    qsort(items, (size_t)ctx.count, sizeof(items[0]), compare_items);
    return ctx.count;
}
