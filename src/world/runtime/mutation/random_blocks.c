#include "mutation_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../presence_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"

typedef struct random_block_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    const char *class_name, *method;
} random_block_frame;

static void rb_at(const random_block_frame *f, unsigned int pc) {
    f->error->hook_class = f->class_name;
    f->error->hook_method = f->method;
    f->error->hook_bytecode = pc;
}

static bool rb_done(const random_block_frame *f) {
    f->error->hook_class = NULL;
    f->error->hook_method = NULL;
    f->error->hook_bytecode = 0;
    return true;
}

static bool rb_storage(const random_block_frame *f, unsigned int pc, mc_world_storage_error cause) {
    rb_at(f, pc);
    f->error->status = cause.status;
    f->error->operation = cause.operation;
    f->error->cause = cause;
    if (f->world != NULL)
        f->world->last_error = cause;
    return false;
}

static bool rb_registry(const random_block_frame *f, unsigned int pc,
                        const mc_registry_error *cause) {
    rb_at(f, pc);
    if (f->world != NULL)
        return mc_world_mutation_registry_fail(f->world, f->error, cause, cause->message);
    f->error->registry = *cause;
    return mc_world_mutation_fail(NULL, f->error, mc_world_runtime_registry_status(cause->status),
                                  cause->message);
}

static bool rb_null(const random_block_frame *f, unsigned int pc, const char *operation) {
    mc_registry_error cause = mc_world_mutation_empty_registry();
    cause.status = MC_REGISTRY_NULL_REFERENCE;
    cause.message = operation;
    return rb_registry(f, pc, &cause);
}

static bool rb_start(random_block_frame *f, mc_world_runtime *world, const mc_block *block,
                     mc_world_mutation_error *error, const char *client, const char *server) {
    const bool is_client = block->classes->options.side == MC_REGISTRY_CLIENT;
    *f = (random_block_frame){world, block, error, is_client ? client : server,
                              is_client ? "a(Lfd;IIILjava/util/Random;)V"
                                        : "a(Ldj;IIILjava/util/Random;)V"};
    rb_at(f, 0);
    if (!is_client)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Actual server random Block tick remains unqualified");
    return true;
}

static bool rb_nested(const random_block_frame *f, unsigned int pc,
                      const mc_world_mutation_error *lower) {
    const mc_world_mutation_error captured = *lower;
    *f->error = captured;
    if (captured.cause_hook_class == NULL) {
        f->error->cause_hook_class = captured.hook_class;
        f->error->cause_hook_method = captured.hook_method;
        f->error->cause_hook_bytecode = captured.hook_bytecode;
    }
    return rb_storage(f, pc, captured.cause);
}

static void rb_acquisition(random_block_frame *f,
                           const mc_world_runtime_acquisition_error *detail) {
    f->error->query.acquisition = detail->status;
    f->error->query.population = detail->population;
    f->error->query.registry = detail->registry;
    f->error->query.source_chunk = detail->source_chunk;
    f->error->registry = detail->registry;
    f->error->tile_entity = detail->tile_entity;
    f->error->batch = detail->batch;
    f->error->load_entity = detail->load_entity;
    f->error->activation_bytecode = detail->activation_bytecode;
    f->error->activation_world_bytecode = detail->activation_world_bytecode;
    f->error->load_bytecode = detail->load_bytecode;
    f->error->load_builder_bytecode = detail->load_builder_bytecode;
    if (detail->population.reached)
        f->error->population = detail->population;
}

static bool rb_get(random_block_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                   bool metadata, int32_t *out) {
    rb_at(f, pc);
    if (f->world == NULL)
        return rb_null(f, pc, "Random Block null World getter receiver");
    mc_world_storage_error cause = {0};
    mc_world_runtime_acquisition_error detail;
    const mc_world_storage_status status =
        metadata ? mc_world_runtime_metadata_get_detailed(f->world, x, y, z, out, &cause, &detail)
                 : mc_world_runtime_block_get_detailed(f->world, x, y, z, out, &cause, &detail);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    rb_acquisition(f, &detail);
    f->error->query.status = status;
    f->error->query.operation = cause.operation;
    f->error->query.cause = cause;
    return rb_storage(f, pc, cause);
}

static bool rb_light(random_block_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                     int32_t *out) {
    rb_at(f, pc);
    if (f->world == NULL)
        return rb_null(f, pc, "Random Block null World light receiver");
    const mc_world_storage_status status =
        mc_world_runtime_block_light(f->world, x, y, z, out, &f->error->query);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    f->error->registry = f->error->query.registry;
    if (f->error->query.population.reached)
        f->error->population = f->error->query.population;
    return rb_storage(f, pc, f->error->query.cause);
}

static bool rb_static_id(const random_block_frame *f, const char *name, unsigned int static_pc,
                         unsigned int field_pc, int32_t *out) {
    rb_at(f, static_pc);
    const mc_block *value = NULL;
    mc_registry_error cause = mc_world_mutation_empty_registry();
    if (mc_registry_block_static_get(f->block->classes, name, &value, &cause) != MC_REGISTRY_OK)
        return rb_registry(f, static_pc, &cause);
    rb_at(f, field_pc);
    if (value == NULL)
        return rb_null(f, field_pc, "Random Block null static Block receiver");
    *out = value->fields.bn;
    return true;
}

static bool rb_draw(const random_block_frame *f, unsigned int pc, mc_java_random *random,
                    int32_t bound, int32_t *out) {
    rb_at(f, pc);
    if (random == NULL)
        return rb_null(f, pc, "Random Block null supplied Random receiver");
    if (!mc_java_random_next_bounded(random, bound, out))
        return mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Random Block bounded Random argument");
    return true;
}

static bool rb_set(const random_block_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                   int32_t id) {
    rb_at(f, pc);
    if (f->world == NULL)
        return rb_null(f, pc, "Random Block null notifying setter receiver");
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    bool ignored;
    if (mc_world_runtime_block_set(f->world, x, y, z, id, &ignored, &lower) != MC_WORLD_STORAGE_OK)
        return rb_nested(f, pc, &lower);
    return true;
}

bool mc_world_mutation_grass_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_java_random *random,
                                  mc_world_mutation_error *error) {
    random_block_frame f;
    if (!rb_start(&f, world, block, error, "wp", "oj"))
        return false;
    rb_at(&f, 1);
    if (world == NULL)
        return rb_null(&f, 1, "Grass tick null World remote field");
    if (world->fields.B)
        return rb_done(&f);
    int32_t light;
    const int32_t above_y = mc_java_i32_add(y, 1);
    if (!rb_light(&f, 15, x, above_y, z, &light))
        return false;
    if (light < 4) {
        rb_at(&f, 22);
        mc_registry_block_q_view q;
        mc_registry_error cause = mc_world_mutation_empty_registry();
        if (mc_registry_block_q_get(block->classes, &q, &cause) != MC_REGISTRY_OK)
            return rb_registry(&f, 22, &cause);
        int32_t above, opacity;
        if (!rb_get(&f, 32, x, above_y, z, false, &above))
            return false;
        rb_at(&f, 35);
        if (mc_registry_block_q_at(&q, above, &opacity, &cause) != MC_REGISTRY_OK)
            return rb_registry(&f, 35, &cause);
        if (opacity > 2) {
            int32_t draw;
            if (!rb_draw(&f, 43, random, 4, &draw))
                return false;
            if (draw != 0)
                return rb_done(&f);
            int32_t dirt;
            if (!rb_static_id(&f, "w", 55, 58, &dirt) || !rb_set(&f, 61, x, y, z, dirt))
                return false;
            return rb_done(&f);
        }
    }
    if (!rb_light(&f, 75, x, above_y, z, &light))
        return false;
    if (light < 9)
        return rb_done(&f);
    int32_t dx, dy, dz;
    if (!rb_draw(&f, 87, random, 3, &dx) || !rb_draw(&f, 99, random, 5, &dy) ||
        !rb_draw(&f, 112, random, 3, &dz))
        return false;
    const int32_t tx = mc_java_i32_sub(mc_java_i32_add(x, dx), 1);
    const int32_t ty = mc_java_i32_sub(mc_java_i32_add(y, dy), 3);
    const int32_t tz = mc_java_i32_sub(mc_java_i32_add(z, dz), 1);
    const int32_t target_above_y = mc_java_i32_add(ty, 1);
    int32_t above, target, dirt;
    if (!rb_get(&f, 129, tx, target_above_y, tz, false, &above) ||
        !rb_get(&f, 141, tx, ty, tz, false, &target) || !rb_static_id(&f, "w", 144, 147, &dirt))
        return false;
    if (target != dirt)
        return rb_done(&f);
    if (!rb_light(&f, 162, tx, target_above_y, tz, &light))
        return false;
    if (light < 4)
        return rb_done(&f);
    rb_at(&f, 169);
    mc_registry_block_q_view q;
    mc_registry_error cause = mc_world_mutation_empty_registry();
    if (mc_registry_block_q_get(block->classes, &q, &cause) != MC_REGISTRY_OK)
        return rb_registry(&f, 169, &cause);
    int32_t opacity;
    rb_at(&f, 174);
    if (mc_registry_block_q_at(&q, above, &opacity, &cause) != MC_REGISTRY_OK)
        return rb_registry(&f, 174, &cause);
    if (opacity <= 2) {
        int32_t grass;
        if (!rb_static_id(&f, "v", 186, 189, &grass) || !rb_set(&f, 192, tx, ty, tz, grass))
            return false;
    }
    return rb_done(&f);
}

static int32_t leaf_index(int32_t x, int32_t y, int32_t z) {
    return (x + 16) * 1024 + (y + 16) * 32 + z + 16;
}

static bool leaf_get(const random_block_frame *f, unsigned int pc, int32_t index, int32_t *out) {
    rb_at(f, pc);
    const int32_t *array = f->block->subtype.bk.a;
    if (array == NULL)
        return rb_null(f, pc, "Leaves tick null retained scratch array");
    if (index < 0 || index >= 32768) {
        mc_registry_error cause = mc_world_mutation_empty_registry();
        cause.status = MC_REGISTRY_INDEX_OUT_OF_RANGE;
        cause.message = "Leaves tick retained scratch index";
        return rb_registry(f, pc, &cause);
    }
    *out = array[index];
    return true;
}

static bool leaf_set(const random_block_frame *f, unsigned int pc, int32_t index, int32_t value) {
    rb_at(f, pc);
    int32_t *array = ((mc_block *)f->block)->subtype.bk.a;
    if (array == NULL)
        return rb_null(f, pc, "Leaves tick null retained scratch array");
    if (index < 0 || index >= 32768) {
        mc_registry_error cause = mc_world_mutation_empty_registry();
        cause.status = MC_REGISTRY_INDEX_OUT_OF_RANGE;
        cause.message = "Leaves tick retained scratch index";
        return rb_registry(f, pc, &cause);
    }
    array[index] = value;
    return true;
}

static bool leaf_decay(random_block_frame *outer, int32_t x, int32_t y, int32_t z) {
    random_block_frame f = *outer;
    f.method = "h(Lfd;III)V";
    int32_t metadata;
    if (!rb_get(&f, 11, x, y, z, true, &metadata))
        return rb_nested(outer, 850, f.error);
    rb_at(&f, 14);
    if (!mc_world_mutation_drop(f.world, f.block, x, y, z, metadata, f.error)) {
        (void)rb_nested(&f, 14, f.error);
        return rb_nested(outer, 850, f.error);
    }
    if (!rb_set(&f, 23, x, y, z, 0))
        return rb_nested(outer, 850, f.error);
    return true;
}

bool mc_world_mutation_leaves_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                   int32_t y, int32_t z, mc_java_random *random,
                                   mc_world_mutation_error *error) {
    (void)random;
    random_block_frame f;
    if (!rb_start(&f, world, block, error, "bk", "ar"))
        return false;
    rb_at(&f, 1);
    if (world == NULL)
        return rb_null(&f, 1, "Leaves tick null World remote field");
    if (world->fields.B)
        return rb_done(&f);
    int32_t metadata;
    if (!rb_get(&f, 13, x, y, z, true, &metadata))
        return false;
    if ((metadata & 8) == 0)
        return rb_done(&f);
    if (block->subtype.bk.a == NULL) {
        rb_at(&f, 68);
        int32_t *array = mc_registry_allocate(block->classes, 32768u * sizeof(*array));
        if (array == NULL) {
            mc_registry_error cause = mc_world_mutation_empty_registry();
            cause.status = block->classes->last_error.status;
            cause.message = block->classes->last_error.message;
            block->classes->last_error = cause;
            return rb_registry(&f, 68, &cause);
        }
        ((mc_block *)block)->subtype.bk.a = array; /* actual publication70 */
    }
    rb_at(&f, 100);
    bool present;
    mc_world_presence_error presence = {0};
    const mc_world_storage_status status = mc_world_runtime_region_present_held(
        world, mc_java_i32_sub(x, 5), mc_java_i32_sub(y, 5), mc_java_i32_sub(z, 5),
        mc_java_i32_add(x, 5), mc_java_i32_add(y, 5), mc_java_i32_add(z, 5), &present, &presence);
    error->presence_bytecode = presence.bytecode;
    error->provider_bytecode = presence.provider_bytecode;
    if (status != MC_WORLD_STORAGE_OK)
        return rb_storage(&f, 100, presence.cause);
    if (present) {
        for (int32_t dx = -4; dx <= 4; ++dx)
            for (int32_t dy = -4; dy <= 4; ++dy)
                for (int32_t dz = -4; dz <= 4; ++dz) {
                    int32_t id, log, leaves;
                    if (!rb_get(&f, 156, mc_java_i32_add(x, dx), mc_java_i32_add(y, dy),
                                mc_java_i32_add(z, dz), false, &id) ||
                        !rb_static_id(&f, "K", 163, 166, &log))
                        return false;
                    if (id == log) {
                        if (!leaf_set(&f, 200, leaf_index(dx, dy, dz), 0))
                            return false;
                    } else {
                        if (!rb_static_id(&f, "L", 206, 209, &leaves))
                            return false;
                        if (!leaf_set(&f, id == leaves ? 244u : 276u, leaf_index(dx, dy, dz),
                                      id == leaves ? -2 : -1))
                            return false;
                    }
                }
        static const int32_t directions[6][3] = {{-1, 0, 0}, {1, 0, 0},  {0, -1, 0},
                                                 {0, 1, 0},  {0, 0, -1}, {0, 0, 1}};
        static const unsigned int reads[] = {404, 471, 538, 605, 672, 739};
        static const unsigned int stores[] = {441, 508, 575, 642, 709, 776};
        for (int32_t level = 1; level <= 4; ++level)
            for (int32_t dx = -4; dx <= 4; ++dx)
                for (int32_t dy = -4; dy <= 4; ++dy)
                    for (int32_t dz = -4; dz <= 4; ++dz) {
                        int32_t center;
                        if (!leaf_get(&f, 367, leaf_index(dx, dy, dz), &center))
                            return false;
                        if (center != level - 1)
                            continue;
                        for (size_t direction = 0; direction < 6; ++direction) {
                            const int32_t index = leaf_index(dx + directions[direction][0],
                                                             dy + directions[direction][1],
                                                             dz + directions[direction][2]);
                            int32_t neighbor;
                            if (!leaf_get(&f, reads[direction], index, &neighbor))
                                return false;
                            if (neighbor == -2 && !leaf_set(&f, stores[direction], index, level))
                                return false;
                        }
                    }
    }
    int32_t center;
    if (!leaf_get(&f, 819, leaf_index(0, 0, 0), &center))
        return false;
    if (center >= 0) {
        rb_at(&f, 837);
        bool ignored;
        mc_world_storage_error cause = {0};
        if (mc_world_runtime_metadata_set_quiet(world, x, y, z, metadata & -9, &ignored, &cause) !=
            MC_WORLD_STORAGE_OK)
            return rb_storage(&f, 837, cause);
    } else if (!leaf_decay(&f, x, y, z))
        return false;
    return rb_done(&f);
}

static bool melt_light(random_block_frame *f, int32_t x, int32_t y, int32_t z, int32_t *out) {
    rb_at(f, 1);
    if (f->world == NULL) {
        mc_registry_error cause = mc_world_mutation_empty_registry();
        cause.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        cause.message = "Melting tick Light class namespace requires actual World context";
        return rb_registry(f, 1, &cause);
    }
    const mc_world_light_type *type = NULL;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_light_type_get(f->world->classes, MC_LIGHT_BLOCK, &type, &cause) !=
        MC_WORLD_STORAGE_OK)
        return rb_storage(f, 1, cause);
    rb_at(f, 8);
    if (mc_world_runtime_saved_light(f->world, type, x, y, z, out, &f->error->query) !=
        MC_WORLD_STORAGE_OK) {
        f->error->registry = f->error->query.registry;
        if (f->error->query.population.reached)
            f->error->population = f->error->query.population;
        return rb_storage(f, 8, f->error->query.cause);
    }
    return true;
}

bool mc_world_mutation_ice_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                int32_t y, int32_t z, mc_java_random *random,
                                mc_world_mutation_error *error) {
    (void)random;
    random_block_frame f;
    if (!rb_start(&f, world, block, error, "nk", "ik"))
        return false;
    int32_t light;
    if (!melt_light(&f, x, y, z, &light))
        return false;
    rb_at(&f, 13);
    mc_registry_block_q_view q;
    mc_registry_error cause = mc_world_mutation_empty_registry();
    if (mc_registry_block_q_get(block->classes, &q, &cause) != MC_REGISTRY_OK)
        return rb_registry(&f, 13, &cause);
    const int32_t id = block->fields.bn;
    int32_t opacity;
    rb_at(&f, 20);
    if (mc_registry_block_q_at(&q, id, &opacity, &cause) != MC_REGISTRY_OK)
        return rb_registry(&f, 20, &cause);
    if (light <= mc_java_i32_sub(11, opacity))
        return rb_done(&f);
    int32_t metadata;
    if (!rb_get(&f, 36, x, y, z, true, &metadata))
        return false;
    rb_at(&f, 39);
    if (!mc_world_mutation_drop(world, block, x, y, z, metadata, error))
        return rb_nested(&f, 39, error);
    int32_t water;
    if (!rb_static_id(&f, "C", 47, 50, &water) || !rb_set(&f, 53, x, y, z, water))
        return false;
    return rb_done(&f);
}

bool mc_world_mutation_snow_block_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, mc_java_random *random,
                                       mc_world_mutation_error *error) {
    (void)random;
    random_block_frame f;
    if (!rb_start(&f, world, block, error, "ac", "p"))
        return false;
    int32_t light;
    if (!melt_light(&f, x, y, z, &light))
        return false;
    if (light <= 11)
        return rb_done(&f);
    int32_t metadata;
    if (!rb_get(&f, 27, x, y, z, true, &metadata))
        return false;
    rb_at(&f, 30);
    if (!mc_world_mutation_drop(world, block, x, y, z, metadata, error))
        return rb_nested(&f, 30, error);
    if (!rb_set(&f, 39, x, y, z, 0))
        return false;
    return rb_done(&f);
}
