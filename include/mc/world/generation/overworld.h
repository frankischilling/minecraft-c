#ifndef MC_WORLD_GENERATION_OVERWORLD_H
#define MC_WORLD_GENERATION_OVERWORLD_H

#include "mc/world/biome/biome.h"
#include "mc/world/chunk/finish.h"
#include "mc/world/terrain/caves.h"
#include "mc/world/terrain/surface.h"

#define MC_OVERWORLD_GENERATOR_BIOME_MATERIALS 13u

typedef enum mc_overworld_generator_status {
    MC_OVERWORLD_GENERATOR_OK = 0,
    MC_OVERWORLD_GENERATOR_INVALID_ARGUMENT,
    MC_OVERWORLD_GENERATOR_LIMIT,
    MC_OVERWORLD_GENERATOR_ALLOCATION_FAILED,
    MC_OVERWORLD_GENERATOR_STATE_ERROR,
    MC_OVERWORLD_GENERATOR_PROFILE_FAILED
} mc_overworld_generator_status;

/* Native initialization state, not Java class initialization. FAILED requires
 * retirement with free before another attempt; it may have consumed profile
 * constructor entropy even though initialized is false. */
typedef enum mc_overworld_generator_phase {
    MC_OVERWORLD_GENERATOR_PHASE_RAW = 0,
    MC_OVERWORLD_GENERATOR_PHASE_INITIALIZING,
    MC_OVERWORLD_GENERATOR_PHASE_READY,
    MC_OVERWORLD_GENERATOR_PHASE_FAILED
} mc_overworld_generator_phase;

typedef enum mc_overworld_generator_origin {
    MC_OVERWORLD_GENERATOR_NUMERICAL = 0,
    MC_OVERWORLD_GENERATOR_PROVIDER
} mc_overworld_generator_origin;

const char *mc_overworld_generator_status_string(mc_overworld_generator_status status);

/* Current original biome byte fields, indexed by mc_biome_id - 1. All 13
 * identities must be supplied, including Hell and Sky; NONE has no slot.
 * Every top/filler byte pattern is accepted. No defaults are substituted. */
typedef struct mc_overworld_generator_materials {
    mc_terrain_surface_material by_biome[MC_OVERWORLD_GENERATOR_BIOME_MATERIALS];
} mc_overworld_generator_materials;

/* Fresh caller-owned identity and voxel data. Blocks are borrowed separately.
 * Keep the wrapper at a stable address while any returned state is retained.
 * Distinct generated results require distinct wrappers and block storage. */
typedef struct mc_overworld_chunk {
    mc_chunk_state chunk;
    mc_chunk_data data;
} mc_overworld_chunk;

/* Per-call borrowed current world resources. This lighting/cache context,
 * the generator seed and its biome manager must belong to the same logical
 * world. Keep all inputs stable and grant exclusive mutable access throughout
 * a call. The optional finishing trace and abort count follow finish.h. */
typedef struct mc_overworld_generation_resources {
    mc_world_lighting *lighting;
    const mc_overworld_generator_materials *materials;
    const int32_t *opacity;
    size_t opacity_length;
    mc_chunk_finish_trace *trace;
    size_t *queue_aborts;
} mc_overworld_generation_resources;

/* Zero-initialize before init; free before reinitializing. The initialized
 * manager and immutable initialized trig table are borrowed, never freed or
 * replaced. The manager's memory owner must outlive this object. Only biomes
 * owns allocation, through that existing owner. Do not copy an owning generator
 * or alter its descriptors, dependencies, terrain state or initialized flag.
 * Scratch contents and Random continuations are available for inspection.
 * Calls require exclusive generator/manager/world access and no callbacks.
 *
 * All 82 terrain layers and constructor Random continuation are retained.
 * Public init selects the entropy-free numerical projection: its cave Random
 * has no reference-defined initial continuation and each pass fresh-seeds it.
 * The private provider constructor instead owns a real default cave Random;
 * its passes use setSeed semantics, retaining cached bits and clearing validity.
 * Origin, phase and range are module-owned state, not caller policy switches.
 * Neither path is the full original provider/World constructor. Fixed scratch
 * retains its address across calls. Direct biome sampling updates manager
 * a/b/c and leaves d untouched. */
typedef struct mc_overworld_generator {
    int64_t world_seed;
    mc_biome_manager *biome_manager;
    const mc_math_trig *trig;
    mc_terrain_overworld_state terrain;
    mc_java_random generator_random;
    mc_java_random cave_random;
    mc_biome_buffer biomes;
    double density[MC_TERRAIN_OVERWORLD_DENSITY_VALUES];
    double mix[MC_TERRAIN_OVERWORLD_DENSITY_VALUES];
    double low[MC_TERRAIN_OVERWORLD_DENSITY_VALUES];
    double high[MC_TERRAIN_OVERWORLD_DENSITY_VALUES];
    double column_scale[MC_TERRAIN_OVERWORLD_COLUMN_VALUES];
    double column_bias[MC_TERRAIN_OVERWORLD_COLUMN_VALUES];
    double sand[MC_TERRAIN_OVERWORLD_SURFACE_VALUES];
    double gravel[MC_TERRAIN_OVERWORLD_SURFACE_VALUES];
    double thickness[MC_TERRAIN_OVERWORLD_SURFACE_VALUES];
    mc_terrain_surface_material materials[MC_TERRAIN_OVERWORLD_CLIMATE_VALUES];
    mc_overworld_generator_phase phase;
    mc_overworld_generator_origin origin;
    int32_t cave_range;
    bool initialized;
} mc_overworld_generator;

/* Allocation-free construction. Invalid arguments preserve the generator.
 * manager must already be initialized for world_seed, with valid module-owned
 * descriptors and unmodified noise/classifier state. This function does not
 * initialize, sample, resize or free any manager buffer, construct a default
 * Random or use a profile/clock. A reached defensive initialization failure
 * latches FAILED; free retires that attempt before another init. */
mc_overworld_generator_status mc_overworld_generator_init(mc_overworld_generator *generator,
                                                          int64_t world_seed,
                                                          mc_biome_manager *manager,
                                                          const mc_math_trig *trig);

/* Releases only the separately owned biome output, then restores zero state.
 * Accepts NULL or a zero-initialized generator. Never frees the borrowed manager,
 * its a/b/c/d arrays, the trig table, result wrappers, result blocks or shared
 * Random profile. Also accepts a failed initialization attempt. */
void mc_overworld_generator_free(mc_overworld_generator *generator);

/* Complete unpublished Overworld generation: reseed, fresh constructor data,
 * direct 16x16 climate/biomes, base terrain, surface, caves and full finishing.
 * Coordinates must lie in INT32_MIN+8 .. INT32_MAX-9 on both axes, matching the
 * existing finite ordinary cave scheduler. Blocks need at least 32768 bytes;
 * their suffix is retained. Every successful result is fresh normal data with
 * zero metadata/block light, computed height/skylight and dirty=true.
 *
 * Preflight checks fixed resources, current classifier IDs, imported world,
 * opacity, trace and worst-case finishing queue reserve before mutation.
 * A bounded queue needs 1024 free slots; million-entry storage uses the original
 * automatic clear policy, reported through queue_aborts. Result state/data
 * identities must be absent from the cache and insertion list.
 * Direct state/data identity equality is rejected. General disjoint storage,
 * including block buffers, remains a caller precondition without pointer-range
 * ordering checks; cached height-only data need not expose initialized blocks.
 *
 * INVALID_ARGUMENT, LIMIT and ALLOCATION_FAILED preserve every caller output,
 * generator/manager state, world/queue/trace and allocation accounting. The
 * existing biome sampler stages all allocations atomically. Constructor data
 * and seeded Random are staged locally until sampling succeeds; successful
 * behavior is preserved under the required exclusive access and no callbacks.
 * No owning descriptors are copied for rollback. After that commit, fixed
 * stages cannot reject valid stable inputs. STATE_ERROR exposes a defensive
 * downstream invariant failure, may retain partial effects, and never denotes
 * a usable generated chunk. Random continuations commit only on OK.
 * No publication, population, loading, propagation, hidden allocation or I/O.
 * Exact numerical results require the existing strict floating-point contract. */
mc_overworld_generator_status
mc_overworld_generator_generate(mc_overworld_generator *generator, int32_t chunk_x, int32_t chunk_z,
                                const mc_overworld_generation_resources *resources,
                                mc_overworld_chunk *result, uint8_t *blocks,
                                size_t blocks_capacity);

#endif
