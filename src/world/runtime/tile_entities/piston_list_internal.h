#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_PISTON_LIST_INTERNAL_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_PISTON_LIST_INTERNAL_H

#include "mc/world/runtime/collision/entities.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

typedef struct mc_piston_list_error {
    mc_world_storage_status status;
    const char *operation;
    mc_entity_status entity_status;
    size_t index;
} mc_piston_list_error;

typedef struct mc_piston_entity_iterator {
    mc_tile_entity_classes *owner;
    const mc_world_reference_list *identity;
    size_t cursor;
    uint32_t expected_mod_count;
} mc_piston_entity_iterator;

/* READY held Piston's actual code-class owner, independent of TileEntity.world.
 * NULL reports invalid native admission; no active use or owner hold is added. */
mc_world_runtime_classes *
mc_tile_entity_piston_code_classes_held(const mc_tile_entity *tile_entity);

/* These private source operations require an actual held READY Piston and
 * exclusive class/list access. Iterator storage borrows that TileEntity's class
 * lifetime, captures m once, and must not outlive the held TileEntity. Each attempt
 * replaces error; failure preserves out and every reached source effect.
 * Append snapshots the actual held nearby view before any destination growth.
 * Each duplicate occurrence owns an independent Entity hold. Native requested
 * byte ceilings do not claim Java allocation/OOM timing. Outputs/errors must
 * be mutually disjoint and disjoint from all owners, views and iterator state.
 * An iterator's cursor is reached state: next may advance it before a later
 * selected-occurrence/native-hold failure while preserving the Entity out. */
mc_world_storage_status
mc_tile_entity_piston_entities_append(mc_tile_entity *tile_entity,
                                      const mc_world_entity_list_view *source,
                                      mc_piston_list_error *error);
mc_world_storage_status mc_tile_entity_piston_entities_iterator(mc_tile_entity *tile_entity,
                                                                mc_piston_entity_iterator *out,
                                                                mc_piston_list_error *error);
mc_world_storage_status
mc_tile_entity_piston_entities_has_next(const mc_piston_entity_iterator *iterator, bool *out,
                                        mc_piston_list_error *error);
/* Success returns an independent Entity hold for the reached Java local.
 * The caller releases that hold after its use, including movement failures. */
mc_world_storage_status mc_tile_entity_piston_entities_next(mc_piston_entity_iterator *iterator,
                                                            mc_entity **out,
                                                            mc_piston_list_error *error);
mc_world_storage_status mc_tile_entity_piston_entities_clear(mc_tile_entity *tile_entity,
                                                             mc_piston_list_error *error);

#endif
