#ifndef MC_CLIENT_STARTUP_GAME_DIRECTORY_H
#define MC_CLIENT_STARTUP_GAME_DIRECTORY_H

#include "mc/core/java/reader.h"

typedef struct mc_client_game_directory mc_client_game_directory;

typedef enum mc_client_game_directory_status {
    MC_CLIENT_GAME_DIRECTORY_OK = 0,
    MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT,
    MC_CLIENT_GAME_DIRECTORY_ALLOCATION_FAILED,
    MC_CLIENT_GAME_DIRECTORY_LIMIT,
    MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING,
    MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_PATH,
    MC_CLIENT_GAME_DIRECTORY_NATIVE_IO_ERROR,
    MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED,
    MC_CLIENT_GAME_DIRECTORY_BUSY,
    MC_CLIENT_GAME_DIRECTORY_STATE_ERROR
} mc_client_game_directory_status;

typedef struct mc_client_game_directory_error {
    mc_client_game_directory_status status;
    int system_error;
    const char *operation;
} mc_client_game_directory_error;

typedef struct mc_client_game_directory_profile {
    /* Linux pw_dir decoding; DEFAULT uses actual native charset discovery.
     * Windows Profile/APPDATA use UTF16 native APIs independently of this field.
     * A filename needing bytes different from the existing UTF8 File boundary
     * is explicitly unfinished, rather than selected under a different name. */
    mc_java_reader_charset filesystem_charset;
} mc_client_game_directory_profile;

typedef enum mc_client_game_directory_home_origin {
    MC_CLIENT_GAME_DIRECTORY_HOME_NONE = 0,
    MC_CLIENT_GAME_DIRECTORY_HOME_ACCOUNT,
    MC_CLIENT_GAME_DIRECTORY_HOME_ACCOUNT_FALLBACK,
    MC_CLIENT_GAME_DIRECTORY_HOME_PROFILE,
    MC_CLIENT_GAME_DIRECTORY_HOME_LEGACY_PROFILE,
    MC_CLIENT_GAME_DIRECTORY_HOME_WINDOWS_FALLBACK
} mc_client_game_directory_home_origin;

/* Actual native route and completed operation prefix; no Java measurements. */
typedef struct mc_client_game_directory_state {
    mc_client_game_directory_home_origin home_origin;
    mc_java_reader_charset filesystem_charset;
    bool selected_directory, appdata_present;
    bool initial_exists, mkdirs_reached, directory_created;
    size_t exists_calls, mkdir_calls, canonicalize_calls;
    int last_native_error;
    int32_t profile_query_hresult, legacy_profile_query_hresult;
} mc_client_game_directory_state;

#define MC_CLIENT_GAME_DIRECTORY_MAX_PATH_BYTES 1048576u
#define MC_CLIENT_GAME_DIRECTORY_MAX_LOOKUP_BYTES 1048576u
#define MC_CLIENT_GAME_DIRECTORY_MAX_PARENT_CALLS 1024u

/* NULL selected_directory uses the native default selection. Nonnull text is
 * an explicit UTF8 directory input, including an empty string; it is never a
 * user.home override. No HOME/assets/cwd/SDL preference fallback is supplied.
 * Creation performs real exists -> mkdirs, accepting an existing file just as
 * the source outer exists check does. The result stays owned above settings.
 * NULL profile selects actual native defaults. No process locale is changed.
 * Owner output is unchanged on failure. Optional prefix receives the actual
 * entered native prefix on success or failure, after argument admission.
 * Error operation names have static lifetime. Success retains no open handles;
 * a failed native release is fatal and reported separately from source false. */
mc_client_game_directory_status mc_client_game_directory_create(
    const char *selected_directory, const mc_client_game_directory_profile *profile,
    mc_client_game_directory **out, mc_client_game_directory_state *prefix,
    mc_client_game_directory_error *error);

const char *mc_client_game_directory_path(const mc_client_game_directory *directory);
mc_client_game_directory_status
mc_client_game_directory_read_state(const mc_client_game_directory *directory,
                                    mc_client_game_directory_state *out,
                                    mc_client_game_directory_error *error);
/* Memory retirement only: no options I/O, save or directory deletion. */
void mc_client_game_directory_free(mc_client_game_directory *directory);

#endif
