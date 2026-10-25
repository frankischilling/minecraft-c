#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "mc/client/sound/sound_file.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <share.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

struct mc_sound_file {
    char *path;
    uint16_t *url_path;
    size_t url_length;
};

struct mc_sound_stream {
    int descriptor;
    size_t read, limit;
    int32_t state;
    int pending_error;
    bool mus, limit_failed;
};

static mc_sound_status result(mc_sound_error *error, mc_sound_status status, int system_error,
                              const char *operation) {
    if (error != NULL && status == MC_SOUND_OK)
        *error = (mc_sound_error){0};
    else if (error != NULL)
        *error = (mc_sound_error){.status = status,
                                  .operation = operation,
                                  .system_error = system_error,
                                  .source_pc = -1,
                                  .exception = MC_SOUND_EXCEPTION_NONE,
                                  .random_status = MC_JAVA_RANDOM_PROFILE_OK};
    return status;
}

static bool separator(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

static bool absolute_path(const char *path, size_t length) {
#ifdef _WIN32
    if (length >= 4u && separator(path[0]) && separator(path[1]) &&
        (path[2] == '.' || path[2] == '?') && separator(path[3]))
        return false;
    return (length >= 3u &&
            ((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) &&
            path[1] == ':' && separator(path[2])) ||
           (length >= 3u && separator(path[0]) && separator(path[1]) && !separator(path[2]));
#else
    return length != 0 && path[0] == '/';
#endif
}

static bool next_utf8(const char *text, size_t length, size_t *offset, uint32_t *out) {
    const unsigned char *bytes = (const unsigned char *)text;
    uint32_t value = bytes[(*offset)++];
    if (value < 128u) {
        *out = value;
        return true;
    }
    size_t following;
    uint32_t minimum;
    if (value >= 194u && value <= 223u) {
        following = 1;
        minimum = 128u;
        value &= 31u;
    } else if (value >= 224u && value <= 239u) {
        following = 2;
        minimum = 2048u;
        value &= 15u;
    } else if (value >= 240u && value <= 244u) {
        following = 3;
        minimum = 65536u;
        value &= 7u;
    } else {
        return false;
    }
    if (following > length - *offset)
        return false;
    for (size_t i = 0; i < following; ++i) {
        const uint32_t next = bytes[(*offset)++];
        if ((next & 192u) != 128u)
            return false;
        value = value * 64u + (next & 63u);
    }
    if (value < minimum || value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu))
        return false;
    *out = value;
    return true;
}

static bool quote_ascii(uint32_t unit) {
    if ((unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z') ||
        (unit >= '0' && unit <= '9'))
        return false;
    const char *allowed = "-_.!~*'():@&=+$,;/";
    return unit == 0 || strchr(allowed, (int)unit) == NULL;
}

static bool quote_unicode(uint32_t unit) {
    return (unit >= 0x80u && unit <= 0x9fu) || unit == 0xa0u || unit == 0x1680u ||
           (unit >= 0x2000u && unit <= 0x200au) || unit == 0x2028u || unit == 0x2029u ||
           unit == 0x202fu || unit == 0x205fu || unit == 0x3000u;
}

#ifdef _WIN32
static wchar_t *native_path(const char *path) {
    const size_t length = strlen(path);
    if (length > (size_t)INT_MAX)
        return NULL;
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, NULL, 0);
    if (count <= 0)
        return NULL;
    wchar_t *wide = malloc(((size_t)count + 1u) * sizeof(*wide));
    if (wide == NULL)
        return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, wide, count) !=
        count) {
        free(wide);
        return NULL;
    }
    wide[count] = L'\0';
    return wide;
}
#endif

static mc_sound_status directory(const char *path, bool *out, mc_sound_error *error) {
#ifdef _WIN32
    wchar_t *wide = native_path(path);
    if (wide == NULL)
        return result(error, MC_SOUND_ALLOCATION_FAILED, 0, "sound File directory path");
    const DWORD attributes = GetFileAttributesW(wide);
    free(wide);
    *out = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    (void)error;
    struct stat info;
    *out = stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
    return MC_SOUND_OK;
}

static mc_sound_status admit_path(const char *native_utf8_path, size_t *out,
                                  mc_sound_error *error) {
    if (native_utf8_path == NULL) {
        result(error, MC_SOUND_SOURCE_EXCEPTION, 0, "sound File is null");
        if (error != NULL)
            error->exception = MC_SOUND_EXCEPTION_NULL_FILE;
        return MC_SOUND_SOURCE_EXCEPTION;
    }
    size_t length = 0;
    while (length <= MC_SOUND_FILE_MAX_PATH_BYTES && native_utf8_path[length] != '\0')
        ++length;
    if (length > MC_SOUND_FILE_MAX_PATH_BYTES)
        return result(error, MC_SOUND_LIMIT, 0, "sound File path length");
    if (!absolute_path(native_utf8_path, length))
        return result(error, MC_SOUND_UNSUPPORTED_PATH, 0, "sound File requires absolute path");
    for (size_t at = 0; at < length;) {
        uint32_t unit;
        if (!next_utf8(native_utf8_path, length, &at, &unit))
            return result(error, MC_SOUND_UNSUPPORTED_PATH, 0, "sound File UTF8 path");
    }
    *out = length;
    return MC_SOUND_OK;
}

mc_sound_status mc_sound_file_resolve(const char *native_utf8_path, mc_sound_file **out,
                                      mc_sound_error *error) {
    if (out == NULL)
        return result(error, MC_SOUND_INVALID_ARGUMENT, 0, "sound File output");
    size_t length;
    const mc_sound_status admitted = admit_path(native_utf8_path, &length, error);
    if (admitted != MC_SOUND_OK)
        return admitted;
    mc_sound_file *file = calloc(1, sizeof(*file));
    if (file == NULL)
        return result(error, MC_SOUND_ALLOCATION_FAILED, 0, "sound File owner");
    file->path = malloc(length + 1u);
    /* Three percent units per input byte, plus slashify/UNC/trailing slash. */
    file->url_path = malloc((length * 3u + 6u) * sizeof(*file->url_path));
    if (file->path == NULL || file->url_path == NULL) {
        mc_sound_file_free(file);
        return result(error, MC_SOUND_ALLOCATION_FAILED, 0, "sound File identity");
    }
    size_t write_at = 0, read_at = 0;
#ifdef _WIN32
    if (separator(native_utf8_path[0]) && separator(native_utf8_path[1])) {
        file->path[write_at++] = '\\';
        file->path[write_at++] = '\\';
        read_at = 2;
    }
    const char native_separator = '\\';
#else
    const char native_separator = '/';
#endif
    while (read_at < length) {
        if (!separator(native_utf8_path[read_at])) {
            file->path[write_at++] = native_utf8_path[read_at++];
            continue;
        }
        while (read_at < length && separator(native_utf8_path[read_at]))
            ++read_at;
        if (read_at != length || write_at == 0
#ifdef _WIN32
            || (write_at == 2u && file->path[1] == ':')
#endif
        )
            file->path[write_at++] = native_separator;
    }
    file->path[write_at] = '\0';
    length = write_at;
    bool is_directory;
    const mc_sound_status status = directory(file->path, &is_directory, error);
    if (status != MC_SOUND_OK) {
        mc_sound_file_free(file);
        return status;
    }
    size_t url_at = 0;
#ifdef _WIN32
    if (separator(file->path[0]) && separator(file->path[1])) {
        file->url_path[url_at++] = '/';
        file->url_path[url_at++] = '/';
    } else {
        file->url_path[url_at++] = '/';
    }
#endif
    static const char hex[] = "0123456789ABCDEF";
    for (size_t at = 0; at < length;) {
        const size_t first = at;
        uint32_t unit;
        (void)next_utf8(file->path, length, &at, &unit);
        if (separator(file->path[first])) {
            file->url_path[url_at++] = '/';
        } else if ((unit < 128u && quote_ascii(unit)) || quote_unicode(unit)) {
            for (size_t i = first; i < at; ++i) {
                const unsigned int byte = (unsigned char)file->path[i];
                file->url_path[url_at++] = '%';
                file->url_path[url_at++] = (uint16_t)hex[byte >> 4u];
                file->url_path[url_at++] = (uint16_t)hex[byte & 15u];
            }
        } else if (unit >= 65536u) {
            const uint32_t pair = unit - 65536u;
            file->url_path[url_at++] = (uint16_t)(0xd800u + (pair >> 10u));
            file->url_path[url_at++] = (uint16_t)(0xdc00u + (pair & 1023u));
        } else {
            file->url_path[url_at++] = (uint16_t)unit;
        }
    }
    if (is_directory && file->url_path[url_at - 1u] != '/')
        file->url_path[url_at++] = '/';
    file->url_path[url_at] = 0;
    file->url_length = url_at;
    *out = file;
    return result(error, MC_SOUND_OK, 0, "sound File identity resolved");
}

bool mc_sound_file_read_identity(const mc_sound_file *file, mc_sound_file_identity *out) {
    if (file == NULL || out == NULL)
        return false;
    *out = (mc_sound_file_identity){file->path, {file->url_path, file->url_length, false}};
    return true;
}

void mc_sound_file_free(mc_sound_file *file) {
    if (file != NULL) {
        free(file->path);
        free(file->url_path);
        free(file);
    }
}

static int close_descriptor(int descriptor) {
#ifdef _WIN32
    return _close(descriptor);
#else
    return close(descriptor);
#endif
}

static int64_t read_descriptor(int descriptor, uint8_t *destination, size_t capacity) {
#ifdef _WIN32
    return _read(descriptor, destination, (unsigned int)capacity);
#else
    return (int64_t)read(descriptor, destination, capacity);
#endif
}

static mc_sound_status open_regular(const char *path, size_t limit, int *out,
                                    mc_sound_error *error) {
    int descriptor = -1;
    int code;
#ifdef _WIN32
    wchar_t *wide = native_path(path);
    if (wide == NULL)
        return result(error, MC_SOUND_ALLOCATION_FAILED, 0, "sound stream native path");
    errno = 0;
#ifdef _MSC_VER
    code = (int)_wsopen_s(&descriptor, wide, _O_RDONLY | _O_BINARY | _O_NOINHERIT, _SH_DENYNO,
                          _S_IREAD);
#else
    descriptor = _wsopen(wide, _O_RDONLY | _O_BINARY | _O_NOINHERIT, _SH_DENYNO);
    code = errno;
#endif
    free(wide);
    if (descriptor < 0)
        return result(error, MC_SOUND_IO_FAILED, code, "sound stream open");
    struct _stat64 info;
    errno = 0;
    code = _fstat64(descriptor, &info);
    const bool regular = code == 0 && (info.st_mode & _S_IFMT) == _S_IFREG;
#else
    errno = 0;
    descriptor = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0)
        return result(error, MC_SOUND_IO_FAILED, errno, "sound stream open");
    struct stat info;
    errno = 0;
    code = fstat(descriptor, &info);
    const bool regular = code == 0 && S_ISREG(info.st_mode);
#endif
    if (!regular || info.st_size < 0 || (uint64_t)info.st_size > (uint64_t)limit) {
        code = errno;
        (void)close_descriptor(descriptor);
        return result(error, regular ? MC_SOUND_LIMIT : MC_SOUND_IO_FAILED, code,
                      "sound stream regular file/size");
    }
    /* Read the descriptor directly. Even an unbuffered stdio facade can
     * retain bytes that hide a later write through another file handle. */
    *out = descriptor;
    return MC_SOUND_OK;
}

mc_sound_status mc_sound_stream_open(const mc_sound_file *file, bool mus, size_t max_bytes,
                                     mc_sound_stream **out, mc_sound_error *error) {
    mc_sound_file_identity identity;
    if (!mc_sound_file_read_identity(file, &identity))
        return result(error, MC_SOUND_INVALID_ARGUMENT, 0, "sound stream admission");
    return mc_sound_stream_open_identity(&identity, mus, max_bytes, out, error);
}

mc_sound_status mc_sound_stream_open_identity(const mc_sound_file_identity *identity, bool mus,
                                              size_t max_bytes, mc_sound_stream **out,
                                              mc_sound_error *error) {
    if (identity == NULL || identity->native_utf8_path == NULL || identity->url_path.is_null ||
        (identity->url_path.length != 0 && identity->url_path.units == NULL) || out == NULL ||
        max_bytes == 0 || max_bytes > MC_SOUND_STREAM_MAX_BYTES)
        return result(error, MC_SOUND_INVALID_ARGUMENT, 0, "sound stream identity admission");
    if (identity->url_path.length > 3u * MC_SOUND_FILE_MAX_PATH_BYTES + 5u)
        return result(error, MC_SOUND_LIMIT, 0, "sound stream URL path length");
    size_t length;
    const mc_sound_status admitted = admit_path(identity->native_utf8_path, &length, error);
    if (admitted != MC_SOUND_OK)
        return admitted;
    (void)length;
    mc_sound_stream *stream = calloc(1, sizeof(*stream));
    if (stream == NULL)
        return result(error, MC_SOUND_ALLOCATION_FAILED, 0, "sound stream owner");
    stream->descriptor = -1;
    const mc_sound_status status =
        open_regular(identity->native_utf8_path, max_bytes, &stream->descriptor, error);
    if (status != MC_SOUND_OK) {
        free(stream);
        return status;
    }
    stream->mus = mus;
    stream->limit = max_bytes;
    size_t first = 0;
    for (size_t i = 0; i < identity->url_path.length; ++i)
        if (identity->url_path.units[i] == '/')
            first = i + 1u;
    for (size_t i = first; i < identity->url_path.length; ++i)
        stream->state =
            mc_java_i32_add(mc_java_i32_mul(stream->state, 31), identity->url_path.units[i]);
    *out = stream;
    return result(error, MC_SOUND_OK, 0, "sound stream opened");
}

mc_sound_status mc_sound_stream_read(mc_sound_stream *stream, uint8_t *destination, size_t capacity,
                                     size_t *count, mc_sound_error *error) {
    if (stream == NULL || count == NULL || (capacity != 0 && destination == NULL))
        return result(error, MC_SOUND_INVALID_ARGUMENT, 0, "sound stream read admission");
    if (stream->descriptor < 0 || stream->pending_error != 0)
        return result(error, MC_SOUND_IO_FAILED, stream->pending_error, "sound stream read");
    if (stream->limit_failed)
        return result(error, MC_SOUND_LIMIT, 0, "sound stream input limit");
    if (capacity == 0) {
        *count = 0;
        return result(error, MC_SOUND_OK, 0, "sound stream empty read");
    }
    if (stream->read == stream->limit) {
        errno = 0;
        uint8_t byte;
        const int64_t extra = read_descriptor(stream->descriptor, &byte, 1u);
        if (extra > 0) {
            stream->limit_failed = true;
            return result(error, MC_SOUND_LIMIT, 0, "sound stream grew beyond input limit");
        }
        if (extra < 0) {
            stream->pending_error = errno != 0 ? errno : EIO;
            return result(error, MC_SOUND_IO_FAILED, stream->pending_error,
                          "sound stream EOF probe");
        }
        *count = 0;
        return result(error, MC_SOUND_OK, 0, "sound stream EOF");
    }
    if (capacity > stream->limit - stream->read)
        capacity = stream->limit - stream->read;
    errno = 0;
    const int64_t received = read_descriptor(stream->descriptor, destination, capacity);
    if (received < 0) {
        stream->pending_error = errno != 0 ? errno : EIO;
        return result(error, MC_SOUND_IO_FAILED, stream->pending_error, "sound stream read");
    }
    const size_t actual = (size_t)received;
    if (stream->mus)
        for (size_t i = 0; i < actual; ++i) {
            /* i2b keeps the low eight XOR bits; the stored signed byte feeds
             * both wrapped products in the original np recurrence. */
            const int32_t shifted = mc_java_i32_shr(stream->state, 8);
            const uint8_t decoded = destination[i] ^ (uint8_t)(uint32_t)shifted;
            destination[i] = decoded;
            const int32_t signed_byte = decoded < 128u ? (int32_t)decoded : (int32_t)decoded - 256;
            stream->state = mc_java_i32_add(mc_java_i32_mul(stream->state, 498729871),
                                            mc_java_i32_mul(85731, signed_byte));
        }
    stream->read += actual;
    *count = actual;
    return result(error, MC_SOUND_OK, 0, "sound stream read");
}

mc_sound_status mc_sound_stream_read_byte(mc_sound_stream *stream, int32_t *value, bool *eof,
                                          mc_sound_error *error) {
    if (value == NULL || eof == NULL)
        return result(error, MC_SOUND_INVALID_ARGUMENT, 0, "sound byte read admission");
    uint8_t byte;
    size_t count;
    const mc_sound_status status = mc_sound_stream_read(stream, &byte, 1u, &count, error);
    if (status != MC_SOUND_OK)
        return status;
    *eof = count == 0;
    *value = count == 0 ? -1 : stream->mus && byte >= 128u ? (int32_t)byte - 256 : (int32_t)byte;
    return MC_SOUND_OK;
}

mc_sound_status mc_sound_stream_close(mc_sound_stream *stream, mc_sound_error *error) {
    if (stream == NULL)
        return result(error, MC_SOUND_INVALID_ARGUMENT, 0, "sound stream close admission");
    if (!stream->mus && stream->descriptor >= 0) {
        const int descriptor = stream->descriptor;
        stream->descriptor = -1;
        errno = 0;
        if (close_descriptor(descriptor) != 0)
            return result(error, MC_SOUND_IO_FAILED, errno, "sound stream source close");
    }
    return result(error, MC_SOUND_OK, 0, "sound stream source close");
}

mc_sound_status mc_sound_stream_free(mc_sound_stream *stream, mc_sound_error *error) {
    mc_sound_status status = MC_SOUND_OK;
    int code = 0;
    if (stream != NULL) {
        if (stream->descriptor >= 0) {
            errno = 0;
            if (close_descriptor(stream->descriptor) != 0) {
                status = MC_SOUND_IO_FAILED;
                code = errno;
            }
        }
        free(stream);
    }
    return result(error, status, code, "sound stream native retirement");
}
