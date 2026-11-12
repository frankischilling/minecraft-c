#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_TICK_INTERNAL_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_TICK_INTERNAL_H
#include "mc/world/runtime/tile_entities/tile_entities.h"

typedef struct mc_live_tile_iterator {
    mc_world_reference_list *identity;
    size_t cursor, last_returned;
    int32_t expected_mod_count;
} mc_live_tile_iterator;

/* Caller holds the actual GAME World and all supplying dependencies. Iterator
 * identity is captured once; source c replacement does not retarget it. Each
 * next returns one independent native hold (NULL has none); remove retires
 * the actual current indexed occurrence, not the selected local's identity. */
bool mc_live_tile_iterator_begin_held(mc_world_runtime *, mc_world_reference_list *,
                                      mc_live_tile_iterator *, mc_live_tile_entity_error *);
bool mc_live_tile_iterator_has_next_held(mc_world_runtime *, mc_live_tile_iterator *, bool *,
                                         mc_live_tile_entity_error *);
bool mc_live_tile_iterator_next_held(mc_world_runtime *, mc_live_tile_iterator *, mc_tile_entity **,
                                     mc_live_tile_entity_error *);
bool mc_live_tile_iterator_remove_held(mc_world_runtime *, mc_live_tile_iterator *,
                                       mc_live_tile_entity_error *);
bool mc_live_tile_chunk_remove_held(mc_world_chunk *, int32_t, int32_t, int32_t,
                                    mc_live_tile_entity_error *);
mc_registry_classes *mc_live_tile_tick_code_registry(const mc_tile_entity *);
#endif
