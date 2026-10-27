#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include "java_file_internal.h"
#include "java_file_native.h"
#include "mc/core/java/reader.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef MC_JAVA_FILE_NATIVE_CLOSE
#ifdef _WIN32
#define MC_JAVA_FILE_NATIVE_CLOSE _close
#else
#define MC_JAVA_FILE_NATIVE_CLOSE close
#endif
#else
extern int MC_JAVA_FILE_NATIVE_CLOSE(int descriptor);
#endif
#ifndef MC_JAVA_FILE_NATIVE_READ
static ptrdiff_t file_native_read(int fd, uint8_t *bytes, size_t n) {
#ifdef _WIN32
    return (ptrdiff_t)_read(fd, bytes, (unsigned int)n);
#else
    return (ptrdiff_t)read(fd, bytes, n);
#endif
}

#define MC_JAVA_FILE_NATIVE_READ file_native_read
#else
extern ptrdiff_t MC_JAVA_FILE_NATIVE_READ(int descriptor, uint8_t *bytes, size_t count);
#endif

mc_java_file_status mc_file_io_result(mc_file_io_status s, const mc_file_io_error *cause,
                                      mc_java_file_error *e) {
    mc_java_file_status mapped =
        s == MC_FILE_IO_OK || s == MC_FILE_IO_CREATION_FALSE ? MC_JAVA_FILE_OK
        : s == MC_FILE_IO_LIMIT                              ? MC_JAVA_FILE_LIMIT
        : s == MC_FILE_IO_ALLOCATION_FAILED                  ? MC_JAVA_FILE_ALLOCATION_FAILED
        : s == MC_FILE_IO_UNSUPPORTED_PATH || s == MC_FILE_IO_UNSUPPORTED_FILENAME_ENCODING
            ? MC_JAVA_FILE_UNSUPPORTED_PROVIDER
        : s == MC_FILE_IO_INVALID_ARGUMENT ? MC_JAVA_FILE_INVALID_ARGUMENT
                                           : MC_JAVA_FILE_IO_ERROR;
    return mc_file_result(e, mapped, cause != NULL && cause->written ? cause->operation : NULL,
                          cause != NULL && cause->written ? cause->system_error : 0);
}

mc_java_file_status mc_file_utf8(const mc_java_file *f, char **out, mc_java_file_error *e) {
    mc_nbt_string p = mc_world_info_name_text(f->state.path);
    if (p.length > (SIZE_MAX - 1u) / 3u)
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File encoding bound", 0);
    size_t bytes = p.length * 3u + 1u;
    if (bytes > f->files->options.max_string_bytes)
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File native encoding allocation bound", 0);
    char *text = malloc(bytes);
    if (text == NULL)
        return mc_file_result(e, MC_JAVA_FILE_ALLOCATION_FAILED, "File native encoding", 0);
    size_t n = 0;
    for (size_t i = 0; i < p.length; ++i) {
        uint32_t c = p.units[i];
        if (c == 0) {
            free(text);
            return mc_file_result(e, MC_JAVA_FILE_IO_ERROR, "File invalid NUL path", EINVAL);
        }
        if (c >= 0xd800u && c <= 0xdbffu && i + 1u < p.length && p.units[i + 1u] >= 0xdc00u &&
            p.units[i + 1u] <= 0xdfffu) {
            c = 0x10000u + ((c - 0xd800u) << 10u) + (p.units[++i] - 0xdc00u);
        } else if (c >= 0xd800u && c <= 0xdfffu) {
            free(text);
            return mc_file_result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER,
                                  "File lone-surrogate native filename provider", 0);
        }
        if (c < 0x80u)
            text[n++] = (char)c;
        else if (c < 0x800u) {
            text[n++] = (char)(0xc0u | (c >> 6u));
            text[n++] = (char)(0x80u | (c & 63u));
        } else if (c < 0x10000u) {
            text[n++] = (char)(0xe0u | (c >> 12u));
            text[n++] = (char)(0x80u | ((c >> 6u) & 63u));
            text[n++] = (char)(0x80u | (c & 63u));
        } else {
            text[n++] = (char)(0xf0u | (c >> 18u));
            text[n++] = (char)(0x80u | ((c >> 12u) & 63u));
            text[n++] = (char)(0x80u | ((c >> 6u) & 63u));
            text[n++] = (char)(0x80u | (c & 63u));
        }
    }
    text[n] = '\0';
    *out = text;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_file_native(const mc_java_file *f, mc_file_native_char **out,
                                   mc_java_file_error *e) {
    char *utf8 = NULL;
    mc_java_file_status s = mc_file_utf8(f, &utf8, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
#ifndef _WIN32
    /* Reuse the existing environment-locale discovery without changing the
     * process C locale. Other filename encodings require their real provider;
     * do not quietly address a different UTF8 byte filename. Constructors and
     * String inspection never perform this native-operation admission. */
    mc_java_reader_charset charset;
    mc_java_reader_status selected = mc_java_reader_default_charset(&charset);
    if (selected != MC_JAVA_READER_OK || charset != MC_JAVA_READER_UTF8) {
        free(utf8);
        return mc_file_result(e,
                              selected == MC_JAVA_READER_ALLOCATION_FAILED
                                  ? MC_JAVA_FILE_ALLOCATION_FAILED
                                  : MC_JAVA_FILE_UNSUPPORTED_PROVIDER,
                              "File native filename charset provider", 0);
    }
#endif
    mc_file_io_error child = {0};
    mc_file_io_status io = mc_file_native_path(utf8, out, &child);
    free(utf8);
    return mc_file_io_result(io, &child, e);
}

static mc_file_io_state state_in(const mc_java_file *f) {
    return (mc_file_io_state){.exists_calls = f->state.exists_calls,
                              .mkdir_calls = f->state.mkdir_calls,
                              .canonicalize_calls = f->state.canonicalize_calls,
                              .last_native_error = f->state.last_native_error,
                              .stop_at_provider_frontier = true};
}

static void state_out(mc_java_file *f, const mc_file_io_state *s) {
    f->state.exists_calls = s->exists_calls;
    f->state.mkdir_calls = s->mkdir_calls;
    f->state.canonicalize_calls = s->canonicalize_calls;
    f->state.last_native_error = s->last_native_error;
}

mc_java_file_status mc_java_file_exists(mc_java_file *f, bool *out, mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File exists output", 0);
    mc_java_file_status s = mc_file_ready(f, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    if (mc_file_invalid(f)) {
        *out = false;
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    }
    f->state.active = true;
    mc_file_native_char *path = NULL;
    s = mc_file_native(f, &path, e);
    if (s == MC_JAVA_FILE_OK) {
        mc_file_io_state c = state_in(f);
        bool value = mc_file_native_exists(path, &c);
        state_out(f, &c);
        if (c.provider_frontier)
            s = mc_file_result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER,
                               "File final attributes provider", c.last_native_error);
        else
            *out = value;
    }
    free(path);
    f->state.active = false;
    return s;
}

mc_java_file_status mc_java_file_mkdirs(mc_java_file *f, bool *out, mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File mkdirs output", 0);
    mc_java_file_status s = mc_file_ready(f, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    if (mc_file_invalid(f)) {
        *out = false;
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    }
    f->state.active = true;
    mc_file_native_char *path = NULL;
    s = mc_file_native(f, &path, e);
    if (s == MC_JAVA_FILE_OK) {
        mc_file_io_state c = state_in(f);
        mc_file_io_error detail = {0};
        bool made = false;
        mc_file_io_status io = mc_file_native_mkdirs(path, 0, &made, &c, &detail);
        state_out(f, &c);
        s = mc_file_io_result(io, &detail, e);
        if (s == MC_JAVA_FILE_OK && c.provider_frontier)
            s = mc_file_result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER,
                               "File final attributes/canonical provider", c.last_native_error);
        if (s == MC_JAVA_FILE_OK)
            *out = made;
    }
    free(path);
    f->state.active = false;
    return s;
}

static mc_java_file_status file_attribute(mc_java_file *f, mc_file_attribute_kind kind,
                                          int64_t *out, mc_java_file_error *e) {
    mc_java_file_status s = mc_file_ready(f, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    if (mc_file_invalid(f)) {
        *out = 0;
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    }
    f->state.active = true;
    mc_file_native_char *path = NULL;
    s = mc_file_native(f, &path, e);
    int64_t value = 0;
    if (s == MC_JAVA_FILE_OK) {
        mc_file_io_state state = state_in(f);
        mc_file_io_error child = {0};
        mc_file_io_status io = mc_file_native_attribute(path, kind, &value, &state, &child);
        state_out(f, &state);
        s = mc_file_io_result(io, &child, e);
    }
    free(path);
    f->state.active = false;
    if (s == MC_JAVA_FILE_OK)
        *out = value;
    return s;
}

mc_java_file_status mc_java_file_is_directory(mc_java_file *f, bool *out, mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File directory output", 0);
    int64_t value = 0;
    mc_java_file_status s = file_attribute(f, MC_FILE_ATTRIBUTE_DIRECTORY, &value, e);
    if (s == MC_JAVA_FILE_OK)
        *out = value != 0;
    return s;
}

mc_java_file_status mc_java_file_is_file(mc_java_file *f, bool *out, mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File regular output", 0);
    int64_t value = 0;
    mc_java_file_status s = file_attribute(f, MC_FILE_ATTRIBUTE_REGULAR, &value, e);
    if (s == MC_JAVA_FILE_OK)
        *out = value != 0;
    return s;
}

mc_java_file_status mc_java_file_length(mc_java_file *f, int64_t *out, mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File length output", 0);
    return file_attribute(f, MC_FILE_ATTRIBUTE_LENGTH, out, e);
}

mc_java_file_status mc_java_file_last_modified(mc_java_file *f, int64_t *out,
                                               mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File lastModified output", 0);
    return file_attribute(f, MC_FILE_ATTRIBUTE_LAST_MODIFIED, out, e);
}

mc_java_file_status mc_java_file_native_path_copy(mc_java_file *f, mc_file_native_char **out,
                                                  mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File native-name output", 0);
    mc_java_file_status s = mc_file_ready(f, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    f->state.active = true;
    s = mc_file_native(f, out, e);
    f->state.active = false;
    return s;
}

mc_java_file_status mc_java_file_stream_allocate(mc_java_files *files, mc_java_file_stream **out,
                                                 mc_java_file_error *e) {
    if (files == NULL || out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "FileInputStream native allocation",
                              0);
    mc_java_file_stream *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return mc_file_result(e, MC_JAVA_FILE_ALLOCATION_FAILED,
                              "FileInputStream native allocation", 0);
    mc_java_file_status status = mc_java_files_retain(files, e);
    if (status != MC_JAVA_FILE_OK) {
        free(s);
        return status;
    }
    s->files = files;
    s->holds = 1;
    s->descriptor = -1;
    s->phase = MC_JAVA_FILE_RAW;
    *out = s;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_file_stream_construct(mc_java_file_stream *s, mc_java_file *file,
                                                  mc_java_file_error *e) {
    if (s == NULL || s->phase != MC_JAVA_FILE_RAW || s->active)
        return mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "FileInputStream constructor receiver",
                              0);
    s->active = true;
    s->phase = MC_JAVA_FILE_CONSTRUCTING;
    mc_java_file_status status;
    if (file == NULL) {
        status = mc_file_result(e, MC_JAVA_FILE_SOURCE_NULL_FILE, "FileInputStream null File", 0);
        goto failed;
    }
    if (file->files != s->files) {
        status = mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "FileInputStream File namespace", 0);
        goto failed;
    }
    status = mc_file_ready(file, e);
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    status = mc_java_file_retain(file, e);
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    s->file = file;
    if (mc_file_invalid(file)) {
        status =
            mc_file_result(e, MC_JAVA_FILE_IO_ERROR, "FileInputStream invalid File path", EINVAL);
        goto failed;
    }
    mc_file_native_char *path = NULL;
    status = mc_file_native(file, &path, e);
    if (status != MC_JAVA_FILE_OK)
        goto failed;
#ifdef _WIN32
    int descriptor = -1;
    errno_t opened = _wsopen_s(&descriptor, path, _O_RDONLY | _O_BINARY, _SH_DENYNO, 0);
    s->descriptor = descriptor;
    int open_error = opened != 0 ? (int)opened : errno;
#else
    do {
        s->descriptor = open(path, O_RDONLY);
    } while (s->descriptor < 0 && errno == EINTR);
    int open_error = errno;
#endif
    free(path);
    if (s->descriptor < 0) {
        status = mc_file_result(
            e, open_error == ENOMEM ? MC_JAVA_FILE_ALLOCATION_FAILED : MC_JAVA_FILE_IO_ERROR,
            "FileInputStream descriptor open", open_error);
        goto failed;
    }
    s->armed = true;
#ifdef _WIN32
    struct _stat64 st;
    int stat_result = _fstat64(s->descriptor, &st);
    bool directory = stat_result == 0 && (st.st_mode & _S_IFMT) == _S_IFDIR;
    bool regular = stat_result == 0 && (st.st_mode & _S_IFMT) == _S_IFREG;
#else
    struct stat st;
    int stat_result = fstat(s->descriptor, &st);
    bool directory = stat_result == 0 && S_ISDIR(st.st_mode);
    bool regular = stat_result == 0 && S_ISREG(st.st_mode);
#endif
    if (directory) {
        int fd = s->descriptor;
        s->descriptor = -1;
        s->armed = false;
        int rc = MC_JAVA_FILE_NATIVE_CLOSE(fd);
        int code = errno;
        status = mc_file_result(e, MC_JAVA_FILE_IO_ERROR,
                                rc != 0 ? "FileInputStream rejected directory close"
                                        : "FileInputStream directory",
                                rc != 0 ? code : EISDIR);
        goto failed;
    }
    if (!regular) {
        status = mc_file_result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER,
                                "FileInputStream nonregular native provider",
                                stat_result != 0 ? errno : 0);
        goto failed;
    }
    s->phase = MC_JAVA_FILE_READY;
    s->active = false;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
failed:
    s->phase = MC_JAVA_FILE_FAILED;
    s->active = false;
    return status;
}

mc_java_file_status mc_java_file_stream_retain(mc_java_file_stream *s, mc_java_file_error *e) {
    if (s == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "FileInputStream retain", 0);
    if (s->active || s->phase == MC_JAVA_FILE_RETIRING)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "FileInputStream active/terminal retain", 0);
    if (s->holds == SIZE_MAX)
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "FileInputStream hold bound", 0);
    ++s->holds;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

static mc_java_file_status stream_ready(mc_java_file_stream *s, mc_java_file_error *e) {
    if (s == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "FileInputStream null receiver", 0);
    if (s->active)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "FileInputStream active operation", 0);
    if (s->phase != MC_JAVA_FILE_READY)
        return mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "FileInputStream nonready operation", 0);
    return MC_JAVA_FILE_OK;
}

static mc_java_file_status stream_read(mc_java_file_stream *s, uint8_t *bytes, size_t n,
                                       mc_java_file_read *out, mc_java_file_error *e, bool single) {
    if (out == NULL || (bytes == NULL && n != 0) || n > INT32_MAX)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "FileInputStream read arguments",
                              0);
    mc_java_file_status status = stream_ready(s, e);
    if (status != MC_JAVA_FILE_OK)
        return status;
    if (n == 0) {
        *out = (mc_java_file_read){0, false};
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    }
    if (!s->armed || s->source_close_reached)
        return mc_file_result(e, MC_JAVA_FILE_IO_ERROR, "FileInputStream closed read", EBADF);
    s->active = true;
    ++s->read_calls;
    if (single)
        ++s->byte_read_calls;
    ptrdiff_t count;
    do {
        ++s->native_read_calls;
        count = MC_JAVA_FILE_NATIVE_READ(s->descriptor, bytes, n);
    } while (count < 0 && errno == EINTR);
    int code = errno;
    s->active = false;
    if (count < 0)
        return mc_file_result(e, MC_JAVA_FILE_IO_ERROR, "FileInputStream descriptor read", code);
    if ((size_t)count > n)
        return mc_file_result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER,
                              "FileInputStream out-of-range native read", 0);
    s->native_bytes_read += (uint64_t)count;
    *out = (mc_java_file_read){(size_t)count, count == 0};
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_file_stream_read(mc_java_file_stream *s, uint8_t *bytes, size_t n,
                                             mc_java_file_read *out, mc_java_file_error *e) {
    return stream_read(s, bytes, n, out, e, false);
}

mc_java_file_status mc_java_file_stream_read_byte(mc_java_file_stream *s, int *out,
                                                  mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "FileInputStream byte output", 0);
    uint8_t b;
    mc_java_file_read read;
    mc_java_file_status status = stream_read(s, &b, 1, &read, e, true);
    if (status == MC_JAVA_FILE_OK)
        *out = read.eof ? -1 : (int)b;
    return status;
}

mc_java_file_status mc_java_file_stream_available(mc_java_file_stream *s, size_t *out,
                                                  mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "FileInputStream available output",
                              0);
    mc_java_file_status status = stream_ready(s, e);
    if (status != MC_JAVA_FILE_OK)
        return status;
    if (!s->armed || s->source_close_reached)
        return mc_file_result(e, MC_JAVA_FILE_IO_ERROR, "FileInputStream closed available", EBADF);
    s->active = true;
#ifdef _WIN32
    struct _stat64 st;
    int rc = _fstat64(s->descriptor, &st);
    __int64 position = _lseeki64(s->descriptor, 0, SEEK_CUR);
#else
    struct stat st;
    int rc = fstat(s->descriptor, &st);
    off_t position = lseek(s->descriptor, 0, SEEK_CUR);
#endif
    int code = errno;
    s->active = false;
    if (rc != 0 || position < 0)
        return mc_file_result(e, MC_JAVA_FILE_IO_ERROR, "FileInputStream descriptor available",
                              code);
    uint64_t remaining = st.st_size > position ? (uint64_t)(st.st_size - position) : 0;
    *out = remaining > INT32_MAX ? (size_t)INT32_MAX : (size_t)remaining;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_file_stream_source_close(mc_java_file_stream *s,
                                                     mc_java_file_error *e) {
    if (s == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "FileInputStream source close", 0);
    if (s->active)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "FileInputStream active close", 0);
    if (s->phase == MC_JAVA_FILE_RETIRING)
        return mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "FileInputStream terminal source close",
                              0);
    if (s->source_close_reached)
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    if (!s->armed)
        return mc_file_result(e, MC_JAVA_FILE_STATE_ERROR,
                              "FileInputStream unconstructed source close", 0);
    s->active = true;
    s->source_close_reached = true;
    int fd = s->descriptor;
    s->descriptor = -1;
    s->armed = false;
    int rc = MC_JAVA_FILE_NATIVE_CLOSE(fd);
    int code = errno;
    s->source_close_succeeded = rc == 0;
    s->active = false;
    return mc_file_result(e, rc == 0 ? MC_JAVA_FILE_OK : MC_JAVA_FILE_IO_ERROR,
                          rc == 0 ? NULL : "FileInputStream source close", rc == 0 ? 0 : code);
}

mc_java_file_status mc_java_file_stream_release(mc_java_file_stream **slot, mc_java_file_error *e) {
    if (slot == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "FileInputStream retirement slot",
                              0);
    mc_java_file_stream *s = *slot;
    if (s == NULL)
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    if (s->active)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "FileInputStream active retirement", 0);
    if (s->holds > 1) {
        --s->holds;
        *slot = NULL;
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    }
    if (s->files->active || s->files->retiring)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "FileInputStream namespace retirement", 0);
    s->phase = MC_JAVA_FILE_RETIRING;
    s->active = true;
    if (s->armed) {
        int fd = s->descriptor;
        s->descriptor = -1;
        s->armed = false;
        int rc = MC_JAVA_FILE_NATIVE_CLOSE(fd);
        int code = errno;
        if (rc != 0) {
            s->active = false;
            return mc_file_result(e, MC_JAVA_FILE_IO_ERROR,
                                  "FileInputStream native retirement close", code);
        }
    }
    mc_java_file_status status = mc_java_file_release(&s->file, e);
    if (status != MC_JAVA_FILE_OK) {
        s->active = false;
        return status;
    }
    mc_java_files *files = s->files;
    *slot = NULL;
    free(s);
    return mc_java_files_release(&files, e);
}

mc_java_file_status mc_java_file_stream_read_state(const mc_java_file_stream *s,
                                                   mc_java_file_stream_state *out,
                                                   mc_java_file_error *e) {
    if (s == NULL || out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "FileInputStream state", 0);
    *out = (mc_java_file_stream_state){s->phase,
                                       s->active,
                                       s->armed,
                                       s->source_close_reached,
                                       s->source_close_succeeded,
                                       s->holds,
                                       s->read_calls,
                                       s->native_read_calls,
                                       s->byte_read_calls,
                                       s->native_bytes_read};
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}
