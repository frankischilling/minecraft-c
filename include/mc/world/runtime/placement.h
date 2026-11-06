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
typedef struct mc_world_solid_support_detail mc_world_solid_support_detail;
typedef struct mc_world_mutation_error mc_world_mutation_error;
typedef struct mc_world_placement_work mc_world_placement_work;
typedef struct mc_world_placement_error mc_world_placement_error;
typedef struct mc_player_sleep_error mc_player_sleep_error;
typedef struct mc_player_error mc_player_error;
typedef struct mc_player_sleep_result_error mc_player_sleep_result_error;
typedef struct mc_player_sleep_cleanup mc_player_sleep_cleanup;
typedef struct mc_world_provider_error mc_world_provider_error;

/* Reached Player virtual children supplied by the enclosing client call.
 * No callback runs for an inherited literal Block body or remote FI return.
 * Context and its retained child graph stay live through checked retirement.
 * A callback preserves this noncopyable diagnostic's work/ownership fields. */
typedef struct mc_world_block_activation_context {
    void *context;
    /* Actual GS defining Registry. Independent of VE and Player.World;
     * consulted lazily by the reached sleep child, never a World substitute. */
    mc_registry_classes *player_code_registry;
    mc_world_storage_status (*workbench)(void *, mc_entity *, int32_t, int32_t, int32_t,
                                         mc_world_placement_error *);
    /* Actual reached Player.b(String) virtual. key is the same nullable
     * Registry String identity from key_owner's code literal pool. Preserve
     * this error's work/ownership fields; retain actual failed client children
     * in the enclosing context through checked retirement. No callback or
     * translated message is supplied for an unreached source invocation. */
    mc_world_storage_status (*player_message)(void *, mc_entity *, mc_registry_classes *key_owner,
                                              const mc_registry_string *key,
                                              mc_world_placement_error *);
} mc_world_block_activation_context;

typedef struct mc_world_placement_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_world_placement_site;

/* Noncopyable, initially zero, caller-owned diagnostic. A reached call owns its
 * private child frames, exact World/Registry occurrences, pooled collision box
 * and failed callback producers until checked retirement. No source work is
 * replayed by retirement. The whole enclosing graph stays live/exclusive. */
struct mc_world_placement_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    mc_world_placement_site site, caller, wrapper, base, substrate, floor;
    bool returned, value, native_policy, source_exception, source_error;
    bool existing_read, candidate_read, box_returned, box_ignored, collision_checked;
    bool existing_replaced, predicate_entered, predicate_returned;
    bool base_entered, base_returned, base_value, below_read;
    bool substrate_entered, substrate_returned;
    bool remote_read, remote, workbench_entered, workbench_returned;
    bool bed_entered, bed_head, bed_occupied, bed_partner_read, bed_occupancy_cleared;
    bool bed_sleep_returned, player_message_entered, player_message_returned;
    int32_t bed_metadata, bed_direction, bed_x, bed_y, bed_z;
    size_t bed_player_index;
    uintptr_t bed_player_list_identity, bed_sleeper_identity, bed_sleep_result_identity;
    bool bed_explosion_pending;
    double bed_explosion_x, bed_explosion_y, bed_explosion_z;
    int32_t existing_id, fresh_id, below_id;
    const mc_block *existing, *candidate;
    const mc_entity_bounds *box;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_entity_status bounds_status;
    const mc_world_runtime_acquisition_error *acquisition;
    /* Every reached ID/metadata query has its own retained frame in source order.
     * acquisition remains the last reached query; no later read overwrites an
     * earlier query's source or publication cause. Borrow until retirement. */
    const mc_world_runtime_acquisition_error *queries[8];
    size_t query_count;
    const mc_world_solid_support_detail *support_queries[5];
    size_t support_query_count;
    const mc_world_mutation_error *mutation;
    const mc_world_collision_error *collision;
    const mc_world_spawn_collision_error *entity_collision;
    const mc_player_sleep_error *sleep;
    const mc_player_error *bed_occupant;
    const mc_player_sleep_result_error *sleep_result;
    const mc_world_provider_error *provider;
    mc_world_storage_error cleanup;
    mc_registry_error cleanup_registry;
    mc_entity_status cleanup_bounds;
    mc_entity_status cleanup_enum_classes;
    const mc_player_sleep_cleanup *cleanup_sleep;
    size_t retained_producers;
    mc_world_placement_work *work;
};

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
/* Independently virtual a(World,III)Z, without the face wrapper. WB first
 * invokes the UU base body, then reads the wrapped below-Y coordinate and
 * invokes the same receiver's actual substrate predicate. No survival/light
 * check or same-World-Registry substitution is performed. */
mc_world_storage_status mc_world_block_can_place_base(mc_registry_classes *code_registry,
                                                      const mc_block *, mc_world_runtime *,
                                                      int32_t x, int32_t y, int32_t z, bool *out,
                                                      mc_world_placement_error *);
mc_world_storage_status mc_world_block_activate(mc_registry_classes *code_registry,
                                                const mc_block *, mc_world_runtime *, int32_t x,
                                                int32_t y, int32_t z, mc_entity *nullable_player,
                                                bool *out, mc_world_placement_error *);
mc_world_storage_status mc_world_block_activate_with_context(
    mc_registry_classes *code_registry, const mc_block *, mc_world_runtime *, int32_t x, int32_t y,
    int32_t z, mc_entity *nullable_player, const mc_world_block_activation_context *, bool *out,
    mc_world_placement_error *);
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
