#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_FURNACE_TICK_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_FURNACE_TICK_H

#include "mc/world/runtime/tile_entities/furnace.h"

typedef struct mc_furnace_tick_work mc_furnace_tick_work;
typedef struct mc_world_mutation_error mc_world_mutation_error;

/* Fresh/noncopyable output. Only a reached World switch/dirty call allocates
 * retained diagnostics; ordinary burn/cook countdown allocates nothing.
 * Child views and their producer graph stay live until checked retirement.
 * A failed source prefix is never replayed, rolled back or completed there. */
typedef struct mc_furnace_tick_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode;
    bool source_null, returned;
    mc_tile_entity_error tile;
    const mc_world_mutation_error *mutation;
    const mc_live_tile_entity_error *dirty;
    mc_world_storage_error cleanup;
    mc_furnace_tick_work *work;
} mc_furnace_tick_error;

/* Actual client SK.n_(). The caller keeps the held READY Tile and its defining
 * namespace/external World/provider/Store/observer graph live and exclusive.
 * Invalidity is the enclosing World loop's guard, not a new SK early return.
 * Nullable World fails only at B36, after an actual positive burn decrement.
 * A fresh error is required to receive any retained World child failure. */
mc_world_storage_status mc_tile_entity_furnace_tick(mc_tile_entity *, mc_furnace_tick_error *);
bool mc_tile_entity_furnace_tick_retire(mc_furnace_tick_error *);

#endif
