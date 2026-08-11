#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    IMAGE_PROBE_OK = 0,
    IMAGE_PROBE_OPEN_FAILED,
    IMAGE_PROBE_EMPTY,
    IMAGE_PROBE_UNSUPPORTED,
    IMAGE_PROBE_CORRUPT,
    IMAGE_PROBE_TRUNCATED,
} image_probe_status_t;

typedef struct {
    image_probe_status_t status;
    uint32_t width;
    uint32_t height;
    uint32_t file_size;
    char format[8];
} image_probe_result_t;

bool image_probe_file(const char *relative_path, image_probe_result_t *result);
const char *image_probe_status_text(image_probe_status_t status);
