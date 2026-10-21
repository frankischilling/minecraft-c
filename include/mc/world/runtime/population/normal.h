#ifndef MC_WORLD_RUNTIME_POPULATION_NORMAL_H
#define MC_WORLD_RUNTIME_POPULATION_NORMAL_H

#include "mc/world/biome/registry.h"
#include "mc/world/dimension/biome_runtime.h"
#include "mc/world/dimension/temperature.h"
#include "mc/world/runtime/lighting.h"
#include "mc/world/runtime/population/context.h"
#include "mc/world/runtime/population/dungeon.h"
#include "mc/world/runtime/population/lake_spring.h"
#include "mc/world/runtime/population/ore_clay.h"
#include "mc/world/runtime/population/plants.h"
#include "mc/world/runtime/population/trees.h"
#include "mc/world/runtime/queries.h"

/* Only the selected child is an entered
 * diagnostic. Unselected child storage is not a reached zero-PC VM frame. */
typedef enum mc_world_normal_population_child {
    MC_NORMAL_CHILD_NONE = 0,
    MC_NORMAL_CHILD_REGISTRY,
    MC_NORMAL_CHILD_BIOME,
    MC_NORMAL_CHILD_TEMPERATURE,
    MC_NORMAL_CHILD_ORE_CLAY,
    MC_NORMAL_CHILD_PLANT,
    MC_NORMAL_CHILD_LAKE_SPRING,
    MC_NORMAL_CHILD_DUNGEON,
    MC_NORMAL_CHILD_TREE,
    MC_NORMAL_CHILD_HEIGHT,
    MC_NORMAL_CHILD_QUERY,
    MC_NORMAL_CHILD_MATERIAL,
    MC_NORMAL_CHILD_MUTATION
} mc_world_normal_population_child;

typedef struct mc_world_normal_population_error {
    mc_world_storage_status status;
    const char *operation;
    const char *provider_class, *method;
    unsigned int bytecode;
    /* The small World manager/seed/air bodies have distinct reached PCs. */
    unsigned int world_bytecode;
    const char *selector_class, *selector_method;
    unsigned int selector_bytecode;
    size_t iteration; /* SIZE_MAX outside an entered controller feature loop. */
    bool native_boundary;
    mc_world_normal_population_child child;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_dimension_status dimension;
    mc_biome_status temperature_status;
    /* A bounded explicitly propagated nested population context. */
    mc_world_population_context nested;

    union {
        mc_world_biome_error biome;
        mc_temperature_error temperature;
        mc_population_ore_clay_error ore_clay;
        mc_population_plant_feature_error plant;
        mc_population_lake_spring_error lake_spring;
        mc_population_dungeon_error dungeon;
        mc_world_tree_error tree;
        mc_world_light_error height;
        mc_world_query_error query;
        mc_world_material_error material;
        mc_world_mutation_error mutation;
    };
} mc_world_normal_population_error;

/* Complete yf.a(Lcl;II)V / pk.a(Lbl;II)V schedule. Actual provider->world and
 * provider->generator.generator_random are used throughout; no configurable
 * callbacks, alternate RNG, ID palette or numerical generator is accepted.
 * The interface-provider source argument is unused and is omitted here.
 *
 * Requires a genuine exclusive owned normal provider and stable native class,
 * profile/noise dependencies. Hold the actual GAME World through this call;
 * RAW/constructing/failed prefixes are admitted, retirement and COMPOSITION
 * are not. No provider-wide reentry lock, Random copy/restore or source-state
 * preflight exists. Synchronous concrete World calls may recursively populate
 * using the same provider Random and mutable w. Do not directly retire/change
 * native owner descriptors or the private p/j identities during such a call.
 *
 * Ordinary feature false is ignored. Native failure retains the entire reached
 * prefix, including flags, RNG, arrays, writes and callbacks. Falling false is
 * written ONLY at normal source2903; no finally/previous-value restoration.
 * A selected private big-oak is always disposed when its enclosing iteration
 * exits, without undoing actual constructor/profile or placement effects.
 * Existing real feature/default budgets and descendant frontiers stay real.
 *
 * The actual nullable w has an independent lazy temperature owner and limits.
 * Direct errors retain one selected typed child. Acquisition propagates the
 * bounded context below; status-only getters preserve their storage cause,
 * not an unbounded recursive feature/Throwable graph. No whole-game or full
 * recursive-stack qualification follows from this interface.
 * Source extreme feature loops may wrap/nonterminate; no success clamp/cap is
 * invented. The captured strict floating profile is checked only where reached.
 * Inputs/error and all owners remain disjoint, live and exclusive. */
mc_world_storage_status mc_world_provider_populate_game(mc_world_provider *provider,
                                                        int32_t chunk_x, int32_t chunk_z,
                                                        mc_world_normal_population_error *error);

/* Pure value projection: current controller and deepest explicitly carried site.
 * Successful errors return an unreached context. No owner lookup occurs. */
mc_world_population_context
mc_world_normal_population_context_get(const mc_world_normal_population_error *error);

#endif
