#ifndef MC_CLIENT_TEXTURE_PACKS_ZIP_H
#define MC_CLIENT_TEXTURE_PACKS_ZIP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct texture_zip texture_zip;
typedef struct texture_zip_stream texture_zip_stream;

typedef enum texture_zip_status {
    TEXTURE_ZIP_OK,
    TEXTURE_ZIP_SOURCE_IO,
    TEXTURE_ZIP_UNSUPPORTED_NATIVE,
    TEXTURE_ZIP_LIMIT,
    TEXTURE_ZIP_ALLOCATION_FAILED,
    TEXTURE_ZIP_INVALID_ARGUMENT
} texture_zip_status;

typedef struct texture_zip_error {
    texture_zip_status status;
    const char *operation;
    int system_error, zlib_error, native_close_error;
} texture_zip_error;

typedef struct texture_zip_limits {
    size_t max_archive_bytes, max_entries, max_encoded_entry_bytes;
    size_t max_decoded_entry_bytes, max_stream_allocation_bytes;
} texture_zip_limits;

typedef struct texture_zip_entry {
    const uint8_t *name;
    size_t name_bytes;
    uint32_t encoded_bytes, decoded_bytes, crc32;
    uint16_t method, flags;
} texture_zip_entry;

/* Takes the actual open FILE on every admitted call, including parse failure.
 * Ordinary non-ZIP64 stored/deflated archives; unsupported native features are
 * separate from actual source-style read failures. Raw archive bytes and each
 * real stream have independent explicit bounds. No archive extraction occurs. */
texture_zip_status texture_zip_open(FILE *, const texture_zip_limits *, texture_zip **,
                                    texture_zip_error *);
texture_zip_status texture_zip_retain(texture_zip *);
texture_zip_status texture_zip_release(texture_zip *, texture_zip_error *);
texture_zip_status texture_zip_close(texture_zip *, texture_zip_error *);
size_t texture_zip_count(const texture_zip *);
bool texture_zip_entry_at(const texture_zip *, size_t, texture_zip_entry *);
/* Actual ZipFile name lookup selects the last equal central-directory name,
 * then tries a trailing '/' directory spelling only when exact lookup misses.
 * Missing succeeds with NULL. Stream keeps the real archive identity alive;
 * source archive close invalidates all its outstanding streams immediately. */
texture_zip_status texture_zip_lookup(texture_zip *, const uint8_t *, size_t, texture_zip_stream **,
                                      texture_zip_error *);
texture_zip_status texture_zip_read(texture_zip_stream *, uint8_t *, size_t, size_t *,
                                    texture_zip_error *);
size_t texture_zip_available(const texture_zip_stream *);
texture_zip_status texture_zip_stream_close(texture_zip_stream *, texture_zip_error *);
texture_zip_status texture_zip_stream_release(texture_zip_stream *, texture_zip_error *);
#endif
