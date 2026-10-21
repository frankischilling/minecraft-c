#ifndef MC_WORLD_RUNTIME_TILES_INTERNAL_H
#define MC_WORLD_RUNTIME_TILES_INTERNAL_H

#include "mc/world/runtime/tiles/tiles.h"

/* The actual native Tile owner layout. Instance field controls used by native
 * invariant/reference tests must be explicitly labeled and restored. Public
 * clients use held owner APIs; raw field access adds no gameplay/tick support. */
struct mc_tile {
    size_t holds;
    mc_tile_kind kind;
    mc_tile_phase phase;
    mc_tile_classes *classes;
    mc_world_runtime *world;
    int32_t x, y, z;
    bool invalid;
    mc_nbt_tree *source;

    union {
        struct {
            mc_item_stack **slots;
            size_t capacity;
        } chest;

        struct {
            int32_t delay;
            double rotation, previous_rotation;
            mc_tile_name *name;
        } spawner;

        struct {
            int32_t block_id, block_data, facing;
            bool extending, render_head;
            uint32_t progress_bits, previous_progress_bits;
        } piston;
    } state;
};

#endif
