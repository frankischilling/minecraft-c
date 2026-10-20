#ifndef MC_WORLD_RUNTIME_POPULATION_PLANTS_H
#define MC_WORLD_RUNTIME_POPULATION_PLANTS_H

#include "mc/core/java/java.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/plants.h"

/* Actual retained feature fields, captured by their source constructors. */
typedef struct mc_population_flower_feature {
    int32_t a;
} mc_population_flower_feature;
typedef struct mc_population_tall_grass_feature {
    int32_t a, b;
} mc_population_tall_grass_feature;
typedef struct mc_population_dead_bush_feature {
    int32_t a;
} mc_population_dead_bush_feature;

typedef struct mc_population_plant_feature_error {
    mc_world_storage_status status;
    const char *operation;
    const char *feature_class, *feature_method;
    unsigned int bytecode;
    /* Only the exact local fd.d/dj.f air wrapper needs this extra frame.
     * Other entered World/predicate/mutation details have their typed causes. */
    unsigned int air_bytecode;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_material_error material;
    mc_world_plant_error plant;
    mc_world_mutation_error mutation;
} mc_population_plant_feature_error;

/* Actual constructors: base pg/jm/Object constructors have no retained fields
 * or effects here; store a at PC6, and tall-grass b at PC11. No Random or class
 * use occurs. Caller supplies fresh exclusive disjoint native field storage.
 * Native allocation/Java object identity and allocation failure are outside
 * these field constructors, rather than silently emulated. NULL is inert. */
mc_world_storage_status mc_population_flower_feature_init(mc_population_flower_feature *feature,
                                                          int32_t id);
mc_world_storage_status
mc_population_tall_grass_feature_init(mc_population_tall_grass_feature *feature, int32_t id,
                                      int32_t metadata);
mc_world_storage_status
mc_population_dead_bush_feature_init(mc_population_dead_bush_feature *feature, int32_t id);

/* Complete be/am, ap/ad, kt/gw, ir/fs, wx/op, fx/ea a(World,Random,III)Z.
 * Stateless ir/fs, wx/op and fx/ea have no retained constructor fields. Their
 * constructor consists solely of the same empty base constructor call.
 * The inherited pg/jm a(DDD)V is empty and is not a placement dependency.
 *
 * Borrow the supplied actual Random continuation; never copy/reseed/create it.
 * NULL Random fails at its first reached draw, after any earlier descent or
 * World effects. Feature fields are read at each reached getfield, including
 * after class active use or survival calls. Do not mutate/retire owner storage
 * except through permitted synchronous concrete operations; require exclusive
 * live dependencies and disjoint arguments/results/error storage.
 *
 * Hold the actual GAME World, its class owner and any bound Registry throughout.
 * RAW/constructing/failed prefixes are admitted. An absent Registry fails only
 * at a reached Registry access. No READY gate, coordinate/ID
 * preflight, future static capture, detached palette or callback seam exists.
 * Every static/array/field remains late. The wb/nw cast admits all six actual
 * descendants, including HE/EU and NI/IJ via the separate concrete extension.
 * Arbitrary other virtual Block kinds remain subject to the predicate module's
 * explicit supported receiver domain; no unknown virtual body is fabricated.
 *
 * Ordinary completion always writes true, even if no block changed. All source
 * writes are quiet. Each ordinary mutation bool is discarded; native failure
 * stops immediately, preserves out, the actual Random continuation and every
 * reached World/class/Chunk/hook/lighting effect. No retry/rollback/work clamp.
 */
mc_world_storage_status mc_population_flowers_place(const mc_population_flower_feature *feature,
                                                    mc_world_runtime *world, mc_java_random *random,
                                                    int32_t x, int32_t y, int32_t z, bool *out,
                                                    mc_population_plant_feature_error *error);
mc_world_storage_status
mc_population_tall_grass_place(const mc_population_tall_grass_feature *feature,
                               mc_world_runtime *world, mc_java_random *random, int32_t x,
                               int32_t y, int32_t z, bool *out,
                               mc_population_plant_feature_error *error);
mc_world_storage_status
mc_population_dead_bush_place(const mc_population_dead_bush_feature *feature,
                              mc_world_runtime *world, mc_java_random *random, int32_t x, int32_t y,
                              int32_t z, bool *out, mc_population_plant_feature_error *error);
mc_world_storage_status mc_population_reeds_place(mc_world_runtime *world, mc_java_random *random,
                                                  int32_t x, int32_t y, int32_t z, bool *out,
                                                  mc_population_plant_feature_error *error);
mc_world_storage_status mc_population_pumpkins_place(mc_world_runtime *world,
                                                     mc_java_random *random, int32_t x, int32_t y,
                                                     int32_t z, bool *out,
                                                     mc_population_plant_feature_error *error);
mc_world_storage_status mc_population_cacti_place(mc_world_runtime *world, mc_java_random *random,
                                                  int32_t x, int32_t y, int32_t z, bool *out,
                                                  mc_population_plant_feature_error *error);

#endif
