#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_ERRORS_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_ERRORS_H

#include "mc/block/registry/registry.h"
#include "mc/entity/errors.h"
#include "mc/item/stack/stack.h"
#include "mc/storage/nbt/nbt.h"
#include "mc/world/runtime/entity_errors.h"
#include "mc/world/runtime/loading/entity_errors.h"
#include "mc/world/storage/status.h"

#include <stddef.h>
#include <stdint.h>

/* Passive value diagnostics shared by the actual TileEntity owner and its World,
 * mutation, activation and Store callers. The Entity/publication error headers
 * retain their domain-owned definitions without importing Entity operations,
 * World construction/runtime or Store.
 *
 * Error strings are static diagnostics. These records own no pointers, memory,
 * class/World bindings or Throwable state. Copying a complete record copies
 * diagnostics only; it never retains/releases owners or changes source state. */
typedef enum mc_tile_entity_status {
    MC_TILE_ENTITY_OK = 0,
    MC_TILE_ENTITY_INVALID_ARGUMENT,
    MC_TILE_ENTITY_INVALID_KIND,
    MC_TILE_ENTITY_INVALID_PHASE,
    MC_TILE_ENTITY_INVALID_FIELD,
    MC_TILE_ENTITY_INVALID_SLOT,
    MC_TILE_ENTITY_ALLOCATION_FAILED,
    MC_TILE_ENTITY_LIMIT,
    MC_TILE_ENTITY_WORLD_FAILED,
    MC_TILE_ENTITY_ITEM_FAILED,
    MC_TILE_ENTITY_NBT_ERROR,
    MC_TILE_ENTITY_STATE_ERROR,
    MC_TILE_ENTITY_UNSUPPORTED_FP
} mc_tile_entity_status;

typedef struct mc_tile_entity_error {
    mc_tile_entity_status status;
    const char *field; /* Static ASCII key or NULL. */
    size_t item_index; /* SIZE_MAX unless a particular Items entry failed. */
    mc_item_stack_error item;
    mc_nbt_error nbt;             /* Whole-tree codec error, separate from item context. */
    unsigned int source_bytecode; /* Factory25 or read46 for composed imports. */
    /* Base and concrete semantic read frames are distinct. Piston read invokes
     * base at2, then reaches its own exact typed getters at9/19/29/40/54. */
    unsigned int base_read_bytecode, read_bytecode;
    /* Native arithmetic admission is separate from NBT/source field lookup. */
    const char *operation;
    unsigned int getter_bytecode;
    /* Reached serialization frames, independent of import/constructor frames.
     * base is ow/jh.b; save is concrete Chest/Spawner/Piston.b; item is Stack.a.
     * Native planning/codec errors without a reached field leave these zero. */
    unsigned int base_save_bytecode, save_bytecode, item_save_bytecode;
    /* Fresh per-attempt Registry cause; empty sentinels on native-only errors.
     * Constructor PC15 identifies spawner Pig resolution after successful
     * base-class initialization; initializer PC remains a separate frame. */
    mc_registry_error registry;
    unsigned int constructor_bytecode, initializer_bytecode;
    /* uk/mu owns a separate initializer; base ow/jh registration of its Class
     * literal does not initialize this subclass. */
    unsigned int piston_initializer_bytecode;
} mc_tile_entity_error;

typedef struct mc_live_tile_entity_call_frame {
    unsigned int bytecode, chunk_bytecode, world_access_bytecode, initializer_bytecode;
    unsigned int caller_bytecode, hook_bytecode, remove_bytecode, provider_bytecode;
    unsigned int tile_entity_bytecode, inventory_bytecode;
} mc_live_tile_entity_call_frame;

typedef struct mc_live_tile_entity_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, chunk_bytecode, world_access_bytecode, initializer_bytecode;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_tile_entity_error tile_entity;
    mc_entity_error entity;
    mc_world_entity_error publication;
    mc_world_entity_batch_error batch;
    /* Source caller frames are independent of the deepest reached child.
     * Removal used to discard TileEntity/Registry/Entity data at this join. */
    unsigned int caller_bytecode, hook_bytecode;
    unsigned int remove_bytecode; /* enclosing fd.p/dj.o, distinct from lazy lookup */
    unsigned int tile_entity_bytecode, inventory_bytecode;
    /* Outer acquiring TileEntity call and its reached Chunk/World activation join.
     * The complete child fields above remain unchanged during propagation. */
    unsigned int acquisition_caller_bytecode, activation_bytecode, activation_world_bytecode;
    unsigned int load_bytecode, load_builder_bytecode;
    mc_world_entity_load_error load_entity;
    unsigned int provider_bytecode;
    /* The acquiring caller's already-reached frames, beside the complete
     * load/activation child above. Current supported import/activation bodies
     * do not perform another acquiring TileEntity call. */
    mc_live_tile_entity_call_frame acquisition_parent;
} mc_live_tile_entity_error;

const char *mc_tile_entity_status_string(mc_tile_entity_status status);

/* Return fresh native value records. All statuses are their named OK value,
 * pointer fields are NULL and source frames/system/codec offsets are zero.
 * item_index is SIZE_MAX and both Registry frames have all five -1 sentinels.
 * No allocation, class use, owner access, Random, I/O or callback occurs.
 * Do not apply these initializers to erase an already reached failure/caller
 * frame during propagation; initialize once before the source attempt. */
mc_tile_entity_error mc_tile_entity_empty_error(void);
mc_live_tile_entity_error mc_live_tile_entities_empty_error(void);

#endif
