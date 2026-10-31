#include "internal.h"

#include "../../../block/registry/registry_internal.h"

#include <emmintrin.h>
#include <float.h>
#include <string.h>

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Block color source arithmetic requires strict floating transformations"
#endif
_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "Block colors require source-width binary64 evaluation");
_Static_assert(MC_BLOCK_KIND_COUNT == 69, "audit source Block color descendants");

/* These calls deliberately do not share a captured manager. The original
 * Block methods call xp.a() independently before sampling and each field read.
 * A sampling resource callback may rebind the actual World Dimension. */
static bool climate(mc_cv_context *c, int32_t x, int32_t z, unsigned int manager_pc,
                    unsigned int sample_pc) {
    mc_world_chunk_manager *manager = NULL;
    if (!mc_cv_manager(c, manager_pc, &manager))
        return false;
    mc_cv_pc(c, sample_pc);
    if (manager == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "actual Block color cached manager receiver", sample_pc, false);
    mc_biome_reference_array *ignored = NULL;
    if (mc_world_chunk_manager_cached(manager, x, z, 1, 1, &ignored, &c->climate_error) ==
        MC_WORLD_PROVIDER_OK)
        return true;
    const mc_world_chunk_manager_error *e = &c->climate_error;
    c->failure.registry = e->sampling.registry;
    c->failure.child = (mc_client_graphics_site){"xv", "a", "(IIII)[Lkd;", (int32_t)e->bytecode};
    return mc_cv_fail(c,
                      e->status == MC_WORLD_PROVIDER_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                      : e->status == MC_WORLD_PROVIDER_ALLOCATION_FAILED
                          ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                      : e->sampling.registry.status == MC_REGISTRY_NULL_REFERENCE
                          ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                          : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                      e->operation, sample_pc,
                      e->sampling.sampler_pc < 0 ||
                          (e->sampling.registry.status != MC_REGISTRY_NULL_REFERENCE &&
                           e->sampling.registry.status != MC_REGISTRY_INDEX_OUT_OF_RANGE));
}

static bool channel(mc_cv_context *c, bool rainfall, unsigned int manager_pc, unsigned int field_pc,
                    unsigned int load_pc, double *out) {
    mc_world_chunk_manager *manager = NULL;
    if (!mc_cv_manager(c, manager_pc, &manager))
        return false;
    if (manager == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "actual Block color climate field receiver", field_pc, false);
    mc_world_chunk_manager_fields fields;
    if (mc_world_chunk_manager_fields_get(manager, &fields) != MC_WORLD_PROVIDER_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          "actual Block color climate native fields", field_pc, true);
    const mc_biome_double_buffer *array = rainfall ? fields.b : fields.a;
    if (array == NULL || array->data == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "actual Block color climate array", load_pc, false);
    if (array->length == 0) {
        c->failure.registry = (mc_registry_error){.status = MC_REGISTRY_INDEX_OUT_OF_RANGE,
                                                  .message = "Block color climate array index0"};
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          "actual Block color climate array index0", load_pc, false);
    }
    memcpy(out, &array->data[0], sizeof(*out));
    mc_cv_pc(c, load_pc);
    return true;
}

static bool colorizer_active(mc_cv_context *c, mc_client_colorizer_kind kind,
                             unsigned int caller_pc) {
    mc_cv_pc(c, caller_pc);
    if (mc_client_colorizer_initialize(c->graphics, kind, NULL, &c->colorizer_error) ==
        MC_CLIENT_COLORIZER_OK)
        return true;
    const mc_client_colorizer_error *e = &c->colorizer_error;
    c->failure.child = e->site;
    return mc_cv_fail(c,
                      e->status == MC_CLIENT_COLORIZER_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                      : e->status == MC_CLIENT_COLORIZER_BUSY
                          ? MC_CLIENT_RENDER_GLOBAL_BUSY
                          : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                      e->operation, caller_pc, e->native_policy);
}

static bool foliage_constant(mc_cv_context *c, bool pine, unsigned int caller_pc, int32_t *out) {
    if (!colorizer_active(c, MC_CLIENT_COLORIZER_FOLIAGE, caller_pc) ||
        !mc_cv_push_source(c, "jh", pine ? "a" : "b", "()I"))
        return false;
    *out = pine ? 6396257 : 8431445;
    mc_cv_pc(c, 2);
    mc_cv_pop(c);
    return true;
}

static bool lookup(mc_cv_context *c, mc_client_colorizer_kind kind, double temperature,
                   double rainfall, unsigned int caller_pc, int32_t *out) {
    if (!colorizer_active(c, kind, caller_pc) ||
        !mc_cv_push_source(c, kind == MC_CLIENT_COLORIZER_GRASS ? "ia" : "jh", "a", "(DD)I") ||
        !mc_cv_fp(c, 2))
        return false;
    /* Source binary64 multiply2, subtract6/multiply10 and
     * subtract16/multiply20; each conversion is Java d2i. */
    rainfall = _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(rainfall), _mm_set_sd(temperature)));
    double u = _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(1.0), _mm_set_sd(temperature)));
    u = _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(u), _mm_set_sd(255.0)));
    const int32_t column = mc_java_double_to_i32(u);
    double v = _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(1.0), _mm_set_sd(rainfall)));
    v = _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(v), _mm_set_sd(255.0)));
    const int32_t row = mc_java_double_to_i32(v);
    const int32_t index = mc_java_i32_shl(row, 8) | column;
    mc_client_colorizer_state colorizer;
    if (!mc_client_colorizer_inspect(c->graphics, kind, &colorizer))
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          "actual colorizer native static view", 24, true);
    const mc_client_graphics_site site = {kind == MC_CLIENT_COLORIZER_GRASS ? "ia" : "jh", "a",
                                          "(DD)I", 35};
    mc_client_color_array_state array;
    if (colorizer.a == NULL || !mc_client_color_array_inspect(colorizer.a, &array) ||
        array.phase != MC_CLIENT_COLOR_ARRAY_READY || index < 0 || (size_t)index >= array.length) {
        const bool null_array = colorizer.a == NULL;
        const bool native_array =
            !null_array && (!mc_client_color_array_inspect(colorizer.a, &array) ||
                            array.phase != MC_CLIENT_COLOR_ARRAY_READY);
        c->colorizer_error = (mc_client_colorizer_error){
            .status = MC_CLIENT_COLORIZER_ARRAY_FAILED,
            .operation = null_array     ? "actual colorizer null static int[]"
                         : native_array ? "actual colorizer native array phase"
                                        : "actual colorizer int[] index",
            .site = site,
            .native_policy = native_array,
            .array = {.status = null_array     ? MC_CLIENT_COLOR_ARRAY_NULL_REFERENCE
                                : native_array ? MC_CLIENT_COLOR_ARRAY_INVALID_PHASE
                                               : MC_CLIENT_COLOR_ARRAY_INDEX_OUT_OF_RANGE,
                      .site = site,
                      .native_policy = native_array}};
        c->failure.child = site;
        return mc_cv_fail(c,
                          null_array ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                     : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          c->colorizer_error.operation, 35, native_array);
    }
    /* This is the actual Java int, including its high byte. RGB extraction is
     * the later CV caller's operation. No color-plane copy or RGB mask. */
    memcpy(out, &array.bits[(size_t)index], sizeof(*out));
    mc_cv_pc(c, 36);
    mc_cv_pop(c);
    return true;
}

bool mc_cv_block_color(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                       unsigned int caller_pc, int32_t *out) {
    const mc_block_fields *fields = NULL;
    mc_cv_pc(c, caller_pc);
    if (!mc_cv_fields(c, block, caller_pc, &fields))
        return false;
    (void)fields;
    if (block->classes != c->registry)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                          "Block color native Registry identity", caller_pc, true);
    const char *source =
        block->kind == MC_BLOCK_GRASS                                                  ? "wp"
        : block->kind == MC_BLOCK_LEAVES                                               ? "bk"
        : block->kind == MC_BLOCK_TALL_GRASS                                           ? "ru"
        : block->kind == MC_BLOCK_REDSTONE                                             ? "sm"
        : block->kind == MC_BLOCK_FLOWING_FLUID || block->kind == MC_BLOCK_STILL_FLUID ? "rp"
                                                                                       : "uu";
    if (!mc_cv_push_source(c, source, "b", "(Lxp;III)I"))
        return false;
    int32_t value;
    if (block->kind != MC_BLOCK_GRASS && block->kind != MC_BLOCK_LEAVES &&
        block->kind != MC_BLOCK_TALL_GRASS) {
        value = block->kind == MC_BLOCK_REDSTONE ? 8388608 : 16777215;
        mc_cv_pc(c, 2);
    } else {
        int32_t metadata = 0;
        if (block->kind != MC_BLOCK_GRASS && !mc_cv_metadata(c, x, y, z, 5, &metadata))
            return false;
        if (block->kind == MC_BLOCK_LEAVES && (metadata & 1) != 0) {
            if (!foliage_constant(c, true, 20, &value))
                return false;
            mc_cv_pc(c, 23);
        } else if (block->kind == MC_BLOCK_LEAVES && (metadata & 2) != 0) {
            if (!foliage_constant(c, false, 32, &value))
                return false;
            mc_cv_pc(c, 35);
        } else if (block->kind == MC_BLOCK_TALL_GRASS && metadata == 0) {
            value = 16777215;
            mc_cv_pc(c, 19);
        } else {
            const bool grass = block->kind == MC_BLOCK_GRASS;
            const bool tall = block->kind == MC_BLOCK_TALL_GRASS;
            if (tall) {
                int64_t hash = mc_java_i32_add(
                    mc_java_i32_add(mc_java_i32_mul(x, 3129871), mc_java_i32_mul(z, 6129781)), y);
                hash = mc_java_i64_add(mc_java_i64_mul(mc_java_i64_mul(hash, hash), 42317861),
                                       mc_java_i64_mul(hash, 11));
                x = mc_java_i32_narrow(mc_java_i64_add(x, mc_java_i64_shr(hash, 14) & 31));
                /* RU also offsets local Y at79. It has no source effect and
                 * is not an operand of any subsequent reached operation. */
                z = mc_java_i32_narrow(mc_java_i64_add(z, mc_java_i64_shr(hash, 24) & 31));
            }
            double temperature, rainfall;
            if (!climate(c, x, z,
                         grass  ? 1u
                         : tall ? 98u
                                : 37u,
                         grass  ? 11u
                         : tall ? 108u
                                : 47u) ||
                !channel(c, false,
                         grass  ? 16u
                         : tall ? 113u
                                : 52u,
                         grass  ? 21u
                         : tall ? 118u
                                : 57u,
                         grass  ? 25u
                         : tall ? 122u
                                : 61u,
                         &temperature) ||
                !channel(c, true,
                         grass  ? 29u
                         : tall ? 126u
                                : 65u,
                         grass  ? 34u
                         : tall ? 131u
                                : 70u,
                         grass  ? 38u
                         : tall ? 135u
                                : 74u,
                         &rainfall) ||
                !lookup(c,
                        block->kind == MC_BLOCK_LEAVES ? MC_CLIENT_COLORIZER_FOLIAGE
                                                       : MC_CLIENT_COLORIZER_GRASS,
                        temperature, rainfall,
                        grass  ? 45u
                        : tall ? 142u
                               : 81u,
                        &value))
                return false;
            mc_cv_pc(c, grass ? 48u : tall ? 145u : 84u);
        }
    }
    *out = value;
    mc_cv_pop(c);
    return true;
}
