#ifndef MC_WORLD_RUNTIME_GENERATION_INTERNAL_H
#define MC_WORLD_RUNTIME_GENERATION_INTERNAL_H

#include "../chunk_provider_state_internal.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/runtime/chunks.h"

/* Source frame PCs remain separate from full nested Registry/manager/Chunk
 * causes. Zero means native admission or an unreached frame. cleanup never
 * replaces the first cause, even if reentry retained the unpublished child. */
typedef struct mc_world_generation_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, base_bytecode, density_bytecode, surface_bytecode;
    unsigned int cave_scheduler_bytecode, cave_chunk_bytecode, cave_path_bytecode;
    mc_world_storage_error cause, cleanup;
    mc_chunk_state *retained_child;
    mc_world_store *retained_store;
    mc_registry_error registry;
    mc_world_chunk_manager_error biome;
    mc_world_chunk_error chunk;
} mc_world_generation_error;

/* A native finite-work departure only for source scheduler wrap loops that
 * otherwise may not terminate. Count BOTH entered outer iterations and entered
 * neighbor iterations. Ordinary range8 uses306 units. Exhaustion is a late
 * LIMIT at cave119/scheduler123, retaining all earlier effects. */
#define MC_WORLD_GENERATION_CAVE_WORK_LIMIT 65536u

/* Concrete GAME Overworld generation operation. Require the actual exclusive
 * module-owned provider and stable class/profile/resource dependencies. No provider-wide
 * reentry lock, numerical preflight, Random copy/commit or palette exists.
 * Hold World and exact constructed Chunk across callbacks. Return the SAME
 * store-owned Chunk only after base/surface/caves/full finishing. Failure
 * preserves out and all reached Random/class/climate/scratch/queue effects;
 * checked retirement removes only this unpublished child when possible.
 * Fixed backing/count/budget admission is native policy, not Java OOM timing.
 * Source v/q publication and passive NULL climate captures remain late. Each
 * climate capture holds its exact allocation across replacement, charged to
 * its live supplier's memory owner. No owning descriptor is copied. */
mc_world_storage_status mc_chunk_provider_generate_game(mc_chunk_provider *provider, int32_t x,
                                                        int32_t z, mc_chunk_state **out,
                                                        mc_world_generation_error *error);

/* Concrete child stages. The enclosing entry owns the World hold/Chunk lease
 * and exact32768 live block storage. These are not arbitrary reflection APIs.
 * Error.bytecode is already set to the actual outer invocation. */
bool mc_world_generation_surface(mc_chunk_provider *provider, int32_t x, int32_t z, uint8_t *blocks,
                                 const mc_biome_reference_array *biomes,
                                 mc_world_generation_error *error);
bool mc_world_generation_caves(mc_chunk_provider *provider, int32_t x, int32_t z, uint8_t *blocks,
                               mc_world_generation_error *error);

/* Actual ordinary Block active use, then the selected receiver's full bn.
 * Keep source getstatic and getfield failure PCs distinct. Callers themselves
 * perform reached signed comparisons/i2b/byte stores; no palette caching. */
bool mc_world_generation_block(mc_chunk_provider *provider, const char *name,
                               unsigned int static_pc, unsigned int field_pc,
                               unsigned int *inner_pc, int32_t *out,
                               mc_world_generation_error *error);
bool mc_world_generation_seed(mc_chunk_provider *provider, unsigned int pc, unsigned int *inner_pc,
                              int64_t *out, mc_world_generation_error *error);
bool mc_world_generation_fail(mc_chunk_provider *provider, mc_world_generation_error *error,
                              mc_world_storage_status status, const char *operation);
bool mc_world_generation_registry_fail(mc_chunk_provider *provider,
                                       mc_world_generation_error *error, mc_registry_status status,
                                       const char *operation);

#endif
