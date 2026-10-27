#ifndef _WIN32
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#endif
#include "directory_native.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define INITKNOWNFOLDERS
#include <windows.h>

/* These SDK declarations require the Windows types and annotation macros. */
#include <knownfolders.h>
#include <objbase.h>
#include <shlobj.h>
#include <wchar.h>
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif
typedef mc_client_game_directory_status directory_status;
typedef mc_client_game_directory_error directory_error;
typedef mc_client_game_directory_state directory_state;

static directory_status result(directory_error *e, directory_status s, int code, const char *op) {
    if (e != NULL)
        *e = (directory_error){s, code, op};
    return s;
}

static directory_status primitive_result(mc_file_io_status status, const mc_file_io_error *child,
                                         directory_error *e) {
    if (e != NULL && child->written)
        *e = (directory_error){(directory_status)child->status, child->system_error,
                               child->operation};
    return (directory_status)status;
}
#ifndef _WIN32
static directory_status text_length(const char *text, size_t *out, directory_error *e) {
    mc_file_io_error child = {0};
    mc_file_io_status s = mc_file_text_length(text, out, &child);
    return primitive_result(s, &child, e);
}
#endif
#ifdef _WIN32
static directory_status wide_copy(const wchar_t *p, wchar_t **out, directory_error *e) {
    mc_file_io_error child = {0};
    mc_file_io_status s = mc_file_native_wide_copy(p, out, &child);
    return primitive_result(s, &child, e);
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

mc_client_game_directory_status
mc_client_directory_native_home_query(mc_client_directory_native_home *out,
                                      const mc_client_game_directory_profile *profile,
                                      directory_state *state, directory_error *error) {
    (void)profile;
    wchar_t *home = NULL;
    directory_status s = home_directory(&home, state, error);
    if (s == MC_CLIENT_GAME_DIRECTORY_OK) {
        out->owned = home;
        out->text = home;
        out->length = wcslen(home);
    }
    return s;
}

mc_client_game_directory_status
mc_client_directory_native_env_open(mc_client_directory_native_env *out, directory_state *state,
                                    directory_error *error) {
    if (out == NULL || state == NULL)
        return result(error, MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT, 0,
                      "native environment arguments");
    if (out->active || out->block != NULL || out->release_issued)
        return result(error, MC_CLIENT_GAME_DIRECTORY_BUSY, 0, "native environment active/used");
    out->active = true;
    wchar_t *block = GetEnvironmentStringsW();
    if (block == NULL) {
        out->active = false;
        return result(error, MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR, (int)GetLastError(),
                      "game directory environment snapshot");
    }
    out->block = block;
    directory_status status = MC_CLIENT_GAME_DIRECTORY_OK;
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
            out->present = true;
            out->value = equals + 1;
        }
        scanned += length + 1u;
        entry += length + 1u;
    }
    out->active = false;
    return status;
}

mc_client_game_directory_status
mc_client_directory_native_env_close(mc_client_directory_native_env *env, directory_error *error) {
    if (env == NULL)
        return result(error, MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT, 0,
                      "native environment close");
    if (env->active)
        return result(error, MC_CLIENT_GAME_DIRECTORY_BUSY, 0, "native environment active close");
    if (env->block == NULL)
        return MC_CLIENT_GAME_DIRECTORY_OK;
    env->active = true;
    wchar_t *block = env->block;
    env->block = NULL;
    env->value = NULL;
    env->issued_token = block;
    env->release_issued = true;
    bool released = FreeEnvironmentStringsW(block) != 0;
    int code = !released && error != NULL ? (int)GetLastError() : 0;
    env->release_failed = !released;
    env->active = false;
    return released ? MC_CLIENT_GAME_DIRECTORY_OK
                    : result(error, MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR, code,
                             "game directory environment release");
}
#else
mc_client_game_directory_status
mc_client_directory_native_home_query(mc_client_directory_native_home *out,
                                      const mc_client_game_directory_profile *profile,
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
    if (status == MC_CLIENT_GAME_DIRECTORY_OK) {
        out->owned = buffer;
        out->text = home;
        out->length = length;
        return status;
    }
    free(buffer);
    return status;
}

mc_client_game_directory_status
mc_client_directory_native_env_open(mc_client_directory_native_env *env, directory_state *state,
                                    directory_error *error) {
    (void)env;
    (void)state;
    return result(error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH, 0,
                  "APPDATA Windows provider not selected on this platform");
}

mc_client_game_directory_status
mc_client_directory_native_env_close(mc_client_directory_native_env *env, directory_error *error) {
    if (env == NULL)
        return result(error, MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT, 0,
                      "native environment close");
    return MC_CLIENT_GAME_DIRECTORY_OK;
}
#endif
void mc_client_directory_native_home_free(mc_client_directory_native_home *home) {
    if (home != NULL) {
        free(home->owned);
        *home = (mc_client_directory_native_home){0};
    }
}
