#ifndef MC_WORLD_RUNTIME_RAYTRACE_H
#define MC_WORLD_RUNTIME_RAYTRACE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_world_runtime mc_world_runtime;
typedef struct mc_entity_vector mc_entity_vector;
typedef struct mc_entity_hit_result mc_entity_hit_result;
typedef struct mc_world_raytrace_call mc_world_raytrace_call;
typedef struct mc_world_collision_error mc_world_collision_error;
typedef struct mc_world_source_chunk_error mc_world_source_chunk_error;
typedef struct mc_world_storage_error mc_world_storage_error;
typedef struct mc_registry_error mc_registry_error;
typedef struct mc_entity_error mc_entity_error;
typedef struct mc_entity_vector_error mc_entity_vector_error;
typedef struct mc_entity_geometry_error mc_entity_geometry_error;
typedef struct mc_entity_bounds_error mc_entity_bounds_error;
typedef struct mc_entity_hit_error mc_entity_hit_error;

typedef enum mc_world_raytrace_status {
    MC_WORLD_RAYTRACE_OK,
    MC_WORLD_RAYTRACE_ARGUMENT,
    MC_WORLD_RAYTRACE_STATE,
    MC_WORLD_RAYTRACE_BUSY,
    MC_WORLD_RAYTRACE_LIMIT,
    MC_WORLD_RAYTRACE_ALLOCATION,
    MC_WORLD_RAYTRACE_SOURCE_EXCEPTION,
    MC_WORLD_RAYTRACE_CHILD_FAILED,
    MC_WORLD_RAYTRACE_UNIMPLEMENTED,
    MC_WORLD_RAYTRACE_CLEANUP_FAILED
} mc_world_raytrace_status;

typedef enum mc_world_raytrace_phase {
    MC_WORLD_RAYTRACE_ACTIVE,
    MC_WORLD_RAYTRACE_RETURNED,
    MC_WORLD_RAYTRACE_FAILED,
    MC_WORLD_RAYTRACE_RETIRING
} mc_world_raytrace_phase;

typedef struct mc_world_raytrace_site {
    const char *class_name, *method, *descriptor;
    int pc;
} mc_world_raytrace_site;

typedef struct mc_world_raytrace_error {
    mc_world_raytrace_status status;
    const char *operation;
    bool native_policy, source_exception;
    const char *exception_class; /* Static exact class name when genuinely known. */
    mc_world_raytrace_site site, caller, child;
    const mc_world_storage_error *world;
    const mc_world_collision_error *collision;
    const mc_world_source_chunk_error *source_chunk;
    const mc_registry_error *registry;
    const mc_entity_error *entity;
    const mc_entity_vector_error *vector;
    const mc_entity_geometry_error *geometry;
    const mc_entity_bounds_error *bounds;
    const mc_entity_hit_error *hit;
} mc_world_raytrace_error;

typedef struct mc_world_raytrace_state {
    mc_world_raytrace_phase phase;
    bool active, retiring, source_returned;
    bool stop_on_liquid, ignore_null_movement_box;
    unsigned int source_pc; /* Core fd.a(bt,bt,ZZ); wrappers keep their own site. */
    size_t cells_started, cells_read, steps_started;
    size_t movement_box_calls, predicate_calls, block_ray_calls;
    int32_t x, y, z, target_x, target_y, target_z, face;
    const mc_world_runtime *world;
    mc_entity_vector *start, *end;              /* SAME mutable operands, including aliasing. */
    mc_entity_hit_result *result;               /* Borrowed nullable actual returned VF only. */
    const mc_entity_hit_result *pending_result; /* Actual RAW/failed candidate. */
    const mc_world_raytrace_error *failure, *cleanup;
} mc_world_raytrace_state;

/* Synchronous one-shot source call; no resume/replay. GAME World and canonical
 * input vectors remain independently held until checked retirement. NULL vector
 * operands are admitted until their actual source read; native World==NULL is
 * rejected, leaving the caller responsible for its invocation NPE. SAME input
 * identities may alias and source/pool mutations remain observable while held.
 * *out starts NULL. Once allocated it receives the call even on failure.
 * Inspect borrows result/errors. Retain a genuine result occurrence separately
 * before App publication or ray-call retirement. A miss is source_returned
 * with result NULL; failed work is never a miss. No pool reset or rollback.
 * All access is exclusive and outputs/errors are disjoint from live owners.
 *
 * Exact two-Vec overload fd.a(bt,bt): delegates false,false at source PC5. */
mc_world_raytrace_status mc_world_raytrace_begin(mc_world_runtime *world,
                                                 mc_entity_vector *nullable_start,
                                                 mc_entity_vector *nullable_end,
                                                 mc_world_raytrace_call **out);

/* Exact fd.a(bt,bt,Z): delegates supplied stop_on_liquid,false at PC5. */
mc_world_raytrace_status mc_world_raytrace_liquid_begin(mc_world_runtime *world,
                                                        mc_entity_vector *nullable_start,
                                                        mc_entity_vector *nullable_end,
                                                        bool stop_on_liquid,
                                                        mc_world_raytrace_call **out);

/* Exact936-byte fd.a(bt,bt,ZZ) body, without an invented overload frame. */
mc_world_raytrace_status
mc_world_raytrace_flags_begin(mc_world_runtime *world, mc_entity_vector *nullable_start,
                              mc_entity_vector *nullable_end, bool stop_on_liquid,
                              bool ignore_null_movement_box, mc_world_raytrace_call **out);

bool mc_world_raytrace_inspect(const mc_world_raytrace_call *call, mc_world_raytrace_state *out);
/* Native-only cleanup. Active callback reentry returns BUSY without mutation.
 * Partial success consumes only the call's owned occurrences, clears their
 * slots, and may invalidate old child borrows. Inspect again after a failure;
 * the same call and remaining rights survive. Another retained VF occurrence
 * does not block releasing this call's occurrence. No source effect is replayed. */
mc_world_raytrace_status mc_world_raytrace_retire(mc_world_raytrace_call **slot);
#endif
