#ifndef MC_CLIENT_STARTUP_DIAGNOSTIC_BUILDER_H
#define MC_CLIENT_STARTUP_DIAGNOSTIC_BUILDER_H

#include "mc/entity/player/player.h"

typedef struct mc_client_diagnostic_builder mc_client_diagnostic_builder;

typedef struct mc_client_diagnostic_builder_error {
    mc_player_status status;
    const char *operation;
    bool native_policy;
} mc_client_diagnostic_builder_error;

typedef struct mc_client_diagnostic_builder_state {
    size_t count, capacity;
    bool constructed, failed, active;
} mc_client_diagnostic_builder_state;

/* Selected StringBuilder(), append(String), append(int), toString. Unified
 * native UTF16 storage preserves values/count/copy semantics, not JDK compact
 * backing-array identity or Java OOM scheduling. MAX_TEXT_UNITS is an explicit
 * native bound; no Java overflow/exception is fabricated for that admission.
 * A failed candidate retains its buffer for inspection/native retirement. */
mc_player_status mc_client_diagnostic_builder_allocate(mc_client_diagnostic_builder **,
                                                       mc_client_diagnostic_builder_error *);
mc_player_status mc_client_diagnostic_builder_construct(mc_client_diagnostic_builder *,
                                                        mc_client_diagnostic_builder_error *);
/* NULL view represents Java null -> "null". */
mc_player_status mc_client_diagnostic_builder_append(mc_client_diagnostic_builder *,
                                                     const mc_player_string_view *,
                                                     mc_client_diagnostic_builder_error *);
mc_player_status mc_client_diagnostic_builder_append_int(mc_client_diagnostic_builder *, int32_t,
                                                         mc_client_diagnostic_builder_error *);
/* Returns an owned occurrence. Nonempty creates a real immutable copy. Empty
 * uses the canonical eternal empty literal (no allocation/global interning).
 * Existing String retain/release convention also accepts that occurrence. */
mc_player_status mc_client_diagnostic_builder_string(mc_client_diagnostic_builder *,
                                                     mc_player_string **,
                                                     mc_client_diagnostic_builder_error *);
bool mc_client_diagnostic_builder_inspect(const mc_client_diagnostic_builder *,
                                          mc_client_diagnostic_builder_state *);
mc_player_status mc_client_diagnostic_builder_retire(mc_client_diagnostic_builder **,
                                                     mc_client_diagnostic_builder_error *);

#endif
