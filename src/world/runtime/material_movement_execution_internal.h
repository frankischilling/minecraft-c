#ifndef MC_WORLD_RUNTIME_MATERIAL_MOVEMENT_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_MATERIAL_MOVEMENT_EXECUTION_INTERNAL_H

#include "material_movement_internal.h"
#include "registry_execution_internal.h"

typedef enum mc_world_material_scan_kind {
    MC_WORLD_MATERIAL_SCAN_MATCH,
    MC_WORLD_MATERIAL_SCAN_LIQUID,
    MC_WORLD_MATERIAL_SCAN_ACCELERATE
} mc_world_material_scan_kind;

typedef enum mc_world_material_scan_leaf {
    MC_WORLD_MATERIAL_SCAN_NO_LEAF,
    MC_WORLD_MATERIAL_SCAN_SCALAR_LEAF,
    MC_WORLD_MATERIAL_SCAN_MATERIAL_LEAF
} mc_world_material_scan_leaf;

/* Compact last-query observation, not a Source classifier or an owner.
 * returned means the selected native C entry returned. SCALAR status is that
 * entry's exact status, including unreached native admission. For MATERIAL,
 * material_completed is its actual bool continuation return; status is the
 * raw lower-status slot with the material leaf's unchanged contract. In
 * particular ownership refusal does NOT establish whether that lower call
 * occurred. Do not infer it from this slot, a sentinel, or error contents.
 * NO_LEAF includes failures before the first query. A last-query receipt may
 * remain after a later Registry/quota/Math/pool failure; it is not an active
 * failure tag for that later operation.
 */
typedef struct mc_world_material_scan_query {
    mc_world_material_scan_leaf leaf;
    bool returned, material_completed;
    mc_world_storage_status status;
} mc_world_material_scan_query;

/* ONE existing three-kind run/execute/recursive-fluid implementation. Legacy
 * and existing Math-only wrappers pass no owned descriptor and keep their
 * domain. The bool here is continuation success; only true publishes *out,
 * whose actual Source answer may be false. A native-only stop must not become
 * success because the outer Source diagnostic's status is still OK.
 *
 * The caller has already retained the exact evaluated nullable World BEFORE
 * subsequent Source arguments. This function neither pushes/pops Worlds nor
 * retires execution, Math, receipts or pending children. It preflights that
 * current node before any entered Source body; HELD/ARMED is native refusal.
 * Caller supplies the actual immutable client defining-code classes borrow
 * and unchanged Chunk code VALUE, independently of Entity.world/Chunk.d. The
 * currently joined nonNULL receiver requires world->classes == code_classes;
 * this native qualification is no Source active use/READY test. All Registry
 * and vector-class operations use that defining namespace at their original
 * reached sites. No namespace is guessed from a callback's mutated receiver.
 * execution->math is real nonNULL context storage; its declared owner may be
 * NULL and is not inspected/retained until the existing reached IN use.
 *
 * Required error/query/owned are fresh, disjoint, caller-stable storage. Native
 * preflight initializes error with its native diagnostic, not preserved poison;
 * this is not a Source failure when the independent owned channel refuses.
 * The selected error is used directly through owned execution, without a
 * stack-local diagnostic followed by a return copy. The legacy route keeps
 * its original local-and-copy behavior. The result
 * is the actual precharged runtime pending/close result; successful queries
 * can reuse its now-drained storage, while the first failure stops immediately.
 * Preserve it plus the active typed block/material scratch verbatim through
 * checked native unwind. Inspect ownership_status independently and first;
 * Source classification remains the parent's exact current typed-cause task,
 * never inferred from a status, bool, old Math failure or inactive error arm.
 * An ownership-only stop preserves reached typed scratch rather than clearing
 * it, even when the outer error.status remains OK. Native retry never resumes
 * this scanner. All locally acquired vectors are released once before return.
 *
 * Keep the actual defining Registry/Entity classes, evaluated World and the
 * complete concrete provider/Store/import/loader producer graph jointly live
 * and exclusive through diagnostics and cleanup. Registered/recursive wrapped
 * Blocks belong to that same defining namespace. Foreign graphs, generic
 * captured-provider/population ownership, Entity/Base callers, parent Source
 * classification and full SL remain separate OPEN joins. No public error or
 * Entity ownership layout grows, and no Source field or query is repaired.
 */
bool mc_world_runtime_material_scan_in_execution(
    mc_entity_tick_execution *, mc_world_runtime_classes *, mc_world_runtime *,
    mc_world_runtime_execution_code, const mc_entity_bounds *, mc_material *, mc_entity *,
    mc_world_material_scan_kind, const mc_world_material_movement_limits *, bool *,
    mc_world_material_movement_error *, mc_world_material_scan_query *,
    mc_world_runtime_execution_result *);

#endif
