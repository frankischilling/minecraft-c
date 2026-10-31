#ifndef MC_ENTITY_MOB_SPAWNING_H
#define MC_ENTITY_MOB_SPAWNING_H

#include "mc/core/math/classes.h"
#include "mc/entity/mob/construction.h"
#include "mc/world/runtime/brightness.h"
#include "mc/world/runtime/chunk_random.h"
#include "mc/world/runtime/collision/spawning.h"
#include "mc/world/runtime/material_movement.h"

typedef struct mc_mob_spawn_state {
    mc_entity_class_kind kind;
    bool returned, eligible, living_reached, creature_reached, path_weight_reached;
    bool saved_sky_reached, light_reached, thunder_reached, thundering;
    bool temporary_subtraction_stored, subtraction_restored, chunk_reached;
    unsigned int source_pc, monster_pc, creature_pc, living_pc, slime_pc, animal_pc;
    int32_t x, y, z, saved_sky, light, saved_subtraction, slime_size;
    int32_t draw32, draw8, draw10, chunk_draw10, below_id, plant_light;
    size_t own_random_calls, chunk_random_calls, collision_count;
    float thunder_strength, path_weight;
    mc_world_chunk_random_state chunk;
    mc_java_random chunk_random;
} mc_mob_spawn_state;

typedef struct mc_mob_spawn_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    mc_mob_site site, child;
    bool native_policy, source_exception;
    mc_entity_status entity;
    mc_math_class_error math;
    mc_world_storage_error world;
    mc_registry_error registry;

    union {
        mc_world_query_error query;
        mc_world_brightness_error brightness;
        mc_world_spawn_collision_error no_entity;
        mc_world_collision_error collision;
        mc_world_material_movement_error material;
        mc_world_runtime_acquisition_error acquisition;
        mc_world_chunk_random_error chunk_random;
    } detail;
} mc_mob_spawn_error;

/* Genuine client virtual spawn predicate on the actual Mob. No replacement
 * World, RNG, bounds or inspection graph is accepted. Entity/math and reached
 * Worlds/Chunks/Lists are held for this synchronous call. The explicit World
 * class context supplies reached Light active use even for a NULL source
 * World, and must match the Entity/profile namespace and actual Math table.
 * Returned false is a successful source result; failure preserves out and all reached effects,
 * including GZ's f=10 when its second light query fails before source restore.
 * state/error are required fresh caller storage, live/exclusive/disjoint from
 * each other, out and all owners. Diagnostics borrow reached loader failures
 * through the caller-held graph. There is no escaping native cleanup handle,
 * replay, implicit pool reset or source-field repair. */
mc_world_storage_status mc_mob_spawn_eligible(mc_entity *, mc_math_classes *,
                                              mc_world_runtime_classes *, bool *out,
                                              mc_mob_spawn_state *, mc_mob_spawn_error *);

#endif
