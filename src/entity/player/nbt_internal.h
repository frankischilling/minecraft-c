#ifndef MC_ENTITY_PLAYER_NBT_INTERNAL_H
#define MC_ENTITY_PLAYER_NBT_INTERNAL_H

#include "mc/entity/nbt/errors.h"
#include "mc/storage/nbt/tree.h"
#include "player_internal.h"

bool mc_player_nbt_read_held(mc_entity *, mc_registry_classes *, const mc_nbt_tag *,
                             const mc_nbt_limits *, mc_entity_nbt_error *);
bool mc_player_nbt_build_fields_held(mc_entity *, const mc_nbt_limits *, mc_nbt_tree **,
                                     mc_entity_nbt_error *);
/* Nonfailing native cleanup after an actual coordinate-pointer source store.
 * Caller holds the canonical Player; aliases v/b/bN keep their same nodes. */
void mc_player_nbt_coordinates_collect_held(mc_entity *);
void mc_player_nbt_coordinates_dispose(mc_entity *);

#endif
