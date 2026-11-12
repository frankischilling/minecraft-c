#include "mc/world/dimension/world_chunk_manager.h"

#include "../biome/biome_internal.h"

#include <emmintrin.h>
#include <float.h>
#include <limits.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Raw temperature requires the captured x86-64/SSE2 profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Raw temperature requires strict floating transformations"
#endif
_Static_assert(CHAR_BIT == 8 && sizeof(float) == 4 && sizeof(double) == 8 && FLT_RADIX == 2 &&
                   FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 &&
                   DBL_MAX_EXP == 1024 && FLT_EVAL_METHOD == 0,
               "Raw temperature requires source-width binary32/binary64");

static mc_world_provider_status result(mc_world_chunk_manager_error *error,
                                       mc_world_provider_status status, const char *operation,
                                       unsigned int pc, mc_biome_status climate,
                                       mc_registry_status registry) {
    if (error != NULL)
        *error =
            (mc_world_chunk_manager_error){.status = status,
                                           .operation = operation,
                                           .bytecode = pc,
                                           .sampling = {.registry = {.status = registry,
                                                                     .message = operation,
                                                                     .initializer_pc = -1,
                                                                     .block_id = -1,
                                                                     .constructor_pc = -1,
                                                                     .active_initializer_pc = -1,
                                                                     .item_argument = -1},
                                                        .climate_status = climate,
                                                        .sampler_pc = (int32_t)pc}};
    return status;
}

mc_world_provider_status
mc_world_chunk_manager_temperature_raw(mc_world_chunk_manager *receiver, int32_t x, int32_t z,
                                       double *out, mc_world_chunk_manager_error *error) {
    if (out == NULL || receiver == NULL)
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "raw temperature owner/output", 0,
                      MC_BIOME_INVALID_ARGUMENT, MC_REGISTRY_INVALID_ARGUMENT);
    mc_world_provider_status status = mc_world_chunk_manager_retain(receiver);
    if (status != MC_WORLD_PROVIDER_OK)
        return result(error, status, "raw temperature native manager occurrence", 0, MC_BIOME_LIMIT,
                      MC_REGISTRY_LIMIT);
    mc_biome_double_reference old = {0};
    mc_biome_double_buffer fresh = {0};
    mc_world_chunk_manager_fields fields;
    status = mc_world_chunk_manager_fields_get(receiver, &fields);
    if (status != MC_WORLD_PROVIDER_OK) {
        result(error, status, "raw temperature canonical fields", 0, MC_BIOME_INVALID_ARGUMENT,
               MC_REGISTRY_INVALID_ARGUMENT);
        goto leave;
    }
    if (fields.fixed) {
        memcpy(out, &fields.fixed_f, sizeof(*out));
        status = result(error, MC_WORLD_PROVIDER_OK, "hd raw temperature fixed return", 4,
                        MC_BIOME_OK, MC_REGISTRY_OK);
        goto leave;
    }
    mc_biome_manager *manager = fields.noise_backed;
    if (manager == NULL || manager->initialized != 1) {
        status = result(error, MC_WORLD_CHUNK_MANAGER_FAILED,
                        "raw temperature constructed canonical noise", 24,
                        MC_BIOME_INVALID_ARGUMENT, MC_REGISTRY_INVALID_ARGUMENT);
        goto leave;
    }
    mc_biome_status climate = mc_biome_double_capture(&manager->a, &old);
    if (climate != MC_BIOME_OK) {
        status = result(error, MC_WORLD_CHUNK_MANAGER_FAILED,
                        "raw temperature actual a native occurrence", 24, climate,
                        MC_REGISTRY_INVALID_ARGUMENT);
        goto leave;
    }
    /* ug long overload divides scales at5/13 before allocating or clearing a.
     * Do not inspect the unused b/c/d descriptors or their future allocators. */
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        status = result(error, MC_WORLD_PROVIDER_UNSUPPORTED_FP,
                        "raw temperature strict floating environment", 24,
                        MC_BIOME_INVALID_ARGUMENT, MC_REGISTRY_INVALID_ARGUMENT);
        goto leave;
    }
    double *data = (double *)(void *)old.data;
    size_t length = old.length;
    if (data == NULL || length < 1) {
        climate = mc_biome_double_buffer_resize(manager->memory, &fresh, 1);
        if (climate != MC_BIOME_OK) {
            status = result(error, MC_WORLD_CHUNK_MANAGER_FAILED,
                            "raw temperature ug array request35", 24, climate,
                            climate == MC_BIOME_LIMIT ? MC_REGISTRY_LIMIT
                                                      : MC_REGISTRY_ALLOCATION_FAILED);
            goto leave;
        }
        data = fresh.data;
        length = fresh.length;
    }
    const mc_noise_simplex_octaves noise = {manager->temperature_layers, 4};
    const mc_noise_simplex_grid grid = {(double)x, (double)z, 1, 1, (double)0.025f, (double)0.025f};
    if (!mc_noise_simplex_octaves_fill(&noise, &grid, 0.5,
                                       MC_NOISE_SIMPLEX_DEFAULT_DIVISOR_MULTIPLIER, data, length)) {
        status = result(error, MC_WORLD_CHUNK_MANAGER_FAILED, "raw temperature actual ug fill", 24,
                        MC_BIOME_INVALID_ARGUMENT, MC_REGISTRY_INVALID_ARGUMENT);
        goto leave;
    }
    if (fresh.data != NULL) {
        mc_biome_double_buffer_free(&manager->a);
        manager->a = fresh; /* Actual a27, after ug returns; move the sole owner. */
        fresh = (mc_biome_double_buffer){0};
    }
    *out = manager->a.data[0];
    status = result(error, MC_WORLD_PROVIDER_OK, "xv raw temperature return", 36, MC_BIOME_OK,
                    MC_REGISTRY_OK);
leave:
    mc_biome_double_buffer_free(&fresh);
    mc_biome_double_release(&old);
    mc_world_chunk_manager_release(receiver);
    return status;
}
