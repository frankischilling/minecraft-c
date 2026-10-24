#include "generation_internal.h"

#include "../../biome/biome_internal.h"
#include "../../storage/generated_internal.h"
#include "../../terrain/terrain_internal.h"
#include "../runtime_internal.h"
#include "mc/block/registry/runtime.h"

#include <stdlib.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "GAME generation requires the qualified x86-64 floating environment"
#endif
#include <emmintrin.h>

static mc_registry_error registry_error(mc_registry_status status, const char *message) {
    return (mc_registry_error){.status = status,
                               .message = message,
                               .initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

bool mc_world_generation_fail(mc_chunk_provider *provider, mc_world_generation_error *error,
                              mc_world_storage_status status, const char *operation) {
    error->status = status;
    error->operation = operation;
    error->cause = (mc_world_storage_error){status, operation, 0};
    provider->world->last_error = error->cause;
    return false;
}

bool mc_world_generation_registry_fail(mc_chunk_provider *provider,
                                       mc_world_generation_error *error, mc_registry_status status,
                                       const char *operation) {
    error->status = mc_world_runtime_registry_result(provider->world, status, &error->registry,
                                                     operation, &error->cause);
    error->operation = operation;
    return false;
}

bool mc_world_generation_block(mc_chunk_provider *provider, const char *name,
                               unsigned int static_pc, unsigned int field_pc,
                               unsigned int *inner_pc, int32_t *out,
                               mc_world_generation_error *error) {
    *inner_pc = static_pc;
    mc_registry_classes *registry = mc_world_runtime_registry(provider->world);
    mc_registry_error cause = registry_error(MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                                             "Overworld generation Registry binding");
    const mc_block *block = NULL;
    mc_registry_status status = registry == NULL
                                    ? cause.status
                                    : mc_registry_block_static_get(registry, name, &block, &cause);
    if (status == MC_REGISTRY_OK) {
        *inner_pc = field_pc;
        const mc_block_fields *fields = mc_block_fields_get(block);
        if (fields != NULL) {
            *out = fields->bn;
            return true;
        }
        status = MC_REGISTRY_NULL_REFERENCE;
        cause = registry_error(status, "Overworld generation Block receiver");
    }
    error->registry = cause;
    return mc_world_generation_registry_fail(provider, error, status,
                                             "Overworld generation Block field");
}

bool mc_world_generation_seed(mc_chunk_provider *provider, unsigned int pc, unsigned int *inner_pc,
                              int64_t *out, mc_world_generation_error *error) {
    *inner_pc = pc;
    mc_world_info_fields fields;
    if (mc_world_info_fields_get(provider->world->fields.x, &fields) != MC_WORLD_INFO_OK)
        return mc_world_generation_fail(provider, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                        "Overworld generation WorldInfo seed receiver");
    *out = fields.value.seed;
    return true;
}

static mc_world_storage_status dimension_status(mc_world_provider_status status) {
    if (status == MC_WORLD_PROVIDER_LIMIT)
        return MC_WORLD_STORAGE_LIMIT;
    if (status == MC_WORLD_PROVIDER_ALLOCATION_FAILED)
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

/* Each invocation reloads World.t then that Dimension's actual b. A retained
 * receiver is not substituted for any later independent source getter. */
static bool manager_capture(mc_chunk_provider *provider, mc_world_chunk_manager **out,
                            mc_world_generation_error *error) {
    mc_world_provider_fields fields;
    mc_world_provider_status status =
        mc_world_provider_fields_get(provider->world->fields.t, &fields);
    if (status != MC_WORLD_PROVIDER_OK)
        return mc_world_generation_fail(provider, error, dimension_status(status),
                                        "Overworld generation current Dimension receiver");
    if (fields.b == NULL)
        return mc_world_generation_fail(provider, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                        "Overworld generation current biome receiver");
    status = mc_world_chunk_manager_retain(fields.b);
    if (status != MC_WORLD_PROVIDER_OK)
        return mc_world_generation_fail(provider, error, dimension_status(status),
                                        "Overworld generation biome receiver hold");
    *out = fields.b;
    return true;
}

/* NULL is a valid source array value. Retain the actual allocation separately
 * from its replaceable field and supplying manager. Even a shared Dimension
 * rebound to another Registry may leave this World's Block class cold; its
 * later callback must not invalidate an already captured climate array. */
static bool climate_capture(mc_chunk_provider *provider, bool rainfall,
                            mc_world_chunk_manager **owner, mc_biome_double_reference *out,
                            mc_world_generation_error *error) {
    if (!manager_capture(provider, owner, error))
        return false;
    mc_world_chunk_manager_fields fields;
    if (mc_world_chunk_manager_fields_get(*owner, &fields) != MC_WORLD_PROVIDER_OK) {
        mc_world_chunk_manager_release(*owner);
        *owner = NULL;
        return mc_world_generation_fail(provider, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                        "Overworld generation climate receiver fields");
    }
    const mc_biome_double_buffer *array = rainfall ? fields.b : fields.a;
    mc_biome_status status = mc_biome_double_capture(array, out);
    if (status != MC_BIOME_OK) {
        mc_world_chunk_manager_release(*owner);
        *owner = NULL;
        return mc_world_generation_fail(
            provider, error,
            status == MC_BIOME_LIMIT ? MC_WORLD_STORAGE_LIMIT : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "Overworld generation climate allocation hold");
    }
    return true;
}

static bool climate_at(mc_chunk_provider *provider, const mc_biome_double_reference *array,
                       size_t index, unsigned int pc, unsigned int *inner_pc, double *out,
                       mc_world_generation_error *error) {
    *inner_pc = pc;
    if (array->data == NULL || index >= array->length) {
        mc_registry_status status =
            array->data == NULL ? MC_REGISTRY_NULL_REFERENCE : MC_REGISTRY_INDEX_OUT_OF_RANGE;
        error->registry = registry_error(status, "Overworld generation climate array element");
        return mc_world_generation_registry_fail(provider, error, status,
                                                 "Overworld generation climate array element");
    }
    *out = array->data[index];
    return true;
}

static bool density_channel(mc_chunk_provider *provider, mc_terrain_density_channel channel,
                            int32_t x, int32_t z, double *backing, size_t length,
                            mc_terrain_double_span *field, unsigned int pc,
                            mc_world_generation_error *error) {
    const mc_terrain_double_span output =
        field->data == NULL ? (mc_terrain_double_span){backing, length} : *field;
    error->density_bytecode = pc;
    if (!mc_terrain_overworld_density_channel(&provider->generator.terrain, channel, x, z, output))
        return mc_world_generation_fail(provider, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                        "Overworld generation fixed density storage");
    *field = output;
    return true;
}

static bool density(mc_chunk_provider *provider, int32_t x, int32_t z, mc_terrain_double_span *out,
                    mc_world_generation_error *error) {
    mc_chunk_provider_generation_fields *fields = &provider->generation;
    mc_overworld_generator *generator = &provider->generator;
    /* q-local allocation12 uses reserved native backing. Publication to the
     * provider remains the caller's store47, only after this method returns. */
    const mc_terrain_double_span output =
        fields->q.data == NULL
            ? (mc_terrain_double_span){generator->density, MC_TERRAIN_OVERWORLD_DENSITY_VALUES}
            : fields->q;
    mc_world_chunk_manager *heat_owner = NULL, *rain_owner = NULL;
    mc_biome_double_reference heat = {0}, rain = {0};
    bool success = false;
    error->density_bytecode = 29;
    if (!climate_capture(provider, false, &heat_owner, &heat, error))
        goto leave;
    error->density_bytecode = 41;
    if (!climate_capture(provider, true, &rain_owner, &rain, error))
        goto leave;
    if (!density_channel(provider, MC_TERRAIN_DENSITY_SCALE, x, z, generator->column_scale,
                         MC_TERRAIN_OVERWORLD_COLUMN_VALUES, &fields->g, 74, error) ||
        !density_channel(provider, MC_TERRAIN_DENSITY_BIAS, x, z, generator->column_bias,
                         MC_TERRAIN_OVERWORLD_COLUMN_VALUES, &fields->h, 105, error) ||
        !density_channel(provider, MC_TERRAIN_DENSITY_MIX, x, z, generator->mix,
                         MC_TERRAIN_OVERWORLD_DENSITY_VALUES, &fields->d, 151, error) ||
        !density_channel(provider, MC_TERRAIN_DENSITY_LOW, x, z, generator->low,
                         MC_TERRAIN_OVERWORLD_DENSITY_VALUES, &fields->e, 185, error) ||
        !density_channel(provider, MC_TERRAIN_DENSITY_HIGH, x, z, generator->high,
                         MC_TERRAIN_OVERWORLD_DENSITY_VALUES, &fields->f, 219, error))
        goto leave;
    const mc_terrain_overworld_workspace workspace = {fields->d, fields->e, fields->f, fields->g,
                                                      fields->h};
    for (size_t column = 0; column < MC_TERRAIN_OVERWORLD_COLUMN_VALUES; ++column) {
        const size_t index = (column / 5u * 3u + 1u) * 16u + column % 5u * 3u + 1u;
        double temperature, rainfall;
        if (!climate_at(provider, &heat, index, 292, &error->density_bytecode, &temperature,
                        error) ||
            !climate_at(provider, &rain, index, 305, &error->density_bytecode, &rainfall, error))
            goto leave;
        mc_terrain_overworld_density_column(&workspace, column, temperature, rainfall, output.data);
    }
    *out = output;
    error->density_bytecode = 0;
    success = true;
leave:
    mc_biome_double_release(&rain);
    mc_biome_double_release(&heat);
    mc_world_chunk_manager_release(rain_owner);
    mc_world_chunk_manager_release(heat_owner);
    return success;
}

static bool base(mc_chunk_provider *provider, int32_t x, int32_t z, uint8_t *blocks,
                 const mc_biome_double_reference *temperature, mc_world_generation_error *error) {
    error->base_bytecode = 44;
    mc_terrain_double_span output;
    if (!density(provider, x, z, &output, error))
        return false;
    provider->generation.q = output;
    mc_terrain_base_cursor cursor;
    mc_terrain_overworld_base_cursor_init(&cursor, output.data);
    mc_terrain_base_voxel voxel;
    while (mc_terrain_overworld_base_cursor_get(&cursor, &voxel)) {
        double heat;
        if (!climate_at(provider, temperature, voxel.temperature_index, 461, &error->base_bytecode,
                        &heat, error))
            return false;
        int32_t block = 0;
        if (voxel.y < 64) {
            if (heat < 0.5 && voxel.y >= 63) {
                if (!mc_world_generation_block(provider, "aU", 504, 507, &error->base_bytecode,
                                               &block, error))
                    return false;
            } else if (!mc_world_generation_block(provider, "C", 515, 518, &error->base_bytecode,
                                                  &block, error))
                return false;
        }
        if (voxel.density > 0.0 && !mc_world_generation_block(provider, "u", 530, 533,
                                                              &error->base_bytecode, &block, error))
            return false;
        blocks[voxel.index] = (uint8_t)((uint32_t)block & 255u);
        mc_terrain_overworld_base_cursor_advance(&cursor);
    }
    error->base_bytecode = 0;
    return true;
}

mc_world_storage_status mc_chunk_provider_generate_game(mc_chunk_provider *provider, int32_t x,
                                                        int32_t z, mc_chunk_state **out,
                                                        mc_world_generation_error *error) {
    mc_world_generation_error frame = {.registry = registry_error(MC_REGISTRY_OK, NULL)};
    if (provider == NULL || out == NULL || provider->world == NULL) {
        frame.status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        frame.operation = "Overworld generation owner/output";
        frame.cause = (mc_world_storage_error){frame.status, frame.operation, 0};
        goto result;
    }
    mc_world_runtime *world = provider->world;
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || !provider->generator.initialized ||
        provider->generator.origin != MC_OVERWORLD_GENERATOR_PROVIDER ||
        (_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        frame.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        frame.operation = "Overworld generation native admission";
        frame.cause = (mc_world_storage_error){frame.status, frame.operation, 0};
        goto result;
    }
    frame.status = mc_world_runtime_call_enter(world, &frame.cause);
    if (frame.status != MC_WORLD_STORAGE_OK) {
        frame.operation = frame.cause.operation;
        goto result;
    }
    mc_world_chunk *chunk = NULL;
    mc_chunk_state *state = NULL;
    mc_world_store *store = NULL;
    mc_world_chunk_manager *receiver = NULL, *temperature_owner = NULL;
    mc_biome_double_reference temperature = {0};
    bool leased = false, success = false;
    uint8_t *blocks = NULL;
    mc_java_random_reseed(&provider->generator.generator_random,
                          mc_terrain_overworld_chunk_seed(x, z));
    frame.bytecode = 22;
    blocks = calloc(MC_CHUNK_BLOCKS, 1u);
    if (blocks == NULL) {
        (void)mc_world_generation_fail(provider, &frame, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                       "Overworld generation block allocation");
        goto leave;
    }
    frame.bytecode = 36;
    store = world->fields.w;
    if (mc_world_runtime_chunk_construct(world, x, z, &blocks, MC_CHUNK_BLOCKS, &chunk,
                                         &frame.chunk) != MC_WORLD_STORAGE_OK) {
        frame.status = frame.chunk.status;
        frame.operation = frame.chunk.operation;
        frame.cause = frame.chunk.cause;
        world->last_error = frame.cause;
        goto leave;
    }
    state = mc_world_chunk_state(chunk);
    frame.status = mc_world_store_lease_chunk(store, state, &frame.cause);
    if (frame.status != MC_WORLD_STORAGE_OK) {
        frame.operation = frame.cause.operation;
        world->last_error = frame.cause;
        goto leave;
    }
    leased = true;
    frame.bytecode = 46;
    if (!manager_capture(provider, &receiver, &frame))
        goto leave;
    mc_biome_reference_array *sampled = NULL;
    frame.bytecode = 65;
    if (mc_world_chunk_manager_sample(receiver, mc_java_i32_mul(x, 16), mc_java_i32_mul(z, 16), 16u,
                                      16u, provider->generation.v, &sampled,
                                      &frame.biome) != MC_WORLD_PROVIDER_OK) {
        frame.registry = frame.biome.sampling.registry;
        if (frame.registry.status != MC_REGISTRY_OK)
            (void)mc_world_generation_registry_fail(provider, &frame, frame.registry.status,
                                                    "Overworld generation direct biome sample");
        else
            (void)mc_world_generation_fail(provider, &frame, dimension_status(frame.biome.status),
                                           "Overworld generation direct biome sample");
        goto leave;
    }
    const mc_world_provider_status v_hold = mc_world_chunk_manager_retain(receiver);
    if (v_hold != MC_WORLD_PROVIDER_OK) {
        (void)mc_world_generation_fail(provider, &frame, dimension_status(v_hold),
                                       "Overworld generation v supplier hold");
        goto leave;
    }
    provider->generation.v = sampled;
    mc_world_chunk_manager *previous_v_owner = provider->v_owner;
    provider->v_owner = receiver;
    mc_world_chunk_manager_release(previous_v_owner);
    frame.bytecode = 75;
    if (!climate_capture(provider, false, &temperature_owner, &temperature, &frame))
        goto leave;
    frame.bytecode = 93;
    /* Source v88 is a passive unused base argument, not a material preflight. */
    if (!base(provider, x, z, state->data->blocks, &temperature, &frame))
        goto leave;
    mc_biome_double_release(&temperature);
    mc_world_chunk_manager_release(temperature_owner);
    temperature_owner = NULL;
    frame.bytecode = 104;
    if (!mc_world_generation_surface(provider, x, z, state->data->blocks, provider->generation.v,
                                     &frame))
        goto leave;
    frame.bytecode = 119;
    if (!mc_world_generation_caves(provider, x, z, state->data->blocks, &frame))
        goto leave;
    frame.bytecode = 124;
    if (mc_world_runtime_chunk_finish(chunk, &frame.chunk) != MC_WORLD_STORAGE_OK) {
        frame.status = frame.chunk.status;
        frame.operation = frame.chunk.operation;
        frame.cause = frame.chunk.cause;
        frame.registry = frame.chunk.registry;
        goto leave;
    }
    *out = state;
    success = true;
leave:
    mc_biome_double_release(&temperature);
    mc_world_chunk_manager_release(temperature_owner);
    mc_world_chunk_manager_release(receiver);
    if (leased)
        (void)mc_world_store_release_chunk(store, state, NULL);
    if (!success && state != NULL &&
        mc_world_store_retire_chunk(store, state, &frame.cleanup) != MC_WORLD_STORAGE_OK) {
        frame.retained_child = state;
        frame.retained_store = store;
    }
    free(blocks);
    mc_world_runtime_call_leave(world);
    if (success)
        frame = (mc_world_generation_error){.registry = registry_error(MC_REGISTRY_OK, NULL)};
result:
    if (error != NULL)
        *error = frame;
    return frame.status;
}
