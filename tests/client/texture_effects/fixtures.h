#ifndef MC_TEST_TEXTURE_EFFECT_FIXTURES_H
#define MC_TEST_TEXTURE_EFFECT_FIXTURES_H
/* Authored resource bytes, not copied game assets or captured effect answers.
 * The future caller supplies a fresh owned root; existing roots are refused.
 * This file is source-only/unrun until native qualification is granted. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#ifdef _WIN32
#include <direct.h>
#define FX_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define FX_MKDIR(p) mkdir(p, 0700)
#endif
static bool fx_path(char *out, size_t capacity, const char *root, const char *suffix) {
    const int n = snprintf(out, capacity, "%s/%s", root, suffix);
    return n >= 0 && (size_t)n < capacity;
}

static void fx_be32(uint8_t out[4], uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static bool fx_chunk(FILE *file, const char name[4], const uint8_t *bytes, size_t size) {
    if (size > UINT32_MAX)
        return false;
    uint8_t count[4], checksum[4];
    fx_be32(count, (uint32_t)size);
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)name, 4);
    if (size != 0)
        crc = crc32(crc, bytes, (uInt)size);
    fx_be32(checksum, (uint32_t)crc);
    return fwrite(count, 1, 4, file) == 4 && fwrite(name, 1, 4, file) == 4 &&
           (size == 0 || fwrite(bytes, 1, size, file) == size) && fwrite(checksum, 1, 4, file) == 4;
}

static bool fx_png(const char *path, unsigned width, unsigned height) {
    const size_t stride = (size_t)width * 4 + 1, total = stride * height;
    uint8_t *raw = malloc(total);
    if (raw == NULL)
        return false;
    for (unsigned y = 0; y < height; ++y) {
        raw[(size_t)y * stride] = 0;
        for (unsigned x = 0; x < width; ++x) {
            uint8_t *pixel = raw + (size_t)y * stride + 1 + (size_t)x * 4;
            pixel[0] = (uint8_t)((x + y) % 255u + 1u);
            pixel[1] = (uint8_t)((x + y) % 3u == 0 ? 0u : (y * 7u + 3u) & 255u);
            pixel[2] = pixel[0];
            pixel[3] = (uint8_t)(64u + (x * 3u + y * 5u) % 192u);
        }
    }
    uLongf length = compressBound((uLong)total);
    uint8_t *encoded = malloc((size_t)length);
    if (encoded == NULL) {
        free(raw);
        return false;
    }
    const int compressed = compress2(encoded, &length, raw, (uLong)total, Z_BEST_SPEED);
    free(raw);
    if (compressed != Z_OK) {
        free(encoded);
        return false;
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        free(encoded);
        return false;
    }
    static const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    uint8_t header[13] = {0};
    fx_be32(header, width);
    fx_be32(header + 4, height);
    header[8] = 8;
    header[9] = 6;
    const bool ok =
        fwrite(signature, 1, 8, file) == 8 && fx_chunk(file, "IHDR", header, sizeof(header)) &&
        fx_chunk(file, "IDAT", encoded, (size_t)length) && fx_chunk(file, "IEND", NULL, 0);
    free(encoded);
    const int closed = fclose(file);
    return ok && closed == 0;
}

static bool fx_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    if (file == NULL)
        return false;
    const size_t n = strlen(text);
    const bool ok = fwrite(text, 1, n, file) == n;
    const int closed = fclose(file);
    return ok && closed == 0;
}

static bool fx_make(const char *root) {
    if (FX_MKDIR(root) != 0)
        return false;
    char path[4096];
    static const char *const directories[] = {"lang", "achievement", "gui", "misc"};
    for (size_t n = 0; n < 4; ++n)
        if (!fx_path(path, sizeof(path), root, directories[n]) || FX_MKDIR(path) != 0)
            return false;
    static const char *const files[] = {"lang/en_US.lang", "lang/stats_US.lang",
                                        "achievement/map.txt"};
    for (size_t n = 0; n < 3; ++n)
        if (!fx_path(path, sizeof(path), root, files[n]) || !fx_text(path, ""))
            return false;
    if (!fx_path(path, sizeof(path), root, "gui/items.png") || !fx_png(path, 256, 256))
        return false;
    if (!fx_path(path, sizeof(path), root, "misc/dial.png") || !fx_png(path, 16, 16))
        return false;
    if (!fx_path(path, sizeof(path), root, "unrecognized.bin") ||
        !fx_text(path, "authored non-image"))
        return false;
    return true;
}

#undef FX_MKDIR
#endif
