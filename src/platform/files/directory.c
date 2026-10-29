#ifndef _WIN32
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#endif
#include "directory.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wchar.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
/* Aliases keep the extracted operation bodies unchanged; the old public
 * GameDirectory status and OS-discovery implementation remain in its module. */
typedef mc_file_io_status directory_status;
typedef mc_file_io_error directory_error;
typedef mc_file_io_state directory_state;
typedef mc_file_native_char native_char;
#define MC_CLIENT_GAME_DIRECTORY_OK MC_FILE_IO_OK
#define MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT MC_FILE_IO_INVALID_ARGUMENT
#define MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED MC_FILE_IO_ALLOCATION_FAILED
#define MC_CLIENT_GAME_DIRECTORY_LIMIT MC_FILE_IO_LIMIT
#define MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING                                     \
    MC_FILE_IO_UNSUPPORTED_FILENAME_ENCODING
#define MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH MC_FILE_IO_UNSUPPORTED_PATH
#define MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR MC_FILE_IO_NATIVE_ERROR
#define MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED MC_FILE_IO_CREATION_FALSE
#define MC_CLIENT_GAME_DIRECTORY_MAX_PATH_BYTES 1048576u
#define MC_CLIENT_GAME_DIRECTORY_MAX_PARENT_CALLS 1024u
#define native_path mc_file_native_path
#define exists_native mc_file_native_exists
#define mkdirs_native mc_file_native_mkdirs
#define mkdir_native mc_file_native_mkdir
#define wide_copy mc_file_native_wide_copy

static directory_status result(directory_error *e, directory_status s, int code, const char *op) {
    if (e != NULL)
        *e = (directory_error){s, code, op, true};
    return s;
}

static bool path_separator(uint16_t unit) {
#ifdef _WIN32
    return unit == '/' || unit == '\\';
#else
    return unit == '/';
#endif
}
#ifdef _WIN32
static bool drive_letter(uint16_t unit) {
    return (unit >= 'A' && unit <= 'Z') || (unit >= 'a' && unit <= 'z');
}
#endif
#define MC_FILE_UNIT char
#define MC_FILE_CODE_UNIT(value) ((uint16_t)(unsigned char)(value))
#define MC_FILE_NORMALIZE normalize_bytes_into
#include "normalize.inc"
#undef MC_FILE_CODE_UNIT
#undef MC_FILE_UNIT
#undef MC_FILE_NORMALIZE
#define MC_FILE_UNIT uint16_t
#define MC_FILE_CODE_UNIT(value) (value)
#define MC_FILE_NORMALIZE normalize_units_into
#include "normalize.inc"
#undef MC_FILE_CODE_UNIT
#undef MC_FILE_UNIT
#undef MC_FILE_NORMALIZE

size_t mc_file_normalize_bytes(const char *in, size_t n, char *out) {
    return normalize_bytes_into(in, n, out);
}

size_t mc_file_normalize_units(const uint16_t *in, size_t n, uint16_t *out) {
    return normalize_units_into(in, n, out);
}

#ifdef _WIN32
/* Preserve the legacy bool primitive while recording the additional Source
 * File provider boundary. Legacy GameDirectory ignores this new observation. */
static DWORD observed_attributes(const wchar_t *path, directory_state *state) {
    DWORD value = GetFileAttributesW(path);
    if ((value != INVALID_FILE_ATTRIBUTES && (value & FILE_ATTRIBUTE_REPARSE_POINT) != 0) ||
        (value == INVALID_FILE_ATTRIBUTES &&
         (GetLastError() == ERROR_SHARING_VIOLATION || GetLastError() == ERROR_ACCESS_DENIED)))
        state->provider_frontier = true;
    return value;
}

#define GetFileAttributesW(path) observed_attributes(path, state)

directory_status wide_copy(const wchar_t *text, wchar_t **out, directory_error *error) {
    const size_t length = wcslen(text);
    if (length > MC_CLIENT_GAME_DIRECTORY_MAX_PATH_BYTES)
        return result(error, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0, "game directory UTF16 length");
    wchar_t *copy = malloc((length + 1u) * sizeof(*copy));
    if (copy == NULL)
        return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                      "game directory UTF16 allocation");
    memcpy(copy, text, (length + 1u) * sizeof(*copy));
    *out = copy;
    return MC_CLIENT_GAME_DIRECTORY_OK;
}

directory_status native_path(const char *path, wchar_t **out, directory_error *error) {
    const size_t length = strlen(path);
    if (length == 0u)
        return wide_copy(L"", out, error);
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, NULL, 0);
    if (count <= 0)
        return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING,
                      (int)GetLastError(), "game directory UTF8 to UTF16");
    /* Extended-length native File conversion is not implemented by this slice. */
    if (count >= MAX_PATH - 12)
        return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, 0,
                      "game directory extended Windows path");
    wchar_t *native = malloc(((size_t)count + 1u) * sizeof(*native));
    if (native == NULL)
        return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                      "game directory native path allocation");
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, native, count) !=
        count) {
        const int failure = (int)GetLastError();
        free(native);
        return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING, failure,
                      "game directory UTF8 conversion");
    }
    native[count] = L'\0';
    *out = native;
    return MC_CLIENT_GAME_DIRECTORY_OK;
}

bool exists_native(const wchar_t *path, directory_state *state) {
    ++state->exists_calls;
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
        return true;
    state->last_native_error = (int)GetLastError();
    return false;
}

bool mkdir_native(const wchar_t *path, directory_state *state) {
    ++state->mkdir_calls;
    if (CreateDirectoryW(path, NULL))
        return true;
    state->last_native_error = (int)GetLastError();
    return false;
}

static bool tolerated_find_error(DWORD code) {
    return code == ERROR_FILE_NOT_FOUND || code == ERROR_DIRECTORY ||
           code == ERROR_PATH_NOT_FOUND || code == ERROR_BAD_NETPATH ||
           code == ERROR_BAD_NET_NAME || code == ERROR_ACCESS_DENIED ||
           code == ERROR_NETWORK_UNREACHABLE || code == ERROR_NETWORK_ACCESS_DENIED ||
           code == ERROR_NO_MORE_FILES;
}

static directory_status canonical_native(const wchar_t *input, wchar_t **out,
                                         directory_state *state, directory_error *error) {
    ++state->canonicalize_calls;
    /* These native forms need source-specific canonicalization still. */
    if (wcspbrk(input, L"*?") != NULL)
        return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, 0,
                      "game directory wildcard canonicalization");
    wchar_t full[MAX_PATH];
    errno = 0;
    if (_wfullpath(full, input, MAX_PATH) == NULL) {
        state->last_native_error = errno;
        return MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED;
    }
    const size_t length = wcslen(full);
    if (length >= MAX_PATH - 12u)
        return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, 0,
                      "game directory extended canonical path");
    for (size_t i = 0; i < length; ++i) {
        if (full[i] == L'.' && (i + 1u == length || full[i + 1u] == L'\\'))
            return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, 0,
                          "game directory trailing-dot canonicalization");
    }
    wchar_t normalized[MAX_PATH];
    size_t used = 0, next;
    if (length >= 3u && full[1] == L':' && full[2] == L'\\') {
        normalized[used++] =
            full[0] >= L'a' && full[0] <= L'z' ? (wchar_t)(full[0] - L'a' + L'A') : full[0];
        normalized[used++] = L':';
        next = 2u;
    } else if (length >= 2u && full[0] == L'\\' && full[1] == L'\\') {
        next = 2u;
        while (next < length && full[next] != L'\\')
            ++next;
        if (next == length)
            return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, 0,
                          "game directory incomplete UNC canonicalization");
        ++next;
        while (next < length && full[next] != L'\\')
            ++next;
        memcpy(normalized, full, next * sizeof(*full));
        used = next;
    } else {
        return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, 0,
                      "game directory canonical prefix");
    }
    while (next < length) {
        size_t end = next + 1u;
        while (end < length && full[end] != L'\\')
            ++end;
        const wchar_t saved = full[end];
        full[end] = L'\0';
        WIN32_FIND_DATAW found;
        HANDLE search = FindFirstFileW(full, &found);
        const DWORD failure = search == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
        full[end] = saved;
        if (search == INVALID_HANDLE_VALUE) {
            state->last_native_error = (int)failure;
            if (!tolerated_find_error(failure))
                return MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED;
            memcpy(normalized + used, full + next, (length - next) * sizeof(*full));
            used += length - next;
            break;
        }
        if (!FindClose(search))
            return result(error, MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR, (int)GetLastError(),
                          "game directory search close");
        const size_t component = wcslen(found.cFileName);
        if (used + component + 1u >= MAX_PATH)
            return result(error, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0,
                          "game directory canonical component length");
        normalized[used++] = L'\\';
        memcpy(normalized + used, found.cFileName, component * sizeof(*full));
        used += component;
        next = end;
    }
    normalized[used] = L'\0';
    return wide_copy(normalized, out, error);
}

static native_char *parent_native(native_char *path) {
    const size_t length = wcslen(path);
    wchar_t *separator = wcsrchr(path, L'\\');
    if (separator == NULL || (length == 3u && path[1] == L':'))
        return NULL;
    if (separator == path + 2 && path[1] == L':') {
        path[3] = L'\0';
        return path;
    }
    if (path[0] == L'\\' && path[1] == L'\\') {
        wchar_t *server = wcschr(path + 2, L'\\');
        if (server == NULL || separator == server)
            return NULL;
    }
    *separator = L'\0';
    return path;
}

#else
directory_status native_path(const char *path, char **out, directory_error *error) {
    const size_t length = strlen(path);
    char *copy = malloc(length + 1u);
    if (copy == NULL)
        return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                      "game directory native path allocation");
    memcpy(copy, path, length + 1u);
    *out = copy;
    return MC_CLIENT_GAME_DIRECTORY_OK;
}

#ifndef MC_FILE_NATIVE_STAT
#define MC_FILE_NATIVE_STAT stat
#else
extern int MC_FILE_NATIVE_STAT(const char *path, struct stat *information);
#endif
static bool file_stat(const char *path, struct stat *information, directory_state *state) {
    if (MC_FILE_NATIVE_STAT(path, information) == 0)
        return true;
    state->last_native_error = errno;
    return false;
}

bool exists_native(const char *path, directory_state *state) {
    struct stat information;
    ++state->exists_calls;
    return file_stat(path, &information, state);
}

bool mkdir_native(const char *path, directory_state *state) {
    ++state->mkdir_calls;
    if (mkdir(path, 0777) == 0)
        return true;
    state->last_native_error = errno;
    return false;
}

static void collapse_absolute(char *path) {
    size_t read_at = 1u, used = 1u;
    while (path[read_at] != '\0') {
        while (path[read_at] == '/')
            ++read_at;
        const size_t start = read_at;
        while (path[read_at] != '\0' && path[read_at] != '/')
            ++read_at;
        const size_t length = read_at - start;
        if (length == 0u)
            break;
        if (length == 1u && path[start] == '.')
            continue;
        if (length == 2u && path[start] == '.' && path[start + 1u] == '.') {
            while (used > 1u && path[used - 1u] != '/')
                --used;
            if (used > 1u)
                --used;
            continue;
        }
        if (used > 1u)
            path[used++] = '/';
        memmove(path + used, path + start, length);
        used += length;
    }
    path[used] = '\0';
}

static directory_status canonical_native(const char *input, char **out, directory_state *state,
                                         directory_error *error) {
    ++state->canonicalize_calls;
    size_t length = strlen(input);
    if (length > PATH_MAX)
        return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, 0,
                      "game directory canonical PATH_MAX");
    char *absolute = NULL;
    if (input[0] == '/') {
        directory_status status = native_path(input, &absolute, error);
        if (status != MC_CLIENT_GAME_DIRECTORY_OK)
            return status;
    } else {
        char working[PATH_MAX + 1u];
        if (getcwd(working, sizeof(working)) == NULL) {
            state->last_native_error = errno;
            return MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED;
        }
        const size_t prefix = strlen(working);
        if (prefix + length + 1u > PATH_MAX)
            return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, 0,
                          "game directory absolute PATH_MAX");
        absolute = malloc(prefix + length + 2u);
        if (absolute == NULL)
            return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                          "game directory absolute allocation");
        memcpy(absolute, working, prefix);
        absolute[prefix] = '/';
        memcpy(absolute + prefix + 1u, input, length + 1u);
    }
    length = strlen(absolute);
    errno = 0;
    char *resolved = realpath(absolute, NULL);
    if (resolved != NULL) {
        free(absolute);
        *out = resolved;
        return MC_CLIENT_GAME_DIRECTORY_OK;
    }
    state->last_native_error = errno;
    if (errno == ENOMEM) {
        free(absolute);
        return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, ENOMEM,
                      "game directory realpath allocation");
    }
    size_t boundary = length;
    while (boundary > 1u) {
        --boundary;
        while (boundary > 0u && absolute[boundary] != '/')
            --boundary;
        if (boundary == 0u)
            break;
        absolute[boundary] = '\0';
        resolved = realpath(absolute, NULL);
        const int failure = errno;
        absolute[boundary] = '/';
        if (resolved != NULL)
            break;
        state->last_native_error = failure;
        if (failure == ENOMEM) {
            free(absolute);
            return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, failure,
                          "game directory prefix realpath allocation");
        }
        if (failure != ENOENT && failure != ENOTDIR && failure != EACCES) {
            free(absolute);
            return MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED;
        }
    }
    if (resolved != NULL) {
        const size_t prefix = strlen(resolved);
        size_t suffix = boundary;
        if (prefix != 0u && resolved[prefix - 1u] == '/' && absolute[suffix] == '/')
            ++suffix;
        const size_t tail = length - suffix;
        if (prefix + tail > PATH_MAX) {
            free(resolved);
            free(absolute);
            return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, 0,
                          "game directory canonical result PATH_MAX");
        }
        char *combined = malloc(prefix + tail + 1u);
        if (combined == NULL) {
            free(resolved);
            free(absolute);
            return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                          "game directory canonical allocation");
        }
        memcpy(combined, resolved, prefix);
        memcpy(combined + prefix, absolute + suffix, tail + 1u);
        free(resolved);
        free(absolute);
        absolute = combined;
    }
    collapse_absolute(absolute);
    *out = absolute;
    return MC_CLIENT_GAME_DIRECTORY_OK;
}

static native_char *parent_native(native_char *path) {
    char *separator = strrchr(path, '/');
    if (separator == NULL || path[1] == '\0')
        return NULL;
    if (separator == path)
        path[1] = '\0';
    else
        *separator = '\0';
    return path;
}

#endif
directory_status mkdirs_native(const native_char *path, size_t depth, bool *created,
                               directory_state *state, directory_error *error) {
    if (depth >= MC_CLIENT_GAME_DIRECTORY_MAX_PARENT_CALLS)
        return result(error, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0, "game directory parent depth");
    *created = false;
    const bool exists = exists_native(path, state);
    if (state->stop_at_provider_frontier && state->provider_frontier)
        return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, state->last_native_error,
                      "source File final attributes provider");
    if (exists)
        return MC_CLIENT_GAME_DIRECTORY_OK;
    if (mkdir_native(path, state)) {
        *created = true;
        return MC_CLIENT_GAME_DIRECTORY_OK;
    }
    native_char *canonical = NULL;
    directory_status status = canonical_native(path, &canonical, state, error);
    if (status == MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED)
        return MC_CLIENT_GAME_DIRECTORY_OK; /* source caught canonical IOException */
    if (status != MC_CLIENT_GAME_DIRECTORY_OK)
        return status;
    native_char *parent_copy = NULL;
#ifdef _WIN32
    status = wide_copy(canonical, &parent_copy, error);
#else
    status = native_path(canonical, &parent_copy, error);
#endif
    if (status != MC_CLIENT_GAME_DIRECTORY_OK) {
        free(canonical);
        return status;
    }
    native_char *parent = parent_native(parent_copy);
    if (parent != NULL) {
        bool parent_created = false;
        status = mkdirs_native(parent, depth + 1u, &parent_created, state, error);
        if (status == MC_CLIENT_GAME_DIRECTORY_OK) {
            const bool parent_exists = parent_created || exists_native(parent, state);
            if (state->stop_at_provider_frontier && state->provider_frontier)
                status = result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH,
                                state->last_native_error,
                                "source File final parent attributes provider");
            else if (parent_exists)
                *created = mkdir_native(canonical, state);
        }
    }
    free(parent_copy);
    free(canonical);
    return status;
}

mc_file_io_status mc_file_text_length(const char *text, size_t *out, mc_file_io_error *error) {
    for (size_t i = 0; i <= MC_CLIENT_GAME_DIRECTORY_MAX_PATH_BYTES; ++i) {
        if (text[i] == '\0') {
            *out = i;
            return MC_CLIENT_GAME_DIRECTORY_OK;
        }
    }
    return result(error, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0, "game directory path length");
}

/* Native attribute operations remain here with the shared stat/path backend.
 * These do not allocate/reconstruct a File or acquire a source snapshot. */
#ifdef _WIN32
#ifndef MC_FILE_NATIVE_GET_TIME
#define MC_FILE_NATIVE_GET_TIME GetFileTime
#else
extern BOOL MC_FILE_NATIVE_GET_TIME(HANDLE file, LPFILETIME creation, LPFILETIME access,
                                    LPFILETIME modified);
#endif
#ifndef MC_FILE_NATIVE_CLOSE_HANDLE
#define MC_FILE_NATIVE_CLOSE_HANDLE CloseHandle
#else
extern BOOL MC_FILE_NATIVE_CLOSE_HANDLE(HANDLE handle);
#endif
static bool file_reserved_device(const wchar_t *path) {
    wchar_t name[9];
    DWORD length = GetFullPathNameW(path, 9, name, NULL);
    if ((length != 7 && length != 8) || name[0] != L'\\' || name[1] != L'\\' || name[2] != L'.' ||
        name[3] != L'\\')
        return false;
    for (size_t i = 4; name[i] != L'\0'; ++i)
        if (name[i] >= L'a' && name[i] <= L'z')
            name[i] = (wchar_t)(name[i] - L'a' + L'A');
    const wchar_t *device = name + 4;
    if (wcscmp(device, L"CON") == 0 || wcscmp(device, L"PRN") == 0 || wcscmp(device, L"AUX") == 0 ||
        wcscmp(device, L"NUL") == 0)
        return true;
    return (wcsncmp(device, L"COM", 3) == 0 || wcsncmp(device, L"LPT", 3) == 0) &&
           device[3] >= L'1' && device[3] <= L'9';
}

static mc_file_io_status file_modified(const wchar_t *path, int64_t *out, mc_file_io_state *state,
                                       mc_file_io_error *error) {
    HANDLE owned = CreateFileW(path, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (owned == INVALID_HANDLE_VALUE) {
        state->last_native_error = (int)GetLastError();
        *out = 0;
        return MC_FILE_IO_OK;
    }
    FILETIME time;
    BOOL got = MC_FILE_NATIVE_GET_TIME(owned, NULL, NULL, &time);
    DWORD query_error = got ? 0 : GetLastError();
    int64_t value = 0;
    if (got) {
        uint64_t ticks = ((uint64_t)time.dwHighDateTime << 32u) | time.dwLowDateTime;
        /* Divide unsigned before signed epoch subtraction, as the real provider. */
        value = (int64_t)(ticks / UINT64_C(10000)) - INT64_C(11644473600000);
    }
    HANDLE issued = owned;
    owned = INVALID_HANDLE_VALUE;
    BOOL closed = MC_FILE_NATIVE_CLOSE_HANDLE(issued);
    if (!closed) {
        int code = (int)GetLastError();
        state->last_native_error = code;
        return result(error, MC_FILE_IO_NATIVE_ERROR, code,
                      "File lastModified native temporary-handle close");
    }
    if (!got)
        state->last_native_error = (int)query_error;
    *out = value;
    return MC_FILE_IO_OK;
}
#endif
mc_file_io_status mc_file_native_attribute(const mc_file_native_char *path,
                                           mc_file_attribute_kind kind, int64_t *out,
                                           mc_file_io_state *state, mc_file_io_error *error) {
    if (path == NULL || out == NULL || state == NULL ||
        (unsigned)kind > MC_FILE_ATTRIBUTE_LAST_MODIFIED)
        return result(error, MC_FILE_IO_INVALID_ARGUMENT, 0, "File attribute native arguments");
    int64_t value = 0;
#ifdef _WIN32
    if (kind == MC_FILE_ATTRIBUTE_LAST_MODIFIED)
        return file_modified(path, out, state, error);
    if (kind == MC_FILE_ATTRIBUTE_DIRECTORY) {
        /* Exact former File.is_directory native body/last-error policy. */
        DWORD attributes = GetFileAttributesW(path);
        DWORD code = GetLastError();
        if (attributes == INVALID_FILE_ATTRIBUTES)
            state->last_native_error = (int)code;
        if ((attributes != INVALID_FILE_ATTRIBUTES &&
             (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) ||
            (attributes == INVALID_FILE_ATTRIBUTES &&
             (code == ERROR_SHARING_VIOLATION || code == ERROR_ACCESS_DENIED)))
            return result(error, MC_FILE_IO_UNSUPPORTED_PATH, (int)code,
                          "File final directory attributes provider");
        value =
            attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    } else {
        if (kind == MC_FILE_ATTRIBUTE_REGULAR && file_reserved_device(path)) {
            *out = 0;
            return MC_FILE_IO_OK;
        }
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data)) {
            DWORD code = GetLastError();
            state->last_native_error = (int)code;
            if (code == ERROR_SHARING_VIOLATION ||
                (kind == MC_FILE_ATTRIBUTE_REGULAR && code == ERROR_ACCESS_DENIED))
                return result(error, MC_FILE_IO_UNSUPPORTED_PATH, (int)code,
                              "File final attribute fallback provider");
        } else {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                return result(error, MC_FILE_IO_UNSUPPORTED_PATH, 0,
                              "File final attribute target provider");
            if (kind == MC_FILE_ATTRIBUTE_REGULAR)
                value = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
            else {
                uint64_t bytes = ((uint64_t)data.nFileSizeHigh << 32u) | data.nFileSizeLow;
                if (bytes > (uint64_t)INT64_MAX)
                    return result(error, MC_FILE_IO_UNSUPPORTED_PATH, 0,
                                  "File native length range provider");
                value = (int64_t)bytes;
            }
        }
    }
#else
    struct stat information;
    if (file_stat(path, &information, state)) {
        if (kind == MC_FILE_ATTRIBUTE_DIRECTORY)
            value = S_ISDIR(information.st_mode);
        else if (kind == MC_FILE_ATTRIBUTE_REGULAR)
            value = S_ISREG(information.st_mode);
        else if (kind == MC_FILE_ATTRIBUTE_LENGTH)
            value = (int64_t)information.st_size;
        else {
            int64_t seconds = (int64_t)information.st_mtim.tv_sec;
            int64_t nanos = (int64_t)information.st_mtim.tv_nsec;
            if (nanos < 0 || nanos >= INT64_C(1000000000) || seconds > INT64_MAX / 1000 ||
                seconds < INT64_MIN / 1000)
                return result(error, MC_FILE_IO_UNSUPPORTED_PATH, 0,
                              "File native timestamp range provider");
            int64_t milliseconds = nanos / 1000000;
            value = seconds * 1000;
            if (value > INT64_MAX - milliseconds)
                return result(error, MC_FILE_IO_UNSUPPORTED_PATH, 0,
                              "File native timestamp addition provider");
            value += milliseconds;
        }
    }
#endif
    *out = value;
    return MC_FILE_IO_OK;
}

/* Shared single native rename attempt. Ordinary false remains a Java boolean;
 * callers own path conversion, source invocation order and diagnostic policy. */
bool mc_file_native_rename(const mc_file_native_char *from, const mc_file_native_char *to,
                           int *system_error) {
#ifdef _WIN32
    if (MoveFileW(from, to)) {
        if (system_error != NULL)
            *system_error = 0;
        return true;
    }
    if (system_error != NULL)
        *system_error = (int)GetLastError();
#else
    if (rename(from, to) == 0) {
        if (system_error != NULL)
            *system_error = 0;
        return true;
    }
    if (system_error != NULL)
        *system_error = errno;
#endif
    return false;
}

#ifdef _WIN32
/* End the legacy GameDirectory observation alias before this independent
 * operation, which has no GameDirectory creation/provider state. */
#undef GetFileAttributesW
#endif

bool mc_file_native_delete(const mc_file_native_char *path, int *system_error) {
#ifdef _WIN32
    /* The selected JDK Windows provider clears attributes before choosing the
     * file or directory removal primitive. A failed attribute write is ignored. */
    (void)SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
    const DWORD attributes = GetFileAttributesW(path);
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? RemoveDirectoryW(path)
                                                      : DeleteFileW(path))) {
        if (system_error != NULL)
            *system_error = 0;
        return true;
    }
    if (system_error != NULL)
        *system_error = (int)GetLastError();
#else
    if (remove(path) == 0) {
        if (system_error != NULL)
            *system_error = 0;
        return true;
    }
    if (system_error != NULL)
        *system_error = errno;
#endif
    return false;
}
