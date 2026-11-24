#ifndef MC_ENTITY_LIGHTNING_CONSTRUCTION_H
#define MC_ENTITY_LIGHTNING_CONSTRUCTION_H

#include "mc/entity/entity.h"
#include "mc/world/runtime/mutations.h"

typedef struct mc_lightning_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_lightning_site;

typedef enum mc_lightning_child {
    MC_LIGHTNING_CHILD_NONE,
    MC_LIGHTNING_CHILD_ENTITY,
    MC_LIGHTNING_CHILD_PRESENCE,
    MC_LIGHTNING_CHILD_BLOCK,
    MC_LIGHTNING_CHILD_REGISTRY,
    MC_LIGHTNING_CHILD_SUPPORT,
    MC_LIGHTNING_CHILD_MUTATION
} mc_lightning_child;

typedef struct mc_lightning_error {
    mc_entity_status status;
    const char *operation, *exception_class;
    mc_lightning_site site, child_site;
    bool native_policy, source_exception;
    mc_lightning_child child;
    mc_entity_error entity;
    mc_world_storage_error world;
    unsigned int presence_pc, provider_pc, fire_pc, fire_neighbor_pc;

    /* Exactly one reached failed child is selected. No recursively duplicated
     * mutation/acquisition graph belongs to this constructor diagnostic. */
    union {
        mc_world_runtime_acquisition_error acquisition;
        mc_registry_error registry;
        mc_world_solid_support_error support;
        mc_world_mutation_error mutation;
    } detail;
} mc_lightning_error;

typedef struct mc_lightning_state {
    mc_entity_phase phase;
    int64_t a;
    int32_t b, c;
    unsigned int ancestor_pc, constructor_pc;
    size_t neighbor_attempts, own_random_calls;
    int32_t sampled_x, sampled_y, sampled_z;
    bool area_reached, area_present;
    const mc_lightning_error *failure;
} mc_lightning_state;

/* Actual client C extends YU extends SN. Allocation initializes the true
 * Entity class before its concrete header, but consumes no ID or entropy.
 * The canonical Class is this namespace's existing MC_ENTITY_CLASS_KIND_C. */
mc_entity_status mc_lightning_allocate(mc_entity_classes *, mc_entity **, mc_entity_error *);
/* One-shot RAW -> CONSTRUCTING -> READY/FAILED constructor. Nonnull World must
 * use this same Entity/profile owner. NULL reaches C52 after pose and both own
 * Random calls. Every reached field/ID/entropy/World mutation prefix survives
 * failure; no Living fields, Math draws, World insertion or Lightning tick
 * are manufactured. Keep owners/out/error live, exclusive and disjoint. */
/* A callback receipt is captured independently by the failed Entity. Copied
 * errors borrow that occurrence until final Entity release; callers that need
 * a longer lifetime must retain their own WorldAccess failure occurrence. */
mc_entity_status mc_lightning_construct(mc_entity *, mc_world_runtime *nullable_world, double x,
                                        double y, double z, mc_lightning_error *);
bool mc_lightning_inspect(const mc_entity *, mc_lightning_state *);
/* Generic Entity holds/releases support every phase. Strict terminal release
 * consumes only a final inactive occurrence and preserves a BUSY slot. */
mc_entity_status mc_lightning_retire(mc_entity **, mc_entity_error *);

#endif
