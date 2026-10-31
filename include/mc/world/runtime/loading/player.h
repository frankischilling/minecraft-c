#ifndef MC_WORLD_RUNTIME_LOADING_PLAYER_H
#define MC_WORLD_RUNTIME_LOADING_PLAYER_H

#include "mc/entity/nbt/errors.h"
#include "mc/world/info/info.h"
#include "mc/world/runtime/entities.h"

typedef struct mc_world_player_load_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    unsigned int bytecode, catch_bytecode;
    bool native_policy, source_exception, caught_exception;
    bool print_stack_trace_pending, source_returned;
    bool saved_data_present, nbt_returned, info_cleared, publication_returned;
    mc_world_storage_error cause;
    mc_world_info_status info;
    mc_entity_error entity;
    mc_entity_nbt_error nbt;
    mc_world_entity_error publication;
} mc_world_player_load_error;

void mc_world_player_load_error_init(mc_world_player_load_error *);

/* Actual client fd.a(gs), once on a live client GAME World and the same
 * application LocalPlayer/Registry/profile. NULL receivers reach their source
 * dereference; other Entity kinds are native admission errors. Captured Info
 * and Player tag occurrences remain live across the real inherited NBT read.
 * Only complete read success reloads current World.x and clears its Player;
 * the actual publication follows and its ordinary boolean is discarded.
 * Current genuine ok map-cache (or NULL) makes instanceof kx false. No
 * centered-cache operation, fake Entity, reset, input or tick is supplied.
 *
 * Known source Exceptions retain their original bytecode/typed child and stop
 * at catch_bytecode88, the real unfinished Exception.printStackTrace call.
 * Native limits, unsupported work and ordinary native-adapter failures are not
 * Java Exceptions. Success alone marks source_returned. Effects are never
 * rolled back or replayed by this operation; do not retry a failed invocation.
 *
 * The complete publication receipt is BORROWED from that exact callback owner,
 * never reconstructed from ambient state. Capture it with
 * mc_world_access_failure_retain before retiring the supplying graph; release
 * that capture before graph retirement. All other error fields are values or
 * static strings. Output/error views must remain disjoint from live owners.
 * Keep World, Player and graph dependencies live/exclusive through the call.
 * No error-owned native occurrence is created. NULL error is supported. */
mc_world_storage_status mc_world_runtime_load_player(mc_world_runtime *, mc_entity *,
                                                     mc_world_player_load_error *);

#endif
