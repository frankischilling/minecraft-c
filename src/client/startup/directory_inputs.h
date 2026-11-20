#ifndef MC_CLIENT_STARTUP_DIRECTORY_INPUTS_H
#define MC_CLIENT_STARTUP_DIRECTORY_INPUTS_H
#include "game_directory.h"
#include "mc/world/info/info.h"
#include <stddef.h>
typedef struct mc_client_directory_inputs mc_client_directory_inputs;

typedef enum mc_client_directory_platform {
    MC_CLIENT_DIRECTORY_WINDOWS,
    MC_CLIENT_DIRECTORY_MACOS,
    MC_CLIENT_DIRECTORY_SOLARIS,
    MC_CLIENT_DIRECTORY_LINUX,
    MC_CLIENT_DIRECTORY_UNKNOWN
} mc_client_directory_platform;

/* Native default-property provider lifetime. Create performs no discovery or
 * filesystem operation. Queries are lazy/cached once; each successful String
 * getter returns its own counted occurrence of the SAME immutable cached
 * mc_world_info_name. max_allocation_bytes bounds each actual owner/String
 * allocation, not a global JVM heap. Environment/profile stays immutable for
 * the whole provider lifetime; use is exclusive. This does not implement a
 * mutable java.lang.System property map or its cleared/missing property case.
 * Native discovery failure is NEVER fabricated as an absent value/default-dot.
 * Source caller owns class/enum activation and the exact branch/query order. */
mc_client_game_directory_status
mc_client_directory_inputs_create(const mc_client_game_directory_profile *profile,
                                  size_t max_allocation_bytes, mc_client_directory_inputs **out,
                                  mc_client_game_directory_error *error);
mc_client_game_directory_status
mc_client_directory_inputs_retain(mc_client_directory_inputs *inputs,
                                  mc_client_game_directory_error *error);
mc_client_game_directory_status
mc_client_directory_inputs_release(mc_client_directory_inputs **occurrence,
                                   mc_client_game_directory_error *error);
/* Successful native default home is present. This actual raw property value
 * has no .minecraft suffix, parent File, exists/mkdirs or normalization. */
mc_client_game_directory_status
mc_client_directory_inputs_home(mc_client_directory_inputs *inputs, mc_world_info_name **owned,
                                mc_client_game_directory_state *prefix,
                                mc_client_game_directory_error *error);
/* Real native identity query/classification only; no invented os.name value.
 * Full property overrides/mutation and other native providers remain explicit. */
mc_client_game_directory_status
mc_client_directory_inputs_platform(mc_client_directory_inputs *inputs,
                                    mc_client_directory_platform *out,
                                    mc_client_game_directory_error *error);
/* Query only when the caller reaches Windows APPDATA. Present-empty returns a
 * nonnull empty String and present=true, independently of home fallback. */
mc_client_game_directory_status mc_client_directory_inputs_appdata(
    mc_client_directory_inputs *inputs, bool *present, mc_world_info_name **nullable_owned,
    mc_client_game_directory_state *prefix, mc_client_game_directory_error *error);
#endif
