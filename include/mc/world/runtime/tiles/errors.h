#ifndef MC_WORLD_RUNTIME_TILES_ERRORS_H
#define MC_WORLD_RUNTIME_TILES_ERRORS_H

#include "mc/block/registry/registry.h"
#include "mc/entity/errors.h"
#include "mc/item/stack/stack.h"
#include "mc/storage/nbt/nbt.h"
#include "mc/world/runtime/entity_errors.h"
#include "mc/world/runtime/loading/entity_errors.h"
#include "mc/world/storage/status.h"

#include <stddef.h>
#include <stdint.h>

/* Passive value diagnostics shared by the actual Tile owner and its World,
 * mutation, activation and Store callers. The Entity/publication error headers
 * retain their domain-owned definitions without importing Entity operations,
 * World construction/runtime or Store.
 *
 * Error strings are static diagnostics. These records own no pointers, memory,
 * class/World bindings or Throwable state. Copying a complete record copies
 * diagnostics only; it never retains/releases owners or changes source state. */
typedef enum mc_tile_status {
    MC_TILE_OK = 0,
    MC_TILE_INVALID_ARGUMENT,
    MC_TILE_INVALID_KIND,
    MC_TILE_INVALID_PHASE,
    MC_TILE_INVALID_FIELD,
    MC_TILE_INVALID_SLOT,
    MC_TILE_ALLOCATION_FAILED,
    MC_TILE_LIMIT,
    MC_TILE_WORLD_FAILED,
    MC_TILE_ITEM_FAILED,
    MC_TILE_NBT_ERROR,
    MC_TILE_STATE_ERROR
} mc_tile_status;

typedef struct mc_tile_error {
    mc_tile_status status;
    const char *field; /* Static ASCII key or NULL. */
    size_t item_index; /* SIZE_MAX unless a particular Items entry failed. */
    mc_item_stack_error item;
    mc_nbt_error nbt;             /* Whole-tree codec error, separate from item context. */
    unsigned int source_bytecode; /* Factory25 or read46 for composed imports. */
    /* Reached serialization frames, independent of import/constructor frames.
     * base is ow/jh.b; save is concrete Chest/Spawner.b; item is Stack.a.
     * Native planning/codec errors without a reached field leave these zero. */
    unsigned int base_save_bytecode, save_bytecode, item_save_bytecode;
    /* Fresh per-attempt Registry cause; empty sentinels on native-only errors.
     * Constructor PC15 identifies spawner Pig resolution after successful
     * base-class initialization; initializer PC remains a separate frame. */
    mc_registry_error registry;
    unsigned int constructor_bytecode, initializer_bytecode;
} mc_tile_error;

typedef struct mc_live_tile_call_frame {
    unsigned int bytecode, chunk_bytecode, observer_bytecode, initializer_bytecode;
    unsigned int caller_bytecode, hook_bytecode, remove_bytecode, provider_bytecode;
    unsigned int tile_bytecode, inventory_bytecode;
} mc_live_tile_call_frame;

typedef struct mc_live_tile_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, chunk_bytecode, observer_bytecode, initializer_bytecode;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_tile_error tile;
    mc_entity_error entity;
    mc_world_entity_error publication;
    mc_world_entity_batch_error batch;
    /* Source caller frames are independent of the deepest reached child.
     * Removal used to discard Tile/Registry/Entity data at this join. */
    unsigned int caller_bytecode, hook_bytecode;
    unsigned int remove_bytecode; /* enclosing fd.p/dj.o, distinct from lazy lookup */
    unsigned int tile_bytecode, inventory_bytecode;
    /* Outer acquiring Tile call and its reached Chunk/World activation join.
     * The complete child fields above remain unchanged during propagation. */
    unsigned int acquisition_caller_bytecode, activation_bytecode, activation_world_bytecode;
    unsigned int load_bytecode, load_builder_bytecode;
    mc_world_entity_load_error load_entity;
    unsigned int provider_bytecode;
    /* The acquiring caller's already-reached frames, beside the complete
     * load/activation child above. Current supported import/activation bodies
     * do not perform another acquiring Tile call. */
    mc_live_tile_call_frame acquisition_parent;
} mc_live_tile_error;

const char *mc_tile_status_string(mc_tile_status status);

/* Return fresh native value records. All statuses are their named OK value,
 * pointer fields are NULL and source frames/system/codec offsets are zero.
 * item_index is SIZE_MAX and both Registry frames have all five -1 sentinels.
 * No allocation, class use, owner access, Random, I/O or callback occurs.
 * Do not apply these initializers to erase an already reached failure/caller
 * frame during propagation; initialize once before the source attempt. */
mc_tile_error mc_tile_empty_error(void);
mc_live_tile_error mc_live_tiles_empty_error(void);

#endif
