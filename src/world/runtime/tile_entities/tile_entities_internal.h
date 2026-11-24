#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_INTERNAL_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_INTERNAL_H

#include "mc/world/runtime/tile_entities/tile_entities.h"

#include "../../../core/java/object_identity_internal.h"

/* The actual native TileEntity owner layout. Instance field controls used by native
 * invariant/reference tests must be explicitly labeled and restored. Public
 * clients use held owner APIs; raw field access adds no gameplay/tick support. */
struct mc_tile_entity {
    size_t holds;
    /* Native Object identity, outside all Source kind-state assignments. The
     * zeroed RAW allocation owns this cell until final native destruction;
     * constructors, invalidation, World rebinding and NBT never reset/copy it. */
    mc_java_identity_cell identity;
    mc_tile_entity_kind kind;
    mc_tile_entity_phase phase;
    mc_tile_entity_classes *classes;
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
            mc_item_stack **slots;
            size_t capacity;
            int32_t burn_remaining, fuel_duration, cook_progress;
        } furnace;

        struct {
            mc_item_stack **slots;
            size_t capacity;
            mc_java_random *random; /* Actual AZ.b, stored only after its constructor returns. */
            mc_java_random *constructor_random; /* Native owner of an unreturned NEW prefix. */
        } dispenser;

        struct {
            int32_t delay;
            double rotation, previous_rotation;
            mc_tile_entity_name *name;
        } spawner;

        struct {
            mc_tile_entity_name **lines;
            uint32_t count, capacity; /* Both bounded to the four constructor entries. */
            int32_t edit_row;
            bool read_flag;
        } sign;

        struct {
            int32_t block_id, block_data, facing;
            bool extending, render_head;
            uint32_t progress_bits, previous_progress_bits;
        } piston;
    } state;
};

#endif
