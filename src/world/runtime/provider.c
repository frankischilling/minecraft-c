#include "provider_internal.h"

#include "../generation/normal_internal.h"
#include "runtime_internal.h"

#include <limits.h>
#include <stdlib.h>

struct mc_world_provider {
    mc_world_runtime *world;
    mc_world_store *store;
    mc_world_dimension_biome *biome;
    mc_biome_manager *manager;
    mc_normal_generator generator;
    int32_t constructor_grid[MC_WORLD_PROVIDER_GRID_WIDTH][MC_WORLD_PROVIDER_GRID_WIDTH];
    mc_chunk_pending pending;
    mc_java_int_set *pending_history;
    mc_java_int_set_limits history_limits;
    int32_t *pending_keys;
    mc_chunk_cache cache;
    mc_chunk_cache_entry *entries;
    mc_chunk_state **insertions;
    mc_chunk_data *fallback_data;
    uint8_t *fallback_blocks;
    size_t max_chunks, direct_bytes, max_direct_bytes;
};

static mc_world_storage_status result(mc_world_storage_error *error, mc_world_storage_status status,
                                      const char *operation)
{
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

/* No request changes accounting until its allocation succeeds. Zero-length
 * arrays are represented by NULL and consume no direct bytes. */
static void *request(mc_world_provider *provider, size_t count, size_t width,
                     mc_world_storage_status *status)
{
    *status = MC_WORLD_STORAGE_OK;
    if (count == 0)
        return NULL;
    if (width > SIZE_MAX / count) {
        *status = MC_WORLD_STORAGE_LIMIT;
        return NULL;
    }
    size_t bytes = count * width;
    if (bytes > provider->max_direct_bytes - provider->direct_bytes) {
        *status = MC_WORLD_STORAGE_LIMIT;
        return NULL;
    }
    void *allocation = calloc(count, width);
    if (allocation == NULL) {
        *status = MC_WORLD_STORAGE_ALLOCATION_FAILED;
        return NULL;
    }
    provider->direct_bytes += bytes;
    return allocation;
}

static mc_world_storage_status profile_status(mc_java_random_profile_status status)
{
    switch (status) {
        case MC_JAVA_RANDOM_PROFILE_OK:
            return MC_WORLD_STORAGE_OK;
        case MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        case MC_JAVA_RANDOM_PROFILE_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_JAVA_RANDOM_PROFILE_PLATFORM_FAILURE:
        case MC_JAVA_RANDOM_PROFILE_RANGE:
        case MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP:
        case MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status generator_status(mc_normal_generator_status status,
                                                mc_java_random_profile_status cause)
{
    switch (status) {
        case MC_NORMAL_GENERATOR_OK:
            return MC_WORLD_STORAGE_OK;
        case MC_NORMAL_GENERATOR_INVALID_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        case MC_NORMAL_GENERATOR_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_NORMAL_GENERATOR_ALLOCATION_FAILED:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_NORMAL_GENERATOR_STATE_ERROR:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        case MC_NORMAL_GENERATOR_PROFILE_FAILED:
            return profile_status(cause);
    }
    return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

void mc_world_provider_free(mc_world_provider *provider)
{
    if (provider == NULL)
        return;
    if (provider->fallback_data != NULL &&
        provider->world->fallback.data == provider->fallback_data)
        provider->world->fallback.data = NULL;
    free(provider->fallback_blocks);
    free(provider->fallback_data);
    free(provider->insertions);
    free(provider->entries);
    free(provider->pending_keys);
    mc_java_int_set_free(provider->pending_history);
    mc_normal_generator_free(&provider->generator);
    mc_world_dimension_biome_release(provider->biome);
    free(provider);
}

mc_world_storage_status mc_world_provider_create(mc_world_runtime *runtime,
                                                 mc_world_dimension *dimension,
                                                 const mc_math_trig *trig,
                                                 mc_java_random_profile *profile, size_t max_chunks,
                                                 size_t max_direct_bytes, mc_world_provider **out,
                                                 mc_java_random_profile_status *profile_cause,
                                                 mc_world_storage_error *error)
{
    if (runtime == NULL || dimension == NULL || trig == NULL || profile == NULL || out == NULL ||
        profile_cause == NULL || max_chunks > (size_t)INT32_MAX)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "provider arguments");
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME ||
        runtime->construction.phase != MC_WORLD_RUNTIME_CONSTRUCTING || runtime->classes == NULL ||
        runtime->fields.w == NULL || runtime->fields.x == NULL || runtime->fields.t != dimension ||
        runtime->fields.v != NULL || runtime->construction.contexts_bound ||
        runtime->construction.cache_registered || runtime->fallback.data != NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "provider World construction state");
    if (profile != mc_world_runtime_random_profile(runtime) ||
        trig != mc_world_runtime_class_trig(runtime))
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "provider class bindings");

    mc_world_info_state info_state;
    if (mc_world_info_state_get(runtime->fields.x, &info_state) != MC_WORLD_INFO_OK ||
        info_state.phase != MC_WORLD_INFO_READY)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "provider WorldInfo is not ready");
    mc_world_dimension_state dimension_state;
    mc_world_dimension_fields dimension_fields;
    if (mc_world_dimension_state_get(dimension, &dimension_state) != MC_WORLD_DIMENSION_OK ||
        mc_world_dimension_fields_get(dimension, &dimension_fields) != MC_WORLD_DIMENSION_OK)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "provider Dimension state");
    if (dimension_state.kind != MC_WORLD_DIMENSION_OVERWORLD)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "provider Dimension kind");
    if (dimension_state.phase != MC_WORLD_DIMENSION_READY || dimension_state.bind_pc != 13u ||
        dimension_fields.a != runtime || dimension_fields.b == NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "provider Dimension binding");
    mc_world_dimension_biome_fields biome_fields;
    if (mc_world_dimension_biome_fields_get(dimension_fields.b, &biome_fields) !=
            MC_WORLD_DIMENSION_OK ||
        biome_fields.fixed || biome_fields.normal == NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "provider normal biome owner");
    if (sizeof(mc_world_provider) > max_direct_bytes)
        return result(error, MC_WORLD_STORAGE_LIMIT, "provider direct owner limit");

    mc_world_provider *provider = calloc(1, sizeof(*provider));
    if (provider == NULL)
        return result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "provider owner allocation");
    *provider = (mc_world_provider){.world = dimension_fields.a,
                                    .store = runtime->fields.w,
                                    .max_chunks = max_chunks,
                                    .direct_bytes = sizeof(*provider),
                                    .max_direct_bytes = max_direct_bytes};
    mc_world_dimension_status biome_status = mc_world_dimension_biome_retain(dimension_fields.b);
    if (biome_status != MC_WORLD_DIMENSION_OK) {
        mc_world_provider_free(provider);
        return result(error,
                      biome_status == MC_WORLD_DIMENSION_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                               : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "provider biome retain");
    }
    provider->biome = dimension_fields.b;
    provider->manager = biome_fields.normal;

    /* xa/os b(): capture a for the World argument, then reload a for its actual
     * seed call at PC12, BEFORE the provider constructor at PC15. The outer
     * caller has already reached the real store's loader factory. */
    mc_level_info info;
    mc_world_storage_status status =
        mc_world_runtime_read_info(mc_world_dimension_world(dimension), &info, error);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_provider_free(provider);
        return status;
    }
    mc_java_random_profile_status cause = MC_JAVA_RANDOM_PROFILE_OK;
    mc_normal_generator_status normal_status = mc_normal_generator_construct_provider(
        &provider->generator, info.seed, provider->manager, trig, profile, &cause);
    if (provider->generator.phase != MC_NORMAL_GENERATOR_PHASE_RAW)
        *profile_cause = cause;
    if (normal_status != MC_NORMAL_GENERATOR_OK) {
        status = generator_status(normal_status, cause);
        mc_world_provider_free(provider);
        return result(error, status, "normal provider constructor");
    }

    /* ok/jb: pending HashSet, then cache map, insertion list, blank chunk.
     * The pre-reserved fixed generator/grid storage and these bounded native
     * arrays do not claim Java object/array allocation failure equivalence. */
    provider->history_limits = mc_java_int_set_default_limits();
    if (provider->history_limits.max_entries > max_chunks)
        provider->history_limits.max_entries = max_chunks;
    mc_java_int_set_status set_status =
        mc_java_int_set_create(&provider->history_limits, &provider->pending_history);
    if (set_status != MC_JAVA_INT_SET_OK) {
        status = set_status == MC_JAVA_INT_SET_ALLOCATION_FAILED
                     ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                 : set_status == MC_JAVA_INT_SET_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                       : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        mc_world_provider_free(provider);
        return result(error, status, "provider pending history allocation");
    }
    const char *operation = "provider pending keys allocation";
    provider->pending_keys =
        request(provider, max_chunks, sizeof(*provider->pending_keys), &status);
    if (status != MC_WORLD_STORAGE_OK)
        goto fail;
    if (!mc_chunk_pending_init_ordered(&provider->pending, provider->pending_keys, max_chunks,
                                       provider->pending_history)) {
        status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        operation = "provider pending initialization";
        goto fail;
    }
    operation = "provider cache entries allocation";
    provider->entries = request(provider, max_chunks, sizeof(*provider->entries), &status);
    if (status != MC_WORLD_STORAGE_OK)
        goto fail;
    operation = "provider insertion list allocation";
    provider->insertions = request(provider, max_chunks, sizeof(*provider->insertions), &status);
    if (status != MC_WORLD_STORAGE_OK)
        goto fail;
    if (!mc_chunk_cache_init(&provider->cache, provider->entries, max_chunks, provider->insertions,
                             max_chunks)) {
        status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        operation = "provider cache initialization";
        goto fail;
    }
    operation = "provider fallback data allocation";
    provider->fallback_data = request(provider, 1u, sizeof(*provider->fallback_data), &status);
    if (status != MC_WORLD_STORAGE_OK)
        goto fail;
    operation = "provider fallback blocks allocation";
    provider->fallback_blocks =
        request(provider, MC_CHUNK_BLOCKS, sizeof(*provider->fallback_blocks), &status);
    if (status != MC_WORLD_STORAGE_OK)
        goto fail;
    if (!mc_chunk_data_init(provider->fallback_data, provider->fallback_blocks, MC_CHUNK_BLOCKS)) {
        status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        operation = "provider fallback initialization";
        goto fail;
    }

    /* The sole fallback identity is embedded in the SAME runtime. Its actual
     * zero blocks and data are provider-owned; the binder must preserve them.
     * Nothing fallible follows this in-place construction/publication. */
    runtime->fallback = (mc_chunk_state){
        .data = provider->fallback_data, .x = 0, .z = 0, .blank = true, .never_save = true};
    *out = provider;
    return result(error, MC_WORLD_STORAGE_OK, NULL);

fail:
    mc_world_provider_free(provider);
    return result(error, status, operation);
}

mc_world_storage_status mc_world_provider_bind_fields(mc_world_provider *provider,
                                                      mc_world_provider_fields *out,
                                                      mc_world_storage_error *error)
{
    if (provider == NULL || out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "provider field arguments");
    *out = (mc_world_provider_fields){.world = provider->world,
                                      .store = provider->store,
                                      .biome = provider->biome,
                                      .manager = provider->manager,
                                      .generator = &provider->generator,
                                      .cache = &provider->cache,
                                      .pending = &provider->pending,
                                      .fallback = &provider->world->fallback,
                                      .constructor_grid = provider->constructor_grid,
                                      .max_chunks = provider->max_chunks,
                                      .direct_allocation_bytes = provider->direct_bytes,
                                      .max_direct_allocation_bytes = provider->max_direct_bytes,
                                      .pending_history_limits = provider->history_limits};
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_provider_maintenance(mc_world_provider *provider, bool *out,
                                                      mc_world_storage_error *error)
{
    if (provider == NULL || out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "provider maintenance arguments");
    *out = false;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}
