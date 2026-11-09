#ifndef MC_ENTITY_ARROW_TICK_H
#define MC_ENTITY_ARROW_TICK_H

#include "mc/entity/arrow/construction.h"
#include "mc/entity/hit_result.h"
#include "mc/entity/tick/environment.h"
#include "mc/world/runtime/raytrace.h"

typedef struct mc_arrow_tick_call mc_arrow_tick_call;
typedef struct mc_entity_tick_execution mc_entity_tick_execution;
typedef struct mc_world_runtime_execution_result mc_world_runtime_execution_result;

/* Explicit classification only. UNRESOLVED never invents Native or Throwable
 * kind/class from a storage status, suffix or unselected diagnostic arm. */
typedef enum mc_arrow_tick_failure_kind {
    MC_ARROW_TICK_FAILURE_NATIVE,
    MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION,
    MC_ARROW_TICK_FAILURE_SOURCE_ERROR,
    MC_ARROW_TICK_FAILURE_UNRESOLVED
} mc_arrow_tick_failure_kind;

typedef enum mc_arrow_tick_native_unwind_phase {
    MC_ARROW_TICK_UNWIND_QUIESCENT,
    MC_ARROW_TICK_UNWIND_DIAGNOSTIC,
    MC_ARROW_TICK_UNWIND_CAPTURE,
    MC_ARROW_TICK_UNWIND_FINISH,
    MC_ARROW_TICK_UNWIND_CHILD_CLOSE,
    MC_ARROW_TICK_UNWIND_OWN_CLOSE,
    MC_ARROW_TICK_UNWIND_REFUSED
} mc_arrow_tick_native_unwind_phase;
typedef struct mc_entity_source_position_error mc_entity_source_position_error;

typedef enum mc_arrow_tick_status {
    MC_ARROW_TICK_OK,
    MC_ARROW_TICK_ARGUMENT,
    MC_ARROW_TICK_STATE,
    MC_ARROW_TICK_BUSY,
    MC_ARROW_TICK_LIMIT,
    MC_ARROW_TICK_ALLOCATION,
    MC_ARROW_TICK_SOURCE_EXCEPTION,
    MC_ARROW_TICK_CHILD_FAILED,
    MC_ARROW_TICK_CLEANUP_FAILED
} mc_arrow_tick_status;

typedef enum mc_arrow_tick_phase {
    MC_ARROW_TICK_ACTIVE,
    MC_ARROW_TICK_RETURNED,
    MC_ARROW_TICK_FAILED,
    MC_ARROW_TICK_RETIRING
} mc_arrow_tick_phase;

typedef enum mc_arrow_tick_child {
    MC_ARROW_TICK_CHILD_NONE,
    MC_ARROW_TICK_CHILD_OWNER,
    MC_ARROW_TICK_CHILD_BASE,
    MC_ARROW_TICK_CHILD_MATH,
    MC_ARROW_TICK_CHILD_REGISTRY,
    MC_ARROW_TICK_CHILD_BLOCK,
    MC_ARROW_TICK_CHILD_BOUNDS,
    MC_ARROW_TICK_CHILD_VECTOR,
    MC_ARROW_TICK_CHILD_GEOMETRY,
    MC_ARROW_TICK_CHILD_RAY,
    MC_ARROW_TICK_CHILD_NEARBY,
    MC_ARROW_TICK_CHILD_INTERCEPT,
    MC_ARROW_TICK_CHILD_HIT,
    MC_ARROW_TICK_CHILD_EFFECT,
    MC_ARROW_TICK_CHILD_ENVIRONMENT,
    MC_ARROW_TICK_CHILD_SOUND,
    MC_ARROW_TICK_CHILD_PARTICLE,
    MC_ARROW_TICK_CHILD_POSITION
} mc_arrow_tick_child;

/* Compact borrowed view. Each nonnull child is this invocation's actual
 * selected record. Checked cleanup can consume a child and clear its pointer
 * while the first scalar cause remains unchanged. Copying this value adds no
 * ownership. Do not use child borrows after corresponding retirement. */
typedef struct mc_arrow_tick_error {
    mc_arrow_tick_status status;
    mc_arrow_tick_failure_kind classification;
    mc_world_storage_status ownership_status; /* This actual native cleanup operation only. */
    const char *operation, *exception_class;
    /* Before cleanup, exception_class borrows its protected actual producer.
     * Before ANY producer-consuming cleanup it is copied into frame-owned text
     * (or cleanup refuses retaining those owners). It ends only with this call. */
    bool native_policy, source_exception, source_error;
    mc_arrow_site site, child_site;
    mc_arrow_tick_child child;
    const mc_entity_error *entity;
    const mc_registry_error *registry;
    const mc_math_class_error *math;
    const mc_entity_tick_error *base;
    const mc_world_collision_error *block;
    const mc_world_nearby_error *nearby;
    const mc_world_raytrace_error *ray;
    const mc_entity_bounds_intercept_error *intercept;
    const mc_entity_hit_error *hit;
    const mc_entity_bounds_error *bounds;
    const mc_entity_vector_error *vector;
    const mc_entity_geometry_error *geometry;
    const mc_entity_effect_error *effect;
    const mc_entity_environment_error *environment;
    const mc_world_movement_error *sound;
    const mc_world_mutation_error *particle;
    const mc_world_source_chunk_error *loader;
    const mc_entity_tick_execution *execution;
    /* Frame VALUE view keeps native firstcause through cleanup refusal. After
     * producer retirement, expired selection tokens must fail checked reads;
     * no pending raw borrow survives the completed unwind stage. */
    const mc_world_runtime_execution_result *query;
    const mc_entity_source_position_error *position;
} mc_arrow_tick_error;

typedef struct mc_arrow_tick_limits {
    /* Parent frame plus its exact owned first diagnostic class-name bytes.
     * Child ray/intercept/source pools retain independent budgets. This is a
     * fixed begin ceiling, not a total-call cap or a mutable cleanup quota. */
    size_t max_parent_bytes;
    size_t max_candidates;
    size_t max_angle_iterations;
    mc_entity_tick_limits base;
} mc_arrow_tick_limits;

typedef struct mc_arrow_tick_state {
    mc_arrow_tick_phase phase;
    bool active, retiring, source_returned;
    unsigned int source_pc, base_wrapper_pc;
    size_t candidates_started, angle_iterations;
    unsigned int random_calls, bubble_calls;
    /* Valid damage-child observations only when hurt_invoked is true.
     * The current741 dependency stops before entry; it records no prior state. */
    bool hurt_invoked, hurt_returned, hurt_result, hurt_prior_pending;
    bool hurt_cleanup_acquired;
    size_t base_particle_work;
    mc_entity *receiver, *candidate, *selected, *hurt_source, *hurt_target;
    const mc_entity_vector *start, *end;
    const mc_entity_bounds *candidate_bounds;
    const mc_entity_hit_result *hit, *pending_hit, *intercept_hit;
    const mc_world_raytrace_call *ray_call;
    const mc_entity_bounds_intercept_call *intercept_call;
    double nearest;
    /* Borrowed latest participating scalar result; query_status is an actual
     * return only when query_invoked. Scope/phase are native cleanup state. */
    const mc_world_runtime_execution_result *query;
    mc_world_storage_status query_status;
    mc_arrow_tick_native_unwind_phase query_unwind;
    bool query_live, query_invoked, direct_world_open;
    size_t diagnostic_bytes; /* Exact frame-owned first class-name bytes, including NUL. */
    const mc_arrow_tick_error *failure, *cleanup;
} mc_arrow_tick_state;

mc_arrow_tick_limits mc_arrow_tick_default_limits(void);

/* Complete client SL.w_ parent, synchronous and one-shot. The same canonical
 * held READY Arrow is kept until checked retirement. Defining World classes
 * are independent of mutable nullable Arrow.world. Native code/Math contexts
 * must remain live/exclusive until the invocation captures their owning rights.
 * Nullable source World/bounds/Random/watcher/shooter remain reached operands.
 * *out is fresh NULL/disjoint storage. Before frame publication refusal leaves
 * it alone and writes admission_error; afterwards it owns the call on either
 * normal return or reached failure. No replay/resume, rollback, pool reset,
 * forced source-field clearing or automatic outer World repair occurs.
 * Returned/pending children are actual identities; retain a separate occurrence
 * before using one after this frame consumes its own occurrence.
 *
 * Base/collision/Ray/nearby/presentation use actual owned published entries.
 * Full victim damage still stops at the reached invocation741; no callee
 * success/entry is claimed. The default World dispatcher owns failed calls;
 * their real defining-code/provider/Store/import graph must remain externally
 * held under the existing contract. This is not full Arrow/SL admission.
 * Source pickup/rider dispatch are separate complete methods. */
mc_arrow_tick_status mc_arrow_tick_begin(mc_entity *, mc_world_runtime_classes *, mc_math_classes *,
                                         const mc_arrow_tick_limits *, mc_arrow_tick_call **,
                                         mc_arrow_tick_error *admission_error);
bool mc_arrow_tick_inspect(const mc_arrow_tick_call *, mc_arrow_tick_state *);
/* Native-only ONE-action cleanup. The Source body must have permanently
 * returned/failed; this never resumes it. quiescent is live/disjoint writable
 * storage, written only on a successful step; true means scopes/attempts drain,
 * with protected producers still owned until checked retirement. The step
 * first snapshots any protected non-NULL first class name into owned exact
 * text within max_parent_bytes. Snapshot success is its one action; limit/OOM
 * refusal preserves quiescent, original cause/name and ALL actual owners.
 * Unknown UNRESOLVED class names have the same lifetime guarantee. A fixed
 * LIMIT cannot progress under that cap; no Source or quota resume is offered.
 * Later steps capture the exact authorized current HELD tuple, then
 * finishes its actual returned pending attempt, retries its actual published
 * close capsule, or closes ONLY this parent's recorded exact scope. A saved
 * capture token avoids repeating capture-only progress. Unknown descendants,
 * entered children, malformed rights or checked refusals preserve the frame,
 * first Source/native result, all pending/close values, Math and owners.
 * quiescent=false means one explicit native action progressed; callers may
 * retry this API. Inspect cleanup for this refusal separately from failure.
 * No getter, Source callback, automatic owner search/pop or field repair.
 * retire drives only the bounded participating actions before Math/owners. */
mc_arrow_tick_status mc_arrow_tick_native_unwind_step(mc_arrow_tick_call *, bool *quiescent);
mc_arrow_tick_status mc_arrow_tick_retire(mc_arrow_tick_call **);

#endif
