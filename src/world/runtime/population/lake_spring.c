#include "mc/world/runtime/population/lake_spring.h"

/* Staging includes; root integration places the body below runtime/population. */
#include "../../../block/registry/runtime_internal.h"
#include "../construction_internal.h"
#include "../mutation/mutation_internal.h"
#include "../runtime_internal.h"
#include "mc/block/registry/runtime.h"

#include <float.h>
#include <stddef.h>
#include <stdlib.h>
#if !defined(__x86_64__) && !defined(_M_X64)
#error "Lake geometry requires the captured x86-64 strict floating profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Lake geometry requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 && sizeof(double) == 8 &&
                   FLT_EVAL_METHOD == 0,
               "Lake geometry requires source-width binary64 evaluation");
_Static_assert(sizeof(bool) == 1, "Lake mask native reserve is exactly2048 bytes");

typedef struct feature_frame {
    mc_world_runtime *world;
    mc_java_random *random;
    bool entered;
    bool *mask;
    mc_population_lake_spring_error error;
} feature_frame;

static mc_registry_error empty_registry(void)
{
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static mc_population_lake_spring_error empty_error(void)
{
    const mc_registry_error registry = empty_registry();
    return (mc_population_lake_spring_error){.registry = registry,
                                             .material = {.registry = registry},
                                             .query = {.registry = registry},
                                             .mutation = mc_world_mutation_empty_error()};
}

static bool fail(feature_frame *frame, mc_world_storage_status status, const char *operation)
{
    frame->error.status = status;
    frame->error.operation = operation;
    frame->error.cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool storage_fail(feature_frame *frame, const mc_world_storage_error *cause)
{
    frame->error.status = cause->status;
    frame->error.operation = cause->operation;
    frame->error.cause = *cause;
    return false;
}

static bool registry_fail(feature_frame *frame, const mc_registry_error *cause,
                          const char *operation)
{
    frame->error.registry = *cause;
    frame->error.status = mc_world_runtime_registry_result(frame->world, cause->status, cause,
                                                           operation, &frame->error.cause);
    frame->error.operation = frame->error.cause.operation;
    return false;
}

static bool null_reference(feature_frame *frame, const char *operation)
{
    mc_registry_error cause = empty_registry();
    cause.status = MC_REGISTRY_NULL_REFERENCE;
    cause.message = operation;
    return registry_fail(frame, &cause, operation);
}

static void stage(feature_frame *frame, unsigned int pc)
{
    frame->error.bytecode = pc;
    frame->error.air_bytecode = 0;
}

static bool begin(feature_frame *frame, const void *feature, mc_world_runtime *world,
                  mc_java_random *random, bool *out, bool lake)
{
    *frame = (feature_frame){.world = world, .random = random, .error = empty_error()};
    if (feature == NULL || world == NULL || out == NULL)
        return fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Lake/spring native arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Lake/spring World phase");
    if (mc_world_runtime_call_enter(world, &frame->error.cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &frame->error.cause);
    frame->entered = true;
    const bool client = world->classes->side == MC_WORLD_INFO_CLIENT;
    frame->error.feature_class = lake ? (client ? "dj" : "cd") : (client ? "xo" : "pc");
    frame->error.feature_method = "a";
    return true;
}

static mc_world_storage_status finish(feature_frame *frame, bool completed, bool ordinary,
                                      bool *out, mc_population_lake_spring_error *error)
{
    free(frame->mask);
    if (completed) {
        *out = ordinary;
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

static bool block_get(feature_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                      int32_t *out)
{
    stage(frame, pc);
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_block_get(frame->world, x, y, z, out, &cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &cause);
    return true;
}

/* Complete fd.d/dj.f(III)Z: World ID invocation4, comparison with zero. */
static bool air(feature_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z, bool *out)
{
    stage(frame, pc);
    frame->error.air_bytecode = 4;
    int32_t id;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_block_get(frame->world, x, y, z, &id, &cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &cause);
    *out = id == 0;
    return true;
}

static bool static_id(feature_frame *frame, const char *name, unsigned int load_pc,
                      unsigned int field_pc, int32_t *out)
{
    stage(frame, load_pc);
    const mc_block *block = NULL;
    mc_registry_error cause = empty_registry();
    if (mc_registry_block_static_get(mc_world_runtime_registry(frame->world), name, &block,
                                     &cause) != MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Lake/spring Block static active use");
    stage(frame, field_pc);
    if (block == NULL)
        return null_reference(frame, "Lake/spring Block.bn receiver");
    *out = mc_block_fields_get(block)->bn;
    return true;
}

static bool block_table(feature_frame *frame, unsigned int pc, mc_registry_block_m_view *out)
{
    stage(frame, pc);
    mc_registry_error cause = empty_registry();
    if (mc_registry_block_m_get(mc_world_runtime_registry(frame->world), out, &cause) !=
        MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Lake/spring Block.m active use");
    return true;
}

static bool block_element(feature_frame *frame, unsigned int pc,
                          const mc_registry_block_m_view *array, int32_t id, const mc_block **out)
{
    stage(frame, pc);
    mc_registry_error cause = empty_registry();
    if (mc_registry_block_m_at(array, id, out, &cause) != MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Lake/spring captured Block.m element");
    return true;
}

static bool material_get(feature_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                         mc_material **out)
{
    stage(frame, pc);
    if (mc_world_runtime_material_get_detailed(frame->world, x, y, z, out,
                                               &frame->error.material) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &frame->error.material.cause);
    return true;
}

static bool material_predicate(feature_frame *frame, unsigned int pc, mc_material *material,
                               bool liquid, bool *out)
{
    stage(frame, pc);
    if (material == NULL)
        return null_reference(frame, "Lake Material predicate receiver");
    *out = liquid ? mc_material_d(material) : mc_material_a(material);
    return true;
}

static bool write_block(feature_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                        int32_t id, bool notify)
{
    stage(frame, pc);
    bool ignored;
    const mc_world_storage_status status =
        notify ? mc_world_runtime_block_set(frame->world, x, y, z, id, &ignored,
                                            &frame->error.mutation)
               : mc_world_runtime_block_set_quiet(frame->world, x, y, z, id, &ignored,
                                                  &frame->error.mutation);
    return status == MC_WORLD_STORAGE_OK || storage_fail(frame, &frame->error.mutation.cause);
}

static bool bounded(feature_frame *frame, unsigned int pc, int32_t bound, int32_t *out)
{
    stage(frame, pc);
    if (frame->random == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Lake Random receiver");
    if (!mc_java_random_next_bounded(frame->random, bound, out))
        return fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Lake Random bound");
    return true;
}

static bool draw_double(feature_frame *frame, unsigned int pc, double *out)
{
    stage(frame, pc);
    if (frame->random == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Lake Random receiver");
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Lake strict floating profile");
    *out = mc_java_random_next_double(frame->random);
    return true;
}

/* Explicit source-width, single-operation rounding barriers. */
static double add_f64(double a, double b)
{
    volatile double r = a + b;
    return r;
}
static double sub_f64(double a, double b)
{
    volatile double r = a - b;
    return r;
}
static double mul_f64(double a, double b)
{
    volatile double r = a * b;
    return r;
}
static double div_f64(double a, double b)
{
    volatile double r = a / b;
    return r;
}

static size_t index_of(int32_t x, int32_t z, int32_t y)
{
    /* All three are bounded source loop locals; largest result is2047. */
    return (size_t)((x * 16 + z) * 8 + y);
}

static bool boundary(const bool *mask, int32_t x, int32_t z, int32_t y)
{
    /* Source short-circuit order: center, +X, -X, +Z, -Z, +Y, -Y. */
    return !mask[index_of(x, z, y)] &&
           ((x < 15 && mask[index_of(x + 1, z, y)]) || (x > 0 && mask[index_of(x - 1, z, y)]) ||
            (z < 15 && mask[index_of(x, z + 1, y)]) || (z > 0 && mask[index_of(x, z - 1, y)]) ||
            (y < 7 && mask[index_of(x, z, y + 1)]) || (y > 0 && mask[index_of(x, z, y - 1)]));
}

static bool build_mask(feature_frame *frame)
{
    int32_t count;
    if (!bounded(frame, 41, 4, &count))
        return false;
    count = mc_java_i32_add(count, 4);
    for (int32_t ellipsoid = 0; ellipsoid < count; ellipsoid = mc_java_i32_add(ellipsoid, 1)) {
        double draw;
        if (!draw_double(frame, 59, &draw))
            return false;
        const double width_x = add_f64(mul_f64(draw, 6.0), 3.0);
        if (!draw_double(frame, 73, &draw))
            return false;
        const double height = add_f64(mul_f64(draw, 4.0), 2.0);
        if (!draw_double(frame, 87, &draw))
            return false;
        const double width_z = add_f64(mul_f64(draw, 6.0), 3.0);

        if (!draw_double(frame, 101, &draw))
            return false;
        double middle_x = sub_f64(16.0, width_x);
        middle_x = sub_f64(middle_x, 2.0);
        middle_x = mul_f64(draw, middle_x);
        middle_x = add_f64(middle_x, 1.0);
        middle_x = add_f64(middle_x, div_f64(width_x, 2.0));
        if (!draw_double(frame, 127, &draw))
            return false;
        double middle_y = sub_f64(8.0, height);
        middle_y = sub_f64(middle_y, 4.0);
        middle_y = mul_f64(draw, middle_y);
        middle_y = add_f64(middle_y, 2.0);
        middle_y = add_f64(middle_y, div_f64(height, 2.0));
        if (!draw_double(frame, 155, &draw))
            return false;
        double middle_z = sub_f64(16.0, width_z);
        middle_z = sub_f64(middle_z, 2.0);
        middle_z = mul_f64(draw, middle_z);
        middle_z = add_f64(middle_z, 1.0);
        middle_z = add_f64(middle_z, div_f64(width_z, 2.0));

        for (int32_t x = 1; x < 15; x = mc_java_i32_add(x, 1)) {
            for (int32_t z = 1; z < 15; z = mc_java_i32_add(z, 1)) {
                for (int32_t y = 1; y < 7; y = mc_java_i32_add(y, 1)) {
                    double nx = sub_f64((double)x, middle_x);
                    nx = div_f64(nx, div_f64(width_x, 2.0));
                    double ny = sub_f64((double)y, middle_y);
                    ny = div_f64(ny, div_f64(height, 2.0));
                    double nz = sub_f64((double)z, middle_z);
                    nz = div_f64(nz, div_f64(width_z, 2.0));
                    double sum = mul_f64(nx, nx);
                    const double yy = mul_f64(ny, ny);
                    sum = add_f64(sum, yy);
                    const double zz = mul_f64(nz, nz);
                    sum = add_f64(sum, zz);
                    if (sum < 1.0)
                        frame->mask[index_of(x, z, y)] = true;
                }
            }
        }
    }
    return true;
}

static bool validate_lake(feature_frame *frame, const mc_population_lake_feature *feature,
                          int32_t x, int32_t y, int32_t z, bool *out)
{
    for (int32_t lx = 0; lx < 16; lx = mc_java_i32_add(lx, 1)) {
        for (int32_t lz = 0; lz < 16; lz = mc_java_i32_add(lz, 1)) {
            for (int32_t ly = 0; ly < 8; ly = mc_java_i32_add(ly, 1)) {
                if (!boundary(frame->mask, lx, lz, ly))
                    continue;
                const int32_t wx = mc_java_i32_add(x, lx);
                const int32_t wy = mc_java_i32_add(y, ly);
                const int32_t wz = mc_java_i32_add(z, lz);
                mc_material *material;
                if (!material_get(frame, 568, wx, wy, wz, &material))
                    return false;
                bool predicate;
                if (ly >= 4) {
                    if (!material_predicate(frame, 581, material, true, &predicate))
                        return false;
                    if (predicate) {
                        *out = false; /* Source ordinary return588. */
                        return true;
                    }
                }
                if (ly < 4) {
                    if (!material_predicate(frame, 597, material, false, &predicate))
                        return false;
                    if (!predicate) {
                        int32_t id;
                        if (!block_get(frame, 618, wx, wy, wz, &id))
                            return false;
                        if (id != feature->a) { /* source live getfield622 */
                            *out = false;       /* Source ordinary return629. */
                            return true;
                        }
                    }
                }
            }
        }
    }
    *out = true;
    return true;
}

static bool fill_lake(feature_frame *frame, const mc_population_lake_feature *feature, int32_t x,
                      int32_t y, int32_t z)
{
    for (int32_t lx = 0; lx < 16; lx = mc_java_i32_add(lx, 1)) {
        for (int32_t lz = 0; lz < 16; lz = mc_java_i32_add(lz, 1)) {
            for (int32_t ly = 0; ly < 8; ly = mc_java_i32_add(ly, 1)) {
                if (!frame->mask[index_of(lx, lz, ly)])
                    continue;
                const int32_t id = ly >= 4 ? 0 : feature->a; /* late getfield724 only below4 */
                if (!write_block(frame, 727, mc_java_i32_add(x, lx), mc_java_i32_add(y, ly),
                                 mc_java_i32_add(z, lz), id, false))
                    return false;
            }
        }
    }
    return true;
}

static bool grass_lake(feature_frame *frame, int32_t x, int32_t y, int32_t z)
{
    for (int32_t lx = 0; lx < 16; lx = mc_java_i32_add(lx, 1)) {
        for (int32_t lz = 0; lz < 16; lz = mc_java_i32_add(lz, 1)) {
            for (int32_t ly = 4; ly < 8; ly = mc_java_i32_add(ly, 1)) {
                if (!frame->mask[index_of(lx, lz, ly)])
                    continue;
                const int32_t wx = mc_java_i32_add(x, lx);
                const int32_t wy = mc_java_i32_add(y, ly);
                const int32_t wz = mc_java_i32_add(z, lz);
                int32_t id, dirt;
                if (!block_get(frame, 816, wx, mc_java_i32_sub(wy, 1), wz, &id) ||
                    !static_id(frame, "w", 819, 822, &dirt))
                    return false;
                if (id != dirt)
                    continue;
                stage(frame, 829);
                const mc_world_light_type *sky = NULL;
                mc_world_storage_error cause = {0};
                if (mc_world_runtime_light_type_get(frame->world->classes, MC_LIGHT_SKY, &sky,
                                                    &cause) != MC_WORLD_STORAGE_OK)
                    return storage_fail(frame, &cause);
                stage(frame, 846);
                int32_t light;
                if (mc_world_runtime_saved_light(frame->world, sky, wx, wy, wz, &light,
                                                 &frame->error.query) != MC_WORLD_STORAGE_OK)
                    return storage_fail(frame, &frame->error.query.cause);
                if (light <= 0)
                    continue;
                int32_t grass;
                if (!static_id(frame, "v", 869, 872, &grass) ||
                    !write_block(frame, 875, wx, mc_java_i32_sub(wy, 1), wz, grass, false))
                    return false;
            }
        }
    }
    return true;
}

static bool target_is_lava(feature_frame *frame, const mc_population_lake_feature *feature,
                           bool *out)
{
    mc_registry_block_m_view array;
    if (!block_table(frame, 897, &array))
        return false;
    const int32_t id = feature->a; /* source getfield901, after active use897 */
    const mc_block *block = NULL;
    if (!block_element(frame, 904, &array, id, &block))
        return false;
    stage(frame, 905);
    if (block == NULL)
        return null_reference(frame, "Lake target Block.bA receiver");
    mc_material *actual = mc_block_fields_get(block)->bA;
    stage(frame, 908);
    mc_material *lava = NULL;
    mc_registry_error cause = empty_registry();
    if (mc_registry_material_get(mc_world_runtime_registry(frame->world), MC_MATERIAL_h, &lava,
                                 &cause) != MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Lake Material.h static");
    *out = actual == lava;
    return true;
}

static bool rim_lake(feature_frame *frame, int32_t x, int32_t y, int32_t z)
{
    for (int32_t lx = 0; lx < 16; lx = mc_java_i32_add(lx, 1)) {
        for (int32_t lz = 0; lz < 16; lz = mc_java_i32_add(lz, 1)) {
            for (int32_t ly = 0; ly < 8; ly = mc_java_i32_add(ly, 1)) {
                if (!boundary(frame->mask, lx, lz, ly))
                    continue;
                if (ly >= 4) {
                    int32_t draw;
                    if (!bounded(frame, 1152, 2, &draw))
                        return false;
                    if (draw == 0)
                        continue;
                }
                const int32_t wx = mc_java_i32_add(x, lx);
                const int32_t wy = mc_java_i32_add(y, ly);
                const int32_t wz = mc_java_i32_add(z, lz);
                mc_material *material;
                bool solid;
                if (!material_get(frame, 1173, wx, wy, wz, &material) ||
                    !material_predicate(frame, 1176, material, false, &solid))
                    return false;
                if (!solid)
                    continue;
                int32_t stone;
                if (!static_id(frame, "u", 1197, 1200, &stone) ||
                    !write_block(frame, 1203, wx, wy, wz, stone, false))
                    return false;
            }
        }
    }
    return true;
}

static bool stone(feature_frame *frame, unsigned int query_pc, unsigned int static_pc,
                  unsigned int field_pc, int32_t x, int32_t y, int32_t z, bool *out)
{
    int32_t id, stone_id;
    if (!block_get(frame, query_pc, x, y, z, &id) ||
        !static_id(frame, "u", static_pc, field_pc, &stone_id))
        return false;
    *out = id == stone_id;
    return true;
}

mc_world_storage_status mc_population_lake_feature_init(mc_population_lake_feature *feature,
                                                        int32_t target)
{
    if (feature == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    feature->a = target; /* dj/cd.<init>6 */
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_population_spring_feature_init(mc_population_spring_feature *feature,
                                                          int32_t target)
{
    if (feature == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    feature->a = target; /* xo/pc.<init>6 */
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_population_lake_feature_scale(mc_population_lake_feature *feature,
                                                         double x, double y, double z)
{
    (void)x;
    (void)y;
    (void)z;
    return feature == NULL ? MC_WORLD_STORAGE_INVALID_ARGUMENT : MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_population_spring_feature_scale(mc_population_spring_feature *feature,
                                                           double x, double y, double z)
{
    (void)x;
    (void)y;
    (void)z;
    return feature == NULL ? MC_WORLD_STORAGE_INVALID_ARGUMENT : MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_population_lake_place(const mc_population_lake_feature *feature,
                                                 mc_world_runtime *world, mc_java_random *random,
                                                 int32_t x, int32_t y, int32_t z, bool *out,
                                                 mc_population_lake_spring_error *error)
{
    feature_frame frame;
    if (!begin(&frame, feature, world, random, out, true))
        return finish(&frame, false, false, out, error);
    x = mc_java_i32_sub(x, 8); /* source iinc0 */
    z = mc_java_i32_sub(z, 8); /* source iinc3 */
    while (y > 0) {
        bool is_air;
        if (!air(&frame, 17, x, y, z, &is_air))
            return finish(&frame, false, false, out, error);
        if (!is_air)
            break;
        y = mc_java_i32_sub(y, 1);
    }
    y = mc_java_i32_sub(y, 4); /* source iinc29, including initially nonpositive Y */
    stage(&frame, 35);
    frame.mask = calloc(2048u, sizeof(*frame.mask));
    if (frame.mask == NULL) {
        (void)fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Lake temporary boolean array");
        return finish(&frame, false, false, out, error);
    }
    if (!build_mask(&frame))
        return finish(&frame, false, false, out, error);
    bool valid;
    if (!validate_lake(&frame, feature, x, y, z, &valid))
        return finish(&frame, false, false, out, error);
    if (!valid)
        return finish(&frame, true, false, out, error);
    if (!fill_lake(&frame, feature, x, y, z) || !grass_lake(&frame, x, y, z))
        return finish(&frame, false, false, out, error);
    bool lava;
    if (!target_is_lava(&frame, feature, &lava) || (lava && !rim_lake(&frame, x, y, z)))
        return finish(&frame, false, false, out, error);
    return finish(&frame, true, true, out, error);
}

mc_world_storage_status mc_population_spring_place(const mc_population_spring_feature *feature,
                                                   mc_world_runtime *world, mc_java_random *random,
                                                   int32_t x, int32_t y, int32_t z, bool *out,
                                                   mc_population_lake_spring_error *error)
{
    feature_frame frame;
    if (!begin(&frame, feature, world, random, out, false))
        return finish(&frame, false, false, out, error);
    bool matches;
    if (!stone(&frame, 8, 11, 14, x, mc_java_i32_add(y, 1), z, &matches))
        return finish(&frame, false, false, out, error);
    if (!matches)
        return finish(&frame, true, false, out, error);
    if (!stone(&frame, 30, 33, 36, x, mc_java_i32_sub(y, 1), z, &matches))
        return finish(&frame, false, false, out, error);
    if (!matches)
        return finish(&frame, true, false, out, error);
    int32_t id;
    if (!block_get(&frame, 50, x, y, z, &id))
        return finish(&frame, false, false, out, error);
    if (id != 0) {
        if (!stone(&frame, 62, 65, 68, x, y, z, &matches))
            return finish(&frame, false, false, out, error);
        if (!matches)
            return finish(&frame, true, false, out, error);
    }

    int32_t stone_count = 0, air_count = 0;
    static const unsigned int stone_query[4] = {87, 110, 133, 156};
    static const unsigned int stone_static[4] = {90, 113, 136, 159};
    static const unsigned int stone_field[4] = {93, 116, 139, 162};
    static const unsigned int air_query[4] = {182, 199, 216, 233};
    for (int32_t direction = 0; direction < 4; direction = mc_java_i32_add(direction, 1)) {
        int32_t nx = x, nz = z;
        if (direction == 0)
            nx = mc_java_i32_sub(x, 1);
        if (direction == 1)
            nx = mc_java_i32_add(x, 1);
        if (direction == 2)
            nz = mc_java_i32_sub(z, 1);
        if (direction == 3)
            nz = mc_java_i32_add(z, 1);
        if (!stone(&frame, stone_query[direction], stone_static[direction], stone_field[direction],
                   nx, y, nz, &matches))
            return finish(&frame, false, false, out, error);
        if (matches)
            stone_count = mc_java_i32_add(stone_count, 1);
    }
    for (int32_t direction = 0; direction < 4; direction = mc_java_i32_add(direction, 1)) {
        int32_t nx = x, nz = z;
        if (direction == 0)
            nx = mc_java_i32_sub(x, 1);
        if (direction == 1)
            nx = mc_java_i32_add(x, 1);
        if (direction == 2)
            nz = mc_java_i32_sub(z, 1);
        if (direction == 3)
            nz = mc_java_i32_add(z, 1);
        bool is_air;
        if (!air(&frame, air_query[direction], nx, y, nz, &is_air))
            return finish(&frame, false, false, out, error);
        if (is_air)
            air_count = mc_java_i32_add(air_count, 1);
    }
    if (stone_count == 3 && air_count == 1) {
        const int32_t target = feature->a; /* source getfield261 */
        if (!write_block(&frame, 264, x, y, z, target, true))
            return finish(&frame, false, false, out, error);
        stage(&frame, 270);
        world->fields.a = true;
        mc_registry_block_m_view array;
        if (!block_table(&frame, 273, &array))
            return finish(&frame, false, false, out, error);
        const int32_t selected_id = feature->a; /* source reload277, after active use */
        const mc_block *block = NULL;
        if (!block_element(&frame, 280, &array, selected_id, &block))
            return finish(&frame, false, false, out, error);
        stage(&frame, 288);
        frame.error.mutation = mc_world_mutation_empty_error();
        if (!mc_world_mutation_block_tick(world, block, x, y, z, random, &frame.error.mutation)) {
            (void)storage_fail(&frame, &frame.error.mutation.cause);
            return finish(&frame, false, false, out, error);
        }
        stage(&frame, 293);
        world->fields.a = false;
    }
    return finish(&frame, true, true, out, error); /* source literal return297 */
}
