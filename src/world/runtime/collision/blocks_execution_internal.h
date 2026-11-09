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

#endif
