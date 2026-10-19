#include "mc/world/runtime/runtime.h"

#include <stdlib.h>

struct mc_world_runtime {
    mc_world_runtime_options options;
    mc_world_runtime_state live;
    mc_chunk_state fallback;
    mc_chunk_references references;
    mc_chunk_acquire_context acquisition;
    mc_chunk_lifecycle_context lifecycle;
    mc_world_save_context saving;
    mc_world_store_loader loader;
    mc_world_storage_error last_error;
    mc_java_random_profile_status last_random_status;
    size_t active_calls, local_leases;
    size_t persistent_bindings;
};

static mc_world_storage_status runtime_error(mc_world_storage_error *error,
                                             mc_world_storage_status status, const char *operation)
{
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_java_random_profile_status random_result(mc_world_runtime *runtime,
                                                   mc_java_random_profile_status status)
{
    runtime->last_random_status = status;
    if (status != MC_JAVA_RANDOM_PROFILE_OK) {
        mc_world_storage_status mapped =
            status == MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT ? MC_WORLD_STORAGE_INVALID_ARGUMENT
            : status == MC_JAVA_RANDOM_PROFILE_LIMIT          ? MC_WORLD_STORAGE_LIMIT
            : status == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED
                ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        runtime_error(&runtime->last_error, mapped, "runtime default random");
    }
    return status;
}

static bool reference_retain(void *owner, mc_chunk_state *chunk)
{
    mc_world_runtime *runtime = owner;
    if (runtime->local_leases == SIZE_MAX)
        return false;
    if (chunk != &runtime->fallback &&
        mc_world_store_lease_chunk(runtime->options.store, chunk, &runtime->last_error) !=
            MC_WORLD_STORAGE_OK)
        return false;
    ++runtime->local_leases;
    return true;
}

static void reference_release(void *owner, mc_chunk_state *chunk)
{
    mc_world_runtime *runtime = owner;
    if (chunk != &runtime->fallback)
        (void)mc_world_store_release_chunk(runtime->options.store, chunk, NULL);
    --runtime->local_leases;
}

static mc_chunk_load_result load(void *owner, const mc_chunk_acquire_context *context, int32_t x,
                                 int32_t z, mc_chunk_state **out)
{
    mc_world_runtime *runtime = owner;
    mc_chunk_load_result result =
        mc_world_store_load_callback(&runtime->loader, context, x, z, out);
    runtime->last_error = runtime->loader.last_error;
    if (result == MC_CHUNK_LOAD_MISS && runtime->options.generator == NULL)
        return MC_CHUNK_LOAD_FATAL;
    return result;
}

static bool generate(void *owner, const mc_chunk_acquire_context *context, int32_t x, int32_t z,
                     mc_chunk_state **out)
{
    (void)context;
    mc_world_runtime *runtime = owner;
    mc_world_store_reservation *reservation = NULL;
    if (mc_world_store_reserve_chunk(runtime->options.store, runtime->options.dimension,
                                     &reservation, &runtime->last_error) != MC_WORLD_STORAGE_OK)
        return false;
    mc_normal_chunk *staging = calloc(1, sizeof(*staging));
    uint8_t *blocks = malloc(MC_CHUNK_BLOCKS);
    bool success = false;
    if (staging == NULL || blocks == NULL) {
        runtime_error(&runtime->last_error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                      "generation staging");
    } else {
        mc_normal_generator_status status = mc_normal_generator_generate(
            runtime->options.generator, x, z, runtime->options.generation, staging, blocks,
            MC_CHUNK_BLOCKS);
        if (status == MC_NORMAL_GENERATOR_OK) {
            success =
                mc_world_store_retain_reserved(runtime->options.store, reservation, &staging->chunk,
                                               out, &runtime->last_error) == MC_WORLD_STORAGE_OK;
        } else {
            mc_world_storage_status mapped =
                status == MC_NORMAL_GENERATOR_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                : status == MC_NORMAL_GENERATOR_LIMIT           ? MC_WORLD_STORAGE_LIMIT
                : status == MC_NORMAL_GENERATOR_INVALID_ARGUMENT
                    ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                    : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
            runtime_error(&runtime->last_error, mapped, mc_normal_generator_status_string(status));
        }
    }
    if (!success)
        (void)mc_world_store_cancel_reservation(runtime->options.store, reservation, NULL);
    free(blocks);
    free(staging);
    return success;
}

static bool register_tiles(void *owner, const mc_chunk_acquire_context *context,
                           mc_chunk_state *chunk)
{
    (void)owner;
    (void)context;
    (void)chunk;
    return true;
}

static bool register_entities(void *owner, const mc_chunk_acquire_context *context,
                              mc_chunk_state *chunk, size_t slice)
{
    (void)owner;
    (void)context;
    (void)chunk;
    (void)slice;
    return true;
}

static bool populate(void *owner, const mc_chunk_acquire_context *context, int32_t x, int32_t z)
{
    mc_world_runtime *runtime = owner;
    return runtime->options.populate(runtime->options.provider, context, x, z);
}

static mc_chunk_callback_result save_main(void *owner, const mc_chunk_lifecycle_context *context,
                                          mc_chunk_state *chunk)
{
    mc_world_runtime *runtime = owner;
    uint32_t growth = 0;
    uint32_t timestamp = runtime->options.region_timestamp(runtime->options.clock);
    mc_world_storage_status status =
        mc_world_store_write_runtime_chunk(runtime->options.store, chunk, *context->world_time,
                                           timestamp, &growth, &runtime->last_error);
    return status == MC_WORLD_STORAGE_OK ? MC_CHUNK_CALLBACK_OK : MC_CHUNK_CALLBACK_FATAL;
}

static mc_chunk_callback_result save_extra(void *owner, const mc_chunk_lifecycle_context *context,
                                           mc_chunk_state *chunk)
{
    (void)owner;
    (void)context;
    (void)chunk;
    return MC_CHUNK_CALLBACK_OK;
}

static mc_chunk_callback_result loader_noop(void *owner, const mc_chunk_lifecycle_context *context)
{
    (void)owner;
    (void)context;
    return MC_CHUNK_CALLBACK_OK;
}

static mc_chunk_callback_result
invalidate_tiles(void *owner, const mc_chunk_lifecycle_context *context, mc_chunk_state *chunk)
{
    (void)owner;
    (void)context;
    (void)chunk;
    return MC_CHUNK_CALLBACK_OK;
}

static mc_chunk_callback_result defer_entities(void *owner,
                                               const mc_chunk_lifecycle_context *context,
                                               mc_chunk_state *chunk, size_t slice)
{
    (void)owner;
    (void)context;
    (void)chunk;
    (void)slice;
    return MC_CHUNK_CALLBACK_OK;
}

static mc_chunk_callback_result
provider_maintenance(void *owner, const mc_chunk_lifecycle_context *context, bool *result)
{
    mc_world_runtime *runtime = owner;
    return runtime->options.provider_maintenance(runtime->options.provider, context, result);
}

static mc_chunk_callback_result session(void *owner, const mc_world_save_context *context)
{
    (void)context;
    mc_world_runtime *runtime = owner;
    return mc_world_store_check_session(runtime->options.store, &runtime->last_error) ==
                   MC_WORLD_STORAGE_OK
               ? MC_CHUNK_CALLBACK_OK
               : MC_CHUNK_CALLBACK_FATAL;
}

static mc_chunk_callback_result metadata(void *owner, const mc_world_save_context *context)
{
    (void)context;
    mc_world_runtime *runtime = owner;
    int64_t timestamp = runtime->options.metadata_timestamp(runtime->options.clock);
    mc_level_info info;
    if (mc_world_runtime_read_info(runtime, &info, &runtime->last_error) != MC_WORLD_STORAGE_OK)
        return MC_CHUNK_CALLBACK_FATAL;
    return mc_world_store_save_info(runtime->options.store, &info, timestamp,
                                    &runtime->last_error) == MC_WORLD_STORAGE_OK
               ? MC_CHUNK_CALLBACK_OK
               : MC_CHUNK_CALLBACK_FATAL;
}

static mc_chunk_callback_result auxiliary(void *owner, const mc_world_save_context *context)
{
    mc_world_runtime *runtime = owner;
    return runtime->options.save_auxiliary(runtime->options.auxiliary, context);
}

static mc_chunk_callback_result progress(void *owner, const mc_world_save_context *context,
                                         mc_world_save_stage stage)
{
    mc_world_runtime *runtime = owner;
    return runtime->options.report_progress(runtime->options.progress, context, stage);
}

mc_world_storage_status mc_world_runtime_create(const mc_world_runtime_options *options,
                                                mc_world_runtime **out,
                                                mc_world_storage_error *error)
{
    if (options == NULL || out == NULL || options->store == NULL ||
        !mc_chunk_cache_validate(options->cache) || !mc_chunk_pending_validate(options->pending) ||
        options->cache->count != 0 || options->cache->insertion_count != 0 ||
        options->pending->count != 0 || options->pending->history == NULL ||
        (options->dimension != 0 && options->dimension != -1) ||
        (options->policy != MC_CHUNK_ACQUIRE_BASE &&
         options->policy != MC_CHUNK_ACQUIRE_DEDICATED) ||
        options->region_timestamp == NULL || options->metadata_timestamp == NULL ||
        options->provider_maintenance == NULL || options->save_auxiliary == NULL ||
        (options->generator == NULL) != (options->generation == NULL) ||
        (options->generator == NULL) != (options->populate == NULL))
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "runtime arguments");
    const mc_level_info *info = mc_world_store_info(options->store);
    if (info == NULL)
        return runtime_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "runtime requires metadata");
    if (options->generator != NULL &&
        (options->dimension != 0 || !options->generator->initialized ||
         options->generator->world_seed != info->seed || options->generation->lighting == NULL ||
         options->generation->lighting->cache != options->cache))
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                             "runtime generation binding");
    mc_world_runtime *runtime = calloc(1, sizeof(*runtime));
    if (runtime == NULL)
        return runtime_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "runtime owner");
    runtime->options = *options;
    runtime->last_random_status = MC_JAVA_RANDOM_PROFILE_OK;
    runtime->live = (mc_world_runtime_state){.time = info->time,
                                             .spawn_x = info->spawn_x,
                                             .spawn_y = info->spawn_y,
                                             .spawn_z = info->spawn_z};
    runtime->fallback = (mc_chunk_state){.blank = true, .never_save = true};
    runtime->references = (mc_chunk_references){runtime, reference_retain, reference_release};
    runtime->loader =
        (mc_world_store_loader){.store = options->store, .dimension = options->dimension};
    runtime->acquisition =
        (mc_chunk_acquire_context){.cache = options->cache,
                                   .pending = options->pending,
                                   .fallback = &runtime->fallback,
                                   .world_time = &runtime->live.time,
                                   .world_force_load = &runtime->live.world_force_load,
                                   .provider_force_load = &runtime->live.provider_force_load,
                                   .spawn_x = &runtime->live.spawn_x,
                                   .spawn_z = &runtime->live.spawn_z,
                                   .policy = options->policy,
                                   .world = runtime,
                                   .loader = runtime,
                                   .provider = runtime,
                                   .load = load,
                                   .generate = options->generator == NULL ? NULL : generate,
                                   .register_tiles = register_tiles,
                                   .register_entities = register_entities,
                                   .populate = options->generator == NULL ? NULL : populate,
                                   .references = &runtime->references};
    runtime->lifecycle =
        (mc_chunk_lifecycle_context){.cache = options->cache,
                                     .pending = options->pending,
                                     .world_time = &runtime->live.time,
                                     .saving_disabled = &runtime->live.saving_disabled,
                                     .policy = options->policy,
                                     .world = runtime,
                                     .loader = runtime,
                                     .provider = runtime,
                                     .save_main = save_main,
                                     .save_extra = save_extra,
                                     .flush = loader_noop,
                                     .loader_maintenance = loader_noop,
                                     .invalidate_tiles = invalidate_tiles,
                                     .defer_entities = defer_entities,
                                     .provider_maintenance = provider_maintenance,
                                     .references = &runtime->references};
    runtime->saving = (mc_world_save_context){
        .chunks = &runtime->lifecycle,
        .world = runtime,
        .progress = runtime,
        .check_session = session,
        .save_metadata = metadata,
        .save_auxiliary = auxiliary,
        .report_progress = options->report_progress == NULL ? NULL : progress};
    if (!mc_chunk_acquire_validate(&runtime->acquisition) ||
        !mc_world_save_validate(&runtime->saving)) {
        free(runtime);
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "runtime context binding");
    }
    mc_world_storage_status status = mc_world_store_register_cache(
        options->store, options->cache, &runtime->acquisition.fallback, error);
    if (status != MC_WORLD_STORAGE_OK) {
        free(runtime);
        return status;
    }
    *out = runtime;
    return runtime_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_runtime_state *mc_world_runtime_live(mc_world_runtime *runtime)
{
    return runtime == NULL ? NULL : &runtime->live;
}

mc_world_storage_status mc_world_runtime_read_info(const mc_world_runtime *runtime,
                                                   mc_level_info *out,
                                                   mc_world_storage_error *error)
{
    if (runtime == NULL || out == NULL)
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "runtime info arguments");
    const mc_level_info *current = mc_world_store_info(runtime->options.store);
    if (current == NULL)
        return runtime_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "runtime metadata is missing");
    mc_level_info snapshot = *current;
    snapshot.time = runtime->live.time;
    snapshot.spawn_x = runtime->live.spawn_x;
    snapshot.spawn_y = runtime->live.spawn_y;
    snapshot.spawn_z = runtime->live.spawn_z;
    *out = snapshot;
    return runtime_error(error, MC_WORLD_STORAGE_OK, NULL);
}

const mc_chunk_acquire_context *mc_world_runtime_acquisition(const mc_world_runtime *runtime)
{
    return runtime == NULL ? NULL : &runtime->acquisition;
}

const mc_chunk_lifecycle_context *mc_world_runtime_lifecycle(const mc_world_runtime *runtime)
{
    return runtime == NULL ? NULL : &runtime->lifecycle;
}

const mc_world_save_context *mc_world_runtime_saving(const mc_world_runtime *runtime)
{
    return runtime == NULL ? NULL : &runtime->saving;
}

const mc_world_storage_error *mc_world_runtime_last_error(const mc_world_runtime *runtime)
{
    return runtime == NULL ? NULL : &runtime->last_error;
}

mc_java_random_profile *mc_world_runtime_random_profile(const mc_world_runtime *runtime)
{
    return runtime == NULL ? NULL : runtime->options.random_profile;
}

mc_java_random_profile_status mc_world_runtime_construct_default_random(mc_world_runtime *runtime,
                                                                        mc_java_random *out)
{
    if (runtime == NULL)
        return MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT;
    if (out == NULL || runtime->options.random_profile == NULL)
        return random_result(runtime, MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    if (runtime->active_calls == SIZE_MAX)
        return random_result(runtime, MC_JAVA_RANDOM_PROFILE_LIMIT);
    ++runtime->active_calls;
    mc_java_random_profile_status status =
        mc_java_random_construct_default(runtime->options.random_profile, out);
    --runtime->active_calls;
    return random_result(runtime, status);
}

mc_java_random_profile_status mc_world_runtime_last_random_status(const mc_world_runtime *runtime)
{
    return runtime == NULL ? MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT : runtime->last_random_status;
}

bool mc_world_runtime_binding_retain(mc_world_runtime *runtime)
{
    if (runtime == NULL)
        return false;
    if (runtime->persistent_bindings == SIZE_MAX) {
        runtime_error(&runtime->last_error, MC_WORLD_STORAGE_LIMIT, "runtime binding retain");
        return false;
    }
    ++runtime->persistent_bindings;
    return true;
}

void mc_world_runtime_binding_release(mc_world_runtime *runtime)
{
    if (runtime == NULL)
        return;
    if (runtime->persistent_bindings == 0) {
        runtime_error(&runtime->last_error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "runtime binding release");
        return;
    }
    --runtime->persistent_bindings;
}

static mc_chunk_acquire_status enter_acquire(mc_world_runtime *runtime)
{
    if (runtime == NULL)
        return MC_CHUNK_ACQUIRE_INVALID_ARGUMENT;
    if (runtime->active_calls == SIZE_MAX)
        return MC_CHUNK_ACQUIRE_LIMIT;
    ++runtime->active_calls;
    return MC_CHUNK_ACQUIRE_OK;
}

mc_chunk_acquire_status mc_world_runtime_acquire(mc_world_runtime *runtime, int32_t x, int32_t z,
                                                 mc_chunk_state **out)
{
    mc_chunk_acquire_status status = enter_acquire(runtime);
    if (status != MC_CHUNK_ACQUIRE_OK)
        return status;
    status = mc_chunk_acquire(&runtime->acquisition, x, z, out);
    --runtime->active_calls;
    return status;
}

mc_chunk_acquire_status mc_world_runtime_get(mc_world_runtime *runtime, int32_t x, int32_t z,
                                             mc_chunk_state **out)
{
    mc_chunk_acquire_status status = enter_acquire(runtime);
    if (status != MC_CHUNK_ACQUIRE_OK)
        return status;
    status = mc_chunk_acquire_get(&runtime->acquisition, x, z, out);
    --runtime->active_calls;
    return status;
}

mc_chunk_acquire_status mc_world_runtime_populate(mc_world_runtime *runtime, int32_t x, int32_t z)
{
    mc_chunk_acquire_status status = enter_acquire(runtime);
    if (status != MC_CHUNK_ACQUIRE_OK)
        return status;
    status = mc_chunk_acquire_populate(&runtime->acquisition, x, z);
    --runtime->active_calls;
    return status;
}

mc_chunk_acquire_status mc_world_runtime_mark_drop(mc_world_runtime *runtime, int32_t x, int32_t z)
{
    mc_chunk_acquire_status status = enter_acquire(runtime);
    if (status != MC_CHUNK_ACQUIRE_OK)
        return status;
    status = mc_chunk_acquire_mark_drop(&runtime->acquisition, x, z);
    --runtime->active_calls;
    return status;
}

static mc_chunk_lifecycle_status enter_lifecycle(mc_world_runtime *runtime)
{
    if (runtime == NULL)
        return MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT;
    if (runtime->active_calls == SIZE_MAX)
        return MC_CHUNK_LIFECYCLE_LIMIT;
    ++runtime->active_calls;
    return MC_CHUNK_LIFECYCLE_OK;
}

mc_chunk_lifecycle_status mc_world_runtime_save_chunks(mc_world_runtime *runtime, bool force,
                                                       bool *completed)
{
    mc_chunk_lifecycle_status status = enter_lifecycle(runtime);
    if (status != MC_CHUNK_LIFECYCLE_OK)
        return status;
    status = mc_chunk_lifecycle_save(&runtime->lifecycle, force, completed);
    --runtime->active_calls;
    return status;
}

mc_chunk_lifecycle_status mc_world_runtime_save(mc_world_runtime *runtime, bool force)
{
    mc_chunk_lifecycle_status status = enter_lifecycle(runtime);
    if (status != MC_CHUNK_LIFECYCLE_OK)
        return status;
    status = mc_world_lifecycle_save(&runtime->saving, force);
    --runtime->active_calls;
    return status;
}

mc_chunk_lifecycle_status mc_world_runtime_save_client(mc_world_runtime *runtime, int32_t pass,
                                                       bool *completed)
{
    mc_chunk_lifecycle_status status = enter_lifecycle(runtime);
    if (status != MC_CHUNK_LIFECYCLE_OK)
        return status;
    status = mc_world_lifecycle_save_client(&runtime->saving, pass, completed);
    --runtime->active_calls;
    return status;
}

mc_chunk_lifecycle_status mc_world_runtime_maintain(mc_world_runtime *runtime,
                                                    bool *provider_result)
{
    mc_chunk_lifecycle_status status = enter_lifecycle(runtime);
    if (status != MC_CHUNK_LIFECYCLE_OK)
        return status;
    status = mc_chunk_lifecycle_maintain(&runtime->lifecycle, provider_result);
    --runtime->active_calls;
    return status;
}

mc_world_storage_status mc_world_runtime_free(mc_world_runtime *runtime,
                                              mc_world_storage_error *error)
{
    if (runtime == NULL)
        return runtime_error(error, MC_WORLD_STORAGE_OK, NULL);
    if (runtime->active_calls != 0 || runtime->local_leases != 0)
        return runtime_error(error, MC_WORLD_STORAGE_BUSY, "runtime call or local lease active");
    if (runtime->persistent_bindings != 0)
        return runtime_error(error, MC_WORLD_STORAGE_BUSY, "runtime persistent binding active");
    if (!mc_chunk_acquire_validate(&runtime->acquisition))
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "invalid runtime bindings");
    if (runtime->options.cache->count != 0 || runtime->options.cache->insertion_count != 0 ||
        runtime->options.pending->count != 0)
        return runtime_error(error, MC_WORLD_STORAGE_BUSY, "runtime cache or pending is nonempty");
    mc_world_storage_status status =
        mc_world_store_unregister_cache(runtime->options.store, runtime->options.cache, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    free(runtime);
    return runtime_error(error, MC_WORLD_STORAGE_OK, NULL);
}
