#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "../startup/game_directory.h"
#include "internal.h"
#include "native_open.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <share.h>
#include <windows.h>
#else
#include <dirent.h>
#include <locale.h>
#include <sys/stat.h>
#endif

static bool separator(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}
#ifdef _WIN32
static bool drive_letter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
#endif
char *texture_path_join(const char *parent, const char *child, size_t limit,
                        mc_texture_pack_error *error) {
    const size_t first = parent == NULL ? 0 : strlen(parent), second = strlen(child);
    if (first > limit || second > limit || first > SIZE_MAX - second - 2 ||
        first + second > limit) {
        texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "Texture File child native path bound",
                          NULL, NULL, NULL, 0, 0);
        return NULL;
    }
    if (parent != NULL) {
        mc_client_texture_string *validated = texture_string_utf8(parent, limit, error);
        if (validated == NULL)
            return NULL;
        texture_string_free(validated);
    }
#ifdef _WIN32
    const char slash = '\\';
    const bool drive_relative = first == 2 && drive_letter(parent[0]) && parent[1] == ':';
#else
    const char slash = '/';
    const bool drive_relative = false;
#endif
    /* File(null,child) is relative. File(new File(""),child) uses the actual
     * platform default parent (/ or \\). They are distinct source inputs. */
    const bool extra =
        parent != NULL && (first == 0 || (!separator(parent[first - 1]) && !drive_relative));
    if (first + second + (extra ? 1u : 0u) > limit) {
        texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "Texture File child native path bound",
                          NULL, NULL, NULL, 0, 0);
        return NULL;
    }
    char *path = malloc(first + second + (extra ? 1u : 0u) + 1u);
    if (path == NULL) {
        texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED, "Texture File child storage",
                          NULL, NULL, NULL, 0, 0);
        return NULL;
    }
    if (first != 0)
        memcpy(path, parent, first);
    size_t used = first;
    if (extra)
        path[used++] = slash;
    memcpy(path + used, child, second + 1);
    /* Source File normalizes duplicate separators and Windows slash spelling.
     * Dots remain literal; mkdirs performs its separately reached canonical use. */
    size_t read = 0, written = 0;
#ifdef _WIN32
    if (separator(path[0]) && separator(path[1])) {
        path[written++] = '\\';
        path[written++] = '\\';
        read = 2;
        while (separator(path[read]))
            ++read;
    }
#endif
    while (path[read] != 0) {
        if (separator(path[read])) {
            while (separator(path[read]))
                ++read;
            if (written == 0 || path[read] != 0)
                path[written++] = slash;
        } else
            path[written++] = path[read++];
    }
    path[written] = 0;
    return path;
}
#ifdef _WIN32
static wchar_t *wide_path(const char *path, mc_texture_pack_error *error) {
    const size_t bytes = strlen(path);
    if (bytes > INT_MAX) {
        texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "Texture native filename conversion", NULL,
                          NULL, NULL, 0, 0);
        return NULL;
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (length == 0 || length >= MAX_PATH - 12) {
        texture_pack_fail(error, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
                          "Texture native filename/extended path dependency", NULL, NULL, NULL, 0,
                          0);
        error->system_error = (int)GetLastError();
        return NULL;
    }
    wchar_t *wide = malloc((size_t)length * sizeof(*wide));
    if (wide == NULL) {
        texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED, "Texture native filename", NULL,
                          NULL, NULL, 0, 0);
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, length) != length) {
        free(wide);
        texture_pack_fail(error, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE, "Texture filename encoding",
                          NULL, NULL, NULL, 0, 0);
        error->system_error = (int)GetLastError();
        return NULL;
    }
    return wide;
}
#endif
mc_texture_pack_status texture_file_stat(const char *path, texture_file_info *out,
                                         mc_texture_pack_error *error) {
    texture_file_info info = {.bytes_valid = true, .modified_valid = true};
#ifdef _WIN32
    wchar_t *wide = wide_path(path, error);
    if (wide == NULL)
        return error->status;
    WIN32_FILE_ATTRIBUTE_DATA data;
    const BOOL found = GetFileAttributesExW(wide, GetFileExInfoStandard, &data);
    free(wide);
    if (found) {
        info.exists = true;
        info.directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        info.regular = !info.directory;
        const uint64_t bytes = ((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow;
        const uint64_t ticks = ((uint64_t)data.ftLastWriteTime.dwHighDateTime << 32) |
                               data.ftLastWriteTime.dwLowDateTime;
        info.bytes_valid = bytes <= INT64_MAX;
        if (info.bytes_valid)
            info.bytes = (int64_t)bytes;
        info.modified_valid = ticks / 10000u <= (uint64_t)INT64_MAX;
        if (info.modified_valid)
            info.modified = (int64_t)(ticks / 10000u) - INT64_C(11644473600000);
    }
#else
    struct stat data;
    if (stat(path, &data) == 0) {
        info.exists = true;
        info.directory = S_ISDIR(data.st_mode);
        info.regular = S_ISREG(data.st_mode);
        info.bytes_valid = data.st_size >= 0;
        if (info.bytes_valid)
            info.bytes = (int64_t)data.st_size;
        info.modified_valid = data.st_mtim.tv_sec <= INT64_MAX / 1000 &&
                              data.st_mtim.tv_sec >= INT64_MIN / 1000 &&
                              !(data.st_mtim.tv_sec == INT64_MAX / 1000 &&
                                data.st_mtim.tv_nsec / 1000000 > INT64_MAX % 1000);
        if (info.modified_valid)
            info.modified = (int64_t)data.st_mtim.tv_sec * 1000 + data.st_mtim.tv_nsec / 1000000;
    }
    (void)error;
#endif
    *out = info;
    return MC_TEXTURE_PACK_OK; /* Source File predicates return false on ordinary OS failure. */
}

bool texture_file_mkdirs(const char *path, mc_texture_pack_error *error) {
    mc_client_game_directory *directory = NULL;
    mc_client_game_directory_state prefix = {0};
    mc_client_game_directory_error cause = {0};
    const mc_client_game_directory_status status =
        mc_client_game_directory_create(path, NULL, &directory, &prefix, &cause);
    mc_client_game_directory_free(directory);
    if (status == MC_CLIENT_GAME_DIRECTORY_OK || status == MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED)
        return prefix.directory_created;
    const mc_texture_pack_status mapped =
        status == MC_CLIENT_GAME_DIRECTORY_LIMIT               ? MC_TEXTURE_PACK_LIMIT
        : status == MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED ? MC_TEXTURE_PACK_ALLOCATION_FAILED
                                                               : MC_TEXTURE_PACK_UNSUPPORTED_NATIVE;
    texture_pack_fail(error, mapped, cause.operation, NULL, NULL, NULL, 0, 0);
    error->system_error = cause.system_error;
    return false;
}

FILE *texture_file_open_native(const mc_file_native_char *path, mc_texture_pack_error *error) {
#ifdef _WIN32
    /* Actual Windows JDK ZipFile blocks rename while open. Do not silently
     * grant FILE_SHARE_DELETE and change rescan's real file-handle effects. */
    FILE *file = _wfsopen(path, L"rb", _SH_DENYNO);
#else
    FILE *file = fopen(path, "rb");
#endif
    if (file == NULL) {
        texture_pack_fail(error, MC_TEXTURE_PACK_SOURCE_IO, "ZipFile actual archive open", NULL,
                          NULL, NULL, 0, 0);
        error->system_error = errno;
    } else if (setvbuf(file, NULL, _IONBF, 0) != 0) {
        const int failure = errno;
        (void)fclose(file);
        texture_pack_fail(error, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
                          "ZipFile actual unbuffered random access", NULL, NULL, NULL, 0, 0);
        error->system_error = failure;
        return NULL;
    }
    return file;
}

FILE *texture_file_open(const char *path, mc_texture_pack_error *error) {
#ifdef _WIN32
    wchar_t *wide = wide_path(path, error);
    if (wide == NULL)
        return NULL;
    FILE *file = texture_file_open_native(wide, error);
    free(wide);
    return file;
#else
    return texture_file_open_native(path, error);
#endif
}

void texture_directory_free(texture_directory *directory) {
    for (size_t i = 0; i < directory->count; ++i) {
        free(directory->entries[i].path);
        texture_string_free(directory->entries[i].name);
    }
    free(directory->entries);
    *directory = (texture_directory){0};
}

static bool append_entry(texture_directory *directory, const char *parent, const char *name,
                         const mc_texture_pack_options *options, mc_texture_pack_error *error) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return true;
    if (directory->count >= options->max_directory_entries ||
        directory->count >= SIZE_MAX / sizeof(*directory->entries)) {
        texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "Texture directory native entry bound",
                          NULL, NULL, NULL, 0, 0);
        return false;
    }
    texture_directory_entry entry = {0};
    entry.name = texture_string_utf8(name, options->max_name_units, error);
    if (entry.name != NULL)
        entry.path = texture_path_join(parent, name, options->max_path_bytes, error);
    if (entry.name == NULL || entry.path == NULL) {
        texture_string_free(entry.name);
        free(entry.path);
        return false;
    }
    texture_directory_entry *entries =
        realloc(directory->entries, (directory->count + 1) * sizeof(*entries));
    if (entries == NULL) {
        free(entry.path);
        texture_string_free(entry.name);
        texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED,
                          "Texture directory actual File array", NULL, NULL, NULL, 0, 0);
        return false;
    }
    directory->entries = entries;
    directory->entries[directory->count++] = entry;
    return true;
}

mc_texture_pack_status texture_directory_read(const char *path,
                                              const mc_texture_pack_options *options,
                                              texture_directory *out,
                                              mc_texture_pack_error *error) {
    texture_directory directory = {0};
#ifdef _WIN32
    char *pattern = texture_path_join(path, "*", options->max_path_bytes, error);
    if (pattern == NULL)
        return error->status;
    wchar_t *wide = wide_path(pattern, error);
    free(pattern);
    if (wide == NULL)
        return error->status;
    WIN32_FIND_DATAW entry;
    HANDLE handle = FindFirstFileW(wide, &entry);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            *out = directory;
            return MC_TEXTURE_PACK_OK;
        }
        return texture_pack_fail(error, MC_TEXTURE_PACK_NULL_REFERENCE,
                                 "ik File.listFiles null array", "ik", "a", "()V", 55, 55);
    }
    do {
        const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry.cFileName, -1,
                                              NULL, 0, NULL, NULL);
        if (bytes <= 0) {
            texture_pack_fail(error, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
                              "Texture directory filename encoding", NULL, NULL, NULL, 0, 0);
            break;
        }
        char *name = malloc((size_t)bytes);
        if (name == NULL) {
            texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED,
                              "Texture directory filename", NULL, NULL, NULL, 0, 0);
            break;
        }
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry.cFileName, -1, name, bytes, NULL,
                            NULL);
        const bool added = append_entry(&directory, path, name, options, error);
        free(name);
        if (!added)
            break;
    } while (FindNextFileW(handle, &entry));
    const DWORD enumeration_error = GetLastError();
    if (!FindClose(handle) && error->status == MC_TEXTURE_PACK_OK) {
        texture_pack_fail(error, MC_TEXTURE_PACK_PROVIDER_FAILED, "Texture directory native close",
                          NULL, NULL, NULL, 0, 0);
        error->system_error = (int)GetLastError();
    }
    if (error->status == MC_TEXTURE_PACK_OK && enumeration_error != ERROR_NO_MORE_FILES)
        texture_pack_fail(error, MC_TEXTURE_PACK_NULL_REFERENCE, "ik File.listFiles null array",
                          "ik", "a", "()V", 55, 55);
#else
    DIR *handle = opendir(path);
    if (handle == NULL)
        return texture_pack_fail(error, MC_TEXTURE_PACK_NULL_REFERENCE,
                                 "ik File.listFiles null array", "ik", "a", "()V", 55, 55);
    for (;;) {
        errno = 0;
        const struct dirent *entry = readdir(handle);
        if (entry == NULL) {
            if (errno != 0)
                texture_pack_fail(error, MC_TEXTURE_PACK_NULL_REFERENCE,
                                  "ik File.listFiles null array", "ik", "a", "()V", 55, 55);
            break;
        }
        if (!append_entry(&directory, path, entry->d_name, options, error))
            break;
    }
    if (closedir(handle) != 0 && error->status == MC_TEXTURE_PACK_OK) {
        texture_pack_fail(error, MC_TEXTURE_PACK_PROVIDER_FAILED, "Texture directory native close",
                          NULL, NULL, NULL, 0, 0);
        error->system_error = errno;
    }
#endif
    if (error->status != MC_TEXTURE_PACK_OK) {
        texture_directory_free(&directory);
        return error->status;
    }
    *out = directory;
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status texture_name_zip(mc_texture_pack_text name, bool *out,
                                        mc_texture_pack_error *error) {
    bool turkic = false;
#ifdef _WIN32
    wchar_t locale[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) == 0)
        return texture_pack_fail(error, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
                                 "Texture filename default Locale discovery", NULL, NULL, NULL, 0,
                                 0);
    turkic = (locale[0] == L't' && locale[1] == L'r') || (locale[0] == L'a' && locale[1] == L'z');
#else
    (void)error;
    const char *locale = getenv("LC_ALL");
    if (locale == NULL || locale[0] == 0)
        locale = getenv("LC_CTYPE");
    if (locale == NULL || locale[0] == 0)
        locale = getenv("LANG");
    locale_t actual = newlocale(LC_CTYPE_MASK, "", (locale_t)0);
    if (actual != (locale_t)0) {
        if (locale != NULL)
            turkic =
                (locale[0] == 't' && locale[1] == 'r') || (locale[0] == 'a' && locale[1] == 'z');
        freelocale(actual);
    }
#endif
    bool zip = false;
    if (!name.is_null && name.length >= 4) {
        const uint16_t *p = name.units + name.length - 4;
        zip = p[0] == '.' && (p[1] == 'z' || p[1] == 'Z') &&
              (p[2] == 'i' || (!turkic && p[2] == 'I') || (turkic && p[2] == 0x130)) &&
              (p[3] == 'p' || p[3] == 'P');
    }
    *out = zip;
    return MC_TEXTURE_PACK_OK;
}
