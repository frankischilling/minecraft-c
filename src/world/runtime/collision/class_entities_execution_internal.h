#ifndef MC_WORLD_RUNTIME_COLLISION_CLASS_ENTITIES_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_COLLISION_CLASS_ENTITIES_EXECUTION_INTERNAL_H

#include "class_entities_internal.h"
#include "entities_execution_internal.h"

/* ONE actual client FD.a(Class,EQ) query and LM.a(Class,EQ,List) gather.
 * Caller owns exact evaluated nullable World BEFORE Class/pool/bounds arguments,
 * actual defining Class/Entity/IN namespace and full external provider/Store graph.
 * code is unchanged defining Chunk provenance, never World/Chunk.d inference.
 * exec->math is stable real storage; declared IN may be NULL until reached use.
 * Required fresh/disjoint stable error/status/result; Native preflight initializes
 * error/result, preserves out/status and never selects legacy on missing context.
 * Bool is exact body continuation, independently of the raw actual status slot.
 * Full result is captured by the shared actual owned acquisition helper BEFORE
 * projections; completed quiet success alone permits another actual loop GET.
 * Original presence probes, fresh List, live slice/List/Class/Bounds order and
 * failed partial-result release are unchanged. Result List owns every occurrence
 * and its ordinary World local lease. No scope/Math retirement or Source retry.
 * Requires the actual nearby candidate's entities_execution helper definition;
 * no fallback shim is declared or supplied by this package.
 */
bool mc_world_runtime_class_entities_in_execution(
    mc_entity_tick_execution *, mc_world_runtime *, mc_world_runtime_execution_code,
    const mc_entity_class_literal *, const mc_entity_bounds *, mc_world_class_entity_list **,
    mc_world_nearby_error *, mc_world_storage_status *, mc_world_runtime_execution_result *);

#endif
