#ifndef MC_ITEM_USE_USE_H
#define MC_ITEM_USE_USE_H

#include "mc/block/registry/runtime.h"
#include "mc/item/stack/stack.h"
#include "mc/world/runtime/runtime.h"

typedef struct mc_entity mc_entity;
typedef struct mc_item mc_item;
typedef struct mc_item_use_call mc_item_use_call;
typedef struct mc_world_mutation_error mc_world_mutation_error;
typedef struct mc_world_runtime_acquisition_error mc_world_runtime_acquisition_error;
typedef struct mc_world_placement_error mc_world_placement_error;
typedef struct mc_player_statistic_error mc_player_statistic_error;
typedef struct mc_item_combat_error mc_item_combat_error;

typedef enum mc_item_use_status {
    MC_ITEM_USE_OK,
    MC_ITEM_USE_ARGUMENT,
    MC_ITEM_USE_BUSY,
    MC_ITEM_USE_ALLOCATION,
    MC_ITEM_USE_LIMIT,
    MC_ITEM_USE_REGISTRY,
    MC_ITEM_USE_STACK,
    MC_ITEM_USE_WORLD,
    MC_ITEM_USE_PLAYER,
    MC_ITEM_USE_SOURCE_NULL,
    MC_ITEM_USE_SOURCE_INDEX,
    MC_ITEM_USE_UNSUPPORTED,
    MC_ITEM_USE_FLOATING
} mc_item_use_status;

typedef struct mc_item_use_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_item_use_site;

typedef struct mc_item_use_error {
    mc_item_use_status status;
    const char *operation, *exception_class;
    mc_item_use_site site, child;
    bool native_policy, source_exception, source_error;
    mc_registry_error registry;
    mc_item_stack_status stack;
    mc_world_storage_error world;
    const mc_world_runtime_acquisition_error *acquisition;
    const mc_world_placement_error *placement;
    const mc_world_mutation_error *mutation, *sound;
    const mc_player_statistic_error *statistic;
    /* Reached LA147 child, including its completed Stack mutation prefix and
     * nested broken-statistic cause. Borrows this call through retirement. */
    const mc_item_combat_error *durability;
    /* A separate native retention/retirement cause never overwrites the
     * reached Source failure. Views borrow the retained call until retire. */
    mc_world_storage_error cleanup;
    mc_registry_error cleanup_registry;
    size_t retained_producers;
} mc_item_use_error;

typedef struct mc_item_use_state {
    mc_registry_classes *registry;
    mc_item_stack *stack, *result_stack;
    mc_world_runtime *world;
    mc_entity *player;
    const mc_item *item;
    const mc_block *captured_block;
    const mc_item_use_error *error;
    int32_t x, y, z, face;
    int32_t target_x, target_y, target_z, target_face;
    int32_t id, block_id, damage, metadata, count_before, count_after;
    double sound_x, sound_y, sound_z;
    float volume, pitch;
    unsigned int source_pc;
    bool in_air, active, returned, result;
    bool item_resolved, snow, count_read, ceiling_checked;
    bool predicate_entered, predicate_returned, can_place;
    bool metadata_returned, setter_returned, setter_value;
    bool placed_face_returned, placed_by_returned;
    bool sound_returned, count_stored, statistic_entered, statistic_returned;
    mc_item_use_status status;
} mc_item_use_state;

/* Synchronous actual client IZ wrappers, on genuine live/exclusive Stack and
 * Registry storage. The Registry is the actual IZ/GM code namespace, never a
 * substitute derived from a nullable World. No READY/provenance gate is added
 * to Source Stack reads. NULL Stack/World/Player are inspected only where the
 * reached bytecode requires them. Unimplemented real virtual overrides stop
 * at that dispatch after preserving its already executed prefix.
 *
 * out is required, initially NULL and disjoint from every input/error. Native
 * admission/allocation failure preserves it. A reached call is published even
 * on Source failure and owns its exact Stack/Registry, reached World/Player,
 * callback producers and child diagnostics until checked retirement. Its
 * borrowed dependency graph must remain live and exclusive. Returned in-air
 * result_stack owns a separate call hold, including when it aliases stack;
 * retain it before retirement if it must outlive this call. No rollback,
 * implicit cleanup, World tick, statistic flush or invented callback occurs. */
mc_item_use_status mc_item_use_on_block_begin(mc_registry_classes *, mc_item_stack *, mc_entity *,
                                              mc_world_runtime *, int32_t x, int32_t y, int32_t z,
                                              int32_t face, mc_item_use_call **out,
                                              mc_item_use_error *);
mc_item_use_status mc_item_use_in_air_begin(mc_registry_classes *, mc_item_stack *,
                                            mc_world_runtime *, mc_entity *, mc_item_use_call **out,
                                            mc_item_use_error *);
bool mc_item_use_inspect(const mc_item_use_call *, mc_item_use_state *out);
/* Busy/failure leaves remaining ownership in *call and reports cleanup
 * separately from the immutable Source error. Success consumes and NULLs it. */
mc_item_use_status mc_item_use_retire(mc_item_use_call **call, mc_item_use_error *);

#endif
