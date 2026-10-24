#ifndef MC_WORLD_RUNTIME_MOVEMENT_H
#define MC_WORLD_RUNTIME_MOVEMENT_H

#include "mc/entity/entity.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/runtime/collision/boxes.h"
#include "mc/world/runtime/presentation.h"
#include "mc/world/runtime/queries.h"

typedef struct mc_world_movement_error {
    mc_world_storage_status status;
    const char *operation;
    /* bytecode belongs to the selected outer World operation. The other
     * fields retain independent reached descendant frames; zero is unreached
     * or native admission. No frame is reconstructed from World last_error. */
    unsigned int bytecode, area_bytecode, presence_bytecode, provider_bytecode;
    unsigned int raining_bytecode, rain_strength_bytecode;
    unsigned int dimension_bytecode, biome_bytecode, world_access_bytecode;
    mc_world_storage_error cause;
    mc_world_query_error query;
    /* Fresh actual bounded block getter/acquisition diagnostics, including
     * full load/activation/TileEntity/Entity children, distinct from rain queries. */
    mc_world_collision_error block;
    mc_registry_error registry;
    mc_world_provider_status dimension;
    mc_world_chunk_manager_error biome;
    mc_world_mutation_error presentation;
    mc_entity_error entity;
} mc_world_movement_error;

mc_world_movement_error mc_world_movement_empty_error(void);

/* Actual fd/dj.a(IIIIII)Z. max_y<0 or min_y>=128 returns false before any
 * provider access. Arithmetic chunk shifts precede inclusive X/Z presence
 * checks. Y is not scanned, reversed ranges retain the source loop behavior,
 * and a missing Chunk is false without loading, generation or payload reads. */
mc_world_storage_status mc_world_runtime_area_loaded(mc_world_runtime *world, int32_t min_x,
                                                     int32_t min_y, int32_t min_z, int32_t max_x,
                                                     int32_t max_y, int32_t max_z, bool *out,
                                                     mc_world_movement_error *error);

/* Actual fd.t/dj.s(III)Z: rain-strength(1)>0.2, actual sky visibility, actual
 * top-solid/liquid height, current Dimension.b point Biome, then its c/d
 * predicates, with each short circuit intact. Retain the captured manager
 * through sampling and current Biome field reads across synchronous reentry. */
mc_world_storage_status mc_world_runtime_rain_at(mc_world_runtime *world, int32_t x, int32_t y,
                                                 int32_t z, bool *out,
                                                 mc_world_movement_error *error);

/* Actual fd.c(eq)/dj.d(cz). Read min/floor(max+1) in X/Y/Z order, check the
 * actual loaded area, then scan exclusive X/Y/Z limits. Each acquired ID
 * compares against current Block.as (Fire), D and E (Lava) bn fields
 * in that order, including for air. No Material, static ID or fake area result
 * substitutes for those reached active uses. bounds may be NULL until its
 * reached source getfield; its live storage remains the caller's obligation. */
mc_world_storage_status mc_world_runtime_burning(mc_world_runtime *world,
                                                 const mc_entity_bounds *bounds, bool *out,
                                                 mc_world_movement_error *error);

/* Distinct fd/dj.a(Entity,String,FF)V overload. An actual empty WorldAccess List
 * returns before dereferencing entity or doing FP arithmetic. A reached null
 * WorldAccess accepts its cast, then reads current Entity x,y,offset,z around
 * y-offset before failing at invocation53. Each typed WorldAccess receives fresh
 * Entity fields after the preceding callback; a missing sound slot fails at53
 * after the same argument evaluation. entity and name may be NULL;
 * no Registry/String read, Random draw or coordinate-sound substitution occurs.
 * A nonnull name borrows its actual live Registry-owned identity. */
mc_world_storage_status mc_world_runtime_entity_sound(mc_world_runtime *world, mc_entity *entity,
                                                      const mc_registry_string *name, float volume,
                                                      float pitch, mc_world_movement_error *error);

/* Each operation holds the exact GAME World through reached descendants.
 * RAW/constructing/failed prefixes are admitted; retirement and COMPOSITION
 * reject. Keep all owners/arguments live and exclusive, and output/error
 * storage disjoint from each other and owner/dependency storage. Boolean query
 * out is required, error may be NULL. Failure preserves out and every reached source
 * effect; success clears this call's error without clearing earlier World
 * diagnostics. Native argument/phase admission changes only this error frame.
 * The strict x86-64 FP environment is checked only at reached arithmetic;
 * area presence and empty sound do not require FP admission. No save, rollback,
 * implicit pool reset or later movement work occurs. */

#endif
