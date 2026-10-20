#include "mc/world/runtime/queries.h"

#include "../../block/registry/runtime_internal.h"
#include "../chunk/object_state_internal.h"
#include "../storage/objects_internal.h"
#include "construction_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "provider_internal.h"
#include "runtime_internal.h"

typedef struct query_chunk {
    mc_chunk_state *state;
    mc_world_chunk *object;
    mc_world_store *store;
    bool leased;
} query_chunk;

static mc_world_query_error empty_error(void)
{
    return (mc_world_query_error){.registry = {.initializer_pc = -1,
                                               .block_id = -1,
                                               .constructor_pc = -1,
                                               .active_initializer_pc = -1,
                                               .item_argument = -1}};
}

static bool fail(mc_world_query_error *error, mc_world_storage_status status, const char *operation)
{
    error->status = status;
    error->operation = operation;
    error->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool storage_fail(mc_world_query_error *error, const mc_world_storage_error *cause)
{
    error->status = cause->status;
    error->operation = cause->operation;
    error->cause = *cause;
    return false;
}

static bool registry_fail(mc_world_runtime *world, mc_world_query_error *error,
                          const mc_registry_error *cause, const char *operation)
{
    error->registry = *cause;
    error->status =
        mc_world_runtime_registry_result(world, cause->status, cause, operation, &error->cause);
    error->operation = error->cause.operation;
    return false;
}

static bool registry_null(mc_world_runtime *world, mc_world_query_error *error,
                          const char *operation)
{
    mc_registry_error cause = empty_error().registry;
    cause.status = MC_REGISTRY_NULL_REFERENCE;
    cause.message = operation;
    return registry_fail(world, error, &cause, operation);
}

static bool admit(mc_world_runtime *world, bool has_out, mc_world_query_error *error)
{
    if (world == NULL || !has_out)
        return fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World query arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World query phase");
    mc_world_storage_error cause;
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(error, &cause);
    return true;
}

static mc_world_storage_status finish(mc_world_runtime *world, query_chunk *chunk,
                                      mc_world_query_error *frame, mc_world_query_error *error)
{
    if (chunk->leased)
        (void)mc_world_store_release_chunk(chunk->store, chunk->state, NULL);
    if (frame->status == MC_WORLD_STORAGE_OK)
        *frame = empty_error();
    else
        world->last_error = frame->cause;
    mc_world_runtime_call_leave(world);
    if (error != NULL)
        *error = *frame;
    return frame->status;
}

/* World.c(II) invokes the provider's b getter, preserving pending on a hit
 * and the dedicated denied-load fallback. Store leases protect the captured
 * source local across later Registry active use, without blocking reentry. */
static bool acquire(mc_world_runtime *world, int32_t x, int32_t z, query_chunk *out,
                    mc_world_query_error *error)
{
    error->provider_bytecode = 6;
    if (world->fields.v == NULL)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World query provider receiver");
    mc_world_runtime_acquisition_error detail;
    error->acquisition = mc_world_runtime_get_detailed(world, mc_java_i32_shr(x, 4),
                                                       mc_java_i32_shr(z, 4), &out->state, &detail);
    if (error->acquisition != MC_CHUNK_ACQUIRE_OK) {
        error->population = detail.population;
        error->registry = detail.registry;
        if (detail.cause.status != MC_WORLD_STORAGE_OK)
            return storage_fail(error, &detail.cause);
        if (error->acquisition == MC_CHUNK_ACQUIRE_LIMIT ||
            error->acquisition == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
            return fail(error, MC_WORLD_STORAGE_LIMIT, "World query acquisition limit");
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World query acquisition");
    }
    error->provider_bytecode = 0;
    if (out->state == NULL)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World query Chunk receiver");
    if (out->state->blank)
        return true;
    out->store = world->fields.w;
    out->object = mc_world_store_chunk_object(out->store, out->state);
    if (out->object == NULL)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World query concrete Chunk");
    mc_world_storage_error cause;
    if (mc_world_store_lease_chunk(out->store, out->state, &cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(error, &cause);
    out->leased = true;
    return true;
}

static bool nibble(const mc_world_chunk_nibble *owner, int32_t x, int32_t y, int32_t z,
                   int32_t *out, mc_world_query_error *error)
{
    if (owner == NULL || owner->a == NULL)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk query Nibble backing");
    const int32_t cell = mc_java_i32_shl(x, 11) | mc_java_i32_shl(z, 7) | y;
    const int32_t index = mc_java_i32_shr(cell, 1);
    if (index < 0 || (size_t)index >= owner->length)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk query Nibble index");
    *out = (int32_t)(((uint32_t)owner->a[(size_t)index] >> (((uint32_t)y & 1u) * 4u)) & 15u);
    return true;
}

static bool top_material(mc_world_runtime *world, int32_t id, mc_material **out,
                         mc_world_query_error *error)
{
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    mc_registry_error cause = empty_error().registry;
    error->bytecode = id == 0 ? 41u : 47u;
    if (classes == NULL) {
        cause.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        cause.message = "World top-solid/liquid Registry binding";
        return registry_fail(world, error, &cause, cause.message);
    }
    if (id == 0) {
        if (mc_registry_material_get(classes, MC_MATERIAL_a, out, &cause) != MC_REGISTRY_OK)
            return registry_fail(world, error, &cause, "World top-solid/liquid Material.a");
        return true;
    }
    mc_registry_block_m_view array;
    if (mc_registry_block_m_get(classes, &array, &cause) != MC_REGISTRY_OK)
        return registry_fail(world, error, &cause, "World top-solid/liquid Block.m active use");
    error->bytecode = 52;
    const mc_block *block;
    if (mc_registry_block_m_at(&array, id, &block, &cause) != MC_REGISTRY_OK)
        return registry_fail(world, error, &cause, "World top-solid/liquid Block.m element");
    error->bytecode = 53;
    const mc_block_fields *fields = mc_block_fields_get(block);
    if (fields == NULL)
        return registry_null(world, error, "World top-solid/liquid Block receiver");
    *out = fields->bA;
    return true;
}

mc_world_storage_status mc_world_runtime_top_solid_liquid(mc_world_runtime *world, int32_t x,
                                                          int32_t z, int32_t *out,
                                                          mc_world_query_error *error)
{
    mc_world_query_error frame = empty_error();
    if (!admit(world, out != NULL, &frame)) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    query_chunk chunk = {0};
    frame.bytecode = 3;
    if (!acquire(world, x, z, &chunk, &frame))
        return finish(world, &chunk, &frame, error);
    const int32_t local_x = (int32_t)((uint32_t)x & 15u);
    const int32_t local_z = (int32_t)((uint32_t)z & 15u);
    int32_t height = -1;
    for (int32_t y = 127; y > 0; --y) {
        frame.bytecode = 31;
        int32_t id = 0;
        if (!chunk.state->blank) {
            frame.chunk_bytecode = 15;
            const int32_t index = (local_x << 11) | (local_z << 7) | y;
            if (chunk.object->data == NULL || chunk.object->data->blocks == NULL ||
                (size_t)index >= chunk.object->data->blocks_capacity) {
                (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World top-solid/liquid Chunk block backing");
                break;
            }
            id = chunk.object->data->blocks[(size_t)index];
        }
        frame.chunk_bytecode = 0;
        mc_material *material;
        if (!top_material(world, id, &material, &frame))
            break;
        frame.bytecode = 60;
        if (material == NULL) {
            (void)registry_null(world, &frame, "World top-solid/liquid Material receiver");
            break;
        }
        bool found = mc_material_c(material);
        if (!found) {
            frame.bytecode = 68;
            found = mc_material_d(material);
        }
        if (found) {
            height = y + 1;
            break;
        }
    }
    if (frame.status == MC_WORLD_STORAGE_OK)
        *out = height;
    return finish(world, &chunk, &frame, error);
}

static bool same_type_class(const mc_world_runtime_classes *classes,
                            const mc_world_light_type *type)
{
    return type == NULL || (classes != NULL && classes->light_types_initialized &&
                            (type == &classes->light_types[0] || type == &classes->light_types[1]));
}

mc_world_storage_status mc_world_runtime_saved_light(mc_world_runtime *world,
                                                     const mc_world_light_type *type, int32_t x,
                                                     int32_t y, int32_t z, int32_t *out,
                                                     mc_world_query_error *error)
{
    mc_world_query_error frame = empty_error();
    if (world != NULL && !same_type_class(world->classes, type)) {
        (void)fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World query Light class");
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    if (!admit(world, out != NULL, &frame)) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    query_chunk chunk = {0};
    int32_t value = 0;
    y = y < 0 ? 0 : y >= 128 ? 127 : y;
    if (x < -32000000 || x >= 32000000 || z < -32000000 || z > 32000000) {
        frame.bytecode = 54;
        if (type == NULL)
            (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World saved-light type receiver");
        else
            value = type->fields.c;
        goto done;
    }
    frame.bytecode = 74;
    frame.provider_bytecode = 6;
    bool present;
    if (mc_world_runtime_provider_contains(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4),
                                           &present, &frame.cause) != MC_WORLD_STORAGE_OK) {
        (void)storage_fail(&frame, &frame.cause);
        goto done;
    }
    frame.provider_bytecode = 0;
    if (!present)
        goto done;
    frame.bytecode = 87;
    if (!acquire(world, x, z, &chunk, &frame))
        goto done;
    frame.bytecode = 105;
    if (chunk.state->blank)
        goto done;
    const mc_world_light_type *selected;
    frame.chunk_bytecode = 1;
    if (mc_world_runtime_light_type_get(world->classes, MC_LIGHT_SKY, &selected, &frame.cause) !=
        MC_WORLD_STORAGE_OK) {
        (void)storage_fail(&frame, &frame.cause);
        goto done;
    }
    if (type == selected) {
        frame.chunk_bytecode = 15;
        (void)nibble(chunk.object->f, (int32_t)((uint32_t)x & 15u), y, (int32_t)((uint32_t)z & 15u),
                     &value, &frame);
        goto done;
    }
    frame.chunk_bytecode = 20;
    if (mc_world_runtime_light_type_get(world->classes, MC_LIGHT_BLOCK, &selected, &frame.cause) !=
        MC_WORLD_STORAGE_OK) {
        (void)storage_fail(&frame, &frame.cause);
        goto done;
    }
    if (type == selected) {
        frame.chunk_bytecode = 34;
        (void)nibble(chunk.object->g, (int32_t)((uint32_t)x & 15u), y, (int32_t)((uint32_t)z & 15u),
                     &value, &frame);
    }
done:
    if (frame.status == MC_WORLD_STORAGE_OK)
        *out = value;
    return finish(world, &chunk, &frame, error);
}

mc_world_storage_status mc_world_runtime_plant_light(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, int32_t *out,
                                                     mc_world_query_error *error)
{
    mc_world_query_error frame = empty_error();
    if (!admit(world, out != NULL, &frame)) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    query_chunk chunk = {0};
    int32_t value = 0;
    if (y < 0)
        goto done;
    if (y >= 128)
        y = 127;
    frame.bytecode = 23;
    if (!acquire(world, x, z, &chunk, &frame))
        goto done;
    frame.bytecode = 36;
    if (chunk.state->blank)
        goto done;
    frame.chunk_bytecode = 7;
    int32_t sky, block;
    if (!nibble(chunk.object->f, (int32_t)((uint32_t)x & 15u), y, (int32_t)((uint32_t)z & 15u),
                &sky, &frame))
        goto done;
    if (sky > 0)
        world->classes->chunk_a = true;
    frame.chunk_bytecode = 35;
    if (!nibble(chunk.object->g, (int32_t)((uint32_t)x & 15u), y, (int32_t)((uint32_t)z & 15u),
                &block, &frame))
        goto done;
    value = sky > block ? sky : block;
done:
    if (frame.status == MC_WORLD_STORAGE_OK)
        *out = value;
    return finish(world, &chunk, &frame, error);
}

mc_world_storage_status mc_world_runtime_sky_visible(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, bool *out,
                                                     mc_world_query_error *error)
{
    mc_world_query_error frame = empty_error();
    if (!admit(world, out != NULL, &frame)) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    query_chunk chunk = {0};
    bool value = false;
    frame.bytecode = 7;
    if (!acquire(world, x, z, &chunk, &frame))
        goto done;
    frame.bytecode = 19;
    if (!chunk.state->blank) {
        frame.chunk_bytecode = 10;
        if (chunk.object->h == NULL) {
            (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Chunk sky-visible height backing");
            goto done;
        }
        const size_t index = ((size_t)((uint32_t)z & 15u) << 4u) | ((uint32_t)x & 15u);
        value = y >= (int32_t)chunk.object->h[index];
    }
done:
    if (frame.status == MC_WORLD_STORAGE_OK)
        *out = value;
    return finish(world, &chunk, &frame, error);
}

bool mc_world_runtime_chunk_light_seen(const mc_world_runtime_classes *classes)
{
    return classes != NULL && classes->chunk_a;
}
