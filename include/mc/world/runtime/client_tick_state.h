#ifndef MC_WORLD_RUNTIME_CLIENT_TICK_STATE_H
#define MC_WORLD_RUNTIME_CLIENT_TICK_STATE_H

#include "mc/world/runtime/acquisition_error.h"
#include "mc/world/runtime/entities.h"

typedef struct mc_world_client_tick_state_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode;
    bool native_policy;
    mc_world_storage_error cause;
} mc_world_client_tick_state_error;

/* Literal client Minecraft.k field operations, on the canonical World cells.
 * These are not invented Java World methods. Difficulty stores q without a
 * clamp; decrement reads n and stores n-1 only when positive. Spawn flags are
 * the complete actual fd.a(ZZ)V, storing N2 then O7 and returning10. Native
 * admission requires a live client GAME owner, including RAW/failed prefixes;
 * there is no additional source construction-phase check. No callback, Random,
 * allocation, save or tick is performed. All access is exclusive/disjoint. */
mc_world_storage_status
mc_world_runtime_client_difficulty_store(mc_world_runtime *, int32_t difficulty,
                                         mc_world_client_tick_state_error *);
mc_world_storage_status
mc_world_runtime_client_lightning_decrement(mc_world_runtime *, mc_world_client_tick_state_error *);
mc_world_storage_status mc_world_runtime_client_spawn_flags(mc_world_runtime *, bool hostile,
                                                            bool peaceful,
                                                            mc_world_client_tick_state_error *);

typedef struct mc_world_refresh_entity_call mc_world_refresh_entity_call;

typedef struct mc_world_refresh_entity_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, provider_bytecode;
    bool native_policy;
    mc_world_storage_error cause;
    const mc_world_runtime_acquisition_error *acquisition;
    const mc_world_entity_error *publication;
    mc_entity_status entity;
} mc_world_refresh_entity_error;

typedef struct mc_world_refresh_entity_state {
    bool active, returned, contains, appended;
    unsigned int source_pc;
    int32_t center_x, center_z;
    size_t chunk_calls, chunk_returns, comparisons;

    struct {
        int32_t x, z;
    } chunks[25];

    mc_world_runtime *world; /* Borrowed from this call's native binding. */
    mc_entity *entity;       /* Borrowed from this call's native occurrence. */
    const mc_world_refresh_entity_error *error;
} mc_world_refresh_entity_state;

/* Actual fd.g(sn)V, distinct from entity insertion and fd.g() entity ticking.
 * Evaluate floor(X/16), then floor(Z/16); acquire the radius2 square X-major,
 * Z-minor with actual fd.c(II), discarding each returned Chunk immediately.
 * No Chunk lease spans another getter because no returned Chunk is read.
 * Then actual World.b.contains uses Entity.equals (same namespace and ID),
 * appending the exact Entity only when absent. No Chunk membership, Player
 * list, Entity.world rewrite or WorldAccess callback is performed.
 *
 * Once allocated, out receives a retained call on success or failure. It owns
 * the World/Entity occurrences and immutable child value diagnostics until
 * retire; source effects are never rolled back or replayed. Earlier native
 * admission failure preserves out. out must begin NULL. Source-null Entity
 * fails at1; all graph owners and error/output storage remain exclusive and
 * disjoint. The native 25-get ceiling reports LIMIT if Java's wrapping loop
 * would continue beyond the ordinary square; it never reports that path as a
 * completed original invocation. FP controls are checked before division7. */
mc_world_storage_status
mc_world_runtime_refresh_entity_begin(mc_world_runtime *, mc_entity *,
                                      mc_world_refresh_entity_call **out,
                                      mc_world_refresh_entity_error *admission_error);
bool mc_world_runtime_refresh_entity_inspect(const mc_world_refresh_entity_call *,
                                             mc_world_refresh_entity_state *);
/* Checked native cleanup only. Active calls preserve the slot and report BUSY;
 * successful cleanup consumes it without changing source fields or containers. */
mc_world_storage_status
mc_world_runtime_refresh_entity_retire(mc_world_refresh_entity_call **,
                                       mc_world_refresh_entity_error *cleanup_error);

#endif
