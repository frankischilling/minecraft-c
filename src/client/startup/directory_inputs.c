#include "directory_inputs.h"
#include "../../world/storage/java_file_text.h"
#include "directory_native.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wchar.h>
#include <windows.h>
#else
#include <errno.h>
#include <sys/utsname.h>
#endif
struct mc_client_directory_inputs {
    size_t holds, maximum;
    bool active, retiring;
    mc_client_game_directory_profile profile;
    mc_client_game_directory_state prefix;
    bool home_attempted, platform_attempted, appdata_attempted, appdata_present;
    mc_client_game_directory_status home_status, platform_status, appdata_status;
    mc_client_game_directory_error home_error, platform_error, appdata_error, cleanup_error;
    mc_client_directory_platform platform;
    mc_world_info_name *home, *appdata, *pending_appdata;
    mc_client_directory_native_home raw_home;
    mc_client_directory_native_env raw_env;
};

static mc_client_game_directory_status result(mc_client_game_directory_error *e,
                                              mc_client_game_directory_status s, int code,
                                              const char *op) {
    if (e != NULL)
        *e = (mc_client_game_directory_error){s, code, op};
    return s;
}

static mc_client_game_directory_status admitted(mc_client_directory_inputs *i,
                                                mc_client_game_directory_error *e) {
    if (i == NULL)
        return result(e, MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT, 0, "directory inputs receiver");
    if (i->active)
        return result(e, MC_CLIENT_GAME_DIRECTORY_BUSY, 0, "directory inputs active operation");
    if (i->retiring)
        return result(e, MC_CLIENT_GAME_DIRECTORY_STATE_ERROR, 0,
                      "directory inputs terminal operation");
    return MC_CLIENT_GAME_DIRECTORY_OK;
}

static mc_client_game_directory_status name_error(mc_client_game_directory_error *e,
                                                  mc_world_info_status s) {
    return result(e,
                  s == MC_WORLD_INFO_LIMIT ? MC_CLIENT_GAME_DIRECTORY_LIMIT
                  : s == MC_WORLD_INFO_ALLOCATION_FAILED
                      ? MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED
                      : MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR,
                  0, "directory input String ownership");
}

static mc_client_game_directory_status native_name(mc_client_directory_inputs *i,
                                                   const mc_file_native_char *text, size_t length,
                                                   mc_world_info_name **out,
                                                   mc_client_game_directory_error *e) {
#ifdef _WIN32
    _Static_assert(sizeof(wchar_t) == sizeof(uint16_t), "Windows property UTF16");
    mc_world_info_status s =
        mc_world_info_name_copy((mc_nbt_string){(const uint16_t *)text, length}, i->maximum, out);
    return s == MC_WORLD_INFO_OK ? MC_CLIENT_GAME_DIRECTORY_OK : name_error(e, s);
#else
    (void)length;
    mc_java_file_error child;
    mc_java_file_status s = mc_file_decode_utf8(text, i->maximum, out, &child);
    if (s == MC_JAVA_FILE_OK)
        return MC_CLIENT_GAME_DIRECTORY_OK;
    return result(e,
                  s == MC_JAVA_FILE_LIMIT ? MC_CLIENT_GAME_DIRECTORY_LIMIT
                  : s == MC_JAVA_FILE_ALLOCATION_FAILED
                      ? MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED
                      : MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING,
                  child.system_error, child.operation);
#endif
}

mc_client_game_directory_status
mc_client_directory_inputs_create(const mc_client_game_directory_profile *profile, size_t maximum,
                                  mc_client_directory_inputs **out,
                                  mc_client_game_directory_error *e) {
    if (out == NULL ||
        (profile != NULL && (unsigned)profile->filesystem_charset > MC_JAVA_READER_US_ASCII))
        return result(e, MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT, 0,
                      "directory inputs arguments");
    if (maximum < sizeof(mc_client_directory_inputs))
        return result(e, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0, "directory inputs owner bound");
    mc_client_directory_inputs *i = calloc(1, sizeof(*i));
    if (i == NULL)
        return result(e, MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED, 0, "directory inputs owner");
    i->holds = 1;
    i->maximum = maximum;
    i->profile =
        profile == NULL ? (mc_client_game_directory_profile){MC_JAVA_READER_DEFAULT} : *profile;
    *out = i;
    return result(e, MC_CLIENT_GAME_DIRECTORY_OK, 0, NULL);
}

mc_client_game_directory_status
mc_client_directory_inputs_retain(mc_client_directory_inputs *i,
                                  mc_client_game_directory_error *e) {
    mc_client_game_directory_status s = admitted(i, e);
    if (s != MC_CLIENT_GAME_DIRECTORY_OK)
        return s;
    if (i->holds == SIZE_MAX)
        return result(e, MC_CLIENT_GAME_DIRECTORY_LIMIT, 0, "directory inputs holds");
    ++i->holds;
    return result(e, MC_CLIENT_GAME_DIRECTORY_OK, 0, NULL);
}

mc_client_game_directory_status
mc_client_directory_inputs_home(mc_client_directory_inputs *i, mc_world_info_name **out,
                                mc_client_game_directory_state *prefix,
                                mc_client_game_directory_error *e) {
    if (out == NULL)
        return result(e, MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT, 0, "directory home output");
    mc_client_game_directory_status s = admitted(i, e);
    if (s != MC_CLIENT_GAME_DIRECTORY_OK)
        return s;
    i->active = true;
    if (!i->home_attempted) {
        i->home_attempted = true;
        i->home_status = mc_client_directory_native_home_query(&i->raw_home, &i->profile,
                                                               &i->prefix, &i->home_error);
        if (i->home_status == MC_CLIENT_GAME_DIRECTORY_OK)
            i->home_status =
                native_name(i, i->raw_home.text, i->raw_home.length, &i->home, &i->home_error);
        if (i->home_status == MC_CLIENT_GAME_DIRECTORY_OK)
            mc_client_directory_native_home_free(&i->raw_home);
    }
    s = i->home_status;
    if (s == MC_CLIENT_GAME_DIRECTORY_OK) {
        mc_world_info_status held = mc_world_info_name_retain(i->home);
        if (held != MC_WORLD_INFO_OK)
            s = name_error(e, held);
        else
            *out = i->home;
    } else if (e != NULL)
        *e = i->home_error;
    if (prefix != NULL)
        *prefix = i->prefix;
    i->active = false;
    return s == MC_CLIENT_GAME_DIRECTORY_OK ? result(e, s, 0, NULL) : s;
}

static mc_client_game_directory_status platform_query(mc_client_directory_platform *out,
                                                      mc_client_game_directory_error *e) {
#ifdef _WIN32
    OSVERSIONINFOEXW version = {0};
    version.dwOSVersionInfoSize = (DWORD)sizeof(version);
    /* This exact JDK17 default-property provider still uses GetVersionEx.
     * Scope only its SDK deprecation warning; no version/name is fabricated. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    BOOL queried = GetVersionExW((OSVERSIONINFOW *)&version);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    if (!queried)
        return result(e, MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR, (int)GetLastError(),
                      "GetVersionExW OS identity");
    *out = version.dwPlatformId == VER_PLATFORM_WIN32_NT ||
                   version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS
               ? MC_CLIENT_DIRECTORY_WINDOWS
               : MC_CLIENT_DIRECTORY_UNKNOWN;
#else
    struct utsname identity;
    if (uname(&identity) != 0)
        return result(e, MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR, errno, "uname OS identity");
    if (strcmp(identity.sysname, "Linux") == 0)
        *out = MC_CLIENT_DIRECTORY_LINUX;
    else if (strcmp(identity.sysname, "SunOS") == 0)
        *out = MC_CLIENT_DIRECTORY_SOLARIS;
    else if (strcmp(identity.sysname, "Darwin") == 0)
        *out = MC_CLIENT_DIRECTORY_MACOS;
    else
        *out = MC_CLIENT_DIRECTORY_UNKNOWN;
#endif
    return MC_CLIENT_GAME_DIRECTORY_OK;
}

mc_client_game_directory_status
mc_client_directory_inputs_platform(mc_client_directory_inputs *i,
                                    mc_client_directory_platform *out,
                                    mc_client_game_directory_error *e) {
    if (out == NULL)
        return result(e, MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT, 0, "directory platform output");
    mc_client_game_directory_status s = admitted(i, e);
    if (s != MC_CLIENT_GAME_DIRECTORY_OK)
        return s;
    i->active = true;
    if (!i->platform_attempted) {
        i->platform_attempted = true;
        i->platform_status = platform_query(&i->platform, &i->platform_error);
    }
    s = i->platform_status;
    if (s == MC_CLIENT_GAME_DIRECTORY_OK)
        *out = i->platform;
    else if (e != NULL)
        *e = i->platform_error;
    i->active = false;
    return s == MC_CLIENT_GAME_DIRECTORY_OK ? result(e, s, 0, NULL) : s;
}

mc_client_game_directory_status
mc_client_directory_inputs_appdata(mc_client_directory_inputs *i, bool *present,
                                   mc_world_info_name **out, mc_client_game_directory_state *prefix,
                                   mc_client_game_directory_error *e) {
    if (present == NULL || out == NULL)
        return result(e, MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT, 0, "directory APPDATA output");
    mc_client_game_directory_status s = admitted(i, e);
    if (s != MC_CLIENT_GAME_DIRECTORY_OK)
        return s;
    i->active = true;
    if (!i->appdata_attempted) {
        i->appdata_attempted = true;
        i->appdata_status =
            mc_client_directory_native_env_open(&i->raw_env, &i->prefix, &i->appdata_error);
        i->appdata_present = i->raw_env.present;
        if (i->appdata_status == MC_CLIENT_GAME_DIRECTORY_OK && i->raw_env.present) {
#ifdef _WIN32
            i->appdata_status = native_name(i, i->raw_env.value, wcslen(i->raw_env.value),
                                            &i->pending_appdata, &i->appdata_error);
#else
            i->appdata_status = result(&i->appdata_error, MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH,
                                       0, "APPDATA provider not reached on this platform");
#endif
        }
        mc_client_game_directory_status closed =
            mc_client_directory_native_env_close(&i->raw_env, &i->cleanup_error);
        if (i->appdata_status == MC_CLIENT_GAME_DIRECTORY_OK &&
            closed != MC_CLIENT_GAME_DIRECTORY_OK) {
            i->appdata_status = closed;
            i->appdata_error = i->cleanup_error;
        }
        if (i->appdata_status == MC_CLIENT_GAME_DIRECTORY_OK) {
            i->appdata = i->pending_appdata;
            i->pending_appdata = NULL;
        }
    }
    s = i->appdata_status;
    if (s == MC_CLIENT_GAME_DIRECTORY_OK) {
        if (i->appdata != NULL) {
            mc_world_info_status held = mc_world_info_name_retain(i->appdata);
            if (held != MC_WORLD_INFO_OK)
                s = name_error(e, held);
        }
        if (s == MC_CLIENT_GAME_DIRECTORY_OK) {
            *present = i->appdata_present;
            *out = i->appdata;
        }
    } else if (e != NULL)
        *e = i->appdata_error;
    if (prefix != NULL)
        *prefix = i->prefix;
    i->active = false;
    return s == MC_CLIENT_GAME_DIRECTORY_OK ? result(e, s, 0, NULL) : s;
}

mc_client_game_directory_status
mc_client_directory_inputs_release(mc_client_directory_inputs **slot,
                                   mc_client_game_directory_error *e) {
    if (slot == NULL)
        return result(e, MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT, 0,
                      "directory inputs retirement slot");
    mc_client_directory_inputs *i = *slot;
    if (i == NULL)
        return result(e, MC_CLIENT_GAME_DIRECTORY_OK, 0, NULL);
    if (i->active)
        return result(e, MC_CLIENT_GAME_DIRECTORY_BUSY, 0, "directory inputs active retirement");
    if (i->holds > 1) {
        --i->holds;
        *slot = NULL;
        return result(e, MC_CLIENT_GAME_DIRECTORY_OK, 0, NULL);
    }
    i->retiring = true;
    i->active = true;
    mc_client_game_directory_status s = mc_client_directory_native_env_close(&i->raw_env, e);
    if (s != MC_CLIENT_GAME_DIRECTORY_OK) {
        i->active = false;
        return s;
    }
    mc_client_directory_native_home_free(&i->raw_home);
    mc_world_info_name_release(i->home);
    mc_world_info_name_release(i->appdata);
    mc_world_info_name_release(i->pending_appdata);
    *slot = NULL;
    free(i);
    return result(e, MC_CLIENT_GAME_DIRECTORY_OK, 0, NULL);
}
