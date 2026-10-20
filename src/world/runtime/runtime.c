#include "../storage/generated_internal.h"
#include "../storage/objects_internal.h"
#include "activation_internal.h"
#include "generation/generation_internal.h"
#include "loading/loading_internal.h"
#include "millis.h"
#include "provider_internal.h"
#include "runtime_internal.h"

#include <stdlib.h>

static mc_world_storage_status runtime_error(mc_world_storage_error *error,
                                             mc_world_storage_status status, const char *operation)
{
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_runtime_acquisition_error acquisition_empty_error(void)
{
    return (mc_world_runtime_acquisition_error){.status = MC_CHUNK_ACQUIRE_OK,
                                                .cause = {MC_WORLD_STORAGE_OK, NULL, 0},
                                                .registry = mc_tile_empty_error().registry,
                                                .tile = mc_live_tiles_empty_error()};
}

static void acquisition_cause(mc_world_runtime *runtime, mc_world_storage_error cause)
{
    if (runtime->acquisition_error != NULL) {
        *runtime->acquisition_error = acquisition_empty_error();
        runtime->acquisition_error->cause = cause;
    }
}

static void acquisition_activation(mc_world_runtime *runtime,
                                   const mc_world_activation_error *error)
{
    if (runtime->acquisition_error != NULL) {
        acquisition_cause(runtime, error->cause);
        runtime->acquisition_error->tile = error->tile;
        runtime->acquisition_error->registry = error->tile.registry;
        runtime->acquisition_error->activation_bytecode = error->bytecode;
        runtime->acquisition_error->activation_world_bytecode = error->world_bytecode;
    }
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
    if (runtime->local_leases == SIZE_MAX) {
        runtime_error(&runtime->last_error, MC_WORLD_STORAGE_LIMIT, "runtime local lease retain");
        acquisition_cause(runtime, runtime->last_error);
        return false;
    }
    if (chunk != &runtime->fallback &&
        mc_world_store_lease_chunk(runtime->options.store, chunk, &runtime->last_error) !=
            MC_WORLD_STORAGE_OK) {
        acquisition_cause(runtime, runtime->last_error);
        return false;
    }
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
    if (runtime->construction.mode == MC_WORLD_RUNTIME_GAME) {
        mc_world_game_load_error error;
        mc_world_storage_status status = mc_world_runtime_load_game(runtime, x, z, out, &error);
        runtime->last_error = error.cause;
        if (status != MC_WORLD_STORAGE_OK && status != MC_WORLD_STORAGE_MISSING) {
            acquisition_cause(runtime, runtime->last_error);
            if (runtime->acquisition_error != NULL) {
                runtime->acquisition_error->tile = error.tile;
                runtime->acquisition_error->registry = error.tile.registry;
                runtime->acquisition_error->load_bytecode = error.region_bytecode;
                runtime->acquisition_error->load_builder_bytecode = error.builder_bytecode;
            }
        }
        return status == MC_WORLD_STORAGE_OK        ? MC_CHUNK_LOAD_LOADED
               : status == MC_WORLD_STORAGE_MISSING ? MC_CHUNK_LOAD_MISS
                                                    : MC_CHUNK_LOAD_FATAL;
    }
    mc_chunk_load_result result =
        mc_world_store_load_callback(&runtime->loader, context, x, z, out);
    runtime->last_error = runtime->loader.last_error;
    if (result == MC_CHUNK_LOAD_FATAL)
        acquisition_cause(runtime, runtime->loader.last_error);
    if (result == MC_CHUNK_LOAD_MISS && runtime->options.generator == NULL &&
        runtime->construction.mode == MC_WORLD_RUNTIME_COMPOSITION)
        return MC_CHUNK_LOAD_FATAL;
    return result;
}

static bool generate(void *owner, const mc_chunk_acquire_context *context, int32_t x, int32_t z,
                     mc_chunk_state **out)
{
    (void)context;
    mc_world_runtime *runtime = owner;
    if (runtime->construction.mode == MC_WORLD_RUNTIME_GAME) {
        mc_world_generation_error error;
        mc_world_storage_status status =
            mc_world_provider_generate_game(runtime->fields.v, x, z, out, &error);
        if (status != MC_WORLD_STORAGE_OK) {
            runtime->last_error = error.cause;
            acquisition_cause(runtime, error.cause);
            if (error.registry.status != MC_REGISTRY_OK)
                runtime->last_registry_error = error.registry;
            if (runtime->acquisition_error != NULL)
                runtime->acquisition_error->registry = error.registry;
        }
        return status == MC_WORLD_STORAGE_OK;
    }
    mc_world_store_reservation *reservation = NULL;
    if (mc_world_store_reserve_chunk(runtime->options.store, runtime->options.dimension,
                                     &reservation, &runtime->last_error) != MC_WORLD_STORAGE_OK) {
        acquisition_cause(runtime, runtime->last_error);
        return false;
    }
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
    if (!success) {
        acquisition_cause(runtime, runtime->last_error);
        (void)mc_world_store_cancel_reservation(runtime->options.store, reservation, NULL);
    }
    free(blocks);
    free(staging);
    return success;
}

static bool register_tiles(void *owner, const mc_chunk_acquire_context *context,
                           mc_chunk_state *chunk)
{
    (void)context;
    mc_world_runtime *runtime = owner;
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME)
        return true;
    mc_world_activation_error error;
    mc_world_storage_status status = mc_world_runtime_register_chunk_tiles(
        mc_world_store_chunk_object(runtime->options.store, chunk), &error);
    if (status != MC_WORLD_STORAGE_OK) {
        runtime->last_error = error.cause;
        acquisition_activation(runtime, &error);
    }
    return status == MC_WORLD_STORAGE_OK;
}

static bool register_entities(void *owner, const mc_chunk_acquire_context *context,
                              mc_chunk_state *chunk, size_t slice)
{
    (void)context;
    mc_world_runtime *runtime = owner;
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME)
        return true;
    mc_world_activation_error error;
    mc_world_storage_status status = mc_world_runtime_register_chunk_entities(
        mc_world_store_chunk_object(runtime->options.store, chunk), slice, &error);
    if (status != MC_WORLD_STORAGE_OK) {
        runtime->last_error = error.cause;
        acquisition_activation(runtime, &error);
    }
    return status == MC_WORLD_STORAGE_OK;
}

static bool populate(void *owner, const mc_chunk_acquire_context *context, int32_t x, int32_t z)
{
    mc_world_runtime *runtime = owner;
    if (runtime->construction.mode == MC_WORLD_RUNTIME_GAME) {
        runtime_error(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World feature population");
        acquisition_cause(runtime, runtime->last_error);
        return false;
    }
    return runtime->options.populate(runtime->options.provider, context, x, z);
}

static mc_chunk_callback_result save_main(void *owner, const mc_chunk_lifecycle_context *context,
                                          mc_chunk_state *chunk)
{
    mc_world_runtime *runtime = owner;
    uint32_t growth = 0;
    uint32_t timestamp;
    if (runtime->construction.mode == MC_WORLD_RUNTIME_GAME) {
        int64_t millis;
        if (mc_platform_current_time_millis(&millis) != MC_PLATFORM_MILLIS_OK) {
            runtime_error(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World region wall clock");
            return MC_CHUNK_CALLBACK_FATAL;
        }
        timestamp = (uint32_t)(millis / INT64_C(1000));
    } else {
        timestamp = runtime->options.region_timestamp(runtime->options.clock);
    }
    mc_world_storage_status status =
        runtime->fields.x == NULL
            ? mc_world_store_write_runtime_chunk(runtime->options.store, chunk,
                                                 *context->world_time, timestamp, &growth,
                                                 &runtime->last_error)
            : mc_world_store_write_runtime_chunk_for_info(runtime->options.store, chunk,
                                                          runtime->fields.x, *context->world_time,
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
    (void)context;
    mc_world_runtime *runtime = owner;
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME)
        return MC_CHUNK_CALLBACK_OK;
    mc_world_activation_error error;
    mc_world_storage_status status = mc_world_runtime_invalidate_chunk_tiles(
        mc_world_store_chunk_object(runtime->options.store, chunk), &error);
    if (status != MC_WORLD_STORAGE_OK)
        runtime->last_error = error.cause;
    return status == MC_WORLD_STORAGE_OK ? MC_CHUNK_CALLBACK_OK : MC_CHUNK_CALLBACK_FATAL;
}

static mc_chunk_callback_result defer_entities(void *owner,
                                               const mc_chunk_lifecycle_context *context,
                                               mc_chunk_state *chunk, size_t slice)
{
    (void)context;
    mc_world_runtime *runtime = owner;
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME)
        return MC_CHUNK_CALLBACK_OK;
    mc_world_activation_error error;
    mc_world_storage_status status = mc_world_runtime_defer_chunk_entities(
        mc_world_store_chunk_object(runtime->options.store, chunk), slice, &error);
    if (status != MC_WORLD_STORAGE_OK)
        runtime->last_error = error.cause;
    return status == MC_WORLD_STORAGE_OK ? MC_CHUNK_CALLBACK_OK : MC_CHUNK_CALLBACK_FATAL;
}

static mc_chunk_callback_result
provider_maintenance(void *owner, const mc_chunk_lifecycle_context *context, bool *result)
{
    mc_world_runtime *runtime = owner;
    if (runtime->construction.mode == MC_WORLD_RUNTIME_GAME)
        return mc_world_provider_maintenance(runtime->fields.v, result, &runtime->last_error) ==
                       MC_WORLD_STORAGE_OK
                   ? MC_CHUNK_CALLBACK_OK
                   : MC_CHUNK_CALLBACK_FATAL;
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
    if (runtime->fields.x != NULL) {
        runtime_error(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "live-player metadata serialization");
        return MC_CHUNK_CALLBACK_FATAL;
    }
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
    if (runtime->construction.mode == MC_WORLD_RUNTIME_GAME) {
        runtime_error(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World named saved-data serialization");
        return MC_CHUNK_CALLBACK_FATAL;
    }
    return runtime->options.save_auxiliary(runtime->options.auxiliary, context);
}

static mc_chunk_callback_result progress(void *owner, const mc_world_save_context *context,
                                         mc_world_save_stage stage)
{
    mc_world_runtime *runtime = owner;
    return runtime->options.report_progress(runtime->options.progress, context, stage);
}

mc_world_storage_status mc_world_runtime_bind_contexts(mc_world_runtime *runtime,
                                                       mc_world_storage_error *error)
{
    if (runtime == NULL || runtime->construction.contexts_bound ||
        runtime->construction.cache_registered ||
        (runtime->construction.mode != MC_WORLD_RUNTIME_COMPOSITION &&
         runtime->construction.mode != MC_WORLD_RUNTIME_GAME))
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "runtime context binding");
    const mc_world_runtime_options *options = &runtime->options;
    bool game = runtime->construction.mode == MC_WORLD_RUNTIME_GAME;
    if (options->store == NULL || (options->dimension != 0 && options->dimension != -1) ||
        (options->policy != MC_CHUNK_ACQUIRE_BASE &&
         options->policy != MC_CHUNK_ACQUIRE_DEDICATED) ||
        (!game && (options->region_timestamp == NULL ||
                   (runtime->fields.x == NULL && options->metadata_timestamp == NULL) ||
                   options->provider_maintenance == NULL || options->save_auxiliary == NULL ||
                   (options->generator == NULL) != (options->generation == NULL) ||
                   (options->generator == NULL) != (options->populate == NULL))) ||
        (game &&
         (runtime->fields.v == NULL || options->generator == NULL || runtime->fields.x == NULL)) ||
        runtime->live.time == NULL || runtime->live.spawn_x == NULL ||
        runtime->live.spawn_y == NULL || runtime->live.spawn_z == NULL ||
        runtime->live.world_force_load == NULL || runtime->live.provider_force_load == NULL ||
        runtime->live.saving_disabled == NULL)
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "runtime context binding");
    if (runtime->construction.mode == MC_WORLD_RUNTIME_COMPOSITION)
        runtime->fallback = (mc_chunk_state){.blank = true, .never_save = true};
    runtime->references = (mc_chunk_references){runtime, reference_retain, reference_release};
    if (!game)
        runtime->loader =
            (mc_world_store_loader){.store = options->store, .dimension = options->dimension};
    else if (runtime->loader.store != options->store ||
             runtime->loader.dimension != options->dimension)
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World loader binding");
    runtime->acquisition =
        (mc_chunk_acquire_context){.cache = options->cache,
                                   .pending = options->pending,
                                   .fallback = &runtime->fallback,
                                   .world_time = runtime->live.time,
                                   .world_force_load = runtime->live.world_force_load,
                                   .provider_force_load = runtime->live.provider_force_load,
                                   .spawn_x = runtime->live.spawn_x,
                                   .spawn_z = runtime->live.spawn_z,
                                   .policy = options->policy,
                                   .world = runtime,
                                   .loader = runtime,
                                   .provider = runtime,
                                   .load = load,
                                   .generate = game || options->generator != NULL ? generate : NULL,
                                   .register_tiles = register_tiles,
                                   .register_entities = register_entities,
                                   .populate = game || options->generator != NULL ? populate : NULL,
                                   .references = &runtime->references};
    runtime->lifecycle =
        (mc_chunk_lifecycle_context){.cache = options->cache,
                                     .pending = options->pending,
                                     .world_time = runtime->live.time,
                                     .saving_disabled = runtime->live.saving_disabled,
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
        !mc_world_save_validate(&runtime->saving))
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "runtime context binding");
    mc_world_storage_status status = mc_world_store_register_cache(
        options->store, options->cache, &runtime->acquisition.fallback, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    runtime->construction.cache_registered = true;
    runtime->construction.contexts_bound = true;
    return runtime_error(error, MC_WORLD_STORAGE_OK, NULL);
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
        options->region_timestamp == NULL ||
        (options->world_info == NULL && options->metadata_timestamp == NULL) ||
        options->provider_maintenance == NULL || options->save_auxiliary == NULL ||
        (options->generator == NULL) != (options->generation == NULL) ||
        (options->generator == NULL) != (options->populate == NULL))
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "runtime arguments");
    const mc_level_info *info = mc_world_store_info(options->store);
    const mc_world_info_live *cells = NULL;
    mc_world_info_fields fields;
    int64_t seed;
    if (options->world_info != NULL) {
        cells = mc_world_info_live_get(options->world_info);
        if (cells == NULL ||
            mc_world_info_fields_get(options->world_info, &fields) != MC_WORLD_INFO_OK)
            return runtime_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "runtime requires READY WorldInfo");
        seed = fields.value.seed;
    } else if (info == NULL) {
        return runtime_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "runtime requires metadata");
    } else {
        seed = info->seed;
    }
    if (options->generator != NULL &&
        (options->dimension != 0 || !options->generator->initialized ||
         options->generator->world_seed != seed || options->generation->lighting == NULL ||
         options->generation->lighting->cache != options->cache))
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                             "runtime generation binding");
    mc_world_runtime *runtime = calloc(1, sizeof(*runtime));
    if (runtime == NULL)
        return runtime_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "runtime owner");
    if (options->world_info != NULL) {
        mc_world_info_status retained = mc_world_info_retain(options->world_info);
        if (retained != MC_WORLD_INFO_OK) {
            free(runtime);
            return runtime_error(error, MC_WORLD_STORAGE_LIMIT, "runtime WorldInfo hold");
        }
    }
    runtime->options = *options;
    runtime->fields.x = options->world_info;
    runtime->last_random_status = MC_JAVA_RANDOM_PROFILE_OK;
    if (cells == NULL) {
        runtime->legacy_info.time = info->time;
        runtime->legacy_info.spawn_x = info->spawn_x;
        runtime->legacy_info.spawn_y = info->spawn_y;
        runtime->legacy_info.spawn_z = info->spawn_z;
    }
    runtime->live = (mc_world_runtime_state){
        .time = cells == NULL ? &runtime->legacy_info.time : cells->time,
        .spawn_x = cells == NULL ? &runtime->legacy_info.spawn_x : cells->spawn_x,
        .spawn_y = cells == NULL ? &runtime->legacy_info.spawn_y : cells->spawn_y,
        .spawn_z = cells == NULL ? &runtime->legacy_info.spawn_z : cells->spawn_z,
        .world_force_load = &runtime->fields.y,
        .provider_force_load = &runtime->provider_force_load,
        .saving_disabled = &runtime->saving_disabled};
    mc_world_storage_status status = mc_world_runtime_bind_contexts(runtime, error);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_info_release(runtime->options.world_info);
        free(runtime);
        return status;
    }
    *out = runtime;
    runtime->construction.mode = MC_WORLD_RUNTIME_COMPOSITION;
    runtime->construction.phase = MC_WORLD_RUNTIME_READY;
    runtime->construction.direct_allocation_bytes = sizeof(*runtime);
    return runtime_error(error, MC_WORLD_STORAGE_OK, NULL);
}

const mc_world_runtime_state *mc_world_runtime_live(mc_world_runtime *runtime)
{
    return runtime == NULL || runtime->live.time == NULL ||
                   runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED
               ? NULL
               : &runtime->live;
}

mc_world_info *mc_world_runtime_info(mc_world_runtime *runtime)
{
    return runtime == NULL ? NULL : runtime->fields.x;
}

mc_world_storage_status mc_world_runtime_read_info(const mc_world_runtime *runtime,
                                                   mc_level_info *out,
                                                   mc_world_storage_error *error)
{
    if (runtime == NULL || out == NULL)
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "runtime info arguments");
    if (runtime->fields.x != NULL) {
        mc_world_info_fields fields;
        (void)mc_world_info_fields_get(runtime->fields.x, &fields);
        *out = (mc_level_info){.seed = fields.value.seed,
                               .spawn_x = fields.value.spawn_x,
                               .spawn_y = fields.value.spawn_y,
                               .spawn_z = fields.value.spawn_z,
                               .time = fields.value.time,
                               .last_played = fields.value.last_played,
                               .size_on_disk = fields.value.size_on_disk,
                               .level_name = mc_world_info_name_text(fields.name),
                               .version = fields.value.version,
                               .rain_time = fields.value.rain_time,
                               .thunder_time = fields.value.thunder_time,
                               .raining = fields.value.raining,
                               .thundering = fields.value.thundering,
                               .player = mc_world_info_player_root(fields.player),
                               .player_dimension = fields.value.player_dimension};
        return runtime_error(error, MC_WORLD_STORAGE_OK, NULL);
    }
    if (runtime->construction.mode == MC_WORLD_RUNTIME_GAME)
        return runtime_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "World Info has not been constructed");
    const mc_level_info *current = mc_world_store_info(runtime->options.store);
    if (current == NULL)
        return runtime_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "runtime metadata is missing");
    mc_level_info snapshot = *current;
    snapshot.time = *runtime->live.time;
    snapshot.spawn_x = *runtime->live.spawn_x;
    snapshot.spawn_y = *runtime->live.spawn_y;
    snapshot.spawn_z = *runtime->live.spawn_z;
    *out = snapshot;
    return runtime_error(error, MC_WORLD_STORAGE_OK, NULL);
}

const mc_chunk_acquire_context *mc_world_runtime_acquisition(const mc_world_runtime *runtime)
{
    return runtime == NULL || !runtime->construction.contexts_bound ||
                   runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED
               ? NULL
               : &runtime->acquisition;
}

const mc_chunk_lifecycle_context *mc_world_runtime_lifecycle(const mc_world_runtime *runtime)
{
    return runtime == NULL || !runtime->construction.contexts_bound ||
                   runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED
               ? NULL
               : &runtime->lifecycle;
}

const mc_world_save_context *mc_world_runtime_saving(const mc_world_runtime *runtime)
{
    return runtime == NULL || !runtime->construction.contexts_bound ||
                   runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED
               ? NULL
               : &runtime->saving;
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
    if (out == NULL || runtime->options.random_profile == NULL ||
        runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED)
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
    if (runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        runtime_error(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "retired World binding");
        return false;
    }
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

static mc_chunk_acquire_status enter_acquire(mc_world_runtime *runtime,
                                             mc_world_storage_error *cause)
{
    if (runtime == NULL) {
        runtime_error(cause, MC_WORLD_STORAGE_INVALID_ARGUMENT, "runtime acquisition owner");
        return MC_CHUNK_ACQUIRE_INVALID_ARGUMENT;
    }
    if (!runtime->construction.contexts_bound ||
        runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        runtime_error(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "runtime acquisition context not installed");
        *cause = runtime->last_error;
        return MC_CHUNK_ACQUIRE_INVALID_ARGUMENT;
    }
    if (runtime->active_calls == SIZE_MAX) {
        runtime_error(cause, MC_WORLD_STORAGE_LIMIT, "runtime acquisition call hold");
        return MC_CHUNK_ACQUIRE_LIMIT;
    }
    ++runtime->active_calls;
    return MC_CHUNK_ACQUIRE_OK;
}

typedef enum acquisition_operation {
    ACQUISITION_ACQUIRE,
    ACQUISITION_GET,
    ACQUISITION_POPULATE,
    ACQUISITION_MARK_DROP
} acquisition_operation;

static mc_chunk_acquire_status acquire_attempt(mc_world_runtime *runtime, int32_t x, int32_t z,
                                               mc_chunk_state **out,
                                               acquisition_operation operation,
                                               mc_world_runtime_acquisition_error *error)
{
    mc_world_runtime_acquisition_error frame = acquisition_empty_error();
    frame.status = enter_acquire(runtime, &frame.cause);
    if (frame.status == MC_CHUNK_ACQUIRE_OK) {
        mc_world_runtime_acquisition_error *previous = runtime->acquisition_error;
        runtime->acquisition_error = &frame;
        switch (operation) {
            case ACQUISITION_ACQUIRE:
                frame.status = mc_chunk_acquire(&runtime->acquisition, x, z, out);
                break;
            case ACQUISITION_GET:
                frame.status = mc_chunk_acquire_get(&runtime->acquisition, x, z, out);
                break;
            case ACQUISITION_POPULATE:
                frame.status = mc_chunk_acquire_populate(&runtime->acquisition, x, z);
                break;
            case ACQUISITION_MARK_DROP:
                frame.status = mc_chunk_acquire_mark_drop(&runtime->acquisition, x, z);
                break;
        }
        runtime->acquisition_error = previous;
        --runtime->active_calls;
    }
    if (frame.status == MC_CHUNK_ACQUIRE_OK)
        frame = acquisition_empty_error();
    if (error != NULL)
        *error = frame;
    return frame.status;
}

mc_chunk_acquire_status mc_world_runtime_acquire(mc_world_runtime *runtime, int32_t x, int32_t z,
                                                 mc_chunk_state **out)
{
    return acquire_attempt(runtime, x, z, out, ACQUISITION_ACQUIRE, NULL);
}

mc_chunk_acquire_status mc_world_runtime_get(mc_world_runtime *runtime, int32_t x, int32_t z,
                                             mc_chunk_state **out)
{
    return mc_world_runtime_get_detailed(runtime, x, z, out, NULL);
}

mc_chunk_acquire_status mc_world_runtime_get_detailed(mc_world_runtime *runtime, int32_t x,
                                                      int32_t z, mc_chunk_state **out,
                                                      mc_world_runtime_acquisition_error *error)
{
    return acquire_attempt(runtime, x, z, out, ACQUISITION_GET, error);
}

mc_chunk_acquire_status mc_world_runtime_populate(mc_world_runtime *runtime, int32_t x, int32_t z)
{
    return acquire_attempt(runtime, x, z, NULL, ACQUISITION_POPULATE, NULL);
}

mc_chunk_acquire_status mc_world_runtime_mark_drop(mc_world_runtime *runtime, int32_t x, int32_t z)
{
    return acquire_attempt(runtime, x, z, NULL, ACQUISITION_MARK_DROP, NULL);
}

static mc_chunk_lifecycle_status enter_lifecycle(mc_world_runtime *runtime)
{
    if (runtime == NULL)
        return MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT;
    if (!runtime->construction.contexts_bound ||
        runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        runtime_error(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "runtime lifecycle context not installed");
        return MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT;
    }
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
    if (runtime->construction.mode == MC_WORLD_RUNTIME_GAME &&
        mc_world_store_has_chunks_for_world(runtime->fields.w, runtime))
        return runtime_error(error, MC_WORLD_STORAGE_BUSY, "runtime Chunk binding active");
    if (runtime->construction.mode == MC_WORLD_RUNTIME_GAME)
        return mc_world_runtime_game_free(runtime, error);
    if (!mc_chunk_acquire_validate(&runtime->acquisition))
        return runtime_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "invalid runtime bindings");
    if (runtime->options.cache->count != 0 || runtime->options.cache->insertion_count != 0 ||
        runtime->options.pending->count != 0)
        return runtime_error(error, MC_WORLD_STORAGE_BUSY, "runtime cache or pending is nonempty");
    mc_world_storage_status status =
        mc_world_store_unregister_cache(runtime->options.store, runtime->options.cache, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_world_info_release(runtime->fields.x);
    free(runtime);
    return runtime_error(error, MC_WORLD_STORAGE_OK, NULL);
}
