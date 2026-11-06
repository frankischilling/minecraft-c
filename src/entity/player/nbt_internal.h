#ifndef MC_ENTITY_PLAYER_NBT_INTERNAL_H
#define MC_ENTITY_PLAYER_NBT_INTERNAL_H

#include "mc/entity/nbt/errors.h"
#include "mc/storage/nbt/tree.h"
#include "player_internal.h"

/* Native nodes wrap genuine BR values from NBT and GS.a(BR). Source fields
 * remain pointers to value, while source-local pins protect replaced nodes. */
struct mc_player_nbt_coordinate {
    mc_player_coordinates value;
    struct mc_player_nbt_coordinate *next;
    size_t pins;
};

bool mc_player_nbt_read_held(mc_entity *, mc_registry_classes *, const mc_nbt_tag *,
                             const mc_nbt_limits *, mc_entity_nbt_error *);
bool mc_player_nbt_build_fields_held(mc_entity *, const mc_nbt_limits *, mc_nbt_tree **,
                                     mc_entity_nbt_error *);
/* Nonfailing native cleanup after an actual coordinate-pointer source store.
 * Caller holds the canonical Player; aliases v/b/bN and local pins keep nodes. */
void mc_player_nbt_coordinates_collect_held(mc_entity *);
void mc_player_nbt_coordinate_store_held(mc_entity *, struct mc_player_nbt_coordinate *,
                                         const mc_player_coordinates **);
void mc_player_nbt_coordinates_dispose(mc_entity *);

#endif
