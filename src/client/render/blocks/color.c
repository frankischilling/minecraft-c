#include "internal.h"

#include "../../../block/registry/leaf_color_internal.h"
#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/tall_grass_color_internal.h"

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
    if (mc_client_colorizer_read_ready(c->graphics, MC_CLIENT_COLORIZER_FOLIAGE,
                                       pine ? MC_CLIENT_COLORIZER_PINE : MC_CLIENT_COLORIZER_BIRCH,
                                       0, 0, out, &c->colorizer_error) != MC_CLIENT_COLORIZER_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->colorizer_error.operation,
                          c->source_pc, c->colorizer_error.native_policy);
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
    if (mc_client_colorizer_read_ready(c->graphics, kind, MC_CLIENT_COLORIZER_SAMPLE, temperature,
                                       rainfall, out,
                                       &c->colorizer_error) != MC_CLIENT_COLORIZER_OK) {
        const mc_client_colorizer_error *e = &c->colorizer_error;
        c->failure.child = e->site;
        return mc_cv_fail(c,
                          e->array.status == MC_CLIENT_COLOR_ARRAY_NULL_REFERENCE
                              ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                              : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          e->operation, (unsigned int)e->site.pc, e->native_policy);
    }
    mc_cv_pc(c, 36);
    mc_cv_pop(c);
    return true;
}

static bool leaf_metadata(void *c, int32_t x, int32_t y, int32_t z, unsigned int pc, int32_t *out) {
    return mc_cv_metadata(c, x, y, z, pc, out);
}

static bool leaf_climate(void *c, int32_t x, int32_t z, unsigned int manager, unsigned int sample) {
    return climate(c, x, z, manager, sample);
}

static bool leaf_channel(void *c, bool rain, unsigned int manager, unsigned int field,
                         unsigned int load, double *out) {
    return channel(c, rain, manager, field, load, out);
}

static bool leaf_constant(void *c, bool pine, unsigned int pc, int32_t *out) {
    return foliage_constant(c, pine, pc, out);
}

static bool leaf_lookup(void *c, double temperature, double rainfall, unsigned int pc,
                        int32_t *out) {
    return lookup(c, MC_CLIENT_COLORIZER_FOLIAGE, temperature, rainfall, pc, out);
}

static void leaf_pc(void *c, unsigned int pc) {
    mc_cv_pc(c, pc);
}

static bool tall_lookup(void *c, double temperature, double rainfall, unsigned int pc,
                        int32_t *out) {
    return lookup(c, MC_CLIENT_COLORIZER_GRASS, temperature, rainfall, pc, out);
}

static const mc_block_world_color_ops leaf_ops = {leaf_metadata, leaf_climate, leaf_channel,
                                                  leaf_constant, leaf_lookup,  leaf_pc};
static const mc_block_world_color_ops tall_ops = {leaf_metadata, leaf_climate, leaf_channel,
                                                  NULL,          tall_lookup,  leaf_pc};

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
    } else if (block->kind == MC_BLOCK_LEAVES) {
        if (!mc_leaf_world_color(&leaf_ops, c, x, y, z, &value))
            return false;
    } else if (block->kind == MC_BLOCK_TALL_GRASS) {
        if (!mc_tall_grass_world_color(&tall_ops, c, x, y, z, &value))
            return false;
    } else {
        double temperature, rainfall;
        if (!climate(c, x, z, 1, 11) || !channel(c, false, 16, 21, 25, &temperature) ||
            !channel(c, true, 29, 34, 38, &rainfall) ||
            !lookup(c, MC_CLIENT_COLORIZER_GRASS, temperature, rainfall, 45, &value))
            return false;
        mc_cv_pc(c, 48);
    }
    *out = value;
    mc_cv_pop(c);
    return true;
}
