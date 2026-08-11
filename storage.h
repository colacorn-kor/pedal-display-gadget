#pragma once

#include <stdbool.h>
#include <stdio.h>

#define STORAGE_MAX_ITEMS 64
#define STORAGE_PATH_MAX 384
#define STORAGE_NAME_MAX 256

typedef enum {
    STORAGE_MEDIA_IMAGE = 0,
    STORAGE_MEDIA_MUSIC,
    STORAGE_MEDIA_GAME,
} storage_media_kind_t;

typedef struct {
    char path[STORAGE_PATH_MAX];
    char name[STORAGE_NAME_MAX];
    bool name_truncated;
} storage_item_t;

typedef enum {
    STORAGE_SCAN_OK = 0,
    STORAGE_SCAN_UNAVAILABLE,
    STORAGE_SCAN_IO_ERROR,
} storage_scan_status_t;

typedef struct {
    storage_scan_status_t status;
    int count;
    int accepted_count;
    int skipped_too_long;
    bool truncated;
} storage_scan_result_t;

bool storage_init(void);
bool storage_ready(void);
const char *storage_status(void);
int storage_scan(storage_media_kind_t kind, storage_item_t *items, int capacity);
storage_scan_result_t storage_scan_ex(storage_media_kind_t kind,
                                      storage_item_t *items, int capacity);
bool storage_media_accepts_filename(storage_media_kind_t kind,
                                    const char *filename);
FILE *storage_open(const char *relative_path, const char *mode);
