#ifndef MC_WORLD_RUNTIME_INTERNAL_H
#define MC_WORLD_RUNTIME_INTERNAL_H

#include "acquisition_internal.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/runtime.h"
#include "mc/world/runtime/saving/errors.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"
#include "mc/world/runtime/world_access.h"

typedef struct mc_world_entity_list_ledger {
    mc_world_reference_list *list;
    struct mc_entity **entries;
    size_t count, capacity;
} mc_world_entity_list_ledger;

typedef struct mc_world_collision_reference {
    struct mc_entity_bounds *bounds;
    struct mc_entity_classes *pool;
    struct mc_entity *entity;
} mc_world_collision_reference;

typedef struct mc_world_access_list_ledger {
    mc_world_reference_list *list;
    void **items;
    mc_world_access **entries;
    size_t count, capacity;
} mc_world_access_list_ledger;

typedef struct mc_world_collision_list_ledger {
    mc_world_reference_list *list;
    mc_world_collision_reference *entries;
    size_t count, capacity;
} mc_world_collision_list_ledger;

struct mc_world_runtime {
    mc_world_runtime_options options;
    mc_world_runtime_state live;

    struct {
        int64_t time;
        int32_t spawn_x, spawn_y, spawn_z;
    } legacy_info;

    bool provider_force_load, saving_disabled;
    mc_chunk_state fallback;
    mc_chunk_references references;
    mc_chunk_acquire_context acquisition;
    mc_chunk_lifecycle_context lifecycle;
    mc_world_save_context saving;
    mc_world_store_loader loader;
    mc_world_storage_error last_error;
    mc_registry_error last_registry_error;
    mc_java_random_profile_status last_random_status;
    mc_world_runtime_acquisition_error *acquisition_error;
    mc_world_runtime_lifecycle_error *lifecycle_error;
    size_t active_calls, local_leases;
    size_t persistent_bindings;
    /* Native typed holds parallel the actual b List, preserving each nonnull
     * duplicate occurrence; typed NULL owns no hold. The exact constructor
     * descriptor is captured at b28, independently of replaceable source b.
     * Unknown void* contents are never adopted as owning Entity references. */
    mc_world_entity_list_ledger entity_membership;
    /* Separate actual D39 capture; one real hold per nonnull occurrence,
     * including duplicates. No source deferred-removal processing is implied. */
    mc_world_entity_list_ledger entity_deferral;
    /* Actual constructor-created R254. Each query occurrence owns one real
     * Entity hold, independently of b/D and replaceable source R. */
    mc_world_entity_list_ledger nearby_entities;
    /* Actual K203, independent of current source K. Each occurrence owns
     * either one pooled bounds hold or one permanent bounds' Entity hold. */
    mc_world_collision_list_ledger collision_boxes;
    /* Constructor-created u192 and its separately owned typed occurrences.
     * Current source u is replaceable; unknown void* values own no WorldAccess. */
    mc_world_access_list_ledger world_accesses;
    bool entity_containers_retired;
    mc_tile_entity_list_ledger tile_entity_membership[2];
    bool tile_entity_containers_retired;
    mc_world_runtime_fields fields;
    mc_world_runtime_dedicated_fields dedicated;
    mc_world_runtime_dedicated_construction dedicated_construction;
    mc_world_runtime_construction construction;
    mc_world_runtime_construction_limits construction_limits;
    mc_world_runtime_classes *classes;
};

/* Bind installed options/live cells without acquiring or retiring their owners.
 * COMPOSITION initializes its blank fallback; GAME preserves its constructed
 * fallback, including data. An already-set binding flag is rejected unchanged.
 * For an initially unbound owner, failure may leave prepared context fields,
 * but both flags remain false; success publishes them only after actual store
 * cache registration. Earlier constructor and dependency validation belongs to
 * the caller. No callback, random draw or file operation is performed here. */
mc_world_storage_status mc_world_runtime_bind_contexts(mc_world_runtime *runtime,
                                                       mc_world_storage_error *error);

mc_world_storage_status mc_world_runtime_game_free(mc_world_runtime *runtime,
                                                   mc_world_storage_error *error);

const mc_math_trig *mc_world_runtime_class_trig(const mc_world_runtime *runtime);
struct mc_entity_classes *mc_world_runtime_entities(const mc_world_runtime *runtime);
mc_world_storage_status mc_world_runtime_initialize_environment(mc_world_runtime *runtime,
                                                                mc_world_storage_error *error);

mc_world_storage_status mc_world_runtime_registry_status(mc_registry_status status);
mc_world_storage_status mc_world_runtime_call_enter(mc_world_runtime *runtime,
                                                    mc_world_storage_error *error);
void mc_world_runtime_call_leave(mc_world_runtime *runtime);
mc_world_storage_status mc_world_runtime_registry_result(mc_world_runtime *runtime,
                                                         mc_registry_status status,
                                                         const mc_registry_error *cause,
                                                         const char *operation,
                                                         mc_world_storage_error *error);

#endif
