#include "mc/world/runtime/queries.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../chunk/object_state_internal.h"
#include "../storage/objects_internal.h"
#include "chunk_provider_state_internal.h"
#include "construction_internal.h"
#include "execution_children_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "queries_execution_internal.h"
#include "runtime_internal.h"
#include "saved_light_internal.h"

typedef struct query_chunk {
    mc_chunk_state *state;
    mc_world_chunk *object;
    mc_world_store *store;
    bool leased;
} query_chunk;

static const mc_world_query_error empty_error = {.registry = {.initializer_pc = -1,
                                                              .block_id = -1,
                                                              .constructor_pc = -1,
                                                              .active_initializer_pc = -1,
                                                              .item_argument = -1}};

static bool fail(mc_world_query_error *error, mc_world_storage_status status,
                 const char *operation) {
    error->status = status;
    error->operation = operation;
    error->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool storage_fail(mc_world_query_error *error, const mc_world_storage_error *cause) {
    error->status = cause->status;
    error->operation = cause->operation;
    error->cause = *cause;
    return false;
}

static bool registry_fail(mc_world_runtime *world, mc_world_query_error *error,
                          const mc_registry_error *cause, const char *operation) {
    error->registry = *cause;
    error->status =
        mc_world_runtime_registry_result(world, cause->status, cause, operation, &error->cause);
    error->operation = error->cause.operation;
    return false;
}

static bool registry_null(mc_world_runtime *world, mc_world_query_error *error,
                          const char *operation) {
    mc_registry_error cause = empty_error.registry;
    cause.status = MC_REGISTRY_NULL_REFERENCE;
    cause.message = operation;
    return registry_fail(world, error, &cause, operation);
}

static bool admit(mc_world_runtime *world, bool has_out, mc_world_query_error *error) {
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
                                      mc_world_query_error *frame, mc_world_query_error *error) {
    if (chunk->leased)
        (void)mc_world_store_release_chunk(chunk->store, chunk->state, NULL);
    if (frame->status != MC_WORLD_STORAGE_OK)
        world->last_error = frame->cause;
    mc_world_runtime_call_leave(world);
    if (error != NULL) {
        if (frame->status == MC_WORLD_STORAGE_OK)
            *error = empty_error;
        else
            *error = *frame;
    }
    return frame->status;
}

static bool query_records_quiet(const mc_world_runtime_execution_result *owned) {
    if (owned->root_pending || owned->math_failed)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (owned->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (owned->closes[i].used)
            return false;
    return true;
}

static void query_selected_view(mc_world_query_execution *execution) {
    if (!execution->owned->selection_ready)
        return;
    mc_entity_tick_execution_loader_view selected;
    if (mc_entity_tick_execution_loader_read(execution->execution, &execution->owned->selection,
                                             &selected))
        execution->acquisition.source_chunk =
            selected.status == MC_CHUNK_ACQUIRE_OK ? NULL : selected.failure;
    else {
        execution->acquisition.source_chunk = NULL;
        mc_world_runtime_execution_result_note(execution->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                               "World query selected loader view",
                                               execution->receiver_site);
    }
}

static bool query_acquisition_failed(mc_world_query_error *error,
                                     const mc_world_query_acquisition_failure *failure) {
    error->source_chunk = failure->source_chunk;
    error->population = failure->population;
    error->registry = failure->registry;
    if (failure->cause.status != MC_WORLD_STORAGE_OK)
        return storage_fail(error, &failure->cause);
    if (error->acquisition == MC_CHUNK_ACQUIRE_LIMIT ||
        error->acquisition == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
        return fail(error, MC_WORLD_STORAGE_LIMIT, "World query acquisition limit");
    return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World query acquisition");
}

/* World.c(II) invokes the provider's b getter, preserving pending on a hit
 * and the dedicated denied-load fallback. Store leases protect the captured
 * source local across later Registry active use, without blocking reentry. */
static bool acquire_query(mc_world_runtime *world, int32_t chunk_x, int32_t chunk_z,
                          query_chunk *out, mc_world_query_error *error,
                          mc_world_query_execution *execution) {
    error->provider_bytecode = 6;
    if (world->fields.v == NULL) {
        error->source_null = true;
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World query provider receiver");
    }
    const mc_chunk_provider *const provider = world->fields.v;
    /* Inspect only the four live bindings needed by this getter. The full
     * provider view has no callbacks and adds unrelated descriptor copies. */
    if (provider->world != world || &provider->cache != world->acquisition.cache ||
        &provider->pending != world->acquisition.pending || provider->store != world->options.store)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World query replacement provider binding");
    if (execution == NULL) {
        mc_world_query_acquisition_failure failure;
        error->acquisition =
            mc_world_runtime_get_query_failure(world, chunk_x, chunk_z, &out->state, &failure);
        if (error->acquisition != MC_CHUNK_ACQUIRE_OK)
            return query_acquisition_failed(error, &failure);
    } else {
        execution->get_entered = true;
        const mc_chunk_acquire_status status = mc_world_runtime_get_failure_in_execution(
            execution->execution, world, execution->code, chunk_x, chunk_z, &out->state,
            &execution->acquisition, execution->owned);
        error->acquisition = status;
        execution->get_status = status;
        execution->get_returned = true;
        /* Successful failure-only output remains untouched. Only an actually
         * failed getter supplies the full diagnostic and selected borrow. */
        if (status != MC_CHUNK_ACQUIRE_OK)
            query_selected_view(execution);
        execution->native_stopped = execution->owned->ownership_status != MC_WORLD_STORAGE_OK;
        if (status != MC_CHUNK_ACQUIRE_OK) {
            const mc_world_query_acquisition_failure failure =
                mc_world_query_acquisition_failure_of(&execution->acquisition);
            return query_acquisition_failed(error, &failure);
        }
    }
    if (execution != NULL) {
        if (!execution->native_stopped) {
            if (execution->owned->selection_ready &&
                execution->owned->selection.status == MC_CHUNK_ACQUIRE_OK &&
                query_records_quiet(execution->owned))
                execution->owned->selection_ready = false;
            else {
                mc_world_runtime_execution_result_note(
                    execution->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World query completed GET outcome", execution->receiver_site);
                execution->native_stopped = true;
            }
        }
        /* The raw getter succeeded; do not manufacture a Source error or
         * inspect its Chunk when native completion refused. */
        if (execution->native_stopped)
            return false;
    }
    error->provider_bytecode = 0;
    if (out->state == NULL)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World query Chunk receiver");
    if (out->state->blank)
        return true;
    out->store = world->fields.w;
    mc_world_storage_error cause;
    if (mc_world_store_lease_chunk_object(out->store, out->state, &out->object, &cause) !=
        MC_WORLD_STORAGE_OK)
        return storage_fail(error, &cause);
    if (out->object == NULL)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World query concrete Chunk");
    out->leased = true;
    return true;
}

static bool acquire(mc_world_runtime *world, int32_t x, int32_t z, query_chunk *out,
                    mc_world_query_error *error) {
    return acquire_query(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), out, error, NULL);
}

/* Every caller has already clamped or rejected Y. */
static size_t light_cell(int32_t x, int32_t y, int32_t z) {
    return (((uint32_t)x & 15u) << 11u) | (((uint32_t)z & 15u) << 7u) | (uint32_t)y;
}

static bool nibble(const mc_world_chunk_nibble *owner, size_t cell, int32_t *out,
                   mc_world_query_error *error) {
    if (owner == NULL || owner->a == NULL)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk query Nibble backing");
    const size_t index = cell >> 1u;
    if (index >= owner->length)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk query Nibble index");
    *out = (int32_t)(((uint32_t)owner->a[index] >> ((cell & 1u) * 4u)) & 15u);
    return true;
}

static bool top_material(mc_world_runtime *world, int32_t id, mc_material **out,
                         mc_world_query_error *error) {
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    mc_registry_error cause = empty_error.registry;
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
                                                          mc_world_query_error *error) {
    mc_world_query_error frame = empty_error;
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
                            const mc_world_light_type *type) {
    return type == NULL || (classes != NULL && classes->light_types_initialized &&
                            (type == &classes->light_types[0] || type == &classes->light_types[1]));
}

static mc_world_storage_status saved_light(mc_world_runtime *world, const mc_world_light_type *type,
                                           int32_t x, int32_t y, int32_t z, int32_t *out,
                                           mc_world_query_error *frame) {
    if (world != NULL && !same_type_class(world->classes, type)) {
        (void)fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World query Light class");
        return frame->status;
    }
    if (!admit(world, out != NULL, frame))
        return frame->status;
    query_chunk chunk = {0};
    int32_t value = 0;
    y = y < 0 ? 0 : y >= 128 ? 127 : y;
    if (x < -32000000 || x >= 32000000 || z < -32000000 || z > 32000000) {
        frame->bytecode = 54;
        if (type == NULL)
            (void)fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World saved-light type receiver");
        else
            value = type->fields.c;
        goto done;
    }
    frame->bytecode = 74;
    frame->provider_bytecode = 6;
    const int32_t chunk_x = mc_java_i32_shr(x, 4), chunk_z = mc_java_i32_shr(z, 4);
    bool present;
    if (mc_world_runtime_chunk_provider_contains(world, chunk_x, chunk_z, &present,
                                                 &frame->cause) != MC_WORLD_STORAGE_OK) {
        (void)storage_fail(frame, &frame->cause);
        goto done;
    }
    frame->provider_bytecode = 0;
    if (!present)
        goto done;
    frame->bytecode = 87;
    if (!acquire_query(world, chunk_x, chunk_z, &chunk, frame, NULL))
        goto done;
    frame->bytecode = 105;
    if (chunk.state->blank)
        goto done;
    const mc_world_light_type *selected;
    frame->chunk_bytecode = 1;
    if (mc_world_runtime_light_type_get(world->classes, MC_LIGHT_SKY, &selected, &frame->cause) !=
        MC_WORLD_STORAGE_OK) {
        (void)storage_fail(frame, &frame->cause);
        goto done;
    }
    if (type == selected) {
        frame->chunk_bytecode = 15;
        (void)nibble(chunk.object->f, light_cell(x, y, z), &value, frame);
        goto done;
    }
    frame->chunk_bytecode = 20;
    if (mc_world_runtime_light_type_get(world->classes, MC_LIGHT_BLOCK, &selected, &frame->cause) !=
        MC_WORLD_STORAGE_OK) {
        (void)storage_fail(frame, &frame->cause);
        goto done;
    }
    if (type == selected) {
        frame->chunk_bytecode = 34;
        (void)nibble(chunk.object->g, light_cell(x, y, z), &value, frame);
    }
done:
    if (frame->status == MC_WORLD_STORAGE_OK)
        *out = value;
    return finish(world, &chunk, frame, NULL);
}

mc_world_storage_status mc_world_runtime_saved_light(mc_world_runtime *world,
                                                     const mc_world_light_type *type, int32_t x,
                                                     int32_t y, int32_t z, int32_t *out,
                                                     mc_world_query_error *error) {
    mc_world_query_error frame = empty_error;
    const mc_world_storage_status status = saved_light(world, type, x, y, z, out, &frame);
    if (error != NULL)
        *error = status == MC_WORLD_STORAGE_OK ? empty_error : frame;
    return status;
}

mc_world_storage_status mc_world_runtime_saved_light_canonical(mc_world_runtime *world,
                                                               const mc_world_light_type *type,
                                                               int32_t x, int32_t y, int32_t z,
                                                               int32_t *out,
                                                               mc_world_query_error *error) {
    const mc_world_storage_status status = saved_light(world, type, x, y, z, out, error);
    /* Only these PCs become noncanonical on a successful saved-light query.
     * All other successful writes retain their initial zero/sentinel values. */
    if (status == MC_WORLD_STORAGE_OK)
        error->bytecode = error->provider_bytecode = error->chunk_bytecode = 0;
    return status;
}

static mc_world_storage_status plant_light(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                           int32_t *out, mc_world_query_error *error,
                                           mc_world_query_execution *execution) {
    mc_world_query_error frame = empty_error;
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
    if (!acquire_query(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &chunk, &frame,
                       execution))
        goto done;
    frame.bytecode = 36;
    if (chunk.state->blank)
        goto done;
    frame.chunk_bytecode = 7;
    const size_t cell = light_cell(x, y, z);
    int32_t sky, block;
    if (!nibble(chunk.object->f, cell, &sky, &frame))
        goto done;
    if (sky > 0) {
        mc_world_runtime_classes *const classes =
            execution == NULL ? world->classes : execution->code_classes;
        classes->chunk_a = true;
    }
    frame.chunk_bytecode = 35;
    if (!nibble(chunk.object->g, cell, &block, &frame))
        goto done;
    value = sky > block ? sky : block;
done:
    if (frame.status == MC_WORLD_STORAGE_OK && (execution == NULL || !execution->native_stopped))
        *out = value;
    return finish(world, &chunk, &frame, error);
}

mc_world_storage_status mc_world_runtime_plant_light(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, int32_t *out,
                                                     mc_world_query_error *error) {
    return plant_light(world, x, y, z, out, error, NULL);
}

static bool block_light_static(mc_world_runtime *world, const char *name, size_t slot,
                               unsigned int capture_pc, unsigned int field_pc, int32_t *out,
                               mc_world_query_error *error) {
    error->bytecode = capture_pc;
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    const mc_block *selected;
    if (classes != NULL && classes->entered == 0 && classes->blocks_phase == MC_REGISTRY_READY)
        selected = classes->blocks.named[slot];
    else {
        mc_registry_error cause = empty_error.registry;
        if (classes == NULL) {
            cause.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
            cause.message = "World block-light Registry binding";
            return registry_fail(world, error, &cause, cause.message);
        }
        if (mc_registry_block_static_get(classes, name, &selected, &cause) != MC_REGISTRY_OK)
            return registry_fail(world, error, &cause, "World block-light Block static active use");
    }
    error->bytecode = field_pc;
    if (selected == NULL)
        return registry_null(world, error, "World block-light static Block receiver");
    *out = selected->fields.bn;
    return true;
}

/* fd/dj.a(IIIZ)I. Only the true branch invokes this same method with false,
 * so each of its five source calls has independent acquisition and cleanup.
 * The enclosing public call holds the World through all synchronous reentry. */
static bool block_light_value(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                              bool neighbors, int32_t *out, query_chunk *chunk,
                              mc_world_query_error *error) {
    if (x < -32000000 || x >= 32000000 || z < -32000000 || z > 32000000) {
        *out = 15;
        return true;
    }
    if (neighbors) {
        error->bytecode = 36;
        int32_t id;
        mc_world_storage_error cause = {0};
        if (mc_world_runtime_block_get(world, x, y, z, &id, &cause) != MC_WORLD_STORAGE_OK)
            return storage_fail(error, &cause);

        static const struct {
            const char *name;
            size_t slot;
            unsigned int capture_pc, field_pc;
        } shapes[] = {
            {"al", 43, 43, 46}, {"aB", 59, 54, 57}, {"aI", 66, 65, 68}, {"au", 52, 76, 79}};

        bool samples_neighbors = false;
        for (size_t index = 0; index < sizeof(shapes) / sizeof(*shapes); ++index) {
            int32_t candidate = 0;
            if (!block_light_static(world, shapes[index].name, shapes[index].slot,
                                    shapes[index].capture_pc, shapes[index].field_pc, &candidate,
                                    error))
                return false;
            if (id == candidate) {
                samples_neighbors = true;
                break;
            }
        }
        if (samples_neighbors) {
            int32_t samples[5];
            static const unsigned int call_pc[5] = {92, 104, 116, 128, 140};
            for (size_t index = 0; index < 5; ++index) {
                int32_t sample_x = x, sample_y = y, sample_z = z;
                if (index == 0)
                    sample_y = mc_java_i32_add(y, 1);
                else if (index == 1)
                    sample_x = mc_java_i32_add(x, 1);
                else if (index == 2)
                    sample_x = mc_java_i32_sub(x, 1);
                else if (index == 3)
                    sample_z = mc_java_i32_add(z, 1);
                else
                    sample_z = mc_java_i32_sub(z, 1);
                error->bytecode = call_pc[index];
                error->chunk_bytecode = 0;
                if (!block_light_value(world, sample_x, sample_y, sample_z, false, &samples[index],
                                       chunk, error))
                    return false;
                if (chunk->leased)
                    (void)mc_world_store_release_chunk(chunk->store, chunk->state, NULL);
                *chunk = (query_chunk){0};
            }
            int32_t value = samples[0];
            for (size_t index = 1; index < 5; ++index) {
                if (samples[index] > value)
                    value = samples[index];
            }
            *out = value;
            return true;
        }
    }
    if (y < 0) {
        *out = 0;
        return true;
    }
    if (y >= 128)
        y = 127;
    error->bytecode = 215;
    if (!acquire(world, x, z, chunk, error))
        return false;
    const int32_t subtraction = world->fields.f;
    error->bytecode = 239;
    if (chunk->state->blank) {
        *out = 0;
        return true;
    }
    error->chunk_bytecode = 7;
    const size_t cell = light_cell(x, y, z);
    int32_t sky, block;
    if (!nibble(chunk->object->f, cell, &sky, error))
        return false;
    if (sky > 0)
        world->classes->chunk_a = true;
    sky = mc_java_i32_sub(sky, subtraction);
    error->chunk_bytecode = 35;
    if (!nibble(chunk->object->g, cell, &block, error))
        return false;
    *out = sky > block ? sky : block;
    return true;
}

mc_world_storage_status mc_world_runtime_block_light(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, int32_t *out,
                                                     mc_world_query_error *error) {
    mc_world_query_error frame = empty_error;
    if (!admit(world, out != NULL, &frame)) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    query_chunk chunk = {0};
    int32_t value;
    if (block_light_value(world, x, y, z, true, &value, &chunk, &frame))
        *out = value;
    return finish(world, &chunk, &frame, error);
}

static mc_world_storage_status sky_visible(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                           bool *out, mc_world_query_error *error,
                                           mc_world_query_execution *execution) {
    mc_world_query_error frame = empty_error;
    if (!admit(world, out != NULL, &frame)) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    query_chunk chunk = {0};
    bool value = false;
    frame.bytecode = 7;
    if (!acquire_query(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &chunk, &frame,
                       execution))
        goto done;
    frame.bytecode = 19;
    if (!chunk.state->blank) {
        frame.chunk_bytecode = 10;
        if (chunk.object->h == NULL) {
            frame.source_null = true;
            (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Chunk sky-visible height backing");
            goto done;
        }
        const size_t index = ((size_t)((uint32_t)z & 15u) << 4u) | ((uint32_t)x & 15u);
        value = y >= (int32_t)chunk.object->h[index];
    }
done:
    if (frame.status == MC_WORLD_STORAGE_OK && (execution == NULL || !execution->native_stopped))
        *out = value;
    return finish(world, &chunk, &frame, error);
}

mc_world_storage_status mc_world_runtime_sky_visible(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, bool *out,
                                                     mc_world_query_error *error) {
    return sky_visible(world, x, y, z, out, error, NULL);
}

void mc_world_query_execution_init(mc_world_query_execution *frame,
                                   mc_entity_tick_execution *execution,
                                   mc_world_runtime_classes *code_classes,
                                   mc_world_runtime_execution_code code,
                                   mc_entity_tick_execution_site site,
                                   mc_world_runtime_execution_result *owned) {
    if (frame != NULL)
        *frame = (mc_world_query_execution){.execution = execution,
                                            .code_classes = code_classes,
                                            .code = code,
                                            .receiver_site = site,
                                            .owned = owned};
}

static bool query_execution_enter(mc_world_query_execution *frame, mc_world_runtime *world,
                                  const void *out, const mc_world_storage_status *status) {
    if (frame == NULL)
        return false;
    if (frame->execution == NULL || frame->execution->identity != frame->execution ||
        world == NULL || frame->code_classes == NULL || frame->owned == NULL || out == NULL ||
        status == NULL || frame->entered) {
        mc_world_runtime_execution_result_note(frame->owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "World query execution/fresh outputs required",
                                               frame->receiver_site);
        return false;
    }
    if (frame->owned->ownership_status != MC_WORLD_STORAGE_OK || frame->owned->selection_ready ||
        !query_records_quiet(frame->owned)) {
        mc_world_runtime_execution_result_note(frame->owned, MC_WORLD_STORAGE_BUSY,
                                               "World query quiet result required",
                                               frame->receiver_site);
        return false;
    }
    mc_world_access_failure_lease *unused = NULL;
    const mc_world_storage_status admitted =
        mc_world_execution_child_lease(frame->execution, world, &unused);
    if (admitted != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(
            frame->owned, admitted, "World query evaluated World ownership", frame->receiver_site);
        return false;
    }
    frame->entered = true;
    frame->detail = empty_error;
    return true;
}

static bool query_execution_finish(mc_world_query_execution *frame, mc_world_storage_status actual,
                                   mc_world_storage_status *status) {
    frame->status = actual;
    *status = actual;
    frame->completed = actual == MC_WORLD_STORAGE_OK && !frame->native_stopped &&
                       frame->owned->ownership_status == MC_WORLD_STORAGE_OK;
    return frame->completed;
}

bool mc_world_runtime_plant_light_in_execution(mc_world_query_execution *frame,
                                               mc_world_runtime *world, int32_t x, int32_t y,
                                               int32_t z, int32_t *out,
                                               mc_world_storage_status *status) {
    if (!query_execution_enter(frame, world, out, status))
        return false;
    const mc_world_storage_status actual = plant_light(world, x, y, z, out, &frame->detail, frame);
    return query_execution_finish(frame, actual, status);
}

bool mc_world_runtime_sky_visible_in_execution(mc_world_query_execution *frame,
                                               mc_world_runtime *world, int32_t x, int32_t y,
                                               int32_t z, bool *out,
                                               mc_world_storage_status *status) {
    if (!query_execution_enter(frame, world, out, status))
        return false;
    const mc_world_storage_status actual = sky_visible(world, x, y, z, out, &frame->detail, frame);
    return query_execution_finish(frame, actual, status);
}

bool mc_world_runtime_chunk_light_seen(const mc_world_runtime_classes *classes) {
    return classes != NULL && classes->chunk_a;
}
