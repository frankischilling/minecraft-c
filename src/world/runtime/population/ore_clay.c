#include "mc/world/runtime/population/ore_clay.h"

/* Staging paths only. Intended home: src/world/runtime/population/ore_clay.c;
 * integration changes these three includes and the public header include. */
#include "../construction_internal.h"
#include "../mutation/mutation_internal.h"
#include "../runtime_internal.h"
#include "mc/block/registry/runtime.h"

#include <float.h>
#include <stddef.h>
#if !defined(__x86_64__) && !defined(_M_X64)
#error "Ore/clay requires the captured x86-64 strict floating profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Ore/clay requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_MAX_EXP == 128 &&
                   DBL_MAX_EXP == 1024 && sizeof(float) == 4 && sizeof(double) == 8,
               "Ore/clay requires IEEE binary32/binary64 precision");
_Static_assert(FLT_EVAL_METHOD == 0, "Ore/clay requires source-width evaluation");

typedef struct feature_frame {
    mc_world_runtime *world;
    mc_java_random *random;
    bool entered;
    mc_population_ore_clay_error error;
} feature_frame;

static mc_registry_error empty_registry(void) {
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static mc_population_ore_clay_error empty_error(void) {
    const mc_registry_error registry = empty_registry();
    return (mc_population_ore_clay_error){.registry = registry,
                                          .material = {.registry = registry},
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

static bool begin(feature_frame *frame, const void *feature, mc_world_runtime *world,
                  mc_java_random *random, bool *out, bool result_required, bool clay,
                  const char *method) {
    *frame = (feature_frame){.world = world, .random = random, .error = empty_error()};
    if (feature == NULL || world == NULL || (result_required && out == NULL))
        return fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Ore/clay native arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Ore/clay World phase");
    if (mc_world_runtime_call_enter(world, &frame->error.cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &frame->error.cause);
    frame->entered = true;
    const bool client = world->classes->side == MC_WORLD_INFO_CLIENT;
    frame->error.feature_class = clay ? (client ? "ms" : "ib") : (client ? "fl" : "dr");
    frame->error.feature_method = method;
    return true;
}

static mc_world_storage_status finish(feature_frame *frame, bool completed, bool ordinary,
                                      bool *out, mc_population_ore_clay_error *error) {
    if (completed) {
        if (out != NULL)
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

static bool static_id(feature_frame *frame, const char *name, unsigned int load_pc,
                      unsigned int field_pc, int32_t *out) {
    frame->error.bytecode = load_pc;
    const mc_block *block = NULL;
    mc_registry_error cause = empty_registry();
    if (mc_registry_block_static_get(mc_world_runtime_registry(frame->world), name, &block,
                                     &cause) != MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Ore/clay Block static active use");
    frame->error.bytecode = field_pc;
    if (block == NULL) {
        cause.status = MC_REGISTRY_NULL_REFERENCE;
        cause.message = "Ore/clay Block.bn receiver";
        return registry_fail(frame, &cause, cause.message);
    }
    *out = mc_block_fields_get(block)->bn;
    return true;
}

/* Each volatile local is one source arithmetic/narrowing operation. Do not
 * replace these with a ratio, fma, extended expression or libc trigonometry. */
static float float_int(int32_t value) {
    volatile float result = (float)value;
    return result;
}

static float add_f32(float a, float b) {
    volatile float r = a + b;
    return r;
}

static float sub_f32(float a, float b) {
    volatile float r = a - b;
    return r;
}

static float mul_f32(float a, float b) {
    volatile float r = a * b;
    return r;
}

static float div_f32(float a, float b) {
    volatile float r = a / b;
    return r;
}

static double add_f64(double a, double b) {
    volatile double r = a + b;
    return r;
}

static double sub_f64(double a, double b) {
    volatile double r = a - b;
    return r;
}

static double mul_f64(double a, double b) {
    volatile double r = a * b;
    return r;
}

static double div_f64(double a, double b) {
    volatile double r = a / b;
    return r;
}

static bool random_ready(feature_frame *frame, unsigned int pc) {
    frame->error.bytecode = pc;
    if (frame->random == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Ore/clay Random receiver");
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Ore/clay strict floating profile");
    return true;
}

static bool sine(feature_frame *frame, unsigned int pc, float angle, bool cosine, float *out) {
    frame->error.bytecode = pc;
    const mc_math_trig *trig = mc_world_runtime_class_trig(frame->world);
    if (trig == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Ore/clay actual class trig binding");
    *out = cosine ? mc_math_cos(trig, angle) : mc_math_sin(trig, angle);
    return true;
}

static bool water(feature_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    frame->error.bytecode = 6;
    mc_material *actual = NULL;
    if (mc_world_runtime_material_get_detailed(frame->world, x, y, z, &actual,
                                               &frame->error.material) != MC_WORLD_STORAGE_OK) {
        frame->error.registry = frame->error.material.registry;
        return storage_fail(frame, &frame->error.material.cause);
    }
    frame->error.bytecode = 9;
    mc_material *expected = NULL;
    mc_registry_error cause = empty_registry();
    if (mc_registry_material_get(mc_world_runtime_registry(frame->world), MC_MATERIAL_g, &expected,
                                 &cause) != MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Clay Material.g static");
    *out = actual == expected;
    return true;
}

static bool candidate(feature_frame *frame, const int32_t *target, bool clay, int32_t x, int32_t y,
                      int32_t z) {
    frame->error.bytecode = clay ? 487u : 496u;
    int32_t existing;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_block_get(frame->world, x, y, z, &existing, &cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &cause);
    int32_t replaceable;
    if (!static_id(frame, clay ? "F" : "u", clay ? 494u : 499u, clay ? 497u : 502u, &replaceable))
        return false;
    if (existing != replaceable)
        return true;
    /* Actual receiver.a getfield516/511 occurs after the late static comparison. */
    const int32_t replacement = *target;
    frame->error.bytecode = clay ? 514u : 519u;
    bool ignored;
    if (mc_world_runtime_block_set_quiet(frame->world, x, y, z, replacement, &ignored,
                                         &frame->error.mutation) != MC_WORLD_STORAGE_OK) {
        frame->error.registry = frame->error.mutation.registry;
        return storage_fail(frame, &frame->error.mutation.cause);
    }
    return true;
}

static double center(double start, double end, int32_t step, const int32_t *size) {
    double value = sub_f64(end, start);
    value = mul_f64(value, (double)step);
    value = div_f64(value, (double)*size);
    return add_f64(start, value);
}

static double normalized(int32_t coordinate, double middle, double extent) {
    double value = add_f64((double)coordinate, 0.5);
    value = sub_f64(value, middle);
    const double half = div_f64(extent, 2.0);
    return div_f64(value, half);
}

static bool ore_candidates(feature_frame *frame, const int32_t *target, double cx, double cy,
                           double cz, double horizontal, double vertical) {
    /* fl/dr floor invocations276,290,304,318,332,346, in this order. */
    const int32_t low_x = mc_math_floor_double(sub_f64(cx, div_f64(horizontal, 2.0)));
    const int32_t low_y = mc_math_floor_double(sub_f64(cy, div_f64(vertical, 2.0)));
    const int32_t low_z = mc_math_floor_double(sub_f64(cz, div_f64(horizontal, 2.0)));
    const int32_t high_x = mc_math_floor_double(add_f64(cx, div_f64(horizontal, 2.0)));
    const int32_t high_y = mc_math_floor_double(add_f64(cy, div_f64(vertical, 2.0)));
    const int32_t high_z = mc_math_floor_double(add_f64(cz, div_f64(horizontal, 2.0)));
    for (int32_t x = low_x; x <= high_x; x = mc_java_i32_add(x, 1)) {
        const double nx = normalized(x, cx, horizontal);
        if (!(mul_f64(nx, nx) < 1.0))
            continue; /* dcmpg387/ifge388, including NaN. */
        for (int32_t y = low_y; y <= high_y; y = mc_java_i32_add(y, 1)) {
            const double ny = normalized(y, cy, vertical);
            double sum = mul_f64(nx, nx);
            const double yy = mul_f64(ny, ny);
            sum = add_f64(sum, yy);
            if (!(sum < 1.0))
                continue; /* dcmpg433/ifge434. */
            for (int32_t z = low_z; z <= high_z; z = mc_java_i32_add(z, 1)) {
                const double nz = normalized(z, cz, horizontal);
                sum = mul_f64(nx, nx);
                const double yy_again = mul_f64(ny, ny);
                sum = add_f64(sum, yy_again);
                const double zz = mul_f64(nz, nz);
                sum = add_f64(sum, zz);
                if (sum < 1.0 && !candidate(frame, target, false, x, y, z))
                    return false; /* dcmpg485/ifge486 precedes candidate496. */
            }
        }
    }
    return true;
}

static bool clay_candidates(feature_frame *frame, const int32_t *target, double cx, double cy,
                            double cz, double horizontal, double vertical) {
    /* ms/ib has a different floor order and no X/XY pruning. */
    const int32_t low_x = mc_math_floor_double(sub_f64(cx, div_f64(horizontal, 2.0)));
    const int32_t high_x = mc_math_floor_double(add_f64(cx, div_f64(horizontal, 2.0)));
    const int32_t low_y = mc_math_floor_double(sub_f64(cy, div_f64(vertical, 2.0)));
    const int32_t high_y = mc_math_floor_double(add_f64(cy, div_f64(vertical, 2.0)));
    const int32_t low_z = mc_math_floor_double(sub_f64(cz, div_f64(horizontal, 2.0)));
    const int32_t high_z = mc_math_floor_double(add_f64(cz, div_f64(horizontal, 2.0)));
    for (int32_t x = low_x; x <= high_x; x = mc_java_i32_add(x, 1)) {
        for (int32_t y = low_y; y <= high_y; y = mc_java_i32_add(y, 1)) {
            for (int32_t z = low_z; z <= high_z; z = mc_java_i32_add(z, 1)) {
                const double nx = normalized(x, cx, horizontal); /*401..418*/
                const double ny = normalized(y, cy, vertical);   /*420..437*/
                const double nz = normalized(z, cz, horizontal); /*439..456*/
                double sum = mul_f64(nx, nx);
                const double yy = mul_f64(ny, ny);
                sum = add_f64(sum, yy);
                const double zz = mul_f64(nz, nz);
                sum = add_f64(sum, zz);
                if (sum < 1.0 && !candidate(frame, target, true, x, y, z))
                    return false; /* dcmpg476/ifge477 precedes candidate487. */
            }
        }
    }
    return true;
}

static bool geometry(feature_frame *frame, const int32_t *target, const int32_t *size, bool clay,
                     int32_t x, int32_t y, int32_t z) {
    /* The endpoint/segment bodies have the same instruction sequence, with
     * clay PCs17 higher until their distinct floor/loop bodies. */
    const unsigned int offset = clay ? 17u : 0u;
    if (!random_ready(frame, 1u + offset))
        return false;
    const float draw = mc_java_random_next_float(frame->random);
    const float angle = mul_f32(draw, 0x1.921fb6p+1f);
    float origin, wave, scaled;
    double x_start, x_end, z_start, z_end, y_start, y_end;

    origin = float_int(mc_java_i32_add(x, 8));
    if (!sine(frame, 16u + offset, angle, false, &wave))
        return false;
    scaled = mul_f32(wave, float_int(*size));
    scaled = div_f32(scaled, 8.0f);
    x_start = (double)add_f32(origin, scaled);

    origin = float_int(mc_java_i32_add(x, 8));
    if (!sine(frame, 39u + offset, angle, false, &wave))
        return false;
    scaled = mul_f32(wave, float_int(*size));
    scaled = div_f32(scaled, 8.0f);
    x_end = (double)sub_f32(origin, scaled);

    origin = float_int(mc_java_i32_add(z, 8));
    if (!sine(frame, 63u + offset, angle, true, &wave))
        return false;
    scaled = mul_f32(wave, float_int(*size));
    scaled = div_f32(scaled, 8.0f);
    z_start = (double)add_f32(origin, scaled);

    origin = float_int(mc_java_i32_add(z, 8));
    if (!sine(frame, 87u + offset, angle, true, &wave))
        return false;
    scaled = mul_f32(wave, float_int(*size));
    scaled = div_f32(scaled, 8.0f);
    z_end = (double)sub_f32(origin, scaled);

    frame->error.bytecode = 107u + offset;
    int32_t bounded;
    if (!mc_java_random_next_bounded(frame->random, 3, &bounded))
        return fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Ore/clay first nextInt bound");
    y_start = (double)mc_java_i32_add(mc_java_i32_add(y, bounded), 2);
    frame->error.bytecode = 120u + offset;
    if (!mc_java_random_next_bounded(frame->random, 3, &bounded))
        return fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Ore/clay second nextInt bound");
    y_end = (double)mc_java_i32_add(mc_java_i32_add(y, bounded), 2);

    for (int32_t step = 0; step <= *size; step = mc_java_i32_add(step, 1)) {
        /* Separate live size field reads153,173,193 (+17 for clay). */
        const double cx = center(x_start, x_end, step, size);
        const double cy = center(y_start, y_end, step, size);
        const double cz = center(z_start, z_end, step, size);
        if (!random_ready(frame, 202u + offset))
            return false;
        const double radius_draw = mc_java_random_next_double(frame->random);
        double radius = mul_f64(radius_draw, (double)*size);
        radius = div_f64(radius, 16.0);

        float phase = mul_f32(float_int(step), 0x1.921fb6p+1f);
        phase = div_f32(phase, float_int(*size));
        if (!sine(frame, 229u + offset, phase, false, &wave))
            return false;
        const float horizontal_wave = add_f32(wave, 1.0f);
        double horizontal = mul_f64((double)horizontal_wave, radius);
        horizontal = add_f64(horizontal, 1.0);

        phase = mul_f32(float_int(step), 0x1.921fb6p+1f);
        phase = div_f32(phase, float_int(*size));
        if (!sine(frame, 254u + offset, phase, false, &wave))
            return false;
        const float vertical_wave = add_f32(wave, 1.0f);
        double vertical = mul_f64((double)vertical_wave, radius);
        vertical = add_f64(vertical, 1.0);

        if (clay) {
            if (!clay_candidates(frame, target, cx, cy, cz, horizontal, vertical))
                return false;
        } else if (!ore_candidates(frame, target, cx, cy, cz, horizontal, vertical)) {
            return false;
        }
    }
    return true;
}

mc_world_storage_status mc_population_ore_feature_init(mc_population_ore_feature *feature,
                                                       int32_t target, int32_t size) {
    if (feature == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    feature->a = target; /* fl/dr.<init>6 */
    feature->b = size;   /* fl/dr.<init>11 */
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_population_clay_feature_init(mc_population_clay_feature *feature,
                                                        mc_world_runtime *world, int32_t size,
                                                        mc_population_ore_clay_error *error) {
    feature_frame frame;
    if (!begin(&frame, feature, world, NULL, NULL, false, true, "<init>"))
        return finish(&frame, false, false, NULL, error);
    int32_t id;
    if (!static_id(&frame, "aX", 5, 8, &id))
        return finish(&frame, false, false, NULL, error);
    feature->a = id;   /* ms/ib.<init>11 */
    feature->b = size; /* ms/ib.<init>16 */
    return finish(&frame, true, false, NULL, error);
}

mc_world_storage_status mc_population_ore_feature_scale(mc_population_ore_feature *feature,
                                                        double x, double y, double z) {
    (void)x;
    (void)y;
    (void)z;
    return feature == NULL ? MC_WORLD_STORAGE_INVALID_ARGUMENT : MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_population_clay_feature_scale(mc_population_clay_feature *feature,
                                                         double x, double y, double z) {
    (void)x;
    (void)y;
    (void)z;
    return feature == NULL ? MC_WORLD_STORAGE_INVALID_ARGUMENT : MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_population_ore_place(const mc_population_ore_feature *feature,
                                                mc_world_runtime *world, mc_java_random *random,
                                                int32_t x, int32_t y, int32_t z, bool *out,
                                                mc_population_ore_clay_error *error) {
    feature_frame frame;
    if (!begin(&frame, feature, world, random, out, true, false, "a"))
        return finish(&frame, false, false, out, error);
    const bool completed = geometry(&frame, &feature->a, &feature->b, false, x, y, z);
    return finish(&frame, completed, true, out, error);
}

mc_world_storage_status mc_population_clay_place(const mc_population_clay_feature *feature,
                                                 mc_world_runtime *world, mc_java_random *random,
                                                 int32_t x, int32_t y, int32_t z, bool *out,
                                                 mc_population_ore_clay_error *error) {
    feature_frame frame;
    if (!begin(&frame, feature, world, random, out, true, true, "a"))
        return finish(&frame, false, false, out, error);
    bool admitted;
    if (!water(&frame, x, y, z, &admitted))
        return finish(&frame, false, false, out, error);
    if (!admitted)
        return finish(&frame, true, false, out, error); /* literal return16 */
    const bool completed = geometry(&frame, &feature->a, &feature->b, true, x, y, z);
    return finish(&frame, completed, true, out, error);
}
