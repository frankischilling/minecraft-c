#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "files.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <direct.h>
#include <io.h>
#include <wchar.h>
#include <windows.h>
typedef wchar_t mc_ws_native_char;
typedef struct _stat64 mc_ws_native_stat;
#else
#include <unistd.h>
typedef char mc_ws_native_char;
typedef struct stat mc_ws_native_stat;
#endif

typedef struct mc_ws_native_path {
    mc_ws_native_char *value;
    size_t length;
} mc_ws_native_path;

static mc_world_storage_status mc_ws_error(mc_world_storage_error *error,
                                           mc_world_storage_status status, const char *operation,
                                           uint32_t system_error) {
    if (error != NULL) {
        *error = (mc_world_storage_error){status, operation, system_error};
    }
    return status;
}

static mc_world_storage_status mc_ws_success(mc_world_storage_error *error) {
    return mc_ws_error(error, MC_WORLD_STORAGE_OK, NULL, 0);
}

static mc_world_storage_status mc_ws_errno(mc_world_storage_error *error, const char *operation,
                                           int code) {
    if (code == 0) {
        code = EIO;
    }
    return mc_ws_error(error, code == ENOENT ? MC_WORLD_STORAGE_MISSING : MC_WORLD_STORAGE_IO_ERROR,
                       operation, (uint32_t)code);
}

static mc_world_storage_status mc_ws_io_errno(mc_world_storage_error *error, const char *operation,
                                              int code) {
    return mc_ws_error(error, MC_WORLD_STORAGE_IO_ERROR, operation,
                       (uint32_t)(code == 0 ? EIO : code));
}

static bool mc_ws_utf8(const unsigned char *text, size_t length) {
    for (size_t i = 0; i < length;) {
        unsigned int first = text[i++];
        if (first < 128u) {
            continue;
        }
        size_t following;
        unsigned int lower = 128u, upper = 191u;
        if (first >= 194u && first <= 223u) {
            following = 1;
        } else if (first >= 224u && first <= 239u) {
            following = 2;
            if (first == 224u)
                lower = 160u;
            if (first == 237u)
                upper = 159u;
        } else if (first >= 240u && first <= 244u) {
            following = 3;
            if (first == 240u)
                lower = 144u;
            if (first == 244u)
                upper = 143u;
        } else {
            return false;
        }
        if (following > length - i || text[i] < lower || text[i] > upper) {
            return false;
        }
        ++i;
        for (size_t j = 1; j < following; ++j, ++i) {
            if (text[i] < 128u || text[i] > 191u) {
                return false;
            }
        }
    }
    return true;
}

static mc_world_storage_status mc_ws_validate_path(const char *path, size_t *length,
                                                   mc_world_storage_error *error) {
    if (path == NULL || path[0] == '\0') {
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "path.validate", 0);
    }
    size_t count = 0;
    while (count <= MC_WS_PATH_MAX_BYTES && path[count] != '\0') {
        ++count;
    }
    if (count > MC_WS_PATH_MAX_BYTES) {
        return mc_ws_error(error, MC_WORLD_STORAGE_LIMIT, "path.length", 0);
    }
    if (!mc_ws_utf8((const unsigned char *)path, count)) {
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "path.utf8", 0);
    }
    *length = count;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status mc_ws_native(const char *path, mc_ws_native_path *out,
                                            mc_world_storage_error *error) {
    size_t bytes = 0;
    mc_world_storage_status status = mc_ws_validate_path(path, &bytes, error);
    if (status != MC_WORLD_STORAGE_OK) {
        return status;
    }
#ifdef _WIN32
    int units = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)bytes, NULL, 0);
    if (units == 0) {
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "path.convert.win32",
                           (uint32_t)GetLastError());
    }
    if ((unsigned int)units >= 32768u) {
        return mc_ws_error(error, MC_WORLD_STORAGE_LIMIT, "path.utf16_length", 0);
    }
    wchar_t *value = malloc(((size_t)units + 1u) * sizeof(*value));
    if (value == NULL) {
        return mc_ws_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "path.allocate", 0);
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)bytes, value, units) !=
        units) {
        DWORD code = GetLastError();
        free(value);
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "path.convert.win32",
                           (uint32_t)code);
    }
    value[units] = L'\0';
    out->value = value;
    out->length = (size_t)units;
#else
    char *value = malloc(bytes + 1u);
    if (value == NULL) {
        return mc_ws_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "path.allocate", 0);
    }
    memcpy(value, path, bytes + 1u);
    out->value = value;
    out->length = bytes;
#endif
    return MC_WORLD_STORAGE_OK;
}

static bool mc_ws_separator(mc_ws_native_char value) {
#ifdef _WIN32
    return value == L'/' || value == L'\\';
#else
    return value == '/';
#endif
}

static int mc_ws_stat(const mc_ws_native_char *path, mc_ws_native_stat *info) {
#ifdef _WIN32
    return _wstat64(path, info);
#else
    return stat(path, info);
#endif
}

static bool mc_ws_is_directory(const mc_ws_native_stat *info) {
#ifdef _WIN32
    return (info->st_mode & _S_IFMT) == _S_IFDIR;
#else
    return S_ISDIR(info->st_mode);
#endif
}

static bool mc_ws_is_regular(const mc_ws_native_stat *info) {
#ifdef _WIN32
    return (info->st_mode & _S_IFMT) == _S_IFREG;
#else
    return S_ISREG(info->st_mode);
#endif
}

static mc_world_storage_status mc_ws_regular(const mc_ws_native_char *path, bool missing_ok,
                                             mc_world_storage_error *error) {
    mc_ws_native_stat info;
    if (mc_ws_stat(path, &info) != 0) {
        int code = errno;
        if (missing_ok && code == ENOENT) {
            return MC_WORLD_STORAGE_OK;
        }
        return mc_ws_errno(error, "file.stat.errno", code);
    }
    if (!mc_ws_is_regular(&info)) {
        return mc_ws_errno(error, "file.not_regular.errno", EINVAL);
    }
    return MC_WORLD_STORAGE_OK;
}

static int mc_ws_unlink(const mc_ws_native_char *path) {
#ifdef _WIN32
    return _wremove(path);
#else
    return unlink(path);
#endif
}

mc_world_storage_status mc_ws_path_join(const char *root, const char *leaf, char **out,
                                        mc_world_storage_error *error) {
    if (out == NULL) {
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "join.output", 0);
    }
    size_t root_length = 0, leaf_length = 0;
    mc_world_storage_status status = mc_ws_validate_path(root, &root_length, error);
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_ws_validate_path(leaf, &leaf_length, error);
    }
    if (status != MC_WORLD_STORAGE_OK) {
        return status;
    }
#ifdef _WIN32
    bool absolute = leaf[0] == '/' || leaf[0] == '\\' || (leaf_length >= 2 && leaf[1] == ':');
    bool has_separator = root[root_length - 1u] == '/' || root[root_length - 1u] == '\\';
    const char separator = '\\';
#else
    bool absolute = leaf[0] == '/';
    bool has_separator = root[root_length - 1u] == '/';
    const char separator = '/';
#endif
    if (absolute) {
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "join.relative_leaf", 0);
    }
    size_t extra = has_separator ? 0u : 1u;
    if (root_length > MC_WS_PATH_MAX_BYTES - extra ||
        leaf_length > MC_WS_PATH_MAX_BYTES - extra - root_length) {
        return mc_ws_error(error, MC_WORLD_STORAGE_LIMIT, "join.length", 0);
    }
    size_t length = root_length + extra + leaf_length;
    char *result = malloc(length + 1u);
    if (result == NULL) {
        return mc_ws_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "join.allocate", 0);
    }
    memcpy(result, root, root_length);
    if (extra != 0) {
        result[root_length] = separator;
    }
    memcpy(result + root_length + extra, leaf, leaf_length + 1u);
    *out = result;
    return mc_ws_success(error);
}

static mc_world_storage_status mc_ws_directory(const mc_ws_native_char *path, bool create,
                                               mc_world_storage_error *error) {
    if (create) {
#ifdef _WIN32
        int result = _wmkdir(path);
#else
        int result = mkdir(path, 0777);
#endif
        if (result == 0) {
            return MC_WORLD_STORAGE_OK;
        }
        int code = errno;
        if (code != EEXIST) {
            return mc_ws_errno(error, "mkdirs.mkdir.errno", code);
        }
    }
    mc_ws_native_stat info;
    if (mc_ws_stat(path, &info) != 0) {
        return mc_ws_errno(error, "mkdirs.stat.errno", errno);
    }
    if (!mc_ws_is_directory(&info)) {
        return mc_ws_errno(error, "mkdirs.not_directory.errno", ENOTDIR);
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status mc_ws_root_length(const mc_ws_native_path *path, size_t *length,
                                                 mc_world_storage_error *error) {
    *length = 0;
#ifdef _WIN32
    if (path->length >= 2 && path->value[1] == L':') {
        bool drive = (path->value[0] >= L'A' && path->value[0] <= L'Z') ||
                     (path->value[0] >= L'a' && path->value[0] <= L'z');
        if (!drive || path->length < 3 || !mc_ws_separator(path->value[2])) {
            return mc_ws_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "mkdirs.drive_relative",
                               0);
        }
        *length = 3;
    } else if (path->length >= 2 && mc_ws_separator(path->value[0]) &&
               mc_ws_separator(path->value[1])) {
        if (path->length >= 4 && (path->value[2] == L'?' || path->value[2] == L'.') &&
            mc_ws_separator(path->value[3])) {
            return mc_ws_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "mkdirs.namespace", 0);
        }
        size_t server = 2;
        while (server < path->length && !mc_ws_separator(path->value[server])) {
            ++server;
        }
        size_t share = server + 1u;
        if (server == 2 || share >= path->length || mc_ws_separator(path->value[share])) {
            return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "mkdirs.unc_root", 0);
        }
        while (share < path->length && !mc_ws_separator(path->value[share])) {
            ++share;
        }
        *length = share;
    } else if (mc_ws_separator(path->value[0])) {
        *length = 1;
    }
#else
    (void)error;
    if (mc_ws_separator(path->value[0])) {
        *length = 1;
    }
#endif
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_ws_mkdirs(const char *path, mc_world_storage_error *error) {
    mc_ws_native_path native = {0};
    mc_world_storage_status status = mc_ws_native(path, &native, error);
    if (status != MC_WORLD_STORAGE_OK) {
        return status;
    }
    size_t root_length = 0;
    status = mc_ws_root_length(&native, &root_length, error);
    if (status == MC_WORLD_STORAGE_OK && root_length != 0) {
        mc_ws_native_char saved = native.value[root_length];
        native.value[root_length] = 0;
        status = mc_ws_directory(native.value, false, error);
        native.value[root_length] = saved;
    }
    size_t start = root_length;
    if (start < native.length && mc_ws_separator(native.value[start])) {
        ++start;
    }
    for (size_t i = start;
         status == MC_WORLD_STORAGE_OK && root_length != native.length && i <= native.length; ++i) {
        if ((i == native.length || mc_ws_separator(native.value[i])) && i != 0 &&
            !mc_ws_separator(native.value[i - 1u])) {
            mc_ws_native_char saved = native.value[i];
            native.value[i] = 0;
            status = mc_ws_directory(native.value, true, error);
            native.value[i] = saved;
        }
    }
    free(native.value);
    return status == MC_WORLD_STORAGE_OK ? mc_ws_success(error) : status;
}

static mc_world_storage_status mc_ws_open(const mc_ws_native_path *path, bool writing,
                                          bool exclusive, FILE **out,
                                          mc_world_storage_error *error) {
    FILE *file = NULL;
    errno = 0;
#ifdef _WIN32
    int code = (int)_wfopen_s(&file, path->value, writing ? (exclusive ? L"wbx" : L"wb") : L"rb");
#else
    file = fopen(path->value, writing ? (exclusive ? "wbx" : "wb") : "rb");
    int code = file == NULL ? errno : 0;
#endif
    if (code != 0 || file == NULL) {
        return mc_ws_errno(error, writing ? "write.open.errno" : "read.open.errno", code);
    }
    *out = file;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status mc_ws_file_stat(FILE *file, mc_ws_native_stat *info,
                                               mc_world_storage_error *error) {
    errno = 0;
#ifdef _WIN32
    int descriptor = _fileno(file);
    int result = descriptor < 0 ? -1 : _fstat64(descriptor, info);
#else
    int descriptor = fileno(file);
    int result = descriptor < 0 ? -1 : fstat(descriptor, info);
#endif
    if (result != 0) {
        return mc_ws_io_errno(error, "file.fstat.errno", descriptor < 0 ? EBADF : errno);
    }
    if (!mc_ws_is_regular(info) || info->st_size < 0) {
        return mc_ws_errno(error, "file.not_regular.errno", EINVAL);
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status mc_ws_close(FILE *file, mc_world_storage_status status,
                                           const char *operation, mc_world_storage_error *error) {
    errno = 0;
    if (fclose(file) != 0 && status == MC_WORLD_STORAGE_OK) {
        return mc_ws_io_errno(error, operation, errno);
    }
    return status;
}

mc_world_storage_status mc_ws_read_file(const char *path, size_t max_bytes, mc_ws_file_bytes *out,
                                        mc_world_storage_error *error) {
    if (out == NULL || max_bytes > MC_WS_FILE_MAX_BYTES) {
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "read.arguments", 0);
    }
    mc_ws_native_path native = {0};
    mc_world_storage_status status = mc_ws_native(path, &native, error);
    FILE *file = NULL;
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_ws_regular(native.value, false, error);
    }
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_ws_open(&native, false, false, &file, error);
    }
    free(native.value);
    if (status != MC_WORLD_STORAGE_OK) {
        return status;
    }
    mc_ws_native_stat info;
    status = mc_ws_file_stat(file, &info, error);
    if (status == MC_WORLD_STORAGE_OK && (uint64_t)info.st_size > (uint64_t)max_bytes) {
        status = mc_ws_error(error, MC_WORLD_STORAGE_LIMIT, "read.length", 0);
    }
    if (status == MC_WORLD_STORAGE_OK) {
        errno = 0;
        if (fseek(file, 0L, SEEK_END) != 0) {
            status = mc_ws_io_errno(error, "read.seek.errno", errno);
        }
    }
    long measured = 0;
    if (status == MC_WORLD_STORAGE_OK) {
        errno = 0;
        measured = ftell(file);
        if (measured < 0) {
            status = mc_ws_io_errno(error, "read.tell.errno", errno);
        } else if ((uint64_t)measured != (uint64_t)info.st_size) {
            status = mc_ws_io_errno(error, "read.changed.errno", EIO);
        }
    }
    if (status == MC_WORLD_STORAGE_OK) {
        errno = 0;
        if (fseek(file, 0L, SEEK_SET) != 0) {
            status = mc_ws_io_errno(error, "read.seek.errno", errno);
        }
    }
    uint8_t *bytes = NULL;
    size_t length = (size_t)measured;
    if (status == MC_WORLD_STORAGE_OK && length != 0) {
        bytes = malloc(length);
        if (bytes == NULL) {
            status = mc_ws_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "read.allocate", 0);
        } else {
            errno = 0;
            if (fread(bytes, 1, length, file) != length) {
                status = mc_ws_io_errno(error, "read.bytes.errno", ferror(file) ? errno : EIO);
            }
        }
    }
    if (status == MC_WORLD_STORAGE_OK) {
        errno = 0;
        int next = fgetc(file);
        if (next != EOF || ferror(file)) {
            status = mc_ws_io_errno(error, "read.eof.errno", ferror(file) ? errno : EIO);
        }
    }
    if (status == MC_WORLD_STORAGE_OK) {
        mc_ws_native_stat after;
        status = mc_ws_file_stat(file, &after, error);
        if (status == MC_WORLD_STORAGE_OK && after.st_size != info.st_size) {
            status = mc_ws_io_errno(error, "read.changed.errno", EIO);
        }
    }
    status = mc_ws_close(file, status, "read.close.errno", error);
    if (status != MC_WORLD_STORAGE_OK) {
        free(bytes);
        return status;
    }
    *out = (mc_ws_file_bytes){bytes, length};
    return mc_ws_success(error);
}

void mc_ws_free_file_bytes(mc_ws_file_bytes *bytes) {
    if (bytes != NULL) {
        free(bytes->data);
        *bytes = (mc_ws_file_bytes){NULL, 0};
    }
}

mc_world_storage_status mc_ws_read_prefix(const char *path, uint8_t *output, size_t length,
                                          mc_world_storage_error *error) {
    if (output == NULL && length != 0) {
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "prefix.arguments", 0);
    }
    if (length > MC_WS_FILE_MAX_BYTES) {
        return mc_ws_error(error, MC_WORLD_STORAGE_LIMIT, "prefix.length", 0);
    }
    mc_ws_native_path native = {0};
    mc_world_storage_status status = mc_ws_native(path, &native, error);
    FILE *file = NULL;
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_ws_regular(native.value, false, error);
    }
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_ws_open(&native, false, false, &file, error);
    }
    free(native.value);
    if (status != MC_WORLD_STORAGE_OK) {
        return status;
    }
    mc_ws_native_stat info;
    status = mc_ws_file_stat(file, &info, error);
    if (status == MC_WORLD_STORAGE_OK && (uint64_t)info.st_size < (uint64_t)length) {
        status = mc_ws_io_errno(error, "prefix.short.errno", EIO);
    }
    uint8_t *staged = NULL;
    if (status == MC_WORLD_STORAGE_OK && length != 0) {
        staged = malloc(length);
        if (staged == NULL) {
            status = mc_ws_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "prefix.allocate", 0);
        } else {
            errno = 0;
            size_t count = fread(staged, 1, length, file);
            if (count != length || ferror(file)) {
                status = mc_ws_io_errno(error, "prefix.read.errno", ferror(file) ? errno : EIO);
            }
        }
    }
    status = mc_ws_close(file, status, "prefix.close.errno", error);
    if (status == MC_WORLD_STORAGE_OK && length != 0) {
        memcpy(output, staged, length);
    }
    free(staged);
    return status == MC_WORLD_STORAGE_OK ? mc_ws_success(error) : status;
}

mc_world_storage_status mc_ws_write_file(const char *path, const uint8_t *bytes, size_t length,
                                         bool exclusive, mc_world_storage_error *error) {
    if (bytes == NULL && length != 0) {
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "write.arguments", 0);
    }
    if (length > MC_WS_FILE_MAX_BYTES) {
        return mc_ws_error(error, MC_WORLD_STORAGE_LIMIT, "write.length", 0);
    }
    mc_ws_native_path native = {0};
    mc_world_storage_status status = mc_ws_native(path, &native, error);
    FILE *file = NULL;
    if (status == MC_WORLD_STORAGE_OK && !exclusive) {
        status = mc_ws_regular(native.value, true, error);
    }
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_ws_open(&native, true, exclusive, &file, error);
    }
    if (status != MC_WORLD_STORAGE_OK) {
        free(native.value);
        return status;
    }
    mc_ws_native_stat info;
    status = mc_ws_file_stat(file, &info, error);
    if (status == MC_WORLD_STORAGE_OK && length != 0) {
        errno = 0;
        if (fwrite(bytes, 1, length, file) != length) {
            status = mc_ws_io_errno(error, "write.bytes.errno", errno);
        }
    }
    if (status == MC_WORLD_STORAGE_OK) {
        errno = 0;
        if (fflush(file) != 0) {
            status = mc_ws_io_errno(error, "write.flush.errno", errno);
        }
    }
    status = mc_ws_close(file, status, "write.close.errno", error);
    if (status != MC_WORLD_STORAGE_OK && exclusive && mc_ws_unlink(native.value) != 0) {
        status = mc_ws_io_errno(error, "write.cleanup.errno", errno);
    }
    free(native.value);
    return status == MC_WORLD_STORAGE_OK ? mc_ws_success(error) : status;
}

mc_world_storage_status mc_ws_output_open(const char *path, size_t max_bytes, mc_ws_output *out,
                                          mc_world_storage_error *error) {
    if (out == NULL)
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "output.arguments", 0);
    if (max_bytes > MC_WS_FILE_MAX_BYTES)
        return mc_ws_error(error, MC_WORLD_STORAGE_LIMIT, "output.limit", 0);
    mc_ws_native_path native = {0};
    mc_world_storage_status status = mc_ws_native(path, &native, error);
    FILE *file = NULL;
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_ws_regular(native.value, true, error);
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_ws_open(&native, true, false, &file, error);
    free(native.value);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    *out = (mc_ws_output){file, max_bytes};
    return mc_ws_success(error);
}

mc_world_storage_status mc_ws_output_write(mc_ws_output *output, const uint8_t *bytes,
                                           size_t length, mc_world_storage_error *error) {
    if (output == NULL || output->file == NULL || (length != 0 && bytes == NULL))
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "output.arguments", 0);
    if (length > output->remaining)
        return mc_ws_error(error, MC_WORLD_STORAGE_LIMIT, "output.limit", 0);
    errno = 0;
    size_t written = length == 0 ? 0 : fwrite(bytes, 1, length, output->file);
    output->remaining -= written;
    if (written != length)
        return mc_ws_io_errno(error, "output.write.errno", errno);
    return mc_ws_success(error);
}

mc_world_storage_status mc_ws_output_close(mc_ws_output *output, mc_world_storage_error *error) {
    if (output == NULL || output->file == NULL)
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "output.arguments", 0);
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    errno = 0;
    if (fflush(output->file) != 0)
        status = mc_ws_io_errno(error, "output.flush.errno", errno);
    status = mc_ws_close(output->file, status, "output.close.errno", error);
    output->file = NULL;
    output->remaining = 0;
    return status == MC_WORLD_STORAGE_OK ? mc_ws_success(error) : status;
}

mc_world_storage_status mc_ws_replace_file(const char *source, const char *target,
                                           mc_world_storage_error *error) {
    mc_ws_native_path from = {0}, to = {0};
    mc_world_storage_status status = mc_ws_native(source, &from, error);
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_ws_native(target, &to, error);
    }
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_ws_regular(from.value, false, error);
    }
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_ws_regular(to.value, true, error);
    }
    if (status == MC_WORLD_STORAGE_OK) {
#ifdef _WIN32
        if (!MoveFileExW(from.value, to.value, MOVEFILE_REPLACE_EXISTING)) {
            DWORD code = GetLastError();
            status = mc_ws_error(error,
                                 code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND
                                     ? MC_WORLD_STORAGE_MISSING
                                     : MC_WORLD_STORAGE_IO_ERROR,
                                 "replace.move.win32", (uint32_t)code);
        }
#else
        if (rename(from.value, to.value) != 0) {
            status = mc_ws_errno(error, "replace.rename.errno", errno);
        }
#endif
    }
    free(from.value);
    free(to.value);
    return status == MC_WORLD_STORAGE_OK ? mc_ws_success(error) : status;
}

mc_world_storage_status mc_ws_remove_file(const char *path, bool missing_ok,
                                          mc_world_storage_error *error) {
    mc_ws_native_path native = {0};
    mc_world_storage_status status = mc_ws_native(path, &native, error);
    if (status == MC_WORLD_STORAGE_OK && mc_ws_unlink(native.value) != 0) {
        int code = errno;
        if (!missing_ok || code != ENOENT) {
            status = mc_ws_errno(error, "remove.unlink.errno", code);
        }
    }
    free(native.value);
    return status == MC_WORLD_STORAGE_OK ? mc_ws_success(error) : status;
}

mc_world_storage_status mc_ws_path_exists(const char *path, bool *out,
                                          mc_world_storage_error *error) {
    if (out == NULL) {
        return mc_ws_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "exists.output", 0);
    }
    mc_ws_native_path native = {0};
    mc_world_storage_status status = mc_ws_native(path, &native, error);
    bool exists = false;
    if (status == MC_WORLD_STORAGE_OK) {
        mc_ws_native_stat info;
        if (mc_ws_stat(native.value, &info) == 0) {
            exists = true;
        } else if (errno != ENOENT) {
            status = mc_ws_errno(error, "exists.stat.errno", errno);
        }
    }
    free(native.value);
    if (status == MC_WORLD_STORAGE_OK) {
        *out = exists;
        return mc_ws_success(error);
    }
    return status;
}
