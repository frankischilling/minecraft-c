#include "mc/world/runtime/population/plants.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../construction_internal.h"
#include "../mutation/mutation_internal.h"
#include "../runtime_internal.h"
#include "mc/block/registry/runtime.h"

typedef struct feature_frame {
    mc_world_runtime *world;
    mc_java_random *random;
    bool entered;
    mc_population_plant_feature_error error;
} feature_frame;

static mc_registry_error empty_registry(void) {
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static mc_population_plant_feature_error empty_error(void) {
    const mc_registry_error registry = empty_registry();
    return (mc_population_plant_feature_error){.registry = registry,
                                               .material = {.registry = registry},
                                               .plant = {.registry = registry,
                                                         .query = {.registry = registry},
                                                         .material = {.registry = registry},
                                                         .support = {.registry = registry}},
                                               .mutation = mc_world_mutation_empty_error()};
}

static bool fail(feature_frame *frame, mc_world_storage_status status, const char *operation) {
    frame->error.status = status;
    frame->error.operation = operation;
    frame->error.cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool storage_fail(feature_frame *frame, const mc_world_storage_error *cause) {
    frame->error.status = cause->status;
    frame->error.operation = cause->operation;
    frame->error.cause = *cause;
    return false;
}

static bool registry_fail(feature_frame *frame, const mc_registry_error *cause,
                          const char *operation) {
    frame->error.registry = *cause;
    frame->error.status = mc_world_runtime_registry_result(frame->world, cause->status, cause,
                                                           operation, &frame->error.cause);
    frame->error.operation = frame->error.cause.operation;
    return false;
}

static bool null_reference(feature_frame *frame, const char *operation) {
    mc_registry_error cause = empty_registry();
    cause.status = MC_REGISTRY_NULL_REFERENCE;
    cause.message = operation;
    return registry_fail(frame, &cause, operation);
}

static void stage(feature_frame *frame, unsigned int pc) {
    frame->error.bytecode = pc;
    frame->error.air_bytecode = 0;
}

static bool begin(feature_frame *frame, mc_world_runtime *world, mc_java_random *random,
                  const void *feature, bool requires_feature, bool *out, const char *client_class,
                  const char *server_class) {
    *frame = (feature_frame){.world = world, .random = random, .error = empty_error()};
    if (world == NULL || out == NULL || (requires_feature && feature == NULL))
        return fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Population plant arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Population plant World phase");
    if (mc_world_runtime_call_enter(world, &frame->error.cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &frame->error.cause);
    frame->entered = true;
    /* Every live GAME owner was allocated with this immutable class binding.
     * This reads native side identity only; it does not activate any class. */
    frame->error.feature_class =
        world->classes->side == MC_WORLD_INFO_CLIENT ? client_class : server_class;
    frame->error.feature_method = "a";
    return true;
}

static mc_world_storage_status finish(feature_frame *frame, bool completed, bool *out,
                                      mc_population_plant_feature_error *error) {
    if (completed) {
        *out = true;
        frame->error = empty_error();
    } else if (frame->entered) {
        frame->world->last_error = frame->error.cause;
    }
    if (frame->entered)
        mc_world_runtime_call_leave(frame->world);
    if (error != NULL)
        *error = frame->error;
    return frame->error.status;
}

static bool draw(feature_frame *frame, unsigned int pc, int32_t bound, int32_t *out) {
    stage(frame, pc);
    if (frame->random == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Population plant Random receiver");
    /* All bounds come from positive source literals or 1+nextInt(3).
     * In particular, bound1 still performs Random.next(31). */
    if (!mc_java_random_next_bounded(frame->random, bound, out))
        return fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Population plant Random bound");
    return true;
}

static bool axis(feature_frame *frame, int32_t origin, int32_t bound, unsigned int positive_pc,
                 unsigned int negative_pc, int32_t *out) {
    int32_t positive, negative;
    if (!draw(frame, positive_pc, bound, &positive))
        return false;
    const int32_t sum = mc_java_i32_add(origin, positive);
    if (!draw(frame, negative_pc, bound, &negative))
        return false;
    *out = mc_java_i32_sub(sum, negative);
    return true;
}

static bool block_get(feature_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                      int32_t *out) {
    stage(frame, pc);
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_block_get(frame->world, x, y, z, out, &cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &cause);
    return true;
}

/* Exact fd.d/dj.f(III)Z: actual World.a ID invocation at4, compare with zero.
 * No separate public air API exists; this bounded concrete helper is its body. */
static bool air(feature_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z, bool *out) {
    stage(frame, pc);
    frame->error.air_bytecode = 4;
    int32_t id;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_block_get(frame->world, x, y, z, &id, &cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &cause);
    *out = id == 0;
    return true;
}

static bool static_block(feature_frame *frame, const char *name, unsigned int pc,
                         const mc_block **out) {
    stage(frame, pc);
    mc_registry_error cause = empty_registry();
    if (mc_registry_block_static_get(mc_world_runtime_registry(frame->world), name, out, &cause) !=
        MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Population plant Block static active use");
    return true;
}

static bool static_id(feature_frame *frame, const char *name, unsigned int load_pc,
                      unsigned int field_pc, int32_t *out) {
    const mc_block *block = NULL;
    if (!static_block(frame, name, load_pc, &block))
        return false;
    stage(frame, field_pc);
    if (block == NULL)
        return null_reference(frame, "Population plant Block.bn receiver");
    *out = block->fields.bn;
    return true;
}

static bool flower_table(feature_frame *frame, unsigned int pc, mc_registry_block_m_view *out) {
    stage(frame, pc);
    mc_registry_error cause = empty_registry();
    if (mc_registry_block_m_get(mc_world_runtime_registry(frame->world), out, &cause) !=
        MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Population plant Block.m active use");
    return true;
}

static bool flower_element(feature_frame *frame, const mc_registry_block_m_view *array,
                           int32_t index, unsigned int element_pc, unsigned int cast_pc,
                           const mc_block **out) {
    stage(frame, element_pc);
    mc_registry_error cause = empty_registry();
    const mc_block *block = NULL;
    if (mc_registry_block_m_at(array, index, &block, &cause) != MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Population plant Block.m element");
    stage(frame, cast_pc);
    /* NULL passes Java checkcast, then fails at the later invocation. These
     * are all actual retained wb/nw descendants, not ID-based classification. */
    if (block != NULL) {
        switch (block->kind) {
            case MC_BLOCK_WB_NW:
            case MC_BLOCK_HE_EU:
            case MC_BLOCK_NI_IJ:
            case MC_BLOCK_RU_KZ:
            case MC_BLOCK_JB_FZ:
            case MC_BLOCK_TL_MD:
                break;
            default:
                /* Existing status vocabulary has no Java ClassCastException.
                 * The exact cast stage and operation identify this failure. */
                return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "Population plant wb/nw checkcast incompatible receiver");
        }
    }
    *out = block;
    return true;
}

static bool predicate(feature_frame *frame, unsigned int pc, const mc_block *receiver, int32_t x,
                      int32_t y, int32_t z, bool placement, bool *out) {
    stage(frame, pc);
    mc_world_storage_status status;
    if (placement)
        status = mc_world_runtime_plant_can_place(frame->world, receiver, x, y, z, out,
                                                  &frame->error.plant);
    else
        status = mc_world_runtime_plant_survives(frame->world, receiver, x, y, z, out,
                                                 &frame->error.plant);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    frame->error.registry = frame->error.plant.registry;
    return storage_fail(frame, &frame->error.plant.cause);
}

static bool write_block(feature_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                        int32_t id) {
    stage(frame, pc);
    bool changed;
    if (mc_world_runtime_block_set_quiet(frame->world, x, y, z, id, &changed,
                                         &frame->error.mutation) == MC_WORLD_STORAGE_OK)
        return true; /* Source pop discards ordinary false as well as true. */
    frame->error.registry = frame->error.mutation.registry;
    return storage_fail(frame, &frame->error.mutation.cause);
}

static bool write_metadata(feature_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                           int32_t id, int32_t metadata) {
    stage(frame, pc);
    bool changed;
    if (mc_world_runtime_block_metadata_set_quiet(frame->world, x, y, z, id, metadata, &changed,
                                                  &frame->error.mutation) == MC_WORLD_STORAGE_OK)
        return true;
    frame->error.registry = frame->error.mutation.registry;
    return storage_fail(frame, &frame->error.mutation.cause);
}

static bool water(feature_frame *frame, unsigned int query_pc, unsigned int static_pc, int32_t x,
                  int32_t y, int32_t z, bool *out) {
    stage(frame, query_pc);
    mc_material *actual = NULL;
    if (mc_world_runtime_material_get_detailed(frame->world, x, y, z, &actual,
                                               &frame->error.material) != MC_WORLD_STORAGE_OK) {
        frame->error.registry = frame->error.material.registry;
        return storage_fail(frame, &frame->error.material.cause);
    }
    stage(frame, static_pc);
    mc_material *expected = NULL;
    mc_registry_error cause = empty_registry();
    if (mc_registry_material_get(mc_world_runtime_registry(frame->world), MC_MATERIAL_g, &expected,
                                 &cause) != MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Population plant Material.g identity");
    *out = actual == expected; /* Actual nullable references; no material-name substitute. */
    return true;
}

/* Identical ap/ad and kt/gw prefix. ID read occurs even for y<=0. Nonair
 * activates and reads actual leaves L before testing the y bound. */
static bool descend(feature_frame *frame, int32_t x, int32_t *y, int32_t z) {
    for (;;) {
        int32_t id;
        if (!block_get(frame, 9, x, *y, z, &id))
            return false;
        if (id != 0) {
            int32_t leaves;
            if (!static_id(frame, "L", 20, 23, &leaves))
                return false;
            if (id != leaves)
                return true;
        }
        if (*y <= 0)
            return true;
        *y = mc_java_i32_sub(*y, 1);
    }
}

static bool flowers(feature_frame *frame, const mc_population_flower_feature *feature, int32_t x,
                    int32_t y, int32_t z) {
    for (int32_t attempt = 0; attempt < 64; ++attempt) {
        int32_t px, py, pz;
        if (!axis(frame, x, 8, 14, 21, &px) || !axis(frame, y, 4, 31, 37, &py) ||
            !axis(frame, z, 8, 48, 55, &pz))
            return false;
        bool empty;
        if (!air(frame, 68, px, py, pz, &empty))
            return false;
        if (!empty)
            continue;
        mc_registry_block_m_view array;
        if (!flower_table(frame, 74, &array))
            return false;
        stage(frame, 78);
        const int32_t index = feature->a;
        const mc_block *receiver = NULL;
        if (!flower_element(frame, &array, index, 81, 82, &receiver))
            return false;
        bool accepted;
        if (!predicate(frame, 92, receiver, px, py, pz, false, &accepted))
            return false;
        if (accepted) {
            stage(frame, 106);
            const int32_t id = feature->a;
            if (!write_block(frame, 109, px, py, pz, id))
                return false;
        }
    }
    return true;
}

static bool tall_grass(feature_frame *frame, const mc_population_tall_grass_feature *feature,
                       int32_t x, int32_t y, int32_t z) {
    if (!descend(frame, x, &y, z))
        return false;
    for (int32_t attempt = 0; attempt < 128; ++attempt) {
        int32_t px, py, pz;
        if (!axis(frame, x, 8, 55, 62, &px) || !axis(frame, y, 4, 72, 78, &py) ||
            !axis(frame, z, 8, 89, 96, &pz))
            return false;
        bool empty;
        if (!air(frame, 109, px, py, pz, &empty))
            return false;
        if (!empty)
            continue;
        mc_registry_block_m_view array;
        if (!flower_table(frame, 115, &array))
            return false;
        stage(frame, 119);
        const int32_t index = feature->a;
        const mc_block *receiver = NULL;
        if (!flower_element(frame, &array, index, 122, 123, &receiver))
            return false;
        bool accepted;
        if (!predicate(frame, 133, receiver, px, py, pz, false, &accepted))
            return false;
        if (accepted) {
            stage(frame, 147);
            const int32_t id = feature->a;
            stage(frame, 151);
            const int32_t metadata = feature->b;
            if (!write_metadata(frame, 154, px, py, pz, id, metadata))
                return false;
        }
    }
    return true;
}

static bool dead_bush(feature_frame *frame, const mc_population_dead_bush_feature *feature,
                      int32_t x, int32_t y, int32_t z) {
    if (!descend(frame, x, &y, z))
        return false;
    for (int32_t attempt = 0; attempt < 4; ++attempt) {
        int32_t px, py, pz;
        if (!axis(frame, x, 8, 53, 60, &px) || !axis(frame, y, 4, 70, 76, &py) ||
            !axis(frame, z, 8, 87, 94, &pz))
            return false;
        bool empty;
        if (!air(frame, 107, px, py, pz, &empty))
            return false;
        if (!empty)
            continue;
        mc_registry_block_m_view array;
        if (!flower_table(frame, 113, &array))
            return false;
        stage(frame, 117);
        const int32_t index = feature->a;
        const mc_block *receiver = NULL;
        if (!flower_element(frame, &array, index, 120, 121, &receiver))
            return false;
        bool accepted;
        if (!predicate(frame, 131, receiver, px, py, pz, false, &accepted))
            return false;
        if (accepted) {
            stage(frame, 145);
            const int32_t id = feature->a;
            if (!write_block(frame, 148, px, py, pz, id))
                return false;
        }
    }
    return true;
}

static bool reeds(feature_frame *frame, int32_t x, int32_t y, int32_t z) {
    for (int32_t attempt = 0; attempt < 20; ++attempt) {
        int32_t px, pz;
        if (!axis(frame, x, 4, 13, 19, &px) || !axis(frame, z, 4, 33, 39, &pz))
            return false;
        bool accepted;
        if (!air(frame, 52, px, y, pz, &accepted))
            return false;
        if (!accepted)
            continue;
        const int32_t below = mc_java_i32_sub(y, 1);
        if (!water(frame, 69, 72, mc_java_i32_sub(px, 1), below, pz, &accepted))
            return false;
        if (!accepted && !water(frame, 89, 92, mc_java_i32_add(px, 1), below, pz, &accepted))
            return false;
        if (!accepted && !water(frame, 109, 112, px, below, mc_java_i32_sub(pz, 1), &accepted))
            return false;
        if (!accepted && !water(frame, 129, 132, px, below, mc_java_i32_add(pz, 1), &accepted))
            return false;
        if (!accepted)
            continue;
        int32_t inner, outer;
        if (!draw(frame, 142, 3, &inner) || !draw(frame, 147, mc_java_i32_add(inner, 1), &outer))
            return false;
        const int32_t height = mc_java_i32_add(outer, 2);
        for (int32_t layer = 0; layer < height; ++layer) {
            const mc_block *receiver = NULL;
            if (!static_block(frame, "aY", 163, &receiver))
                return false;
            const int32_t py = mc_java_i32_add(y, layer);
            if (!predicate(frame, 176, receiver, px, py, pz, false, &accepted))
                return false;
            if (accepted) {
                int32_t id;
                if (!static_id(frame, "aY", 192, 195, &id) ||
                    !write_block(frame, 198, px, py, pz, id))
                    return false;
            } /* Source false continues the next layer, without a break. */
        }
    }
    return true;
}

static bool pumpkins(feature_frame *frame, int32_t x, int32_t y, int32_t z) {
    for (int32_t attempt = 0; attempt < 64; ++attempt) {
        int32_t px, py, pz;
        if (!axis(frame, x, 8, 14, 21, &px) || !axis(frame, y, 4, 31, 37, &py) ||
            !axis(frame, z, 8, 48, 55, &pz))
            return false;
        bool accepted;
        if (!air(frame, 68, px, py, pz, &accepted))
            return false;
        if (!accepted)
            continue;
        int32_t below, grass;
        if (!block_get(frame, 83, px, mc_java_i32_sub(py, 1), pz, &below) ||
            !static_id(frame, "v", 86, 89, &grass))
            return false;
        if (below != grass)
            continue;
        const mc_block *receiver = NULL;
        if (!static_block(frame, "bb", 95, &receiver) ||
            !predicate(frame, 105, receiver, px, py, pz, true, &accepted))
            return false;
        if (accepted) {
            int32_t id, metadata;
            /* Write ID is selected before the reached orientation draw. */
            if (!static_id(frame, "bb", 118, 121, &id) || !draw(frame, 126, 4, &metadata) ||
                !write_metadata(frame, 129, px, py, pz, id, metadata))
                return false;
        }
    }
    return true;
}

static bool cacti(feature_frame *frame, int32_t x, int32_t y, int32_t z) {
    for (int32_t attempt = 0; attempt < 10; ++attempt) {
        int32_t px, py, pz;
        if (!axis(frame, x, 8, 14, 21, &px) || !axis(frame, y, 4, 31, 37, &py) ||
            !axis(frame, z, 8, 48, 55, &pz))
            return false;
        bool accepted;
        if (!air(frame, 68, px, py, pz, &accepted))
            return false;
        if (!accepted)
            continue;
        int32_t inner, outer;
        if (!draw(frame, 78, 3, &inner) || !draw(frame, 83, mc_java_i32_add(inner, 1), &outer))
            return false;
        const int32_t height = mc_java_i32_add(outer, 1);
        for (int32_t layer = 0; layer < height; ++layer) {
            const mc_block *receiver = NULL;
            if (!static_block(frame, "aW", 99, &receiver))
                return false;
            const int32_t ly = mc_java_i32_add(py, layer);
            if (!predicate(frame, 112, receiver, px, ly, pz, false, &accepted))
                return false;
            if (accepted) {
                int32_t id;
                if (!static_id(frame, "aW", 128, 131, &id) ||
                    !write_block(frame, 134, px, ly, pz, id))
                    return false;
            }
        }
    }
    return true;
}

mc_world_storage_status mc_population_flower_feature_init(mc_population_flower_feature *feature,
                                                          int32_t id) {
    if (feature == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    feature->a = id;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status
mc_population_tall_grass_feature_init(mc_population_tall_grass_feature *feature, int32_t id,
                                      int32_t metadata) {
    if (feature == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    feature->a = id;
    feature->b = metadata;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status
mc_population_dead_bush_feature_init(mc_population_dead_bush_feature *feature, int32_t id) {
    if (feature == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    feature->a = id;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_population_flowers_place(const mc_population_flower_feature *feature,
                                                    mc_world_runtime *world, mc_java_random *random,
                                                    int32_t x, int32_t y, int32_t z, bool *out,
                                                    mc_population_plant_feature_error *error) {
    feature_frame frame;
    const bool completed = begin(&frame, world, random, feature, true, out, "be", "am") &&
                           flowers(&frame, feature, x, y, z);
    return finish(&frame, completed, out, error);
}

mc_world_storage_status
mc_population_tall_grass_place(const mc_population_tall_grass_feature *feature,
                               mc_world_runtime *world, mc_java_random *random, int32_t x,
                               int32_t y, int32_t z, bool *out,
                               mc_population_plant_feature_error *error) {
    feature_frame frame;
    const bool completed = begin(&frame, world, random, feature, true, out, "ap", "ad") &&
                           tall_grass(&frame, feature, x, y, z);
    return finish(&frame, completed, out, error);
}

mc_world_storage_status
mc_population_dead_bush_place(const mc_population_dead_bush_feature *feature,
                              mc_world_runtime *world, mc_java_random *random, int32_t x, int32_t y,
                              int32_t z, bool *out, mc_population_plant_feature_error *error) {
    feature_frame frame;
    const bool completed = begin(&frame, world, random, feature, true, out, "kt", "gw") &&
                           dead_bush(&frame, feature, x, y, z);
    return finish(&frame, completed, out, error);
}

mc_world_storage_status mc_population_reeds_place(mc_world_runtime *world, mc_java_random *random,
                                                  int32_t x, int32_t y, int32_t z, bool *out,
                                                  mc_population_plant_feature_error *error) {
    feature_frame frame;
    const bool completed =
        begin(&frame, world, random, NULL, false, out, "ir", "fs") && reeds(&frame, x, y, z);
    return finish(&frame, completed, out, error);
}

mc_world_storage_status mc_population_pumpkins_place(mc_world_runtime *world,
                                                     mc_java_random *random, int32_t x, int32_t y,
                                                     int32_t z, bool *out,
                                                     mc_population_plant_feature_error *error) {
    feature_frame frame;
    const bool completed =
        begin(&frame, world, random, NULL, false, out, "wx", "op") && pumpkins(&frame, x, y, z);
    return finish(&frame, completed, out, error);
}

mc_world_storage_status mc_population_cacti_place(mc_world_runtime *world, mc_java_random *random,
                                                  int32_t x, int32_t y, int32_t z, bool *out,
                                                  mc_population_plant_feature_error *error) {
    feature_frame frame;
    const bool completed =
        begin(&frame, world, random, NULL, false, out, "fx", "ea") && cacti(&frame, x, y, z);
    return finish(&frame, completed, out, error);
}
