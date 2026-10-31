#ifndef _WIN32
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#endif

#include "game_directory.h"
#include "../../platform/files/directory.h"
#include "directory_native.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32) && !defined(__linux__)
#error "GameDirectory native bridge still requires this platform's implementation"
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <wchar.h>
typedef wchar_t native_char;
#else
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
typedef char native_char;
#endif

struct mc_client_game_directory {
    char *path;
    mc_client_game_directory_state state;
};

typedef mc_client_game_directory_status directory_status;
typedef mc_client_game_directory_error directory_error;
typedef mc_client_game_directory_state directory_state;

static directory_status result(directory_error *error, directory_status status, int system_error,
                               const char *operation) {
    if (error != NULL)
        *error = (directory_error){status, system_error, operation};
    return status;
}

static directory_status primitive_status(mc_file_io_status s) {
    switch (s) {
        case MC_FILE_IO_OK:
            return MC_CLIENT_GAME_DIRECTORY_OK;
        case MC_FILE_IO_INVALID_ARGUMENT:
            return MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT;
        case MC_FILE_IO_ALLOCATION_FAILED:
            return MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED;
        case MC_FILE_IO_LIMIT:
            return MC_CLIENT_GAME_DIRECTORY_LIMIT;
        case MC_FILE_IO_UNSUPPORTED_FILENAME_ENCODING:
            return MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING;
        case MC_FILE_IO_UNSUPPORTED_PATH:
            return MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH;
        case MC_FILE_IO_NATIVE_ERROR:
            return MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR;
        case MC_FILE_IO_CREATION_FALSE:
            return MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED;
    }
    return MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR;
}

static directory_status primitive_result(mc_file_io_status s, const mc_file_io_error *child,
                                         directory_error *e) {
    if (e != NULL && child->written)
        *e = (directory_error){primitive_status(child->status), child->system_error,
                               child->operation};
    return primitive_status(s);
}

static mc_file_io_state primitive_state(const directory_state *s) {
    return (mc_file_io_state){.exists_calls = s->exists_calls,
                              .mkdir_calls = s->mkdir_calls,
                              .canonicalize_calls = s->canonicalize_calls,
                              .last_native_error = s->last_native_error};
}

static void primitive_state_copy(directory_state *s, const mc_file_io_state *child) {
    s->exists_calls = child->exists_calls;
    s->mkdir_calls = child->mkdir_calls;
    s->canonicalize_calls = child->canonicalize_calls;
    s->last_native_error = child->last_native_error;
}

static directory_status text_length(const char *text, size_t *out, directory_error *error) {
    mc_file_io_error child = {0};
    mc_file_io_status status = mc_file_text_length(text, out, &child);
    return primitive_result(status, &child, error);
}

/* Strict UTF8 is the existing native File boundary, not Java String storage. */
static bool valid_utf8(const char *text, size_t length) {
    const unsigned char *bytes = (const unsigned char *)text;
    for (size_t i = 0; i < length;) {
        const unsigned int first = bytes[i++];
        if (first < 0x80u)
            continue;
        unsigned int count, value, minimum;
        if (first >= 0xc2u && first <= 0xdfu) {
            count = 1u;
            value = first & 0x1fu;
            minimum = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            count = 2u;
            value = first & 0x0fu;
            minimum = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            count = 3u;
            value = first & 7u;
            minimum = 0x10000u;
        } else {
            return false;
        }
        if (count > length - i)
            return false;
        for (unsigned int j = 0; j < count; ++j) {
            const unsigned int next = bytes[i++];
            if ((next & 0xc0u) != 0x80u)
                return false;
            value = (value << 6u) | (next & 0x3fu);
        }
        if (value < minimum || value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu))
            return false;
    }
    return true;
}

static bool path_separator(char unit) {
#ifdef _WIN32
    return unit == '/' || unit == '\\';
#else
    return unit == '/';
#endif
}

#ifdef _WIN32
static bool drive_letter(char unit) {
    return (unit >= 'A' && unit <= 'Z') || (unit >= 'a' && unit <= 'z');
}
#endif

/* Independent normalization of source File's separator and prefix rules.
 * Dots are retained here; mkdirs canonicalizes only after a failed mkdir. */
static directory_status normalize(const char *input, char **out, directory_error *error) {
    size_t length;
    directory_status status = text_length(input, &length, error);
    if (status != MC_CLIENT_GAME_DIRECTORY_OK)
        return status;
    if (!valid_utf8(input, length))
        return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING, 0,
                      "game directory UTF8 filename");
    char *path = malloc(length + 1u);
    if (path == NULL)
        return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                      "game directory path allocation");
    (void)mc_file_normalize_bytes(input, length, path);
    *out = path;
    return MC_CLIENT_GAME_DIRECTORY_OK;
}

static directory_status child_path(const char *parent, char **out, directory_error *error) {
    char *normal = NULL;
    directory_status status = normalize(parent, &normal, error);
    if (status != MC_CLIENT_GAME_DIRECTORY_OK)
        return status;
    size_t length = strlen(normal);
#ifdef _WIN32
    const char separator = '\\';
    const bool drive_relative = length == 2u && drive_letter(normal[0]) && normal[1] == ':';
#else
    const char separator = '/';
    const bool drive_relative = false;
#endif
    const bool add_separator =
        length == 0u || (!path_separator(normal[length - 1u]) && !drive_relative);
    const size_t child_bytes = sizeof(".minecraft") - 1u;
    if (length > MC_CLIENT_GAME_DIRECTORY_MAX_PATH_BYTES - child_bytes - 1u) {
        free(normal);
        return result(error, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0, "game directory child length");
    }
    char *path = malloc(length + (add_separator ? 1u : 0u) + child_bytes + 1u);
    if (path == NULL) {
        free(normal);
        return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                      "game directory child allocation");
    }
    memcpy(path, normal, length);
    free(normal);
    if (add_separator)
        path[length++] = separator;
    memcpy(path + length, ".minecraft", child_bytes + 1u);
    *out = path;
    return MC_CLIENT_GAME_DIRECTORY_OK;
}

#ifdef _WIN32

static directory_status wide_utf8(const wchar_t *text, char **out, directory_error *error) {
    const size_t length = wcslen(text);
    if (length > (size_t)INT_MAX || length > MC_CLIENT_GAME_DIRECTORY_MAX_PATH_BYTES)
        return result(error, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0, "game directory UTF16 length");
    if (length == 0u) {
        char *empty = malloc(1u);
        if (empty == NULL)
            return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                          "game directory empty allocation");
        empty[0] = '\0';
        *out = empty;
        return MC_CLIENT_GAME_DIRECTORY_OK;
    }
    const int count =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, (int)length, NULL, 0, NULL, NULL);
    if (count <= 0)
        return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING,
                      (int)GetLastError(), "game directory UTF16 to UTF8");
    if ((unsigned int)count > MC_CLIENT_GAME_DIRECTORY_MAX_PATH_BYTES)
        return result(error, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0, "game directory UTF8 length");
    char *path = malloc((size_t)count + 1u);
    if (path == NULL)
        return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                      "game directory UTF8 allocation");
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, (int)length, path, count, NULL,
                            NULL) != count) {
        const int failure = (int)GetLastError();
        free(path);
        return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING, failure,
                      "game directory UTF16 conversion");
    }
    path[count] = '\0';
    *out = path;
    return MC_CLIENT_GAME_DIRECTORY_OK;
}

static directory_status native_path(const char *p, wchar_t **out, directory_error *e) {
    mc_file_io_error child = {0};
    mc_file_io_status s = mc_file_native_path(p, out, &child);
    return primitive_result(s, &child, e);
}

static directory_status default_path(char **out, const mc_client_game_directory_profile *profile,
                                     directory_state *state, directory_error *error) {
    (void)profile;
    mc_client_directory_native_home home = {0};
    mc_client_directory_native_env env = {0};
    directory_status status = mc_client_directory_native_home_query(&home, NULL, state, error);
    if (status != MC_CLIENT_GAME_DIRECTORY_OK)
        return status;
    status = mc_client_directory_native_env_open(&env, state, error);
    char *utf8 = NULL;
    if (status == MC_CLIENT_GAME_DIRECTORY_OK)
        status = wide_utf8(env.present ? env.value : home.text, &utf8, error);
    directory_status closed = mc_client_directory_native_env_close(
        &env, status == MC_CLIENT_GAME_DIRECTORY_OK ? error : NULL);
    if (status == MC_CLIENT_GAME_DIRECTORY_OK)
        status = closed;
    mc_client_directory_native_home_free(&home);
    if (status == MC_CLIENT_GAME_DIRECTORY_OK)
        status = child_path(utf8, out, error);
    free(utf8);
    return status;
}

static bool exists_native(const wchar_t *p, directory_state *state) {
    mc_file_io_state child = primitive_state(state);
    bool value = mc_file_native_exists(p, &child);
    primitive_state_copy(state, &child);
    return value;
}

#else

static directory_status native_path(const char *p, char **out, directory_error *e) {
    mc_file_io_error child = {0};
    mc_file_io_status s = mc_file_native_path(p, out, &child);
    return primitive_result(s, &child, e);
}

static directory_status default_path(char **out, const mc_client_game_directory_profile *profile,
                                     directory_state *state, directory_error *error) {
    mc_client_directory_native_home home = {0};
    directory_status status = mc_client_directory_native_home_query(&home, profile, state, error);
    if (status == MC_CLIENT_GAME_DIRECTORY_OK)
        status = child_path(home.text, out, error);
    mc_client_directory_native_home_free(&home);
    return status;
}

static bool exists_native(const char *p, directory_state *state) {
    mc_file_io_state child = primitive_state(state);
    bool value = mc_file_native_exists(p, &child);
    primitive_state_copy(state, &child);
    return value;
}

#endif

/* false is the source mkdirs boolean, distinct from native admission failure.
 * No preflight isDirectory check, rollback or recursive-mkdir substitute. */
static directory_status mkdirs_native(const native_char *p, size_t depth, bool *created,
                                      directory_state *state, directory_error *e) {
    mc_file_io_state child = primitive_state(state);
    mc_file_io_error detail = {0};
    mc_file_io_status s = mc_file_native_mkdirs(p, depth, created, &child, &detail);
    primitive_state_copy(state, &child);
    return primitive_result(s, &detail, e);
}

mc_client_game_directory_status mc_client_game_directory_create(
    const char *selected_directory, const mc_client_game_directory_profile *profile,
    mc_client_game_directory **out, mc_client_game_directory_state *prefix,
    mc_client_game_directory_error *error) {
    if (out == NULL ||
        (profile != NULL && (unsigned int)profile->filesystem_charset > MC_JAVA_READER_US_ASCII))
        return result(error, MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT, 0,
                      "game directory arguments");
    directory_state state = {.selected_directory = selected_directory != NULL};
    char *path = NULL;
    directory_status status = selected_directory != NULL
                                  ? normalize(selected_directory, &path, error)
                                  : default_path(&path, profile, &state, error);
    native_char *native = NULL;
    if (status == MC_CLIENT_GAME_DIRECTORY_OK)
        status = native_path(path, &native, error);
    if (status == MC_CLIENT_GAME_DIRECTORY_OK) {
        state.initial_exists = exists_native(native, &state);
        if (!state.initial_exists) {
            state.mkdirs_reached = true;
            status = mkdirs_native(native, 0u, &state.directory_created, &state, error);
            if (status == MC_CLIENT_GAME_DIRECTORY_OK && !state.directory_created)
                status = result(error, MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED,
                                state.last_native_error, "game directory mkdirs returned false");
        }
    }
    free(native);
    if (prefix != NULL)
        *prefix = state;
    if (status != MC_CLIENT_GAME_DIRECTORY_OK) {
        free(path);
        return status;
    }
    mc_client_game_directory *directory = malloc(sizeof(*directory));
    if (directory == NULL) {
        free(path);
        return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                      "game directory owner allocation");
    }
    *directory = (mc_client_game_directory){path, state};
    *out = directory;
    return result(error, MC_CLIENT_GAME_DIRECTORY_OK, 0, NULL);
}

const char *mc_client_game_directory_path(const mc_client_game_directory *directory) {
    return directory == NULL ? NULL : directory->path;
}

mc_client_game_directory_status
mc_client_game_directory_read_state(const mc_client_game_directory *directory,
                                    mc_client_game_directory_state *out,
                                    mc_client_game_directory_error *error) {
    if (directory == NULL || out == NULL)
        return result(error, MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT, 0,
                      "game directory state arguments");
    *out = directory->state;
    return result(error, MC_CLIENT_GAME_DIRECTORY_OK, 0, NULL);
}

void mc_client_game_directory_free(mc_client_game_directory *directory) {
    if (directory != NULL) {
        free(directory->path);
        free(directory);
    }
}
