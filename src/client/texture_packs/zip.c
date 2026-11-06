#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "zip.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

typedef struct zip_record {
    texture_zip_entry view;
    uint32_t local_offset;
} zip_record;

struct texture_zip {
    FILE *file;
    size_t holds, count, file_bytes;
    texture_zip_limits limits;
    uint8_t *central;
    zip_record *entries;
    bool closed;
    texture_zip_stream *streams;
};
typedef union zip_allocation zip_allocation;

union zip_allocation {
    size_t bytes;
    long double alignment;
    void *pointer_alignment;
    uint64_t integer_alignment;
};

struct texture_zip_stream {
    texture_zip_stream *next;
    texture_zip *owner;
    size_t offset, remaining, decoded, expected, memory;
    bool closed, ended, inflater_ready, allocation_failed, allocation_limited, dummy, data_ready;
    bool pending_error;
    int pending_system_error;
    uint16_t method;
    z_stream inflater;
    uint8_t input[8192];
};

static texture_zip_status failure(texture_zip_error *error, texture_zip_status status,
                                  const char *operation) {
    if (error != NULL)
        *error =
            (texture_zip_error){.status = status, .operation = operation, .system_error = errno};
    return status;
}

static texture_zip_status success(texture_zip_error *error) {
    if (error != NULL)
        *error = (texture_zip_error){0};
    return TEXTURE_ZIP_OK;
}

static uint16_t u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static uint32_t u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool seek_file(FILE *file, size_t offset) {
    if (offset > INT64_MAX)
        return false;
#ifdef _WIN32
    return _fseeki64(file, (__int64)offset, SEEK_SET) == 0;
#else
    return fseeko(file, (off_t)offset, SEEK_SET) == 0;
#endif
}

static bool extent(FILE *file, size_t *out) {
#ifdef _WIN32
    if (_fseeki64(file, 0, SEEK_END) != 0)
        return false;
    const __int64 bytes = _ftelli64(file);
#else
    if (fseeko(file, 0, SEEK_END) != 0)
        return false;
    const off_t bytes = ftello(file);
#endif
    if (bytes < 0 || (uint64_t)bytes > SIZE_MAX)
        return false;
    *out = (size_t)bytes;
    return true;
}

static bool read_at(texture_zip *zip, size_t offset, void *out, size_t bytes) {
    return !zip->closed && offset <= zip->file_bytes && bytes <= zip->file_bytes - offset &&
           seek_file(zip->file, offset) && fread(out, 1, bytes, zip->file) == bytes;
}

static texture_zip_status read_some(texture_zip_stream *stream, uint8_t *out, size_t capacity,
                                    size_t *count, texture_zip_error *error) {
    if (stream->pending_error) {
        errno = stream->pending_system_error;
        return failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile pending entry read failure");
    }
    if (!seek_file(stream->owner->file, stream->offset))
        return failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile entry seek");
    const size_t bytes = fread(out, 1, capacity, stream->owner->file);
    if (ferror(stream->owner->file)) {
        if (bytes == 0)
            return failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile entry read");
        stream->pending_error = true;
        stream->pending_system_error = errno;
    }
    stream->offset += bytes;
    *count = bytes;
    return TEXTURE_ZIP_OK;
}

static bool valid_utf8(const uint8_t *bytes, size_t length) {
    size_t at = 0;
    while (at < length) {
        const uint8_t first = bytes[at++];
        uint32_t value = first;
        unsigned int trailing = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            value = first & 31u;
            trailing = 1;
        } else if (first >= 0xe0 && first <= 0xef) {
            value = first & 15u;
            trailing = 2;
        } else if (first >= 0xf0 && first <= 0xf4) {
            value = first & 7u;
            trailing = 3;
        } else if (first >= 0x80)
            return false;
        if (trailing > length - at)
            return false;
        for (unsigned int i = 0; i < trailing; ++i) {
            const uint8_t next = bytes[at++];
            if ((next & 0xc0u) != 0x80u)
                return false;
            value = (value << 6) | (next & 63u);
        }
        if ((trailing == 1 && value < 0x80) || (trailing == 2 && value < 0x800) ||
            (trailing == 3 && value < 0x10000) || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff))
            return false;
    }
    return true;
}

texture_zip_status texture_zip_open(FILE *file, const texture_zip_limits *limits, texture_zip **out,
                                    texture_zip_error *error) {
    if (file == NULL || limits == NULL || out == NULL)
        return failure(error, TEXTURE_ZIP_INVALID_ARGUMENT, "ZipFile native arguments");
    texture_zip_status result = TEXTURE_ZIP_OK;
    texture_zip *zip = calloc(1, sizeof(*zip));
    uint8_t *tail = NULL;
    if (zip == NULL) {
        (void)fclose(file);
        return failure(error, TEXTURE_ZIP_ALLOCATION_FAILED, "ZipFile native owner");
    }
    zip->file = file;
    zip->holds = 1;
    zip->limits = *limits;
    if (!extent(file, &zip->file_bytes)) {
        result = failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile archive extent");
        goto done;
    }
    if (zip->file_bytes > limits->max_archive_bytes) {
        result = failure(error, TEXTURE_ZIP_LIMIT, "ZipFile native archive bound");
        goto done;
    }
    const size_t tail_bytes = zip->file_bytes < 65557u ? zip->file_bytes : 65557u;
    if (tail_bytes < 22) {
        result = failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile missing end header");
        goto done;
    }
    tail = malloc(tail_bytes);
    if (tail == NULL) {
        result = failure(error, TEXTURE_ZIP_ALLOCATION_FAILED, "ZipFile end header storage");
        goto done;
    }
    if (!read_at(zip, zip->file_bytes - tail_bytes, tail, tail_bytes)) {
        result = failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile end header read");
        goto done;
    }
    size_t at = tail_bytes - 22;
    for (;;) {
        if (u32(tail + at) == UINT32_C(0x06054b50) && u16(tail + at + 20) <= tail_bytes - at - 22)
            break;
        if (at == 0) {
            result = failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile missing end header");
            goto done;
        }
        --at;
    }
    const uint8_t *end = tail + at;
    if (u16(end + 4) != 0 || u16(end + 6) != 0 || u16(end + 8) != u16(end + 10) ||
        u16(end + 10) == UINT16_MAX || u32(end + 12) == UINT32_MAX || u32(end + 16) == UINT32_MAX) {
        result =
            failure(error, TEXTURE_ZIP_UNSUPPORTED_NATIVE, "ZipFile ZIP64/multidisk dependency");
        goto done;
    }
    zip->count = u16(end + 10);
    const size_t central_bytes = u32(end + 12), central_offset = u32(end + 16);
    const size_t end_offset = zip->file_bytes - tail_bytes + at;
    if (zip->count > limits->max_entries || central_bytes > limits->max_archive_bytes ||
        zip->count > SIZE_MAX / sizeof(*zip->entries)) {
        result = failure(error, TEXTURE_ZIP_LIMIT, "ZipFile native directory bound");
        goto done;
    }
    if (central_offset > end_offset || central_bytes > end_offset - central_offset) {
        result = failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile invalid central directory extent");
        goto done;
    }
    zip->central = malloc(central_bytes == 0 ? 1 : central_bytes);
    zip->entries = calloc(zip->count == 0 ? 1 : zip->count, sizeof(*zip->entries));
    if (zip->central == NULL || zip->entries == NULL) {
        result = failure(error, TEXTURE_ZIP_ALLOCATION_FAILED, "ZipFile central directory storage");
        goto done;
    }
    if (!read_at(zip, central_offset, zip->central, central_bytes)) {
        result = failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile central directory read");
        goto done;
    }
    size_t cursor = 0;
    for (size_t i = 0; i < zip->count; ++i) {
        if (cursor > central_bytes || central_bytes - cursor < 46 ||
            u32(zip->central + cursor) != UINT32_C(0x02014b50)) {
            result = failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile invalid central entry");
            goto done;
        }
        const uint8_t *entry = zip->central + cursor;
        const size_t name_bytes = u16(entry + 28);
        const size_t trailing = name_bytes + u16(entry + 30) + u16(entry + 32);
        if (trailing > central_bytes - cursor - 46) {
            result = failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile truncated central entry");
            goto done;
        }
        if (u16(entry + 34) != 0 || u32(entry + 20) == UINT32_MAX ||
            u32(entry + 24) == UINT32_MAX || u32(entry + 42) == UINT32_MAX) {
            result =
                failure(error, TEXTURE_ZIP_UNSUPPORTED_NATIVE, "ZipFile ZIP64 entry dependency");
            goto done;
        }
        const uint16_t method = u16(entry + 10), flags = u16(entry + 8);
        if ((flags & UINT16_C(0x0001)) != 0 || (method != 0 && method != 8)) {
            result = failure(error, TEXTURE_ZIP_UNSUPPORTED_NATIVE,
                             "ZipFile encrypted/method dependency");
            goto done;
        }
        if (!valid_utf8(entry + 46, name_bytes)) {
            result = failure(error, TEXTURE_ZIP_UNSUPPORTED_NATIVE,
                             "ZipFile malformed UTF8 name outside native profile");
            goto done;
        }
        zip->entries[i] = (zip_record){{entry + 46, name_bytes, u32(entry + 20), u32(entry + 24),
                                        u32(entry + 16), method, flags},
                                       u32(entry + 42)};
        cursor += 46 + trailing;
    }
    *out = zip;
    zip = NULL;
    success(error);
done:
    free(tail);
    if (zip != NULL) {
        texture_zip_error close_error = {0};
        if (texture_zip_close(zip, &close_error) != TEXTURE_ZIP_OK && error != NULL)
            error->native_close_error = close_error.system_error;
    }
    texture_zip_release(zip, NULL);
    return result;
}

texture_zip_status texture_zip_retain(texture_zip *zip) {
    if (zip == NULL)
        return TEXTURE_ZIP_INVALID_ARGUMENT;
    if (zip->holds == SIZE_MAX)
        return TEXTURE_ZIP_LIMIT;
    ++zip->holds;
    return TEXTURE_ZIP_OK;
}

texture_zip_status texture_zip_release(texture_zip *zip, texture_zip_error *error) {
    if (zip == NULL || --zip->holds != 0)
        return success(error);
    texture_zip_status status = TEXTURE_ZIP_OK;
    if (zip->file != NULL)
        status = texture_zip_close(zip, error);
    free(zip->entries);
    free(zip->central);
    free(zip);
    if (status == TEXTURE_ZIP_OK)
        success(error);
    return status;
}

texture_zip_status texture_zip_close(texture_zip *zip, texture_zip_error *error) {
    if (zip == NULL)
        return failure(error, TEXTURE_ZIP_INVALID_ARGUMENT, "ZipFile close receiver");
    if (!zip->closed) {
        zip->closed = true;
        texture_zip_error first = {0};
        for (texture_zip_stream *stream = zip->streams; stream != NULL; stream = stream->next) {
            texture_zip_error cause = {0};
            if (texture_zip_stream_close(stream, &cause) != TEXTURE_ZIP_OK &&
                first.status == TEXTURE_ZIP_OK)
                first = cause;
        }
        FILE *file = zip->file;
        zip->file = NULL;
        if (fclose(file) != 0 && first.status == TEXTURE_ZIP_OK)
            return failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile source close");
        if (first.status != TEXTURE_ZIP_OK) {
            if (error != NULL)
                *error = first;
            return first.status;
        }
    }
    return success(error);
}

size_t texture_zip_count(const texture_zip *zip) {
    return zip == NULL ? 0 : zip->count;
}

bool texture_zip_entry_at(const texture_zip *zip, size_t at, texture_zip_entry *out) {
    if (zip == NULL || out == NULL || at >= zip->count)
        return false;
    *out = zip->entries[at].view;
    return true;
}

static voidpf zip_allocate(voidpf context, uInt count, uInt width) {
    texture_zip_stream *stream = context;
    if ((size_t)count > SIZE_MAX / (size_t)(width == 0 ? 1 : width)) {
        stream->allocation_failed = true;
        stream->allocation_limited = true;
        return NULL;
    }
    const size_t bytes = (size_t)count * width;
    if (bytes > SIZE_MAX - sizeof(zip_allocation) ||
        bytes + sizeof(zip_allocation) >
            stream->owner->limits.max_stream_allocation_bytes - stream->memory) {
        stream->allocation_failed = true;
        stream->allocation_limited = true;
        return NULL;
    }
    zip_allocation *block = calloc(1, sizeof(*block) + bytes);
    if (block == NULL) {
        stream->allocation_failed = true;
        return NULL;
    }
    block->bytes = sizeof(*block) + bytes;
    stream->memory += block->bytes;
    return block + 1;
}

static void zip_free(voidpf context, voidpf address) {
    if (address == NULL)
        return;
    texture_zip_stream *stream = context;
    zip_allocation *block = (zip_allocation *)address - 1;
    stream->memory -= block->bytes;
    free(block);
}

static const zip_record *find_entry(const texture_zip *zip, const uint8_t *name, size_t name_bytes,
                                    bool directory_suffix) {
    if (directory_suffix && name_bytes == SIZE_MAX)
        return NULL;
    const size_t required = name_bytes + (directory_suffix ? 1u : 0u);
    for (size_t at = zip->count; at != 0;) {
        const zip_record *entry = &zip->entries[--at];
        if (entry->view.name_bytes == required &&
            (name_bytes == 0 || memcmp(entry->view.name, name, name_bytes) == 0) &&
            (!directory_suffix || entry->view.name[name_bytes] == '/'))
            return entry;
    }
    return NULL;
}

texture_zip_status texture_zip_lookup(texture_zip *zip, const uint8_t *name, size_t name_bytes,
                                      texture_zip_stream **out, texture_zip_error *error) {
    if (zip == NULL || out == NULL || (name == NULL && name_bytes != 0))
        return failure(error, TEXTURE_ZIP_INVALID_ARGUMENT, "ZipFile lookup arguments");
    if (zip->closed)
        return failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile lookup after close");
    const zip_record *entry = find_entry(zip, name, name_bytes, false);
    /* ZipFile.getEntry checks the exact name before its optional '/' fallback.
     * Reverse search preserves the last CEN occurrence for either spelling. */
    if (entry == NULL && (name_bytes == 0 || name[name_bytes - 1u] != '/'))
        entry = find_entry(zip, name, name_bytes, true);
    if (entry == NULL) {
        *out = NULL;
        return success(error);
    }
    if (entry->view.encoded_bytes > zip->limits.max_encoded_entry_bytes ||
        entry->view.decoded_bytes > zip->limits.max_decoded_entry_bytes ||
        sizeof(texture_zip_stream) > zip->limits.max_stream_allocation_bytes)
        return failure(error, TEXTURE_ZIP_LIMIT, "ZipFile native stream bound");
    texture_zip_stream *stream = calloc(1, sizeof(*stream));
    if (stream == NULL)
        return failure(error, TEXTURE_ZIP_ALLOCATION_FAILED, "ZipFile native stream owner");
    if (texture_zip_retain(zip) != TEXTURE_ZIP_OK) {
        free(stream);
        return failure(error, TEXTURE_ZIP_LIMIT, "ZipFile stream archive hold");
    }
    stream->owner = zip;
    stream->offset = entry->local_offset;
    stream->remaining = entry->view.encoded_bytes;
    stream->expected = entry->view.decoded_bytes;
    stream->method = entry->view.method;
    stream->memory = sizeof(*stream);
    stream->next = zip->streams;
    zip->streams = stream;
    if (stream->method == 8) {
        stream->inflater.zalloc = zip_allocate;
        stream->inflater.zfree = zip_free;
        stream->inflater.opaque = stream;
        const int result = inflateInit2(&stream->inflater, -MAX_WBITS);
        if (result != Z_OK) {
            const texture_zip_status status =
                stream->allocation_limited ? TEXTURE_ZIP_LIMIT : TEXTURE_ZIP_ALLOCATION_FAILED;
            texture_zip_stream_release(stream, NULL);
            return failure(error, status, "ZipFile raw inflater owner");
        }
        stream->inflater_ready = true;
    }
    *out = stream;
    return success(error);
}

texture_zip_status texture_zip_read(texture_zip_stream *stream, uint8_t *out, size_t capacity,
                                    size_t *count, texture_zip_error *error) {
    if (stream == NULL || count == NULL || (out == NULL && capacity != 0))
        return failure(error, TEXTURE_ZIP_INVALID_ARGUMENT, "ZipFile read arguments");
    if (stream->owner->closed)
        return failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile stream after archive close");
    if (stream->closed) {
        if (stream->method != 0)
            return failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile inflater stream after close");
        *count = 0;
        return success(error);
    }
    if (capacity == 0 || stream->ended) {
        *count = 0;
        return success(error);
    }
    if (!stream->data_ready) {
        uint8_t local[30];
        if (!read_at(stream->owner, stream->offset, local, sizeof(local)) ||
            u32(local) != UINT32_C(0x04034b50))
            return failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile invalid local header on read");
        const size_t skip = sizeof(local) + (size_t)u16(local + 26) + u16(local + 28);
        if (stream->offset > stream->owner->file_bytes ||
            skip > stream->owner->file_bytes - stream->offset)
            return failure(error, TEXTURE_ZIP_UNSUPPORTED_NATIVE,
                           "ZipFile unqualified local data offset");
        stream->offset += skip;
        stream->data_ready = true;
    }
    if (capacity > UINT_MAX)
        capacity = UINT_MAX;
    size_t made = 0;
    if (stream->method == 0) {
        made = capacity < stream->remaining ? capacity : stream->remaining;
        if (read_some(stream, out, made, &made, error) != TEXTURE_ZIP_OK)
            return error == NULL ? TEXTURE_ZIP_SOURCE_IO : error->status;
        stream->remaining -= made;
        stream->ended = stream->remaining == 0;
    } else {
        z_stream *z = &stream->inflater;
        z->next_out = out;
        z->avail_out = (uInt)capacity;
        while (z->avail_out == (uInt)capacity && !stream->ended) {
            if (z->avail_in == 0 && stream->remaining != 0) {
                const size_t bytes = stream->remaining < sizeof(stream->input)
                                         ? stream->remaining
                                         : sizeof(stream->input);
                size_t actual = 0;
                if (read_some(stream, stream->input, bytes, &actual, error) != TEXTURE_ZIP_OK)
                    return error == NULL ? TEXTURE_ZIP_SOURCE_IO : error->status;
                stream->remaining -= actual;
                if (actual == 0)
                    stream->remaining = 0;
                z->next_in = stream->input;
                z->avail_in = (uInt)actual;
            }
            if (z->avail_in == 0 && stream->remaining == 0 && !stream->dummy) {
                /* ZipFileInflaterInputStream supplies one dummy zero byte for
                 * the real nowrap inflater at compressed EOF. */
                stream->input[0] = 0;
                z->next_in = stream->input;
                z->avail_in = 1;
                stream->dummy = true;
            }
            const int result = inflate(z, Z_NO_FLUSH);
            if (result == Z_STREAM_END)
                stream->ended = true;
            else if (result != Z_OK || (z->avail_in == 0 && stream->remaining == 0 &&
                                        z->avail_out == (uInt)capacity)) {
                const texture_zip_status status = stream->allocation_limited ? TEXTURE_ZIP_LIMIT
                                                  : stream->allocation_failed
                                                      ? TEXTURE_ZIP_ALLOCATION_FAILED
                                                      : TEXTURE_ZIP_SOURCE_IO;
                failure(error, status, "ZipFile raw deflate data");
                if (error != NULL)
                    error->zlib_error = result;
                return status;
            }
        }
        made = capacity - z->avail_out;
    }
    if (made > stream->owner->limits.max_decoded_entry_bytes - stream->decoded)
        return failure(error, TEXTURE_ZIP_LIMIT, "ZipFile native decoded stream bound");
    stream->decoded += made;
    *count = made;
    return success(error);
}

size_t texture_zip_available(const texture_zip_stream *stream) {
    if (stream == NULL || stream->closed || stream->owner->closed ||
        stream->decoded >= stream->expected)
        return 0;
    return stream->expected - stream->decoded;
}

texture_zip_status texture_zip_stream_close(texture_zip_stream *stream, texture_zip_error *error) {
    if (stream == NULL)
        return failure(error, TEXTURE_ZIP_INVALID_ARGUMENT, "ZipFile stream close receiver");
    stream->closed = true;
    if (stream->inflater_ready) {
        const int result = inflateEnd(&stream->inflater);
        stream->inflater_ready = false;
        if (result != Z_OK)
            return failure(error, TEXTURE_ZIP_SOURCE_IO, "ZipFile inflater close");
    }
    return success(error);
}

texture_zip_status texture_zip_stream_release(texture_zip_stream *stream,
                                              texture_zip_error *error) {
    if (stream == NULL)
        return success(error);
    texture_zip_error primary = {0};
    texture_zip_stream_close(stream, &primary);
    texture_zip_stream **at = &stream->owner->streams;
    while (*at != NULL && *at != stream)
        at = &(*at)->next;
    if (*at == stream)
        *at = stream->next;
    texture_zip_error released = {0};
    texture_zip_release(stream->owner, &released);
    if (primary.status == TEXTURE_ZIP_OK)
        primary = released;
    free(stream);
    if (error != NULL)
        *error = primary;
    return primary.status;
}
