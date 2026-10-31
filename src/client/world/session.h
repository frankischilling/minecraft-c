#ifndef MC_CLIENT_WORLD_SESSION_H
#define MC_CLIENT_WORLD_SESSION_H

#include "mc/world/runtime/runtime.h"

typedef struct mc_client_world_session mc_client_world_session;

typedef struct mc_client_world_session_error {
    mc_world_storage_error storage;
    mc_registry_error registry;
    mc_world_info_status name_status;
    int32_t clock_status;
} mc_client_world_session_error;

/* Borrow the application's matching Registry/WorldClasses until retirement.
 * Allocation performs no class active use, clock, file or Random operation. */
bool mc_client_world_session_allocate(mc_world_runtime_classes *classes,
                                      mc_registry_classes *registry, mc_client_world_session **out,
                                      mc_client_world_session_error *error);

/* Exactly one open attempt per owner. Initialize actual Blocks/Biomes, open the
 * selected read/write root, then run CLIENT_LOAD_OR_CREATE on its real World.
 * Earlier constructor/file effects remain on failure; retire the same session.
 * Tests must supply a fresh directory or an owned world copy. No save is implicit.
 * max_chunks bounds both the actual provider and handler; it must fit int32. */
bool mc_client_world_session_open(mc_client_world_session *session, const char *directory,
                                  mc_nbt_string name, int64_t seed, size_t max_chunks,
                                  mc_client_world_session_error *error);
mc_world_runtime *mc_client_world_session_world(const mc_client_world_session *session);

/* Terminal native teardown, separate from gameplay save/unload. Retire all
 * mesh/view/lease uses first. Failure retains *session and its remaining owners
 * for retry; earlier terminal retirement steps may already have completed.
 * A failed retirement prevents further world access through this owner.
 * Success consumes/nulls *session. NULL *session succeeds. */
bool mc_client_world_session_retire(mc_client_world_session **session,
                                    mc_client_world_session_error *error);

#endif
