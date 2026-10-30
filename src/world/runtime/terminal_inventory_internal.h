#ifndef MC_WORLD_RUNTIME_TERMINAL_INVENTORY_INTERNAL_H
#define MC_WORLD_RUNTIME_TERMINAL_INVENTORY_INTERNAL_H

#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

/* Passive canonical-owner visitors. The identity is the ADDRESS of the actual
 * owning slot, not a source alias. A visitor may inspect/count or fail, but may
 * not mutate any owner or invoke source work. Propagate its error unchanged. */
typedef mc_world_storage_status (*mc_world_terminal_entity_visitor)(void *context,
                                                                    const void *slot_identity,
                                                                    mc_entity *value,
                                                                    mc_world_storage_error *error);
typedef mc_world_storage_status (*mc_world_terminal_tile_visitor)(void *context,
                                                                  const void *slot_identity,
                                                                  mc_tile_entity *value,
                                                                  mc_world_storage_error *error);
typedef mc_world_storage_status (*mc_world_terminal_chunk_visitor)(void *context,
                                                                   const mc_world_chunk *chunk,
                                                                   mc_world_storage_error *error);

typedef struct mc_world_terminal_entity_anchor {
    mc_entity *const *slot;
} mc_world_terminal_entity_anchor;

typedef struct mc_world_terminal_world_anchor {
    mc_world_runtime *const *slot;
} mc_world_terminal_world_anchor;

typedef struct mc_world_terminal_anchors {
    const mc_world_terminal_entity_anchor *entities;
    size_t entity_count;
    const mc_world_terminal_world_anchor *worlds;
    size_t world_count;
} mc_world_terminal_anchors;

/* Trusted internal owner hooks supply their exact OWNED slots. Mirrors and
 * diagnostic/source aliases are never anchors; no caller supplies a guessed
 * hold exemption. The public no-anchor entry delegates this same audit. */
mc_world_storage_status mc_world_runtime_terminal_admit_owned(mc_world_runtime *,
                                                              const mc_world_terminal_anchors *,
                                                              const mc_world_terminal_limits *,
                                                              mc_world_terminal_inventory *,
                                                              mc_world_storage_error *);

/* Same structural preflight used by the existing joint container commit,
 * without releasing anything or requiring the new strict scope. */
mc_world_storage_status mc_world_runtime_entity_containers_validate(mc_world_runtime *,
                                                                    mc_world_storage_error *);

/* Store and Chunk hooks validate their canonical storage before visiting.
 * Nonempty Chunk slices require the genuine mc_entity_release finalizer. */
mc_world_storage_status mc_world_store_terminal_visit_chunks(mc_world_store *,
                                                             const mc_world_runtime *,
                                                             mc_world_terminal_chunk_visitor,
                                                             void *, mc_world_storage_error *);
mc_world_storage_status mc_world_chunk_terminal_visit_entities(const mc_world_chunk *,
                                                               mc_world_terminal_entity_visitor,
                                                               void *, mc_world_storage_error *);

/* Tile hooks remain in the owner of the opaque layout and embedded Map. No
 * active class initialization or source-map projection adoption occurs. */
mc_world_storage_status mc_live_tile_entities_terminal_visit_lists(const mc_world_runtime *,
                                                                   mc_world_terminal_tile_visitor,
                                                                   void *,
                                                                   mc_world_storage_error *);
mc_world_storage_status mc_live_tile_entities_terminal_visit_owned_map(
    const mc_world_chunk *, mc_world_terminal_tile_visitor, void *, mc_world_storage_error *);

typedef struct mc_tile_entity_terminal_view {
    const mc_world_runtime_classes *parent;
    mc_world_runtime *world;
    size_t holds;
} mc_tile_entity_terminal_view;

mc_world_storage_status mc_tile_entity_terminal_read(const mc_tile_entity *,
                                                     mc_tile_entity_terminal_view *,
                                                     mc_world_storage_error *);

typedef struct mc_tile_entity_terminal_class_inventory {
    size_t live_tiles, piston_occurrences;
} mc_tile_entity_terminal_class_inventory;

mc_world_storage_status mc_tile_entity_piston_terminal_visit_entities(
    const mc_world_runtime_classes *, mc_world_terminal_entity_visitor, void *,
    mc_tile_entity_terminal_class_inventory *, mc_world_storage_error *);

#endif
