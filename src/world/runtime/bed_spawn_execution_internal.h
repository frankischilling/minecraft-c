#ifndef MC_WORLD_RUNTIME_BED_SPAWN_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_BED_SPAWN_EXECUTION_INTERNAL_H

#include "blocks_execution_internal.h"
#include "mc/world/runtime/bed_spawn.h"
#include "solid_support_execution_internal.h"

/* Actual VE.f only, sharing the public complete search body. Caller already
 * owns the exact nullable World argument before later source arguments. Bed
 * Registry and defining World code_classes are independent supplied graphs;
 * neither is inferred from World, Chunk.d or the other Registry.
 *
 * Frame, result, error, status and output are required/fresh/disjoint. Keep the
 * large frame/error in stable caller heap storage through checked unwind.
 * A NULL World token is admitted and reaches VE activation0 then Source NPE4.
 * Native preflight preserves error/status/out. Missing context never falls back.
 * A false bool, including raw OK/native refusal, stops before any later event
 * completion, facing read, query or position publication. Each successful quiet
 * query permits lower-frame reuse; failed selections/continuations stay intact.
 *
 * Helper retains reached ordinary World calls on every incomplete invocation,
 * including raw OK/native refusal. Caller restores only known scope tokens,
 * drains explicit result records, retires the bed error and clears all lower
 * typed views before releasing their owners/frame. No scope push/pop, native
 * completion, Source replay or context/Math retirement occurs here. The GS
 * validation/captured-provider wrapper remains legacy and separately unjoined. */
typedef struct mc_world_bed_spawn_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_entity_tick_execution_site receiver_site;
    mc_world_runtime_execution_result *owned;
    mc_world_block_execution scalar;
    mc_world_solid_support_execution support;
    bool entered, native_stopped, completed;
} mc_world_bed_spawn_execution;

void mc_world_bed_spawn_execution_init(mc_world_bed_spawn_execution *, mc_entity_tick_execution *,
                                       mc_world_runtime_classes *, mc_world_runtime_execution_code,
                                       mc_entity_tick_execution_site,
                                       mc_world_runtime_execution_result *);
bool mc_world_bed_spawn_exit_in_execution(mc_world_bed_spawn_execution *, mc_world_runtime *,
                                          mc_registry_classes *, int32_t, int32_t, int32_t, int32_t,
                                          const mc_world_bed_spawn_limits *,
                                          mc_player_coordinates **, mc_world_bed_spawn_error *,
                                          mc_world_storage_status *);

#endif
