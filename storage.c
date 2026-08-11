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
    int accepted_count;
    int skipped_too_long;
} scan_context_t;

static int compare_items(const void *left, const void *right);

static bool collect_file(const char *filename, void *opaque)
{
    scan_context_t *ctx = (scan_context_t *)opaque;
    if (!storage_media_accepts_filename(ctx->kind, filename)) return true;
    ctx->accepted_count++;

    storage_item_t candidate = {0};
    const int path_length = snprintf(
        candidate.path, sizeof(candidate.path), "%s/%s",
        ctx->spec->directory, filename);
    if (path_length < 0 || path_length >= (int)sizeof(candidate.path)) {
        ctx->skipped_too_long++;
        return true;
    }

    const char *extension = strrchr(filename, '.');
    size_t name_length = extension ? (size_t)(extension - filename)
                                   : strlen(filename);
    candidate.name_truncated = name_length >= sizeof(candidate.name);
    if (name_length >= sizeof(candidate.name)) {
        name_length = sizeof(candidate.name) - 1;
    }
    memcpy(candidate.name, filename, name_length);
    candidate.name[name_length] = '\0';

    int insert_at = ctx->count;
    while (insert_at > 0 &&
           compare_items(&candidate, &ctx->items[insert_at - 1]) < 0) {
        insert_at--;
    }
    if (ctx->count < ctx->capacity) {
        memmove(&ctx->items[insert_at + 1], &ctx->items[insert_at],
                (size_t)(ctx->count - insert_at) * sizeof(ctx->items[0]));
        ctx->items[insert_at] = candidate;
        ctx->count++;
    } else if (insert_at < ctx->capacity) {
        memmove(&ctx->items[insert_at + 1], &ctx->items[insert_at],
                (size_t)(ctx->capacity - insert_at - 1) *
                    sizeof(ctx->items[0]));
        ctx->items[insert_at] = candidate;
    }
    return true;
}

static int natural_casecmp(const char *a, const char *b)
{
    while (*a && *b) {
        if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
            const char *a_run = a;
            const char *b_run = b;
            while (*a_run == '0') a_run++;
            while (*b_run == '0') b_run++;

            const char *a_end = a_run;
            const char *b_end = b_run;
            while (isdigit((unsigned char)*a_end)) a_end++;
            while (isdigit((unsigned char)*b_end)) b_end++;
            const size_t a_digits = (size_t)(a_end - a_run);
            const size_t b_digits = (size_t)(b_end - b_run);
            if (a_digits != b_digits) return a_digits < b_digits ? -1 : 1;
            if (a_digits > 0) {
                const int number_order = memcmp(a_run, b_run, a_digits);
                if (number_order != 0) return number_order;
            }

            const char *a_full_end = a;
            const char *b_full_end = b;
            while (isdigit((unsigned char)*a_full_end)) a_full_end++;
            while (isdigit((unsigned char)*b_full_end)) b_full_end++;
            const size_t a_full = (size_t)(a_full_end - a);
            const size_t b_full = (size_t)(b_full_end - b);
            if (a_full != b_full) return a_full < b_full ? -1 : 1;
            a = a_full_end;
            b = b_full_end;
            continue;
        }

        const int ca = tolower((unsigned char)*a);
        const int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int compare_items(const void *left, const void *right)
{
    const storage_item_t *a = (const storage_item_t *)left;
    const storage_item_t *b = (const storage_item_t *)right;
    int order = natural_casecmp(a->name, b->name);
    if (order == 0) order = natural_casecmp(a->path, b->path);
    if (order == 0) order = strcmp(a->path, b->path);
    return order;
}

storage_scan_result_t storage_scan_ex(storage_media_kind_t kind,
                                      storage_item_t *items, int capacity)
{
    storage_scan_result_t result = {
        .status = STORAGE_SCAN_IO_ERROR,
    };
    if (!valid_kind(kind) || !items || capacity <= 0) return result;
    if (capacity > STORAGE_MAX_ITEMS) capacity = STORAGE_MAX_ITEMS;
    if (!storage_backend_ready() && !storage_backend_mount()) {
        result.status = STORAGE_SCAN_UNAVAILABLE;
        return result;
    }

    scan_context_t ctx = {
        .spec = &MEDIA_SPECS[(int)kind],
        .kind = kind,
        .items = items,
        .capacity = capacity,
        .count = 0,
    };
    if (storage_backend_list(ctx.spec->directory, collect_file, &ctx) < 0) {
        result.status = STORAGE_SCAN_IO_ERROR;
        return result;
    }

    result.status = STORAGE_SCAN_OK;
    result.count = ctx.count;
    result.accepted_count = ctx.accepted_count;
    result.skipped_too_long = ctx.skipped_too_long;
    result.truncated = ctx.accepted_count - ctx.skipped_too_long > ctx.count;
    return result;
}

int storage_scan(storage_media_kind_t kind, storage_item_t *items, int capacity)
{
    return storage_scan_ex(kind, items, capacity).count;
}
