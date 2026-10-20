#include "mc/world/runtime/entities.h"

#include "../../entity/entity_internal.h"
#include "../chunk/object_internal.h"
#include "../storage/objects_internal.h"
#include "construction_internal.h"
#include "entity_publication_internal.h"
#include "mc/core/math/math.h"
#include "provider_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64) && !(defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#error "World Entity coordinates require SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "World Entity coordinates require strict floating transformations"
#endif
#include <emmintrin.h>

static mc_world_storage_status entity_result(mc_world_entity_error *out,
                                             const mc_world_entity_error *frame)
{
    if (out != NULL)
        *out = *frame;
    return frame->status;
}

static bool entity_fail(mc_world_entity_error *frame, mc_world_storage_status status,
                        const char *operation)
{
    frame->status = status;
    frame->operation = operation;
    frame->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool entity_cause(mc_world_entity_error *frame, mc_world_storage_error cause)
{
    frame->status = cause.status;
    frame->operation = cause.operation;
    frame->cause = cause;
    return false;
}

static mc_world_storage_status ownership_status(mc_entity_status status)
{
    switch (status) {
        case MC_ENTITY_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_ENTITY_ALLOCATION_FAILED:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_ENTITY_BUSY:
            return MC_WORLD_STORAGE_BUSY;
        case MC_ENTITY_INVALID_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        default:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
}

static bool list_shape(const mc_world_reference_list *list)
{
    return list != NULL && list->count <= list->capacity && list->capacity <= (size_t)INT32_MAX &&
           list->capacity <= SIZE_MAX / sizeof(*list->items) &&
           ((list->capacity == 0 && list->items == NULL) ||
            (list->capacity != 0 && list->items != NULL));
}

bool mc_world_runtime_entity_list_valid(const mc_world_runtime *world)
{
    if (world == NULL)
        return false;
    const mc_world_reference_list *list = world->fields.b;
    if (list != world->entity_membership.list)
        return false;
    if (list == NULL)
        return world->entity_membership.entries == NULL && world->entity_membership.count == 0 &&
               world->entity_membership.capacity == 0;
    if (!list_shape(list) || world->entity_membership.count > world->entity_membership.capacity ||
        world->entity_membership.capacity > (size_t)INT32_MAX ||
        world->entity_membership.capacity > SIZE_MAX / sizeof(*world->entity_membership.entries) ||
        list->count != world->entity_membership.count ||
        list->capacity != world->entity_membership.capacity ||
        ((world->entity_membership.capacity == 0) != (world->entity_membership.entries == NULL)) ||
        (list->items != NULL && (void *)list->items == (void *)world->entity_membership.entries))
        return false;
    const size_t source_bytes = list->capacity * sizeof(*list->items);
    const size_t hold_bytes =
        world->entity_membership.capacity * sizeof(*world->entity_membership.entries);
    if (source_bytes > SIZE_MAX - hold_bytes ||
        source_bytes + hold_bytes > world->construction.direct_allocation_bytes)
        return false;
    for (size_t index = 0; index < list->count; ++index)
        if (world->entity_membership.entries[index] == NULL ||
            list->items[index] != world->entity_membership.entries[index])
            return false;
    return true;
}

void mc_world_runtime_entity_list_release(mc_world_runtime *world)
{
    mc_world_reference_list *list = world->entity_membership.list;
    if (list == NULL)
        return;
    const size_t bytes =
        list->capacity * sizeof(*list->items) +
        world->entity_membership.capacity * sizeof(*world->entity_membership.entries);
    const size_t count = world->entity_membership.count;
    mc_entity **entries = world->entity_membership.entries;
    world->entity_membership.entries = NULL;
    world->entity_membership.count = 0;
    world->entity_membership.capacity = 0;
    for (size_t index = 0; index < count; ++index) {
        list->items[index] = NULL;
        mc_entity_release(entries[index]);
    }
    free(entries);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    world->construction.direct_allocation_bytes -= bytes;
}

static bool append_entity(mc_world_runtime *world, mc_entity *entity, mc_world_entity_error *frame)
{
    mc_world_reference_list *list = world->fields.b; /* getfield91 */
    frame->bytecode = 95;
    if (!mc_world_runtime_entity_list_valid(world) || list == NULL)
        return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World Entity List ownership");
    if (list->count == (size_t)INT32_MAX)
        return entity_fail(frame, MC_WORLD_STORAGE_LIMIT, "World Entity List size");

    void **items = list->items;
    mc_entity **holds = world->entity_membership.entries;
    size_t capacity = list->capacity;
    size_t bytes = 0;
    if (list->count == capacity) {
        capacity = capacity == 0                       ? 4u
                   : capacity > (size_t)INT32_MAX / 2u ? (size_t)INT32_MAX
                                                       : capacity * 2u;
        if (capacity <= list->count || capacity > SIZE_MAX / (sizeof(*items) + sizeof(*holds)))
            return entity_fail(frame, MC_WORLD_STORAGE_LIMIT, "World Entity List capacity");
        bytes = capacity * (sizeof(*items) + sizeof(*holds));
        const size_t used = world->construction.direct_allocation_bytes;
        const size_t limit = world->construction_limits.max_direct_allocation_bytes;
        if (used > limit || bytes > limit - used)
            return entity_fail(frame, MC_WORLD_STORAGE_LIMIT,
                               "World Entity List allocation budget");
        /* Native peak budget includes the old and both new spans. Distinct
         * typed hold storage proves ownership of the actual void* List values;
         * this is not a claim about Java ArrayList layout or OOM timing. */
        items = malloc(capacity * sizeof(*items));
        holds = malloc(capacity * sizeof(*holds));
        if (items == NULL || holds == NULL) {
            free(items);
            free(holds);
            return entity_fail(frame, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                               "World Entity List allocation");
        }
        world->construction.direct_allocation_bytes += bytes;
        if (list->count != 0) {
            memcpy(items, list->items, list->count * sizeof(*items));
            memcpy(holds, world->entity_membership.entries, list->count * sizeof(*holds));
        }
    }
    const mc_entity_status held = mc_entity_retain(entity);
    if (held != MC_ENTITY_OK) {
        if (bytes != 0) {
            free(items);
            free(holds);
            world->construction.direct_allocation_bytes -= bytes;
        }
        frame->entity =
            (mc_entity_error){.status = held, .operation = "World List Entity ownership"};
        return entity_fail(frame, ownership_status(held), "World List Entity ownership");
    }
    items[list->count] = entity;
    holds[list->count] = entity;
    if (bytes != 0) {
        const size_t old_bytes = list->capacity * sizeof(*list->items) +
                                 world->entity_membership.capacity * sizeof(*holds);
        free(list->items);
        free(world->entity_membership.entries);
        list->items = items;
        list->capacity = capacity;
        world->entity_membership.entries = holds;
        world->entity_membership.capacity = capacity;
        world->construction.direct_allocation_bytes -= old_bytes;
    }
    ++list->count;
    ++world->entity_membership.count;
    return true;
}

static bool observe_entity(mc_world_runtime *world, mc_world_entity_error *frame)
{
    frame->bytecode = 103;
    frame->observer_bytecode = 7;
    const mc_world_reference_list *list = world->fields.u; /* getfield4 */
    if (!list_shape(list))
        return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World Entity observer List size");
    if (list->count == 0)
        return true; /* Actual base fd/dj.c(Entity)V return40. */
    frame->observer_bytecode = 20;
    list = world->fields.u; /* Separate current u16 before get20. */
    if (!list_shape(list) || list->count == 0)
        return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World Entity observer List get");
    frame->observer_bytecode = list->items[0] == NULL ? 29u : 25u;
    return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Entity observer dispatch");
}

static bool enter_entity(mc_world_runtime *world, mc_entity *entity, mc_world_entity_error *frame)
{
    if (world == NULL)
        return entity_fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World Entity receiver");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Entity phase");
    if (entity != NULL && (world->classes == NULL || world->classes->entities != entity->classes))
        return entity_fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "World Entity application class");
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK)
        return entity_cause(frame, cause);
    if (entity != NULL) {
        const mc_entity_status held = mc_entity_retain(entity);
        if (held != MC_ENTITY_OK) {
            mc_world_runtime_call_leave(world);
            frame->entity =
                (mc_entity_error){.status = held, .operation = "World call Entity ownership"};
            return entity_fail(frame, ownership_status(held), "World call Entity ownership");
        }
    }
    return true;
}

static bool add_to_chunk(mc_world_runtime *world, mc_chunk_state *state, mc_entity *entity,
                         mc_world_entity_error *frame)
{
    if (state->blank)
        return true; /* Actual li/hg.a(Entity)V return0, including NULL. */
    mc_world_chunk *chunk = mc_world_store_chunk_object(world->fields.w, state);
    if (chunk == NULL)
        return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World Entity concrete Chunk");
    mc_world_storage_error cause = {0};
    if (mc_world_store_lease_chunk(world->fields.w, state, &cause) != MC_WORLD_STORAGE_OK)
        return entity_cause(frame, cause);
    mc_world_chunk_entity_error detail = {0};
    const mc_world_storage_status status = mc_world_chunk_insert_entity(chunk, entity, &detail);
    (void)mc_world_store_release_chunk(world->fields.w, state, NULL);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    frame->chunk_bytecode = detail.bytecode;
    frame->entity = detail.entity;
    return entity_cause(frame, detail.cause);
}

static bool insert_entity(mc_world_runtime *world, mc_entity *entity, bool *value,
                          mc_world_entity_error *frame)
{
    frame->bytecode = 1;
    mc_entity_base_fields *fields = mc_entity_runtime_fields(entity);
    if (fields == NULL)
        return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Entity x receiver");
    const double x = fields->x;
    frame->bytecode = 7;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World Entity coordinate FP admission");
    const int32_t chunk_x = mc_math_floor_double(x / 16.0); /* floor8 */
    frame->bytecode = 13;
    fields = mc_entity_runtime_fields(entity);
    const int32_t chunk_z = mc_math_floor_double(fields->z / 16.0); /* floor20 */
    /* Current actual Entities are hl/ez ItemEntities, so the two distinct
     * source instanceof gs/em tests28/52 both return false. No fabricated
     * player flag selects the still-unimplemented player/sleep descendants. */
    bool present;
    frame->bytecode = 45;
    frame->presence_bytecode = 6;
    frame->provider_bytecode = 6;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_provider_contains(world, chunk_x, chunk_z, &present, &cause) !=
        MC_WORLD_STORAGE_OK)
        return entity_cause(frame, cause);
    if (!present) {
        *value = false; /* Actual false108/return109. */
        return true;
    }
    frame->presence_bytecode = 0;
    frame->bytecode = 83;
    mc_chunk_state *state = NULL;
    /* World.c invokes provider b: a cached hit preserves pending work. */
    world->last_error = (mc_world_storage_error){0};
    frame->acquisition = mc_world_runtime_get(world, chunk_x, chunk_z, &state);
    if (frame->acquisition != MC_CHUNK_ACQUIRE_OK) {
        if (frame->acquisition == MC_CHUNK_ACQUIRE_LIMIT ||
            frame->acquisition == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
            return entity_fail(frame, MC_WORLD_STORAGE_LIMIT,
                               "World Entity Chunk acquisition limit");
        if (world->last_error.status != MC_WORLD_STORAGE_OK)
            return entity_cause(frame, world->last_error);
        return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World Entity Chunk acquisition");
    }
    frame->provider_bytecode = 0;
    frame->bytecode = 87;
    if (state == NULL)
        return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World Entity acquired Chunk receiver");
    if (!add_to_chunk(world, state, entity, frame) || !append_entity(world, entity, frame) ||
        !observe_entity(world, frame))
        return false;
    *value = true;
    return true;
}

static mc_world_storage_status finish_entity(mc_world_runtime *world, mc_entity *entity,
                                             bool completed, mc_world_entity_error *frame,
                                             mc_world_entity_error *error)
{
    if (completed) {
        *frame = (mc_world_entity_error){0};
        world->last_error = (mc_world_storage_error){0};
    } else
        world->last_error = frame->cause;
    mc_entity_release(entity);
    mc_world_runtime_call_leave(world);
    return entity_result(error, frame);
}

mc_world_storage_status mc_world_runtime_insert_entity(mc_world_runtime *world, mc_entity *entity,
                                                       bool *out, mc_world_entity_error *error)
{
    mc_world_entity_error frame = {0};
    if (out == NULL) {
        (void)entity_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World Entity output");
        return entity_result(error, &frame);
    }
    if (!enter_entity(world, entity, &frame))
        return entity_result(error, &frame);
    bool value = false;
    const bool completed = insert_entity(world, entity, &value, &frame);
    if (completed)
        *out = value;
    return finish_entity(world, entity, completed, &frame, error);
}

mc_world_storage_status mc_world_runtime_chunk_add_entity(mc_world_chunk *chunk, mc_entity *entity,
                                                          mc_world_entity_error *error)
{
    mc_world_entity_error frame = {0};
    mc_world_chunk_fields fields;
    if (mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK) {
        (void)entity_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Chunk Entity receiver");
        return entity_result(error, &frame);
    }
    mc_world_runtime *world = fields.d;
    if (!enter_entity(world, entity, &frame))
        return entity_result(error, &frame);
    bool completed = false;
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    if (mc_world_store_chunk_object(world->fields.w, state) != chunk)
        (void)entity_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Chunk Entity store identity");
    else
        completed = add_to_chunk(world, state, entity, &frame);
    return finish_entity(world, entity, completed, &frame, error);
}
