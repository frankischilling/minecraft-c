#ifndef MC_CLIENT_STARTUP_DIRECTORY_NATIVE_H
#define MC_CLIENT_STARTUP_DIRECTORY_NATIVE_H
#include "../../platform/files/directory.h"
#include "game_directory.h"

typedef struct mc_client_directory_native_home {
    mc_file_native_char *owned;
    const mc_file_native_char *text;
    size_t length;
} mc_client_directory_native_home;

typedef struct mc_client_directory_native_env {
    mc_file_native_char *block;
    const mc_file_native_char *value;
    bool present, active, release_issued, release_failed;
    void *issued_token;
} mc_client_directory_native_env;

/* Shared actual discovery only: no path suffix, File, exists/mkdirs or source
 * System-property map. Records use caller storage; no extra native allocation
 * precedes the old discovery bodies. Home free is memory-only. Env close is
 * active-guarded, consumes its native release right before issuing once, and
 * retains any uncertain issued token only for diagnosis. */
mc_client_game_directory_status mc_client_directory_native_home_query(
    mc_client_directory_native_home *out, const mc_client_game_directory_profile *profile,
    mc_client_game_directory_state *prefix, mc_client_game_directory_error *error);
void mc_client_directory_native_home_free(mc_client_directory_native_home *home);
mc_client_game_directory_status
mc_client_directory_native_env_open(mc_client_directory_native_env *env,
                                    mc_client_game_directory_state *prefix,
                                    mc_client_game_directory_error *error);
mc_client_game_directory_status
mc_client_directory_native_env_close(mc_client_directory_native_env *env,
                                     mc_client_game_directory_error *error);
#endif
