#ifndef MC_WORLD_RUNTIME_PLANTS_H
#define MC_WORLD_RUNTIME_PLANTS_H

#include "mc/world/runtime/queries.h"

typedef struct mc_world_plant_error {
    mc_world_storage_status status;
    const char *operation;
    /* Deepest reached concrete predicate, separate from its immediate source
     * predicate caller. NULL names mark native admission or no entered
     * receiver; PC0 can be a real source operation. This is finite attribution,
     * not a Java Throwable stack. */
    const char *declaring_class, *declaring_method;
    unsigned int bytecode;
    const char *caller_class, *caller_method;
    unsigned int caller_bytecode;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_query_error query;
    mc_world_material_error material;
    mc_world_solid_support_error support;
} mc_world_plant_error;

/* A tree failure reached by sapling growth. Its lower mutation is retained in
 * the enclosing mutation error; this nonrecursive site keeps the tree's own
 * current method, entry and constructor-profile cause separate. */
typedef struct mc_world_plant_tree_error {
    bool reached, native_boundary;
    const char *feature_class, *method;
    unsigned int bytecode, entry_bytecode;
    mc_java_random_profile_status profile_status;
} mc_world_plant_tree_error;

/* Concrete client g/server f(World,III)Z and a(World,III)Z virtual bodies for
 * flowers wb/nw, saplings he/eu, crops ni/ij, tall grass ru/kz, dead bush
 * jb/fz, mushrooms tl/md, reeds ri/ko, cactus or/je and pumpkin fc/di. Resolve actual receiver
 * ancestry, never a numeric ID or detached property palette. Pumpkin survival is the proved
 * inherited uu.g/na.f literal true body; other unlisted kinds reject natively instead of receiving
 * a guessed inherited implementation.
 *
 * receiver is an actual same-bound-Registry Block or NULL. A NULL receiver
 * fails at the entered invocation, without a fabricated concrete declaration.
 * Source array selection/checkcast belongs to the feature caller before this
 * interface; no substitute ID lookup or assumed flower cast occurs here.
 * Native foreign receiver/class and unsupported-kind admission is inert.
 *
 * Hold the actual GAME World and any immutable Registry binding throughout.
 * Admit raw/constructing/failed World prefixes, reject retired/composition.
 * No READY, block-ID, coordinate, air, finite, light or future field preflight
 * is added. Each body performs its original short circuits and late actual
 * static/field/material/World reads. No callback table or private RNG is
 * introduced; reached acquisition may retain its normal nested effects.
 *
 * Require live exclusive owners/dependencies and disjoint output/error/owner
 * storage. Native failure preserves out and all earlier effects. Error retains
 * the original predicate invocation and complete fresh nested causes; never
 * manufacture Registry attribution from an older World diagnostic. Successful
 * source completion writes the ordinary bool and clears this call's frame.
 * Earlier World diagnostics remain subject to the actual reached lower calls.
 */
mc_world_storage_status mc_world_runtime_plant_survives(mc_world_runtime *world,
                                                        const mc_block *receiver, int32_t x,
                                                        int32_t y, int32_t z, bool *out,
                                                        mc_world_plant_error *error);
mc_world_storage_status mc_world_runtime_plant_can_place(mc_world_runtime *world,
                                                         const mc_block *receiver, int32_t x,
                                                         int32_t y, int32_t z, bool *out,
                                                         mc_world_plant_error *error);

#endif
