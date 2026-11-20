#include "mc/world/runtime/population/overworld.h"
#include "mc/block/registry/falling.h"

#include "../chunk_provider_state_internal.h"
#include "../construction_internal.h"
#include "../runtime_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/world/noise/noise.h"

#include <float.h>
#include <limits.h>

#if !defined(__x86_64__) && !defined(_M_X64) && !defined(_M_AMD64)
#error "Overworld population requires the captured x86-64 floating profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Overworld population requires strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 &&
                   sizeof(float) == 4 && sizeof(double) == 8 && FLT_EVAL_METHOD == 0,
               "Overworld population requires source-width binary32/binary64");

typedef struct overworld_frame {
    mc_chunk_provider *provider;
    mc_world_runtime *world;
    bool entered, client;
    mc_world_overworld_population_error error;
} overworld_frame;

typedef struct overworld_point {
    int32_t x, y, z;
} overworld_point;

typedef enum selected_tree { TREE_OAK, TREE_BIRCH, TREE_TALL, TREE_DENSE, TREE_BIG } selected_tree;

static mc_registry_error empty_registry(void) {
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static mc_world_overworld_population_error empty_error(void) {
    return (mc_world_overworld_population_error){.iteration = SIZE_MAX,
                                                 .registry = empty_registry()};
}

static void stage(overworld_frame *f, unsigned int pc) {
    f->error.bytecode = pc;
    f->error.world_bytecode = 0u;
    f->error.selector_class = NULL;
    f->error.selector_method = NULL;
    f->error.selector_bytecode = 0u;
    f->error.child = MC_OVERWORLD_CHILD_NONE;
    f->error.native_boundary = false;
    f->error.registry = empty_registry();
    f->error.nested = (mc_world_population_context){0};
}

static bool fail(overworld_frame *f, mc_world_storage_status status, const char *operation,
                 bool native) {
    f->error.status = status;
    f->error.operation = operation;
    f->error.native_boundary = native;
    f->error.cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static mc_world_population_context mutation_context(const mc_world_mutation_error *error) {
    return error->population.reached ? error->population : error->light.population;
}

/* Read only the active union member. Feature errors already own finite typed
 * children; carry only their explicitly returned failed acquisition context. */
static mc_world_population_context child_context(const mc_world_overworld_population_error *error) {
    switch (error->child) {
        case MC_OVERWORLD_CHILD_ORE_CLAY:
            return mutation_context(&error->ore_clay.mutation);
        case MC_OVERWORLD_CHILD_PLANT:
            if (error->plant.plant.query.population.reached)
                return error->plant.plant.query.population;
            return mutation_context(&error->plant.mutation);
        case MC_OVERWORLD_CHILD_LAKE_SPRING:
            if (error->lake_spring.query.population.reached)
                return error->lake_spring.query.population;
            return mutation_context(&error->lake_spring.mutation);
        case MC_OVERWORLD_CHILD_DUNGEON:
            return mutation_context(&error->dungeon.mutation);
        case MC_OVERWORLD_CHILD_TREE:
            return mutation_context(&error->tree.mutation);
        case MC_OVERWORLD_CHILD_HEIGHT:
            return error->height.population;
        case MC_OVERWORLD_CHILD_QUERY:
            return error->query.population;
        case MC_OVERWORLD_CHILD_MUTATION:
            return mutation_context(&error->mutation);
        default:
            return (mc_world_population_context){0};
    }
}

static bool child_result(overworld_frame *f, mc_world_storage_status status,
                         const mc_world_storage_error *cause, const mc_registry_error *registry) {
    if (status == MC_WORLD_STORAGE_OK) {
        f->error.child = MC_OVERWORLD_CHILD_NONE;
        return true;
    }
    f->error.status = status;
    f->error.operation = cause->operation;
    f->error.cause = *cause;
    if (registry != NULL)
        f->error.registry = *registry;
    f->error.nested = child_context(&f->error);
    if (f->error.nested.reached)
        f->error.registry = f->error.nested.registry;
    return false;
}

mc_world_population_context
mc_world_overworld_population_context_get(const mc_world_overworld_population_error *error) {
    if (error == NULL || error->status == MC_WORLD_STORAGE_OK)
        return (mc_world_population_context){0};
    const mc_world_population_site site = {.provider_class = error->provider_class,
                                           .method = error->method,
                                           .bytecode = error->bytecode,
                                           .world_bytecode = error->world_bytecode,
                                           .selector_class = error->selector_class,
                                           .selector_method = error->selector_method,
                                           .selector_bytecode = error->selector_bytecode,
                                           .iteration = error->iteration,
                                           .native_boundary = error->native_boundary};
    return (mc_world_population_context){.reached = true,
                                         .current = site,
                                         .origin =
                                             error->nested.reached ? error->nested.origin : site,
                                         .cause = error->cause,
                                         .registry = error->registry};
}

static bool registry_failure(overworld_frame *f, mc_registry_status status,
                             const mc_registry_error *cause, const char *operation) {
    f->error.child = MC_OVERWORLD_CHILD_REGISTRY;
    f->error.registry = *cause;
    f->error.status =
        mc_world_runtime_registry_result(f->world, status, cause, operation, &f->error.cause);
    f->error.operation = f->error.cause.operation;
    return false;
}

static mc_world_storage_status dimension_status(mc_world_provider_status status) {
    return status == MC_WORLD_PROVIDER_LIMIT               ? MC_WORLD_STORAGE_LIMIT
           : status == MC_WORLD_PROVIDER_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                           : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status climate_status(mc_biome_status status) {
    return status == MC_BIOME_LIMIT               ? MC_WORLD_STORAGE_LIMIT
           : status == MC_BIOME_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                  : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static bool flag(overworld_frame *f, bool value, unsigned int pc) {
    stage(f, pc);
    mc_registry_classes *classes = mc_world_runtime_registry(f->world);
    mc_registry_error cause = empty_registry();
    mc_registry_status status;
    if (classes == NULL) {
        status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        cause.status = status;
        cause.message = "Overworld population Registry binding";
    } else {
        status = mc_registry_falling_flag_set(classes, value, &cause);
    }
    return status == MC_REGISTRY_OK ||
           registry_failure(f, status, &cause, "Overworld population falling flag");
}

static bool block_id(overworld_frame *f, const char *name, unsigned int static_pc,
                     unsigned int field_pc, int32_t *out) {
    stage(f, static_pc);
    const mc_block *block = NULL;
    mc_registry_error cause = empty_registry();
    mc_registry_status status =
        mc_registry_block_static_get(mc_world_runtime_registry(f->world), name, &block, &cause);
    if (status != MC_REGISTRY_OK)
        return registry_failure(f, status, &cause, "Overworld population Block static");
    stage(f, field_pc);
    const mc_block_fields *fields = mc_block_fields_get(block);
    if (fields == NULL) {
        cause = empty_registry();
        cause.status = MC_REGISTRY_NULL_REFERENCE;
        cause.message = "Overworld population Block.bn receiver";
        return registry_failure(f, cause.status, &cause, "Overworld population Block.bn");
    }
    *out = fields->bn;
    return true;
}

static bool biome_is(overworld_frame *f, const mc_biome *biome, char field, unsigned int pc,
                     bool *out) {
    stage(f, pc);
    const mc_biome *expected = NULL;
    mc_registry_error cause = empty_registry();
    mc_registry_status status =
        mc_biome_static_get(mc_world_runtime_registry(f->world), field, &expected, &cause);
    if (status != MC_REGISTRY_OK)
        return registry_failure(f, status, &cause, "Overworld population Biome static");
    *out = biome == expected;
    return true;
}

/* World.a: t1, Dimension.b4, return7. NULL b is a successful returned value;
 * it is dereferenced only by the later point33 or temperature2681 invocation. */
static bool manager_capture(overworld_frame *f, unsigned int pc, mc_world_chunk_manager **out) {
    stage(f, pc);
    f->error.world_bytecode = 4u;
    mc_world_provider_fields fields;
    mc_world_provider_status status = mc_world_provider_fields_get(f->world->fields.t, &fields);
    if (status != MC_WORLD_PROVIDER_OK) {
        f->error.dimension = status;
        return fail(f, dimension_status(status), "Overworld population World Dimension receiver",
                    false);
    }
    if (fields.b != NULL) {
        status = mc_world_chunk_manager_retain(fields.b);
        if (status != MC_WORLD_PROVIDER_OK) {
            f->error.dimension = status;
            return fail(f, dimension_status(status), "Overworld population manager native hold",
                        true);
        }
    }
    *out = fields.b;
    return true;
}

static bool world_seed(overworld_frame *f, unsigned int pc, int64_t *out) {
    stage(f, pc);
    f->error.world_bytecode = 4u;
    mc_world_info_fields fields;
    if (mc_world_info_fields_get(f->world->fields.x, &fields) != MC_WORLD_INFO_OK)
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Overworld population WorldInfo seed",
                    false);
    *out = fields.value.seed;
    return true;
}

static bool bounded(overworld_frame *f, unsigned int pc, int32_t bound, int32_t *out) {
    stage(f, pc);
    if (!mc_java_random_next_bounded(&f->provider->generator.generator_random, bound, out))
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Overworld population Random.nextInt",
                    false);
    return true;
}

static bool fp(overworld_frame *f, unsigned int pc) {
    stage(f, pc);
    return ((_mm_getcsr() & 0xffc0u) == 0x1f80u) ||
           fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "Overworld population strict floating profile", true);
}

static double add(double a, double b) {
    volatile double r = a + b;
    return r;
}

static double sub(double a, double b) {
    volatile double r = a - b;
    return r;
}

static double mul(double a, double b) {
    volatile double r = a * b;
    return r;
}

static double divide(double a, double b) {
    volatile double r = a / b;
    return r;
}

static int64_t signed_bits(uint64_t bits) {
    if (bits <= (uint64_t)INT64_MAX)
        return (int64_t)bits;
    return mc_java_i64_add(INT64_MIN, (int64_t)(bits - (UINT64_C(1) << 63)));
}

static bool point(overworld_frame *f, int32_t ox, int32_t oz, int32_t offset, int32_t y_bound,
                  unsigned int xpc, unsigned int ypc, unsigned int zpc, overworld_point *out) {
    int32_t value;
    if (!bounded(f, xpc, 16, &value))
        return false;
    out->x = mc_java_i32_add(mc_java_i32_add(ox, value), offset);
    if (!bounded(f, ypc, y_bound, &out->y))
        return false;
    if (!bounded(f, zpc, 16, &value))
        return false;
    out->z = mc_java_i32_add(mc_java_i32_add(oz, value), offset);
    return true;
}

static bool nested_point(overworld_frame *f, int32_t ox, int32_t oz, int32_t first_bound,
                         unsigned int xpc, const unsigned int *ypc, size_t y_count,
                         unsigned int zpc, overworld_point *out) {
    int32_t value;
    if (!bounded(f, xpc, 16, &value))
        return false;
    out->x = mc_java_i32_add(mc_java_i32_add(ox, value), 8);
    if (!bounded(f, ypc[0], first_bound, &value))
        return false;
    for (size_t i = 1u; i < y_count; ++i) {
        int32_t bound = mc_java_i32_add(value, 8);
        if (!bounded(f, ypc[i], bound, &value))
            return false;
    }
    out->y = value;
    if (!bounded(f, zpc, 16, &value))
        return false;
    out->z = mc_java_i32_add(mc_java_i32_add(oz, value), 8);
    return true;
}

static bool lake(overworld_frame *f, overworld_point p, const char *target, unsigned int spc,
                 unsigned int fpc, unsigned int cpc, unsigned int ppc) {
    int32_t id;
    if (!block_id(f, target, spc, fpc, &id))
        return false;
    mc_population_lake_feature feature = {0};
    stage(f, cpc);
    mc_world_storage_status status = mc_population_lake_feature_init(&feature, id);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(f, status, "Overworld population lake native field constructor", true);
    stage(f, ppc);
    f->error.child = MC_OVERWORLD_CHILD_LAKE_SPRING;
    bool ignored;
    status = mc_population_lake_place(&feature, f->world, &f->provider->generator.generator_random,
                                      p.x, p.y, p.z, &ignored, &f->error.lake_spring);
    return child_result(f, status, &f->error.lake_spring.cause, &f->error.lake_spring.registry);
}

typedef struct ore_schedule {
    int32_t count, y_bound, size;
    const char *field;
    unsigned int xpc, ypc, zpc, spc, fpc, cpc, ppc;
} ore_schedule;

static bool ores(overworld_frame *f, int32_t ox, int32_t oz) {
    /* Bytecode operands and sites, not cached Blocks/IDs or property palettes. */
    static const ore_schedule schedule[] = {
        {20, 128, 32, "w", 514, 527, 540, 550, 553, 558, 575},
        {10, 128, 32, "G", 603, 616, 629, 639, 642, 647, 664},
        {20, 128, 16, "J", 692, 705, 718, 728, 731, 736, 753},
        {20, 64, 8, "I", 781, 793, 806, 816, 819, 824, 841},
        {2, 32, 8, "H", 868, 880, 893, 903, 906, 911, 928},
        {8, 16, 7, "aO", 956, 968, 981, 991, 994, 999, 1016},
        {1, 16, 7, "ax", 1043, 1055, 1068, 1078, 1081, 1086, 1103},
        {1, 16, 6, "O", 1130, 1142, 1165, 1175, 1178, 1183, 1200}};
    for (size_t group = 0u; group < sizeof(schedule) / sizeof(schedule[0]); ++group) {
        const ore_schedule *s = &schedule[group];
        for (int32_t i = 0; i < s->count; i = mc_java_i32_add(i, 1)) {
            f->error.iteration = (size_t)i;
            overworld_point p;
            if (group != 7u) {
                if (!point(f, ox, oz, 0, s->y_bound, s->xpc, s->ypc, s->zpc, &p))
                    return false;
            } else {
                int32_t value, y2;
                if (!bounded(f, 1130, 16, &value))
                    return false;
                p.x = mc_java_i32_add(ox, value);
                if (!bounded(f, 1142, 16, &p.y))
                    return false;
                if (!bounded(f, 1151, 16, &y2))
                    return false;
                p.y = mc_java_i32_add(p.y, y2);
                if (!bounded(f, 1165, 16, &value))
                    return false;
                p.z = mc_java_i32_add(oz, value);
            }
            int32_t id;
            if (!block_id(f, s->field, s->spc, s->fpc, &id))
                return false;
            mc_population_ore_feature feature = {0};
            stage(f, s->cpc);
            mc_world_storage_status status = mc_population_ore_feature_init(&feature, id, s->size);
            if (status != MC_WORLD_STORAGE_OK)
                return fail(f, status, "Overworld population ore native field constructor", true);
            stage(f, s->ppc);
            f->error.child = MC_OVERWORLD_CHILD_ORE_CLAY;
            bool ignored;
            status = mc_population_ore_place(&feature, f->world,
                                             &f->provider->generator.generator_random, p.x, p.y,
                                             p.z, &ignored, &f->error.ore_clay);
            if (!child_result(f, status, &f->error.ore_clay.cause, &f->error.ore_clay.registry))
                return false;
        }
    }
    f->error.iteration = SIZE_MAX;
    return true;
}

static bool flowers(overworld_frame *f, int32_t ox, int32_t oz, int32_t count, bool source_loop,
                    const char *target, unsigned int xpc, unsigned int ypc, unsigned int zpc,
                    unsigned int spc, unsigned int fpc, unsigned int cpc, unsigned int ppc) {
    for (int32_t i = 0; i < count; i = mc_java_i32_add(i, 1)) {
        f->error.iteration = source_loop ? (size_t)i : SIZE_MAX;
        overworld_point p;
        if (!point(f, ox, oz, 8, 128, xpc, ypc, zpc, &p))
            return false;
        int32_t id;
        if (!block_id(f, target, spc, fpc, &id))
            return false;
        mc_population_flower_feature feature = {0};
        stage(f, cpc);
        mc_world_storage_status status = mc_population_flower_feature_init(&feature, id);
        if (status != MC_WORLD_STORAGE_OK)
            return fail(f, status, "Overworld population flower native field constructor", true);
        stage(f, ppc);
        f->error.child = MC_OVERWORLD_CHILD_PLANT;
        bool ignored;
        status = mc_population_flowers_place(&feature, f->world,
                                             &f->provider->generator.generator_random, p.x, p.y,
                                             p.z, &ignored, &f->error.plant);
        if (!child_result(f, status, &f->error.plant.cause, &f->error.plant.registry))
            return false;
    }
    f->error.iteration = SIZE_MAX;
    return true;
}

static bool selector_draw(overworld_frame *f, unsigned int pc, int32_t bound, int32_t *out) {
    f->error.child = MC_OVERWORLD_CHILD_NONE;
    f->error.selector_bytecode = pc;
    if (!mc_java_random_next_bounded(&f->provider->generator.generator_random, bound, out))
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Biome tree Random.nextInt", false);
    return true;
}

static bool select_tree(overworld_frame *f, const mc_biome *biome, selected_tree *out,
                        mc_world_tree_big_oak **big) {
    stage(f, 1430);
    if (biome == NULL)
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Biome tree selector receiver", false);
    f->error.selector_method =
        f->client ? "a(Ljava/util/Random;)Lpg;" : "a(Ljava/util/Random;)Ljm;";
    int32_t draw;
    unsigned int constructor;
    selected_tree kind;
    switch (mc_biome_kind_get(biome)) {
        case MC_BIOME_KIND_KD_GS:
        case MC_BIOME_KIND_FS_DW:
        case MC_BIOME_KIND_TF_MA:
        case MC_BIOME_KIND_T_M:
        case MC_BIOME_KIND_RY_LE:
            f->error.selector_class = f->client ? "kd" : "gs";
            if (!selector_draw(f, 3, 10, &draw))
                return false;
            kind = draw == 0 ? TREE_BIG : TREE_OAK;
            constructor = draw == 0 ? 13u : 21u;
            break;
        case MC_BIOME_KIND_YJ_PP:
            f->error.selector_class = f->client ? "yj" : "pp";
            if (!selector_draw(f, 2, 3, &draw))
                return false;
            kind = draw == 0 ? TREE_BIG : TREE_OAK;
            constructor = draw == 0 ? 12u : 20u;
            break;
        case MC_BIOME_KIND_RB_KK:
            f->error.selector_class = f->client ? "rb" : "kk";
            if (!selector_draw(f, 2, 5, &draw))
                return false;
            if (draw == 0) {
                kind = TREE_BIRCH;
                constructor = 12u;
            } else {
                if (!selector_draw(f, 18, 3, &draw))
                    return false;
                kind = draw == 0 ? TREE_BIG : TREE_OAK;
                constructor = draw == 0 ? 28u : 36u;
            }
            break;
        case MC_BIOME_KIND_G_F:
            f->error.selector_class = f->client ? "g" : "f";
            if (!selector_draw(f, 2, 3, &draw))
                return false;
            kind = draw == 0 ? TREE_TALL : TREE_DENSE;
            constructor = draw == 0 ? 12u : 20u;
            break;
        default:
            return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Biome tree actual native kind",
                        true);
    }
    f->error.selector_bytecode = constructor;
    if (kind == TREE_BIG) {
        f->error.child = MC_OVERWORLD_CHILD_TREE;
        mc_world_storage_status status =
            mc_world_tree_big_oak_create(f->world, NULL, big, &f->error.tree);
        if (!child_result(f, status, &f->error.tree.cause, &f->error.tree.registry))
            return false;
    }
    /* The four small constructors invoke only their actual empty base. */
    *out = kind;
    return true;
}

static bool tree_iteration(overworld_frame *f, const mc_biome *biome, int32_t ox, int32_t oz) {
    int32_t value, x, z;
    if (!bounded(f, 1398, 16, &value))
        return false;
    x = mc_java_i32_add(mc_java_i32_add(ox, value), 8);
    if (!bounded(f, 1415, 16, &value))
        return false;
    z = mc_java_i32_add(mc_java_i32_add(oz, value), 8);
    selected_tree kind = TREE_OAK;
    mc_world_tree_big_oak *big = NULL;
    bool success = false;
    if (!select_tree(f, biome, &kind, &big))
        goto leave;
    stage(f, 1440);
    if (kind == TREE_BIG) {
        f->error.child = MC_OVERWORLD_CHILD_TREE;
        mc_world_storage_status status =
            mc_world_tree_big_oak_scale(big, 1.0, 1.0, 1.0, &f->error.tree);
        if (!child_result(f, status, &f->error.tree.cause, &f->error.tree.registry))
            goto leave;
    }
    /* Base small scale is literal return0. Source captures j1450 BEFORE the
     * acquiring cached-height call1463. Reentry changes this SAME Random. */
    mc_java_random *random = &f->provider->generator.generator_random;
    int32_t y;
    stage(f, 1463);
    f->error.child = MC_OVERWORLD_CHILD_HEIGHT;
    mc_world_storage_status status = mc_world_runtime_height(f->world, x, z, &y, &f->error.height);
    if (!child_result(f, status, &f->error.height.cause, NULL))
        goto leave;
    stage(f, 1468);
    f->error.child = MC_OVERWORLD_CHILD_TREE;
    bool ignored;
    switch (kind) {
        case TREE_OAK:
            status = mc_world_tree_oak(f->world, random, x, y, z, &ignored, &f->error.tree);
            break;
        case TREE_BIRCH:
            status = mc_world_tree_birch(f->world, random, x, y, z, &ignored, &f->error.tree);
            break;
        case TREE_TALL:
            status = mc_world_tree_taiga_tall(f->world, random, x, y, z, &ignored, &f->error.tree);
            break;
        case TREE_DENSE:
            status = mc_world_tree_taiga_dense(f->world, random, x, y, z, &ignored, &f->error.tree);
            break;
        case TREE_BIG:
            status = mc_world_tree_big_oak_place(big, f->world, random, x, y, z, &ignored,
                                                 &f->error.tree);
            break;
        default:
            (void)fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Overworld selected tree native kind",
                       true);
            goto leave;
    }
    success = child_result(f, status, &f->error.tree.cause, &f->error.tree.registry);
leave:
    mc_world_tree_big_oak_dispose(big);
    return success;
}

static bool tree_count(overworld_frame *f, const mc_biome *biome, int32_t ox, int32_t oz,
                       int32_t *out) {
    if (!fp(f, 1231))
        return false;
    const mc_noise_octaves noise = {f->provider->generator.terrain.later_layers, 8u};
    const double sample =
        mc_noise_octaves_sample2(&noise, mul((double)ox, 0.5), mul((double)oz, 0.5));
    const double divided = divide(sample, 8.0);
    if (!fp(f, 1242))
        return false;
    const double uniform = mc_java_random_next_double(&f->provider->generator.generator_random);
    const int32_t density =
        mc_java_double_to_i32(divide(add(add(divided, mul(uniform, 4.0)), 4.0), 3.0));
    int32_t draw, count = 0;
    if (!bounded(f, 1270, 10, &draw))
        return false;
    if (draw == 0)
        count = mc_java_i32_add(count, 1);
    bool matches;
    static const char fields[] = {'d', 'a', 'c', 'g', 'h', 'k', 'i'};
    static const unsigned int pcs[] = {1281, 1298, 1315, 1332, 1349, 1360, 1371};
    for (size_t i = 0u; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        if (!biome_is(f, biome, fields[i], pcs[i], &matches))
            return false;
        if (matches)
            count = i < 4u ? mc_java_i32_add(count, mc_java_i32_add(density, i == 2u ? 2 : 5))
                           : mc_java_i32_sub(count, 20);
    }
    *out = count;
    return true;
}

static bool temperature_refresh(overworld_frame *f, int32_t ox, int32_t oz) {
    mc_world_chunk_manager *receiver = NULL;
    if (!manager_capture(f, 2660, &receiver))
        return false;
    /* Actual source w2664 is captured before argument arithmetic/call2681.
     * There are no World callbacks in the pure native owner/sample calls. */
    mc_temperature_array *input = f->provider->w;
    mc_temperature_array *returned = NULL;
    bool success = false;
    const int32_t x = mc_java_i32_add(ox, 8), z = mc_java_i32_add(oz, 8);
    stage(f, 2681);
    if (receiver == NULL) {
        (void)fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "temperature manager invocation receiver",
                   false);
        goto leave;
    }
    bool needs_output = input == NULL;
    if (input != NULL) {
        mc_temperature_array_view view;
        mc_biome_status status = mc_temperature_array_get(input, &view);
        if (status != MC_BIOME_OK) {
            f->error.temperature_status = status;
            (void)fail(f, climate_status(status), "temperature input native identity", true);
            goto leave;
        }
        needs_output = view.length < 256u;
    }
    /* Native allocator bookkeeping is lazy and separately bounded. Do not
     * allocate a new owner when a sufficient existing input can be reused. */
    if (needs_output && f->provider->temperature_allocations == NULL) {
        mc_temperature_owner *owner = NULL;
        f->error.child = MC_OVERWORLD_CHILD_TEMPERATURE;
        mc_biome_status status = mc_temperature_owner_create(&f->provider->temperature_limits,
                                                             &owner, &f->error.temperature);
        if (status != MC_BIOME_OK) {
            f->error.temperature_status = status;
            (void)fail(f, climate_status(status), f->error.temperature.operation, true);
            goto leave;
        }
        f->provider->temperature_allocations = owner;
    }
    f->error.child = MC_OVERWORLD_CHILD_TEMPERATURE;
    mc_biome_status status =
        mc_temperature_sample(receiver, f->provider->temperature_allocations, input, x, z, 16u, 16u,
                              &returned, &f->error.temperature);
    if (status != MC_BIOME_OK) {
        f->error.temperature_status = status;
        (void)fail(f, climate_status(status), f->error.temperature.operation,
                   f->error.temperature.native_boundary);
        goto leave;
    }
    stage(f, 2684);
    mc_temperature_array *previous = f->provider->w;
    f->provider->w = returned; /* actual putfield; transfer returned hold */
    returned = NULL;
    mc_temperature_array_release(previous);
    success = true;
leave:
    mc_temperature_array_release(returned);
    mc_world_chunk_manager_release(receiver);
    return success;
}

static bool snow(overworld_frame *f, int32_t ox, int32_t oz) {
    if (!temperature_refresh(f, ox, oz))
        return false;
    const int32_t start_x = mc_java_i32_add(ox, 8), start_z = mc_java_i32_add(oz, 8);
    const int32_t end_x = mc_java_i32_add(start_x, 16), end_z = mc_java_i32_add(start_z, 16);
    /* Absolute signed comparisons are source semantics: wrap may empty a loop. */
    for (int32_t x = start_x; x < end_x; x = mc_java_i32_add(x, 1)) {
        for (int32_t z = start_z; z < end_z; z = mc_java_i32_add(z, 1)) {
            const int32_t local_x = mc_java_i32_sub(x, start_x);
            const int32_t local_z = mc_java_i32_sub(z, start_z);
            int32_t height;
            stage(f, 2755);
            f->error.child = MC_OVERWORLD_CHILD_QUERY;
            mc_world_storage_status status =
                mc_world_runtime_top_solid_liquid(f->world, x, z, &height, &f->error.query);
            if (!child_result(f, status, &f->error.query.cause, &f->error.query.registry))
                return false;
            /* RELOAD actual w2761 AFTER acquiring height, which can reenter. */
            mc_temperature_array *array = f->provider->w;
            const int32_t index = mc_java_i32_add(mc_java_i32_mul(local_x, 16), local_z);
            stage(f, 2772);
            if (array == NULL)
                return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "snow temperature array receiver", false);
            mc_temperature_array_view view;
            mc_biome_status climate = mc_temperature_array_get(array, &view);
            if (climate != MC_BIOME_OK) {
                f->error.temperature_status = climate;
                return fail(f, climate_status(climate), "snow temperature native array identity",
                            true);
            }
            if (index < 0 || (size_t)index >= view.length)
                return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "snow temperature array index",
                            false);
            const double current_temperature = view.data[(size_t)index];
            const int32_t relative_height = mc_java_i32_sub(height, 64);
            if (!fp(f, 2778))
                return false;
            const double adjusted =
                sub(current_temperature, mul(divide((double)relative_height, 64.0), 0.3));
            if (!(adjusted < 0.5) || height <= 0 || height >= 128)
                continue;

            stage(f, 2822);
            f->error.world_bytecode = 4u; /* fd.d/dj.f air wrapper */
            int32_t id;
            mc_world_storage_error storage;
            status = mc_world_runtime_block_get(f->world, x, height, z, &id, &storage);
            if (!child_result(f, status, &storage, NULL))
                return false;
            if (id != 0)
                continue;
            const int32_t below = mc_java_i32_sub(height, 1);
            mc_material *material = NULL;
            stage(f, 2840);
            f->error.child = MC_OVERWORLD_CHILD_MATERIAL;
            status = mc_world_runtime_material_get_detailed(f->world, x, below, z, &material,
                                                            &f->error.material);
            if (!child_result(f, status, &f->error.material.cause, &f->error.material.registry))
                return false;
            stage(f, 2843);
            if (material == NULL)
                return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "snow Material.c receiver",
                            false);
            if (!mc_material_c(material))
                continue;
            /* A distinct fresh material query, not reuse of the first result. */
            stage(f, 2861);
            f->error.child = MC_OVERWORLD_CHILD_MATERIAL;
            status = mc_world_runtime_material_get_detailed(f->world, x, below, z, &material,
                                                            &f->error.material);
            if (!child_result(f, status, &f->error.material.cause, &f->error.material.registry))
                return false;
            stage(f, 2864);
            mc_material *ice = NULL;
            mc_registry_error registry = empty_registry();
            mc_registry_status rs = mc_registry_material_get(mc_world_runtime_registry(f->world),
                                                             MC_MATERIAL_s, &ice, &registry);
            if (rs != MC_REGISTRY_OK)
                return registry_failure(f, rs, &registry, "snow Material.s static");
            if (material == ice)
                continue;
            if (!block_id(f, "aT", 2880, 2883, &id))
                return false;
            stage(f, 2886);
            f->error.child = MC_OVERWORLD_CHILD_MUTATION;
            bool ignored;
            status = mc_world_runtime_block_set(f->world, x, height, z, id, &ignored,
                                                &f->error.mutation);
            if (!child_result(f, status, &f->error.mutation.cause, &f->error.mutation.registry))
                return false;
        }
    }
    return true;
}

typedef enum stateless_plant { PLANT_REED, PLANT_PUMPKIN, PLANT_CACTUS } stateless_plant;

static bool plant_place(overworld_frame *f, stateless_plant kind, overworld_point p,
                        unsigned int constructor_pc, unsigned int place_pc) {
    stage(f, constructor_pc); /* real empty pg/jm base-only constructor */
    stage(f, place_pc);
    f->error.child = MC_OVERWORLD_CHILD_PLANT;
    mc_world_storage_status status;
    mc_java_random *random = &f->provider->generator.generator_random;
    bool ignored;
    switch (kind) {
        case PLANT_REED:
            status = mc_population_reeds_place(f->world, random, p.x, p.y, p.z, &ignored,
                                               &f->error.plant);
            break;
        case PLANT_PUMPKIN:
            status = mc_population_pumpkins_place(f->world, random, p.x, p.y, p.z, &ignored,
                                                  &f->error.plant);
            break;
        case PLANT_CACTUS:
            status = mc_population_cacti_place(f->world, random, p.x, p.y, p.z, &ignored,
                                               &f->error.plant);
            break;
        default:
            return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Overworld stateless plant native kind", true);
    }
    return child_result(f, status, &f->error.plant.cause, &f->error.plant.registry);
}

static bool spring(overworld_frame *f, overworld_point p, const char *target, unsigned int spc,
                   unsigned int fpc, unsigned int cpc, unsigned int ppc) {
    int32_t id;
    if (!block_id(f, target, spc, fpc, &id))
        return false;
    mc_population_spring_feature feature = {0};
    stage(f, cpc);
    mc_world_storage_status status = mc_population_spring_feature_init(&feature, id);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(f, status, "Overworld spring native field constructor", true);
    stage(f, ppc);
    f->error.child = MC_OVERWORLD_CHILD_LAKE_SPRING;
    bool ignored;
    status =
        mc_population_spring_place(&feature, f->world, &f->provider->generator.generator_random,
                                   p.x, p.y, p.z, &ignored, &f->error.lake_spring);
    return child_result(f, status, &f->error.lake_spring.cause, &f->error.lake_spring.registry);
}

mc_world_storage_status
mc_chunk_provider_populate_game(mc_chunk_provider *provider, int32_t chunk_x, int32_t chunk_z,
                                mc_world_overworld_population_error *error) {
    overworld_frame f = {.provider = provider, .error = empty_error()};
    mc_world_chunk_manager *point_owner = NULL;
    bool success = false;
    if (provider == NULL || provider->world == NULL) {
        (void)fail(&f, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Overworld population native provider",
                   true);
        goto leave;
    }
    f.world = provider->world;
    if (f.world->construction.mode != MC_WORLD_RUNTIME_GAME || f.world->classes == NULL) {
        (void)fail(&f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Overworld population native GAME owner",
                   true);
        goto leave;
    }
    f.client = f.world->classes->side == MC_WORLD_INFO_CLIENT;
    f.error.provider_class = f.client ? "yf" : "pk";
    f.error.method = f.client ? "a(Lcl;II)V" : "a(Lbl;II)V";
    f.error.status = mc_world_runtime_call_enter(f.world, &f.error.cause);
    if (f.error.status != MC_WORLD_STORAGE_OK) {
        f.error.operation = f.error.cause.operation;
        f.error.native_boundary = true;
        goto leave;
    }
    f.entered = true;
    if (!flag(&f, true, 1))
        goto leave;
    const int32_t ox = mc_java_i32_mul(chunk_x, 16), oz = mc_java_i32_mul(chunk_z, 16);
    if (!manager_capture(&f, 20, &point_owner))
        goto leave;
    stage(&f, 33);
    if (point_owner == NULL) {
        (void)fail(&f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Biome point invocation receiver",
                   false);
        goto leave;
    }
    const mc_biome *biome = NULL;
    f.error.child = MC_OVERWORLD_CHILD_BIOME;
    mc_world_provider_status dimension = mc_world_chunk_manager_point(
        point_owner, mc_java_i32_add(ox, 16), mc_java_i32_add(oz, 16), &biome, &f.error.biome);
    if (dimension != MC_WORLD_PROVIDER_OK) {
        f.error.dimension = dimension;
        f.error.registry = f.error.biome.sampling.registry;
        f.error.status = dimension_status(dimension);
        f.error.operation = f.error.biome.operation;
        f.error.cause = (mc_world_storage_error){f.error.status, f.error.operation, 0};
        goto leave;
    }
    f.error.child = MC_OVERWORLD_CHILD_NONE;

    int64_t seed;
    if (!world_seed(&f, 46, &seed))
        goto leave;
    stage(&f, 49);
    mc_java_random_reseed(&provider->generator.generator_random, seed);
    stage(&f, 56);
    int64_t first = mc_java_random_next_i64(&provider->generator.generator_random);
    const int64_t odd_x =
        mc_java_i64_add(mc_java_i64_mul(first / INT64_C(2), INT64_C(2)), INT64_C(1));
    stage(&f, 75);
    int64_t second = mc_java_random_next_i64(&provider->generator.generator_random);
    const int64_t odd_z =
        mc_java_i64_add(mc_java_i64_mul(second / INT64_C(2), INT64_C(2)), INT64_C(1));
    const int64_t mixed = mc_java_i64_add(mc_java_i64_mul((int64_t)chunk_x, odd_x),
                                          mc_java_i64_mul((int64_t)chunk_z, odd_z));
    if (!world_seed(&f, 109, &seed))
        goto leave;
    stage(&f, 113);
    mc_java_random_reseed(&provider->generator.generator_random,
                          signed_bits((uint64_t)mixed ^ (uint64_t)seed));
    /* Source unused dstore11=0.25 at116 has no retained effect. */

    int32_t gate;
    overworld_point p;
    if (!bounded(&f, 126, 4, &gate))
        goto leave;
    if (gate == 0) {
        if (!point(&f, ox, oz, 8, 128, 140, 156, 169, &p) || !lake(&f, p, "C", 182, 185, 188, 205))
            goto leave;
    }
    if (!bounded(&f, 215, 8, &gate))
        goto leave;
    if (gate == 0) {
        static const unsigned int ypc[] = {248, 254};
        if (!nested_point(&f, ox, oz, 120, 229, ypc, 2u, 267, &p))
            goto leave;
        bool admitted = p.y < 64;
        if (!admitted) {
            if (!bounded(&f, 289, 10, &gate))
                goto leave;
            admitted = gate == 0;
        }
        if (admitted && !lake(&f, p, "E", 299, 302, 305, 322))
            goto leave;
    }
    for (int32_t i = 0; i < 8; i = mc_java_i32_add(i, 1)) {
        f.error.iteration = (size_t)i;
        if (!point(&f, ox, oz, 8, 128, 344, 360, 373, &p))
            goto leave;
        stage(&f, 386); /* actual er/da empty constructor */
        stage(&f, 403);
        f.error.child = MC_OVERWORLD_CHILD_DUNGEON;
        bool ignored;
        mc_world_storage_status status =
            mc_population_dungeon_place(f.world, &provider->generator.generator_random, NULL, p.x,
                                        p.y, p.z, &ignored, &f.error.dungeon);
        if (!child_result(&f, status, &f.error.dungeon.cause, &f.error.dungeon.registry))
            goto leave;
    }
    for (int32_t i = 0; i < 10; i = mc_java_i32_add(i, 1)) {
        f.error.iteration = (size_t)i;
        if (!point(&f, ox, oz, 0, 128, 431, 444, 457, &p))
            goto leave;
        mc_population_clay_feature feature = {0};
        stage(&f, 469);
        f.error.child = MC_OVERWORLD_CHILD_ORE_CLAY;
        mc_world_storage_status status =
            mc_population_clay_feature_init(&feature, f.world, 32, &f.error.ore_clay);
        if (!child_result(&f, status, &f.error.ore_clay.cause, &f.error.ore_clay.registry))
            goto leave;
        stage(&f, 486);
        f.error.child = MC_OVERWORLD_CHILD_ORE_CLAY;
        bool ignored;
        status = mc_population_clay_place(&feature, f.world, &provider->generator.generator_random,
                                          p.x, p.y, p.z, &ignored, &f.error.ore_clay);
        if (!child_result(&f, status, &f.error.ore_clay.cause, &f.error.ore_clay.registry))
            goto leave;
    }
    f.error.iteration = SIZE_MAX;
    if (!ores(&f, ox, oz))
        goto leave;
    int32_t count;
    if (!tree_count(&f, biome, ox, oz, &count))
        goto leave;
    for (int32_t i = 0; i < count; i = mc_java_i32_add(i, 1)) {
        f.error.iteration = (size_t)i;
        if (!tree_iteration(&f, biome, ox, oz))
            goto leave;
    }
    f.error.iteration = SIZE_MAX;
    bool matches;
    count = 0;
    static const char flower_biomes[] = {'d', 'c', 'g', 'i'};
    static const int32_t flower_counts[] = {2, 4, 2, 3};
    static const unsigned int flower_pcs[] = {1483, 1494, 1505, 1516};
    for (size_t i = 0u; i < sizeof(flower_biomes) / sizeof(flower_biomes[0]); ++i) {
        if (!biome_is(&f, biome, flower_biomes[i], flower_pcs[i], &matches))
            goto leave;
        if (matches)
            count = flower_counts[i];
    }
    if (!flowers(&f, ox, oz, count, true, "ae", 1543, 1559, 1572, 1585, 1588, 1591, 1608))
        goto leave;

    count = 0;
    static const char grass_biomes[] = {'d', 'a', 'c', 'g', 'i'};
    static const int32_t grass_counts[] = {2, 10, 2, 1, 10};
    static const unsigned int grass_pcs[] = {1623, 1634, 1646, 1657, 1668};
    for (size_t i = 0u; i < sizeof(grass_biomes) / sizeof(grass_biomes[0]); ++i) {
        if (!biome_is(&f, biome, grass_biomes[i], grass_pcs[i], &matches))
            goto leave;
        if (matches)
            count = grass_counts[i];
    }
    for (int32_t i = 0; i < count; i = mc_java_i32_add(i, 1)) {
        f.error.iteration = (size_t)i;
        int32_t metadata = 1;
        if (!biome_is(&f, biome, 'a', 1693, &matches))
            goto leave;
        if (matches) {
            if (!bounded(&f, 1704, 3, &gate))
                goto leave;
            if (gate != 0)
                metadata = 2;
        }
        if (!point(&f, ox, oz, 8, 128, 1721, 1737, 1750, &p))
            goto leave;
        int32_t id;
        if (!block_id(&f, "Y", 1763, 1766, &id))
            goto leave;
        mc_population_tall_grass_feature feature = {0};
        stage(&f, 1771);
        mc_world_storage_status status =
            mc_population_tall_grass_feature_init(&feature, id, metadata);
        if (status != MC_WORLD_STORAGE_OK) {
            (void)fail(&f, status, "Overworld grass native field constructor", true);
            goto leave;
        }
        stage(&f, 1788);
        f.error.child = MC_OVERWORLD_CHILD_PLANT;
        bool ignored;
        status =
            mc_population_tall_grass_place(&feature, f.world, &provider->generator.generator_random,
                                           p.x, p.y, p.z, &ignored, &f.error.plant);
        if (!child_result(&f, status, &f.error.plant.cause, &f.error.plant.registry))
            goto leave;
    }
    f.error.iteration = SIZE_MAX;
    count = 0;
    if (!biome_is(&f, biome, 'h', 1803, &matches))
        goto leave;
    if (matches)
        count = 2;
    for (int32_t i = 0; i < count; i = mc_java_i32_add(i, 1)) {
        f.error.iteration = (size_t)i;
        if (!point(&f, ox, oz, 8, 128, 1830, 1846, 1859, &p))
            goto leave;
        int32_t id;
        if (!block_id(&f, "Z", 1872, 1875, &id))
            goto leave;
        mc_population_dead_bush_feature feature = {0};
        stage(&f, 1878);
        mc_world_storage_status status = mc_population_dead_bush_feature_init(&feature, id);
        if (status != MC_WORLD_STORAGE_OK) {
            (void)fail(&f, status, "Overworld dead-bush native field constructor", true);
            goto leave;
        }
        stage(&f, 1895);
        f.error.child = MC_OVERWORLD_CHILD_PLANT;
        bool ignored;
        status =
            mc_population_dead_bush_place(&feature, f.world, &provider->generator.generator_random,
                                          p.x, p.y, p.z, &ignored, &f.error.plant);
        if (!child_result(&f, status, &f.error.plant.cause, &f.error.plant.registry))
            goto leave;
    }
    f.error.iteration = SIZE_MAX;
    if (!bounded(&f, 1910, 2, &gate))
        goto leave;
    if (gate == 0 && !flowers(&f, ox, oz, 1, false, "af", 1924, 1940, 1953, 1966, 1969, 1972, 1989))
        goto leave;
    if (!bounded(&f, 1998, 4, &gate))
        goto leave;
    if (gate == 0 && !flowers(&f, ox, oz, 1, false, "ag", 2012, 2028, 2041, 2054, 2057, 2060, 2077))
        goto leave;
    if (!bounded(&f, 2087, 8, &gate))
        goto leave;
    if (gate == 0 && !flowers(&f, ox, oz, 1, false, "ah", 2101, 2117, 2130, 2143, 2146, 2149, 2166))
        goto leave;
    for (int32_t i = 0; i < 10; i = mc_java_i32_add(i, 1)) {
        f.error.iteration = (size_t)i;
        if (!point(&f, ox, oz, 8, 128, 2188, 2204, 2217, &p) ||
            !plant_place(&f, PLANT_REED, p, 2230, 2247))
            goto leave;
    }
    f.error.iteration = SIZE_MAX;
    if (!bounded(&f, 2263, 32, &gate))
        goto leave;
    if (gate == 0) {
        if (!point(&f, ox, oz, 8, 128, 2277, 2293, 2306, &p) ||
            !plant_place(&f, PLANT_PUMPKIN, p, 2319, 2336))
            goto leave;
    }
    count = 0;
    if (!biome_is(&f, biome, 'h', 2345, &matches))
        goto leave;
    if (matches)
        count = mc_java_i32_add(count, 10);
    for (int32_t i = 0; i < count; i = mc_java_i32_add(i, 1)) {
        f.error.iteration = (size_t)i;
        if (!point(&f, ox, oz, 8, 128, 2372, 2388, 2401, &p) ||
            !plant_place(&f, PLANT_CACTUS, p, 2414, 2431))
            goto leave;
    }
    f.error.iteration = SIZE_MAX;
    for (int32_t i = 0; i < 50; i = mc_java_i32_add(i, 1)) {
        f.error.iteration = (size_t)i;
        static const unsigned int ypc[] = {2478, 2484};
        if (!nested_point(&f, ox, oz, 120, 2459, ypc, 2u, 2497, &p) ||
            !spring(&f, p, "B", 2510, 2513, 2516, 2533))
            goto leave;
    }
    for (int32_t i = 0; i < 20; i = mc_java_i32_add(i, 1)) {
        f.error.iteration = (size_t)i;
        static const unsigned int ypc[] = {2584, 2590, 2596};
        if (!nested_point(&f, ox, oz, 112, 2561, ypc, 3u, 2609, &p) ||
            !spring(&f, p, "D", 2622, 2625, 2628, 2645))
            goto leave;
    }
    f.error.iteration = SIZE_MAX;
    if (!snow(&f, ox, oz))
        goto leave;
    if (!flag(&f, false, 2903))
        goto leave;
    success = true;
leave:
    mc_world_chunk_manager_release(point_owner);
    if (f.entered) {
        if (!success)
            f.world->last_error = f.error.cause;
        mc_world_runtime_call_leave(f.world);
    }
    if (success)
        f.error = empty_error();
    if (error != NULL)
        *error = f.error;
    return f.error.status;
}
