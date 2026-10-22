#ifndef MC_WORLD_RUNTIME_CONTACT_H
#define MC_WORLD_RUNTIME_CONTACT_H

#include "mc/entity/movement/effects.h"
#include "mc/world/runtime/collision/boxes.h"
#include "mc/world/runtime/mutations.h"

typedef struct mc_world_entity_contact_error {
    mc_world_storage_status status;
    const char *operation;
    const mc_block *block;
    const char *block_class;
    unsigned int bytecode, helper_bytecode;
    int32_t x, y, z;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_query_error query;
    /* The full outer metadata-guard read, including activation, loading and
     * population children. query projects the failing World operation; helper
     * acquisition/query children remain complete in mutation.nearby. */
    mc_world_collision_error metadata;
    mc_entity_error entity;
    mc_entity_effect_error effect;
    mc_world_mutation_error mutation;
} mc_world_entity_contact_error;

mc_world_entity_contact_error mc_world_entity_contact_empty_error(void);

/* Actual Block.a(World,III,Entity) contact dispatch on the supplied live Block.
 * The caller already captured/indexed Block.m and keeps that same Registry,
 * Block, Entity (when nonnull) and reached World live/exclusive through this
 * synchronous call. No fresh Block lookup, constructor/READY preflight,
 * implicit pool reset, coordinate copy or World rebind occurs here.
 *
 * The 63 inherited base bodies are genuine no-ops. Portal checks riding then
 * passenger before actual Entity S/P; web only sets the next-move flag; soul
 * sand multiplies current X then Z velocity by exact double0.4; cactus invokes
 * actual nullable-source damage and discards its ordinary bool. Pressure and
 * detector execute remote/metadata guards. Pressure plates query actual
 * published entities, including real Class-filtered Living/Player queries,
 * notify metadata/neighbors and schedule their release. The detector's
 * positive helper remains an explicit reached dependency.
 *
 * Unused nullable World/Entity arguments remain unread by the selected source
 * body. Reached World operations require GAME/nonretired owners; reached Entity
 * effects retain their own documented native admission. Bounds/phase, damage,
 * publication and gameplay relationships are not repaired. Failure retains
 * every completed source effect and its actual child error; success clears the
 * error frame. Error storage must be disjoint from all owners and inputs.
 * block in an error is borrowed while its supplying Registry remains live. */
mc_world_storage_status mc_world_runtime_entity_contact(mc_world_runtime *world,
                                                        const mc_block *block, int32_t x, int32_t y,
                                                        int32_t z, mc_entity *entity,
                                                        mc_world_entity_contact_error *error);

#endif
