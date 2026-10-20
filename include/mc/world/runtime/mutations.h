#ifndef MC_WORLD_RUNTIME_MUTATIONS_H
#define MC_WORLD_RUNTIME_MUTATIONS_H

#include "mc/entity/entity.h"
#include "mc/world/runtime/entities.h"

#include "mc/world/runtime/lighting.h"
#include "mc/world/runtime/tiles/errors.h"

typedef struct mc_world_mutation_error {
    mc_world_storage_status status;
    const char *operation;
    /* Separate reached source frames; zero is native admission or unreached.
     * wrapper is the notifying entry, bytecode its quiet World callee. */
    unsigned int wrapper_bytecode, bytecode, chunk_bytecode, height_bytecode;
    unsigned int column_bytecode, interval_bytecode, observer_bytecode;
    unsigned int neighbor_call_bytecode, neighbor_bytecode, hook_bytecode;
    unsigned int presence_bytecode, provider_bytecode;
    const char *hook_class;
    const char *hook_method;
    const char *drop_class, *drop_method;
    unsigned int drop_bytecode, drop_call_bytecode, drop_spawn_bytecode;
    mc_item_stack_status item;
    mc_entity_error entity;
    mc_world_entity_error publication;
    mc_world_entity_batch_error batch;
    mc_live_tile_error tile;
    unsigned int activation_bytecode, activation_world_bytecode;
    unsigned int load_bytecode, load_builder_bytecode;
    mc_world_entity_load_error load_entity;
    /* Flat nested diagnostics retain the current concrete caller and deepest
     * reached concrete hook origin, rather than a recursive call-stack owner.
     * No gameplay recursion or work limit follows from this representation. */
    const char *cause_hook_class, *cause_hook_method;
    unsigned int cause_hook_bytecode;
    unsigned int schedule_bytecode, support_bytecode;
    mc_scheduled_tick_status record;
    mc_scheduled_membership_status membership;
    mc_scheduled_ordered_status ordered;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_light_error light;
} mc_world_mutation_error;

/* Actual GAME World setters. Bounds and acquisition precede the selected
 * concrete Chunk call. ID and metadata remain full Java ints until each source
 * narrowing/index operation. Equal results are ordinary booleans, independent
 * of native status. Blank overrides return true without changing their bytes.
 * The two Chunk overloads retain their distinct hook/remote/no-sky ordering.
 *
 * Hold the World and exact selected Chunk through synchronous calls. Failure
 * preserves out and every reached byte, metadata, height, queue, hook and dirty
 * effect; no rollback/retry occurs. Missing reached descendants fail explicitly.
 * Require live exclusive owners and disjoint arguments/results/errors. GAME
 * construction/failed prefixes are admitted; RETIRED/COMPOSITION are rejected.
 * No whole-ID-table or future-array preflight replaces the source accesses. */
mc_world_storage_status mc_world_runtime_block_set_quiet(mc_world_runtime *world, int32_t x,
                                                         int32_t y, int32_t z, int32_t id,
                                                         bool *out, mc_world_mutation_error *error);
mc_world_storage_status mc_world_runtime_block_metadata_set_quiet(mc_world_runtime *world,
                                                                  int32_t x, int32_t y, int32_t z,
                                                                  int32_t id, int32_t metadata,
                                                                  bool *out,
                                                                  mc_world_mutation_error *error);

/* Notifying variants call the corresponding quiet operation, then observers
 * and six live neighbors only on ordinary true. They pass the original full
 * requested ID. Observer and neighbor failures preserve prior effects/out.
 * These operations do not enable a future lighting drain or later tick. */
mc_world_storage_status mc_world_runtime_block_set(mc_world_runtime *world, int32_t x, int32_t y,
                                                   int32_t z, int32_t id, bool *out,
                                                   mc_world_mutation_error *error);
mc_world_storage_status mc_world_runtime_block_metadata_set(mc_world_runtime *world, int32_t x,
                                                            int32_t y, int32_t z, int32_t id,
                                                            int32_t metadata, bool *out,
                                                            mc_world_mutation_error *error);

/* Original notifying metadata operation is void. After quiet true, acquire the
 * current ID again, then read live Block.t[id&255]. That flag selects observer
 * plus neighbors or neighbors alone. Repeated/equal nibble writes still run it.
 * Native status is the only ordinary return channel of this C operation. */
mc_world_storage_status mc_world_runtime_metadata_set(mc_world_runtime *world, int32_t x, int32_t y,
                                                      int32_t z, int32_t metadata,
                                                      mc_world_mutation_error *error);

#endif
