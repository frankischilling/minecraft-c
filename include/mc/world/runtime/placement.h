#ifndef MC_WORLD_RUNTIME_PLACEMENT_H
#define MC_WORLD_RUNTIME_PLACEMENT_H

#include "mc/block/registry/runtime.h"
#include "mc/entity/errors.h"
#include "mc/world/runtime/runtime.h"

typedef struct mc_entity mc_entity;
typedef struct mc_entity_bounds mc_entity_bounds;
typedef struct mc_world_runtime_acquisition_error mc_world_runtime_acquisition_error;
typedef struct mc_world_collision_error mc_world_collision_error;
typedef struct mc_world_spawn_collision_error mc_world_spawn_collision_error;
typedef struct mc_world_placement_work mc_world_placement_work;

typedef struct mc_world_placement_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_world_placement_site;

/* Noncopyable, initially zero, caller-owned diagnostic. A reached call owns its
 * private child frames, exact World/Registry occurrences, pooled collision box
 * and failed callback producers until checked retirement. No source work is
 * replayed by retirement. The whole enclosing graph stays live/exclusive. */
typedef struct mc_world_placement_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    mc_world_placement_site site, caller, wrapper;
    bool returned, value, native_policy, source_exception, source_error;
    bool existing_read, candidate_read, box_returned, box_ignored, collision_checked;
    bool existing_replaced, predicate_entered, predicate_returned;
    int32_t existing_id, fresh_id;
    const mc_block *existing, *candidate;
    const mc_entity_bounds *box;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_entity_status bounds_status;
    const mc_world_runtime_acquisition_error *acquisition;
    const mc_world_collision_error *collision;
    const mc_world_spawn_collision_error *entity_collision;
    mc_world_storage_error cleanup;
    mc_registry_error cleanup_registry;
    mc_entity_status cleanup_bounds;
    size_t retained_producers;
    mc_world_placement_work *work;
} mc_world_placement_error;

/* Actual client FD.a(IIIIZI)Z. Collision-box getter32 and the real Entity
 * collision test53 precede occupancy. ignore_entities discards the source box
 * only AFTER its getter. B/C/D/E/as/aT comparisons use actual singleton
 * identities, then UU.a(World,IIII) dispatches and freshly queries occupancy.
 * The source World supplies the actual code Registry. Failure preserves out
 * and all preceding acquisition/Block/pool/callback effects. */
mc_world_storage_status mc_world_runtime_can_place(mc_world_runtime *, int32_t block_id, int32_t x,
                                                   int32_t y, int32_t z, bool ignore_entities,
                                                   int32_t face, bool *out,
                                                   mc_world_placement_error *);

/* Actual Block virtual entries. code_registry is the actual invoking UU
 * namespace, independent of the nullable source World/Player arguments. Dirt
 * and other receivers inheriting UU execute its exact bodies. Distinct
 * overrides stop at the reached declaring class until implemented. The face
 * predicate resolves its own virtual base predicate and retains the fresh
 * World query/Material.g order. Literal hooks never inspect World or actors. */
mc_world_storage_status mc_world_block_can_place(mc_registry_classes *code_registry,
                                                 const mc_block *, mc_world_runtime *, int32_t x,
                                                 int32_t y, int32_t z, int32_t face, bool *out,
                                                 mc_world_placement_error *);
mc_world_storage_status mc_world_block_activate(mc_registry_classes *code_registry,
                                                const mc_block *, mc_world_runtime *, int32_t x,
                                                int32_t y, int32_t z, mc_entity *nullable_player,
                                                bool *out, mc_world_placement_error *);
mc_world_storage_status mc_world_block_placed_face(mc_registry_classes *code_registry,
                                                   const mc_block *, mc_world_runtime *, int32_t x,
                                                   int32_t y, int32_t z, int32_t face,
                                                   mc_world_placement_error *);
mc_world_storage_status mc_world_block_placed_by(mc_registry_classes *code_registry,
                                                 const mc_block *, mc_world_runtime *, int32_t x,
                                                 int32_t y, int32_t z, mc_entity *nullable_living,
                                                 mc_world_placement_error *);

/* Consumes only native occurrences/diagnostics. False preserves remaining
 * ownership and the original source failure; cleanup has the separate cause.
 * Successful consumption clears borrowed views and zeros the whole record. */
bool mc_world_placement_error_retire(mc_world_placement_error *);

#endif
