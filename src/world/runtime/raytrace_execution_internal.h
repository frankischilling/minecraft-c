#ifndef MC_WORLD_RUNTIME_RAYTRACE_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_RAYTRACE_EXECUTION_INTERNAL_H

#include "../../core/math/execution_internal.h"
#include "collision/blocks_execution_internal.h"
#include "mc/world/runtime/raytrace.h"

/* One SAME fd.a(bt,bt,ZZ) ray body. The caller has already captured and owns
 * this exact evaluated nullable World before later arguments. Defining
 * code_classes and code are independent real held code/Registry/Entity/Tile
 * graphs; never selected from World/Chunk.d or the Block receiver. They and
 * execution/owned stay live/exclusive through checked call retirement. A NULL
 * invocation receiver is attributed by the Source caller, as in legacy API.
 *
 * Required owned is fresh/stable/disjoint enclosing-frame VALUE storage. This
 * child borrows it until call retirement; it never pushes/pops caller scopes,
 * completes a pending attempt, releases neutral receipts/Math, or resumes Source.
 * Missing execution/result/status does NOT select legacy. Checked IDLE admission
 * precedes World source fields; refusal preserves *status and *out.
 *
 * The bool is the continuation condition, separate from the actual ray status
 * and source_returned witness. Native bookkeeping refusal can stop the body
 * with status OK and source_returned=false. Never treat an OK number as a miss.
 * All actual scalar/box/state-shape getters use owned collision entries, with
 * the full result captured before projection. No native status overwrites an
 * earlier Source failure; first ownership cause remains in owned separately.
 * Results/temporary pools/bounds/collision scratch remain real call-owned work.
 * Inspect borrows expire only after successful checked retirement. Returned VF
 * must be retained independently before retirement exactly as in legacy API.
 *
 * Existing unjoined import/generation/population/provider/foreign Store graph
 * obligations remain external. The Tile lazy close without its own published
 * capsule is OPEN; a refusal never invents a capsule/pop or replays a getter.
 */
bool mc_world_raytrace_flags_begin_in_execution(
    mc_entity_tick_execution *, mc_world_runtime_classes *, mc_world_runtime *,
    mc_world_collision_block_execution_code, mc_entity_vector *, mc_entity_vector *, bool, bool,
    mc_world_raytrace_call **, mc_world_raytrace_status *, mc_world_runtime_execution_result *);

typedef struct mc_world_raytrace_execution_state {
    bool query_failed, native_stopped;
    const mc_world_collision_error *collision;      /* Actual active query scratch only. */
    const mc_world_runtime_execution_result *owned; /* Borrowed enclosing-frame VALUE. */
    /* Set only by this Ray's actual reached failed IN invocation. These are
     * copied diagnostic VALUES, independent of later Math cleanup/re-entry. */
    bool math_failed;
    mc_math_class_error math_error;
    mc_math_execution_site math_caller, math_child;
} mc_world_raytrace_execution_state;

bool mc_world_raytrace_execution_inspect(const mc_world_raytrace_call *,
                                         mc_world_raytrace_execution_state *);

#endif
