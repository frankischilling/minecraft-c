#ifndef MC_WORLD_RUNTIME_COLLISION_BLOCKS_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_COLLISION_BLOCKS_EXECUTION_INTERNAL_H

#include "../runtime_execution_internal.h"
#include "mc/world/runtime/collision/boxes.h"

/* ONE actual complete collision ID/metadata body, shared with the existing held
 * entry. Caller captured/owns this exact nullable World BEFORE later source
 * arguments, and keeps the real defining runtime/Registry/Entity code and
 * provider/Store/import/resource graph live/exclusive. code is the unchanged
 * defining Chunk caller VALUE bundle, never inferred from World or Chunk.d.
 * No source namespace/READY/side gate or early initializer is introduced.
 *
 * Required owned is fresh/stable/disjoint enclosing-frame storage. execution,
 * out and optional error are disjoint from it and from actual owners. error,
 * when supplied, is initialized caller scratch; outer collision source PCs are
 * preserved just as in the held entry. Missing execution/result never selects
 * legacy mode. Exact current checked IDLE-node admission precedes World source
 * fields; HELD/ARMED/malformed refuses without consuming their rights. Binding
 * is distinct from entered call: this child pairs only its own World call.
 * Invocation-NULL Source attribution belongs to its actual source caller.
 *
 * Domain guards, normal/Blank/Nibble branches and the actual post-get Store
 * lease/read/release are unchanged. Only the one reached GET uses Kernel2;
 * the kernel alone reserves/finishes. Full actual Source child projection is
 * primary; owned separately keeps first participating native ownership error,
 * actual selection and all pending continuations before that projection.
 * Completed failed loader pointer is rewired to ONLY the explicit stable
 * selected view; NONE stays NULL. A refused selected-view read clears that
 * unsafe outward raw borrow, keeps Source cause/token and notes native error.
 * NULL error still retains the exact actual selected outcome in owned.
 *
 * Actual GET success with native completion refusal stops before scalar payload
 * work and preserves out/pending. Checked continuation retry never resumes this
 * Source body. Only a completed successful NONE selection is cleared before
 * later nonquery failures. Failed/pending outcomes remain untouched. This child
 * never begins/pops caller scope, cancels an attempt, replays Source, retires
 * context/Math, restores fields or supplies a missing/generic scalar fallback.
 * Source failure remains primary even if native selected-view capture refuses.
 *
 * Caller preserves the same complete result until genuine checked child unwind
 * restores the exact pending World, then native-only finish_pending as needed.
 * Caller clears typed diagnostic views before actual lease/World retirement.
 * Parent SL frame/result/classifier/cleanup threading remains a separate OPEN
 * join. Other collision virtual/scan paths remain legacy. Unjoined import,
 * generation/population/captured-provider and foreign-Store graphs retain their
 * existing external lifetime contract; arbitrary cold/full SL is not closed.
 */
mc_world_storage_status mc_world_collision_block_value_in_execution(
    mc_entity_tick_execution *, mc_world_runtime *, mc_world_runtime_execution_code, int32_t,
    int32_t, int32_t, bool metadata, int32_t *, mc_world_collision_error *,
    mc_world_runtime_execution_result *);

/* Ordinary caller VALUE provenance, not an owner. Registry and factory are
 * actual independently held defining graphs for the reached Chunk lookup/static
 * and Tile factory operations. Do not infer either from Block/World/Chunk fields.
 * NULL borrows are not tested until their actual reached lower static/factory use.
 * The acquisition bundle passes unchanged to every actual owned kernel call. */
typedef struct mc_world_collision_block_execution_code {
    mc_world_runtime_execution_code chunk;
    mc_registry_classes *tile_registry;
    mc_world_runtime_classes *tile_factory_classes;
    mc_entity_tick_execution_site lazy_receiver_site;
} mc_world_collision_block_execution_code;

/* SAME actual virtual collision-box/state-shape bodies, including carried
 * MovingPiston recursion. Defining code_classes owns the actual Block Registry
 * and Entity pool; code independently names reached Tile lookup/factory graphs.
 * The exact nullable World and mutable Chunk.d are independent Source operands,
 * never code-owner selectors. No equality between those defining graphs is added.
 * Caller holds this defining graph, every reached provider/Store/import graph,
 * and the evaluated nullable World scope before later Source arguments.
 * code.chunk and code.lazy_receiver_site name actual defining Chunk activation
 * and lazy lm.d/hi.d receiver loads; these are caller-supplied VALUE provenance.
 *
 * Required owned is fresh/stable/disjoint caller-frame result storage; status
 * is a separate required output. All outputs, scratch and owners are disjoint.
 * Initialized error scratch is optional and
 * preserves outer collision PCs as in the held entries. Native preflight
 * initializes owned ONLY, preserving status/error/out; missing execution/result
 * never selects legacy. The returned bool is the continuation condition, not
 * an encoding of Source status. status is assigned only after the actual shared
 * body returns. Inspect owned first; a raw OK status cannot authorize work.
 * No entered World call is added for a Source-unused World. Only reached scalar
 * and Tile getters enter/leave their own exact calls and use the owned kernel.
 *
 * Each lower getter captures its actual selection/pending/close result before
 * typed projection. The one stable result is reused only after complete success
 * with no selection or live continuation; first failure/native refusal stops
 * every recursive Source continuation. All native records remain in owned.
 * Source singleton stores, argument/query order, independent Direction reads,
 * and actual ordinary Tile/pooled-box hold cleanup remain unchanged. No push,
 * pop, context/Math retirement, fallback, retry, inference or field repair here.
 * Existing unjoined import/generation/population/captured-provider lifetime
 * obligations remain the caller's contract; these entries do not close them.
 */
bool mc_world_collision_block_box_in_classes_in_execution(
    mc_entity_tick_execution *, mc_world_runtime_classes *, mc_world_runtime *,
    mc_world_collision_block_execution_code, const mc_block *, int32_t, int32_t, int32_t,
    mc_entity_bounds **, mc_world_collision_error *, mc_world_storage_status *,
    mc_world_runtime_execution_result *);
bool mc_world_collision_block_state_shape_in_classes_in_execution(
    mc_entity_tick_execution *, mc_world_runtime_classes *, mc_world_runtime *,
    mc_world_collision_block_execution_code, const mc_block *, int32_t, int32_t, int32_t,
    mc_world_collision_error *, mc_world_storage_status *, mc_world_runtime_execution_result *);

#endif
