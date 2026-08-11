#include <stdio.h>
#include <string.h>

#include "image_probe.h"
#include "storage.h"

static const unsigned char VALID_BMP[] = {
    'B', 'M', 58, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,
    40, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 24, 0,
    0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0x20, 0x80, 0xe0, 0,
};

static const unsigned char HEADER_PNG[] = {
    0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a,
    0, 0, 0, 13, 'I', 'H', 'D', 'R',
    0, 0, 0, 1, 0, 0, 0, 1,
    8, 2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 'I', 'E', 'N', 'D', 0, 0, 0, 0,
};

static const unsigned char VALID_JPEG[] = {
    0xff, 0xd8,
    0xff, 0xc0, 0, 11, 8, 0, 1, 0, 2, 1, 1, 0x11, 0,
    0xff, 0xd9,
};

static const unsigned char VALID_GIF[] = {
    'G', 'I', 'F', '8', '9', 'a', 1, 0, 1, 0, 0, 0, 0, 0x3b,
};

static FILE *file_with_data(const unsigned char *data, size_t size)
{
    FILE *file = tmpfile();
    if (!file) return NULL;
    if (size > 0 && fwrite(data, 1, size, file) != size) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    return file;
}

FILE *storage_open(const char *relative_path, const char *mode)
{
    (void)mode;
    static const unsigned char BAD[] = { 1, 2, 3, 4 };
    static const unsigned char BIN[12] = { 0 };
    if (strcmp(relative_path, "valid.bmp") == 0) {
        return file_with_data(VALID_BMP, sizeof(VALID_BMP));
    }
    if (strcmp(relative_path, "header.png") == 0) {
        return file_with_data(HEADER_PNG, sizeof(HEADER_PNG));
    }
    if (strcmp(relative_path, "valid.jpg") == 0) {
        return file_with_data(VALID_JPEG, sizeof(VALID_JPEG));
    }
    if (strcmp(relative_path, "valid.gif") == 0) {
        return file_with_data(VALID_GIF, sizeof(VALID_GIF));
    }
    if (strcmp(relative_path, "valid.bin") == 0) {
        return file_with_data(BIN, sizeof(BIN));
    }
    if (strcmp(relative_path, "empty.png") == 0) {
        return file_with_data(NULL, 0);
    }
    if (strcmp(relative_path, "broken.png") == 0 ||
        strcmp(relative_path, "short.bin") == 0) {
        return file_with_data(BAD, sizeof(BAD));
    }
    return NULL;
}

static int fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static bool expect_valid(const char *path, const char *format,
                         uint32_t width, uint32_t height)
{
    image_probe_result_t result;
    return image_probe_file(path, &result) &&
           result.status == IMAGE_PROBE_OK &&
           strcmp(result.format, format) == 0 &&
           result.width == width && result.height == height;
}

int main(void)
{
    image_probe_result_t result;
    if (!expect_valid("valid.bmp", "BMP", 1, 1)) return fail("BMP");
    if (!expect_valid("header.png", "PNG", 1, 1)) return fail("PNG");
    if (!expect_valid("valid.jpg", "JPEG", 2, 1)) return fail("JPEG");
    if (!expect_valid("valid.gif", "GIF", 1, 1)) return fail("GIF");
    if (!expect_valid("valid.bin", "BIN", 0, 0)) return fail("BIN");

    if (image_probe_file("empty.png", &result) ||
        result.status != IMAGE_PROBE_EMPTY) {
        return fail("empty image");
    }
    if (image_probe_file("broken.png", &result) ||
        result.status != IMAGE_PROBE_TRUNCATED) {
        return fail("broken PNG");
    }
    if (image_probe_file("short.bin", &result) ||
        result.status != IMAGE_PROBE_TRUNCATED) {
        return fail("short BIN");
    }
    if (image_probe_file("missing.bmp", &result) ||
        result.status != IMAGE_PROBE_OPEN_FAILED) {
        return fail("missing file");
    }

    printf("PASS: image signatures, dimensions, and truncation states\n");
    return 0;
}
