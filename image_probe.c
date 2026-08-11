#include "image_probe.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "storage.h"

static uint16_t read_be16(const unsigned char *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8u) | p[1]);
}

static uint32_t read_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24u) |
           ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) |
           p[3];
}

static uint16_t read_le16(const unsigned char *p)
{
    return (uint16_t)(((uint16_t)p[1] << 8u) | p[0]);
}

static uint32_t read_le32(const unsigned char *p)
{
    return ((uint32_t)p[3] << 24u) |
           ((uint32_t)p[2] << 16u) |
           ((uint32_t)p[1] << 8u) |
           p[0];
}

static bool extension_is(const char *path, const char *expected)
{
    const char *extension = path ? strrchr(path, '.') : NULL;
    if (!extension) return false;
    extension++;
    while (*extension && *expected) {
        if (tolower((unsigned char)*extension) !=
            tolower((unsigned char)*expected)) {
            return false;
        }
        extension++;
        expected++;
    }
    return *extension == '\0' && *expected == '\0';
}

static bool read_at(FILE *file, long offset, void *data, size_t size)
{
    return fseek(file, offset, SEEK_SET) == 0 &&
           fread(data, 1, size, file) == size;
}

static bool probe_png(FILE *file, image_probe_result_t *result)
{
    static const unsigned char SIGNATURE[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
    };
    unsigned char header[24];
    unsigned char tail[12];
    if (result->file_size < 45u ||
        !read_at(file, 0, header, sizeof(header)) ||
        memcmp(header, SIGNATURE, sizeof(SIGNATURE)) != 0) {
        result->status = result->file_size < 45u
            ? IMAGE_PROBE_TRUNCATED : IMAGE_PROBE_CORRUPT;
        return false;
    }
    if (read_be32(header + 8) != 13u ||
        memcmp(header + 12, "IHDR", 4) != 0) {
        result->status = IMAGE_PROBE_CORRUPT;
        return false;
    }
    result->width = read_be32(header + 16);
    result->height = read_be32(header + 20);
    if (result->width == 0u || result->height == 0u ||
        !read_at(file, (long)result->file_size - 12, tail, sizeof(tail)) ||
        read_be32(tail) != 0u || memcmp(tail + 4, "IEND", 4) != 0) {
        result->status = IMAGE_PROBE_TRUNCATED;
        return false;
    }
    return true;
}

static bool jpeg_sof_marker(unsigned char marker)
{
    return (marker >= 0xc0u && marker <= 0xc3u) ||
           (marker >= 0xc5u && marker <= 0xc7u) ||
           (marker >= 0xc9u && marker <= 0xcbu) ||
           (marker >= 0xcdu && marker <= 0xcfu);
}

static bool probe_jpeg(FILE *file, image_probe_result_t *result)
{
    unsigned char bytes[8];
    if (result->file_size < 8u || !read_at(file, 0, bytes, 2) ||
        bytes[0] != 0xffu || bytes[1] != 0xd8u) {
        result->status = result->file_size < 8u
            ? IMAGE_PROBE_TRUNCATED : IMAGE_PROBE_CORRUPT;
        return false;
    }
    if (!read_at(file, (long)result->file_size - 2, bytes, 2) ||
        bytes[0] != 0xffu || bytes[1] != 0xd9u) {
        result->status = IMAGE_PROBE_TRUNCATED;
        return false;
    }

    long offset = 2;
    while ((uint32_t)offset + 4u <= result->file_size) {
        if (!read_at(file, offset, bytes, 2)) break;
        if (bytes[0] != 0xffu) {
            offset++;
            continue;
        }
        unsigned char marker = bytes[1];
        while (marker == 0xffu && (uint32_t)offset + 3u < result->file_size) {
            offset++;
            if (!read_at(file, offset, bytes, 2)) break;
            marker = bytes[1];
        }
        offset += 2;
        if (marker == 0xd9u || marker == 0xdau) break;
        if (marker == 0x01u || (marker >= 0xd0u && marker <= 0xd8u)) continue;
        if (!read_at(file, offset, bytes, 2)) break;
        const uint16_t segment_size = read_be16(bytes);
        if (segment_size < 2u ||
            (uint32_t)offset + segment_size > result->file_size) {
            result->status = IMAGE_PROBE_TRUNCATED;
            return false;
        }
        if (jpeg_sof_marker(marker)) {
            if (segment_size < 7u || !read_at(file, offset + 2, bytes, 5)) {
                result->status = IMAGE_PROBE_TRUNCATED;
                return false;
            }
            result->height = read_be16(bytes + 1);
            result->width = read_be16(bytes + 3);
            if (result->width == 0u || result->height == 0u) {
                result->status = IMAGE_PROBE_CORRUPT;
                return false;
            }
            return true;
        }
        offset += segment_size;
    }
    result->status = IMAGE_PROBE_CORRUPT;
    return false;
}

static bool probe_gif(FILE *file, image_probe_result_t *result)
{
    unsigned char header[10];
    unsigned char trailer;
    if (result->file_size < 14u ||
        !read_at(file, 0, header, sizeof(header))) {
        result->status = IMAGE_PROBE_TRUNCATED;
        return false;
    }
    if (memcmp(header, "GIF87a", 6) != 0 &&
        memcmp(header, "GIF89a", 6) != 0) {
        result->status = IMAGE_PROBE_CORRUPT;
        return false;
    }
    result->width = read_le16(header + 6);
    result->height = read_le16(header + 8);
    if (result->width == 0u || result->height == 0u ||
        !read_at(file, (long)result->file_size - 1, &trailer, 1) ||
        trailer != 0x3bu) {
        result->status = IMAGE_PROBE_TRUNCATED;
        return false;
    }
    return true;
}

static bool probe_bmp(FILE *file, image_probe_result_t *result)
{
    unsigned char header[30];
    if (result->file_size < 54u ||
        !read_at(file, 0, header, sizeof(header))) {
        result->status = IMAGE_PROBE_TRUNCATED;
        return false;
    }
    if (header[0] != 'B' || header[1] != 'M') {
        result->status = IMAGE_PROBE_CORRUPT;
        return false;
    }
    const uint32_t declared_size = read_le32(header + 2);
    const uint32_t pixel_offset = read_le32(header + 10);
    const uint32_t dib_size = read_le32(header + 14);
    const int32_t width = (int32_t)read_le32(header + 18);
    const int32_t height = (int32_t)read_le32(header + 22);
    if ((declared_size != 0u && declared_size > result->file_size) ||
        pixel_offset >= result->file_size || dib_size < 12u ||
        width <= 0 || height == 0 || height == INT32_MIN) {
        result->status = IMAGE_PROBE_TRUNCATED;
        return false;
    }
    result->width = (uint32_t)width;
    result->height = height < 0 ? (uint32_t)(-height) : (uint32_t)height;
    return true;
}

bool image_probe_file(const char *relative_path, image_probe_result_t *result)
{
    if (!result) return false;
    memset(result, 0, sizeof(*result));
    result->status = IMAGE_PROBE_OPEN_FAILED;
    if (!relative_path || !*relative_path) return false;

    FILE *file = storage_open(relative_path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    const long length = ftell(file);
    if (length <= 0 || (unsigned long)length > UINT32_MAX) {
        result->status = length == 0 ? IMAGE_PROBE_EMPTY
                                     : IMAGE_PROBE_OPEN_FAILED;
        fclose(file);
        return false;
    }
    result->file_size = (uint32_t)length;

    bool valid;
    if (extension_is(relative_path, "png")) {
        memcpy(result->format, "PNG", 4);
        valid = probe_png(file, result);
    } else if (extension_is(relative_path, "jpg") ||
               extension_is(relative_path, "jpeg")) {
        memcpy(result->format, "JPEG", 5);
        valid = probe_jpeg(file, result);
    } else if (extension_is(relative_path, "gif")) {
        memcpy(result->format, "GIF", 4);
        valid = probe_gif(file, result);
    } else if (extension_is(relative_path, "bmp")) {
        memcpy(result->format, "BMP", 4);
        valid = probe_bmp(file, result);
    } else if (extension_is(relative_path, "bin")) {
        memcpy(result->format, "BIN", 4);
        valid = result->file_size >= 12u;
        if (!valid) result->status = IMAGE_PROBE_TRUNCATED;
    } else {
        result->status = IMAGE_PROBE_UNSUPPORTED;
        valid = false;
    }
    fclose(file);
    if (valid) result->status = IMAGE_PROBE_OK;
    return valid;
}

const char *image_probe_status_text(image_probe_status_t status)
{
    switch (status) {
    case IMAGE_PROBE_OPEN_FAILED: return "File unavailable";
    case IMAGE_PROBE_EMPTY: return "Empty file";
    case IMAGE_PROBE_UNSUPPORTED: return "Unsupported image";
    case IMAGE_PROBE_TRUNCATED: return "Incomplete image data";
    case IMAGE_PROBE_CORRUPT: return "Invalid image data";
    case IMAGE_PROBE_OK:
    default:
        return "Ready";
    }
}
