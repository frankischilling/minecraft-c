#ifndef _WIN32
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#endif

#include "game_directory.h"

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
#define INITKNOWNFOLDERS
#include <windows.h>

#include <knownfolders.h>
#include <objbase.h>
#include <shlobj.h>
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

static directory_status text_length(const char *text, size_t *out, directory_error *error) {
    for (size_t i = 0; i <= MC_CLIENT_GAME_DIRECTORY_MAX_PATH_BYTES; ++i) {
        if (text[i] == '\0') {
            *out = i;
            return MC_CLIENT_GAME_DIRECTORY_OK;
        }
    }
    return result(error, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0, "game directory path length");
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
    size_t read_at = 0, used = 0;
#ifdef _WIN32
    size_t leading = 0;
    while (leading < length && path_separator(input[leading]))
        ++leading;
    if (length - leading >= 2u && drive_letter(input[leading]) && input[leading + 1u] == ':') {
        path[used++] = input[leading];
        path[used++] = ':';
        read_at = leading + 2u;
    } else if (leading >= 2u) {
        path[used++] = '\\';
        path[used++] = '\\';
        read_at = leading;
    }
    const char separator = '\\';
#else
    const char separator = '/';
#endif
    while (read_at < length) {
        if (!path_separator(input[read_at])) {
            path[used++] = input[read_at++];
            continue;
        }
        while (read_at < length && path_separator(input[read_at]))
            ++read_at;
        if (read_at != length || used == 0u
#ifdef _WIN32
            || (used == 2u && path[1] == ':')
#endif
        )
            path[used++] = separator;
    }
    path[used] = '\0';
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

static directory_status wide_copy(const wchar_t *text, wchar_t **out, directory_error *error) {
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

static directory_status native_path(const char *path, wchar_t **out, directory_error *error) {
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

static directory_status home_directory(wchar_t **out, directory_state *state,
                                       directory_error *error) {
    wchar_t *shell_path = NULL;
    HRESULT code;
#ifdef _MSC_VER
    __try {
        code = SHGetKnownFolderPath(&FOLDERID_Profile, KF_FLAG_DONT_VERIFY, NULL, &shell_path);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        code = E_FAIL;
    }
#else
    code = SHGetKnownFolderPath(&FOLDERID_Profile, KF_FLAG_DONT_VERIFY, NULL, &shell_path);
#endif
    state->profile_query_hresult = (int32_t)code;
    directory_status status;
    if (SUCCEEDED(code) && shell_path != NULL) {
        status = wide_copy(shell_path, out, error);
        CoTaskMemFree(shell_path);
        state->home_origin = MC_CLIENT_GAME_DIRECTORY_HOME_PROFILE;
        return status;
    }
    CoTaskMemFree(shell_path);
    wchar_t legacy[MAX_PATH + 1u] = {0};
    code = SHGetFolderPathW(NULL, CSIDL_PROFILE | CSIDL_FLAG_DONT_VERIFY, NULL, SHGFP_TYPE_CURRENT,
                            legacy);
    state->legacy_profile_query_hresult = (int32_t)code;
    if (SUCCEEDED(code)) {
        legacy[MAX_PATH] = L'\0';
        state->home_origin = MC_CLIENT_GAME_DIRECTORY_HOME_LEGACY_PROFILE;
        return wide_copy(legacy, out, error);
    }
    state->home_origin = MC_CLIENT_GAME_DIRECTORY_HOME_WINDOWS_FALLBACK;
    return wide_copy(L"C:\\", out, error);
}

static bool appdata_name(const wchar_t *text, size_t length) {
    static const wchar_t key[] = L"APPDATA";
    if (length != sizeof(key) / sizeof(key[0]) - 1u)
        return false;
    for (size_t i = 0; i < length; ++i) {
        wchar_t unit = text[i];
        if (unit >= L'a' && unit <= L'z')
            unit = (wchar_t)(unit - L'a' + L'A');
        if (unit != key[i])
            return false;
    }
    return true;
}

static directory_status default_path(char **out, const mc_client_game_directory_profile *profile,
                                     directory_state *state, directory_error *error) {
    (void)profile;
    wchar_t *home = NULL;
    directory_status status = home_directory(&home, state, error);
    if (status != MC_CLIENT_GAME_DIRECTORY_OK)
        return status;
    wchar_t *block = GetEnvironmentStringsW();
    if (block == NULL) {
        const int failure = (int)GetLastError();
        free(home);
        return result(error, MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR, failure,
                      "game directory environment snapshot");
    }
    const wchar_t *parent = home;
    size_t scanned = 0;
    const size_t unit_limit = MC_CLIENT_GAME_DIRECTORY_MAX_LOOKUP_BYTES / sizeof(wchar_t);
    for (const wchar_t *entry = block; *entry != L'\0';) {
        size_t length = 0;
        while (entry[length] != L'\0' && scanned + length <= unit_limit)
            ++length;
        if (scanned + length > unit_limit) {
            status =
                result(error, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0, "game directory environment size");
            break;
        }
        const wchar_t *equals = wcschr(entry + 1, L'=');
        if (equals != NULL && appdata_name(entry, (size_t)(equals - entry))) {
            if (state->appdata_present) {
                status = result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, 0,
                                "game directory duplicate APPDATA names");
                break;
            }
            state->appdata_present = true;
            parent = equals + 1;
        }
        scanned += length + 1u;
        entry += length + 1u;
    }
    char *utf8 = NULL;
    if (status == MC_CLIENT_GAME_DIRECTORY_OK)
        status = wide_utf8(parent, &utf8, error);
    if (!FreeEnvironmentStringsW(block) && status == MC_CLIENT_GAME_DIRECTORY_OK)
        status = result(error, MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR, (int)GetLastError(),
                        "game directory environment release");
    free(home);
    if (status == MC_CLIENT_GAME_DIRECTORY_OK)
        status = child_path(utf8, out, error);
    free(utf8);
    return status;
}

static bool exists_native(const wchar_t *path, directory_state *state) {
    ++state->exists_calls;
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
        return true;
    state->last_native_error = (int)GetLastError();
    return false;
}

static bool mkdir_native(const wchar_t *path, directory_state *state) {
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

static directory_status native_path(const char *path, char **out, directory_error *error) {
    const size_t length = strlen(path);
    char *copy = malloc(length + 1u);
    if (copy == NULL)
        return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                      "game directory native path allocation");
    memcpy(copy, path, length + 1u);
    *out = copy;
    return MC_CLIENT_GAME_DIRECTORY_OK;
}

static directory_status default_path(char **out, const mc_client_game_directory_profile *profile,
                                     directory_state *state, directory_error *error) {
    mc_java_reader_charset charset =
        profile == NULL ? MC_JAVA_READER_DEFAULT : profile->filesystem_charset;
    if (charset == MC_JAVA_READER_DEFAULT) {
        const mc_java_reader_status discovered = mc_java_reader_default_charset(&charset);
        if (discovered != MC_JAVA_READER_OK)
            return result(error,
                          discovered == MC_JAVA_READER_ALLOCATION_FAILED
                              ? MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED
                              : MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING,
                          0, "game directory native charset");
    }
    state->filesystem_charset = charset;
    long suggested = sysconf(_SC_GETPW_R_SIZE_MAX);
    size_t capacity =
        suggested > 0 && (unsigned long)suggested <= MC_CLIENT_GAME_DIRECTORY_MAX_LOOKUP_BYTES
            ? (size_t)suggested
            : 1024u;
    char *buffer = NULL;
    struct passwd account;
    struct passwd *found = NULL;
    const uid_t user = getuid();
    int code;
    for (;;) {
        char *replacement = realloc(buffer, capacity);
        if (replacement == NULL) {
            free(buffer);
            return result(error, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0,
                          "game directory account buffer");
        }
        buffer = replacement;
        code = getpwuid_r(user, &account, buffer, capacity, &found);
        if (code != ERANGE)
            break;
        if (capacity >= MC_CLIENT_GAME_DIRECTORY_MAX_LOOKUP_BYTES) {
            free(buffer);
            return result(error, MC_CLIENT_GAME_DIRECTORY_LIMIT, code,
                          "game directory account lookup limit");
        }
        capacity = capacity > MC_CLIENT_GAME_DIRECTORY_MAX_LOOKUP_BYTES / 2u
                       ? MC_CLIENT_GAME_DIRECTORY_MAX_LOOKUP_BYTES
                       : capacity * 2u;
    }
    if (code == 0 && found != NULL && found->pw_dir == NULL) {
        free(buffer);
        return result(error, MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR, 0,
                      "game directory account home unavailable");
    }
    const char *home = code == 0 && found != NULL ? found->pw_dir : "?";
    state->home_origin = code == 0 && found != NULL
                             ? MC_CLIENT_GAME_DIRECTORY_HOME_ACCOUNT
                             : MC_CLIENT_GAME_DIRECTORY_HOME_ACCOUNT_FALLBACK;
    if (code != 0)
        state->last_native_error = code;
    size_t length;
    directory_status status = text_length(home, &length, error);
    if (status == MC_CLIENT_GAME_DIRECTORY_OK && charset != MC_JAVA_READER_UTF8) {
        for (size_t i = 0; i < length; ++i) {
            if ((unsigned char)home[i] >= 0x80u) {
                status = result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING, 0,
                                "game directory nonUTF8 native File boundary");
                break;
            }
        }
    }
    if (status == MC_CLIENT_GAME_DIRECTORY_OK)
        status = child_path(home, out, error);
    free(buffer);
    return status;
}

static bool exists_native(const char *path, directory_state *state) {
    struct stat information;
    ++state->exists_calls;
    if (stat(path, &information) == 0)
        return true;
    state->last_native_error = errno;
    return false;
}

static bool mkdir_native(const char *path, directory_state *state) {
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

/* false is the source mkdirs boolean, distinct from native admission failure.
 * No preflight isDirectory check, rollback or recursive-mkdir substitute. */
static directory_status mkdirs_native(const native_char *path, size_t depth, bool *created,
                                      directory_state *state, directory_error *error) {
    if (depth >= MC_CLIENT_GAME_DIRECTORY_MAX_PARENT_CALLS)
        return result(error, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0, "game directory parent depth");
    *created = false;
    if (exists_native(path, state))
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
        if (status == MC_CLIENT_GAME_DIRECTORY_OK &&
            (parent_created || exists_native(parent, state)))
            *created = mkdir_native(canonical, state);
    }
    free(parent_copy);
    free(canonical);
    return status;
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
