#ifndef MC_WORLD_RUNTIME_SOLID_SUPPORT_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_SOLID_SUPPORT_EXECUTION_INTERNAL_H

#include "block_queries_internal.h"
#include "collision/blocks_execution_internal.h"

/* SAME held support body. Caller supplies the actual defining World classes,
 * independently of the evaluated World and mutable Chunk.d. Its Block.m is
 * captured before the one collision GET7 and indexed only after continuation.
 * Caller keeps its ordinary entered World call and all defining/provider/
 * Store/import graphs live and exclusive.
 *
 * Fresh frame/result/status/out are disjoint and stable in caller-owned storage.
 * Native preflight preserves status/out/detail. Missing context never selects
 * legacy. The bool authorizes continuation; raw status alone does not. Lower
 * diagnostics remain here on every refusal, including actual OK/native failure.
 * No scope, pending completion, retry or owner retirement occurs here. Caller
 * restores exact recorded scopes, drains native work and clears typed borrows
 * before releasing the supplying owners or this frame. */
typedef struct mc_world_solid_support_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_entity_tick_execution_site receiver_site;
    mc_world_runtime_execution_result *owned;
    mc_world_collision_error lower;
    mc_world_solid_support_detail detail;
    bool entered, native_stopped, completed;
} mc_world_solid_support_execution;

void mc_world_solid_support_execution_init(mc_world_solid_support_execution *,
                                           mc_entity_tick_execution *, mc_world_runtime_classes *,
                                           mc_world_runtime_execution_code,
                                           mc_entity_tick_execution_site,
                                           mc_world_runtime_execution_result *);
bool mc_world_solid_support_detailed_held_in_execution(mc_world_solid_support_execution *,
                                                       mc_world_runtime *, int32_t, int32_t,
                                                       int32_t, bool *, mc_world_storage_status *);

#endif
