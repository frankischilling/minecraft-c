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

/* Native held own-data prefix, deliberately without an owning wake output.
 * This legacy preparation/checkpoint entry stops at reached93; the complete
 * Entity reader uses the same body with its actual output availability. */
bool mc_player_nbt_read_held(mc_entity *, mc_registry_classes *, const mc_nbt_tag *,
                             const mc_nbt_limits *, mc_entity_nbt_error *);
bool mc_player_nbt_read_with_child_output_held(mc_entity *, mc_registry_classes *,
                                               const mc_nbt_tag *, const mc_nbt_limits *,
                                               bool can_transfer_children, mc_entity_nbt_error *);
/* Move the already-owned reader entry occurrence into its stable failed wake
 * work. Only the one reader with a nonnull sleep_work calls this once. */
void mc_player_nbt_sleep_adopt_registry(mc_entity_nbt_error *, mc_registry_classes *);
bool mc_player_nbt_build_fields_held(mc_entity *, const mc_nbt_limits *, mc_nbt_tree **,
                                     mc_entity_nbt_error *);
/* Nonfailing native cleanup after an actual coordinate-pointer source store.
 * Caller holds the canonical Player; aliases v/b/bN and local pins keep nodes. */
void mc_player_nbt_coordinates_collect_held(mc_entity *);
void mc_player_nbt_coordinate_store_held(mc_entity *, struct mc_player_nbt_coordinate *,
                                         const mc_player_coordinates **);
void mc_player_nbt_coordinates_dispose(mc_entity *);

/* Native source-local ownership; caller already holds the canonical Player.
 * Pin preserves the actual mutable XYZ identity, including NULL and aliases.
 * These operations perform no Q/v read or source field store. */
mc_player_status mc_player_coordinate_pin_held(mc_entity *, const mc_player_coordinates *,
                                               struct mc_player_nbt_coordinate **,
                                               mc_player_error *);
mc_player_status mc_player_coordinate_unpin_admit_held(mc_entity *,
                                                       const struct mc_player_nbt_coordinate *,
                                                       size_t occurrences);
void mc_player_coordinate_unpin_commit_held(mc_entity *, struct mc_player_nbt_coordinate **);
mc_player_status mc_player_coordinate_new_held(mc_entity *, mc_player_site, const char *operation,
                                               struct mc_player_nbt_coordinate **,
                                               mc_player_error *);

#endif
