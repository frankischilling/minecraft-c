#ifndef MC_WORLD_RUNTIME_RANDOM_TICK_H
#define MC_WORLD_RUNTIME_RANDOM_TICK_H

#include "mc/core/math/classes.h"
#include "mc/world/runtime/chunk_tick.h"
#include "mc/world/runtime/entity_errors.h"
#include "mc/world/runtime/movement.h"
#include "mc/world/runtime/mutations.h"

typedef struct mc_world_random_tick_call mc_world_random_tick_call;
typedef struct mc_lightning_error mc_lightning_error;

typedef enum mc_world_random_tick_child {
    MC_WORLD_RANDOM_TICK_NONE,
    MC_WORLD_RANDOM_TICK_MATH,
    MC_WORLD_RANDOM_TICK_ACQUISITION,
    MC_WORLD_RANDOM_TICK_CHUNK,
    MC_WORLD_RANDOM_TICK_QUERY,
    MC_WORLD_RANDOM_TICK_REGISTRY,
    MC_WORLD_RANDOM_TICK_BIOME,
    MC_WORLD_RANDOM_TICK_RAIN,
    MC_WORLD_RANDOM_TICK_NEAREST,
    MC_WORLD_RANDOM_TICK_SOUND,
    MC_WORLD_RANDOM_TICK_LIGHTNING,
    MC_WORLD_RANDOM_TICK_PUBLICATION,
    MC_WORLD_RANDOM_TICK_MUTATION
} mc_world_random_tick_child;

typedef struct mc_world_random_tick_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    unsigned int bytecode, child_bytecode;
    const char *child_class, *child_method;
    bool native_policy;
    mc_world_random_tick_child child;
    mc_world_storage_error cause;
    mc_math_class_error math;
    const mc_world_runtime_acquisition_error *acquisition;
    const mc_world_chunk_tick_error *chunk;
    const mc_world_query_error *query;
    const mc_registry_error *registry;
    const mc_world_chunk_manager_error *biome;
    const mc_world_movement_error *rain;
    const mc_lightning_error *lightning;
    const mc_world_entity_error *publication;
    const mc_world_mutation_error *mutation;
    mc_world_access_failure world_access;
} mc_world_random_tick_error;

typedef struct mc_world_random_tick_state {
    bool active, returned, candidates_returned, cooldown_decremented;
    size_t chunk_count, chunks_started, chunks_returned;
    size_t samples_started, samples_returned, block_ticks, random_calls, lcg_updates;
    size_t ambient_attempts, ambient_sounds, lightning_attempts, weather_publications;
    size_t snow_attempts, snow_writes, ice_writes;
    unsigned int source_pc;
    int32_t chunk_x, chunk_z, offset_x, offset_z, sample;
    int32_t x, y, z, local_x, local_y, local_z, block_id, below_id, current_id;
    int32_t top_y, light, draw, last_g;
    float rain_strength, thunder_strength, pitch;
    bool raining, thundering, snowy, publication_result;
    const mc_world_active_chunks *iterator_set;
    mc_world_chunk_tick_state chunk;
    mc_entity *nearest, *lightning;
    mc_world_runtime *world;
    mc_math_classes *math;
    const mc_world_random_tick_error *error;
} mc_world_random_tick_state;

/* Actual client fd.n()V, including its P clear/radius-nine Player gathering,
 * Q decrement, captured iterator and acquiring Chunk locals. Every reached
 * Chunk performs all eighty LCG samples; World.r and World.g are distinct
 * source streams. Ambient sound, weather, snow/ice and actual Block virtuals
 * preserve source order and all effects before a later failure.
 *
 * Requires a live exclusive client GAME owner. Actual Math is held until
 * retirement and initialized only at reached floors. The call owns every
 * captured Chunk, returned Player and Lightning candidate across callbacks
 * and failure. No pool reset, profile rebinding, source repair, configurable
 * work cap or successful substitute for a missing child is supplied.
 * out begins NULL; a published partial call MUST be retired. Native admission
 * preserves World diagnostics. State/child error references borrow the call.
 * Arguments, outputs, errors and owners must be live/exclusive/disjoint. */
mc_world_storage_status mc_world_runtime_random_tick_begin(mc_world_runtime *, mc_math_classes *,
                                                           mc_world_random_tick_call **,
                                                           mc_world_random_tick_error *admission);
bool mc_world_runtime_random_tick_inspect(const mc_world_random_tick_call *,
                                          mc_world_random_tick_state *);
mc_world_storage_status mc_world_runtime_random_tick_retire(mc_world_random_tick_call **,
                                                            mc_world_random_tick_error *cleanup);

#endif
