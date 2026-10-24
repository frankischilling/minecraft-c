#include "mc/world/runtime/entities.h"

#include "../../entity/entity_internal.h"
#include "../chunk/object_internal.h"
#include "../storage/objects_internal.h"
#include "chunk_provider_internal.h"
#include "construction_internal.h"
#include "dedicated/entity_index.h"
#include "entity_publication_internal.h"
#include "mc/core/math/math.h"
#include "world_access_internal.h"

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

mc_world_entity_batch_error mc_world_entity_batch_empty_error(void) {
    return (mc_world_entity_batch_error){.index = SIZE_MAX};
}

static mc_world_storage_status entity_result(mc_world_entity_error *out,
                                             const mc_world_entity_error *frame) {
    if (out != NULL)
        *out = *frame;
    return frame->status;
}

static bool entity_fail(mc_world_entity_error *frame, mc_world_storage_status status,
                        const char *operation) {
    frame->status = status;
    frame->operation = operation;
    frame->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool entity_cause(mc_world_entity_error *frame, mc_world_storage_error cause) {
    frame->status = cause.status;
    frame->operation = cause.operation;
    frame->cause = cause;
    return false;
}

static mc_world_storage_status ownership_status(mc_entity_status status) {
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

static bool list_shape(const mc_world_reference_list *list) {
    return list != NULL && list->count <= list->capacity && list->capacity <= (size_t)INT32_MAX &&
           list->capacity <= SIZE_MAX / sizeof(*list->items) &&
           ((list->capacity == 0 && list->items == NULL) ||
            (list->capacity != 0 && list->items != NULL));
}

static bool entity_ledger_valid(const mc_world_runtime *world,
                                const mc_world_entity_list_ledger *ledger, size_t *out) {
    const mc_world_reference_list *list = ledger->list;
    if (list == NULL) {
        *out = 0;
        return ledger->entries == NULL && ledger->count == 0 && ledger->capacity == 0;
    }
    if (!list_shape(list) || ledger->count > ledger->capacity ||
        ledger->capacity > (size_t)INT32_MAX ||
        ledger->capacity > SIZE_MAX / sizeof(*ledger->entries) || list->count != ledger->count ||
        list->capacity != ledger->capacity ||
        ((ledger->capacity == 0) != (ledger->entries == NULL)) ||
        (list->items != NULL && (void *)list->items == (void *)ledger->entries))
        return false;
    const size_t source_bytes = list->capacity * sizeof(*list->items);
    const size_t hold_bytes = ledger->capacity * sizeof(*ledger->entries);
    if (source_bytes > SIZE_MAX - hold_bytes ||
        source_bytes + hold_bytes > world->construction.direct_allocation_bytes)
        return false;
    for (size_t index = 0; index < list->count; ++index)
        if (list->items[index] != ledger->entries[index])
            return false;
    *out = source_bytes + hold_bytes;
    return true;
}

bool mc_world_runtime_entity_list_valid(const mc_world_runtime *world) {
    if (world == NULL)
        return false;
    const mc_world_entity_list_ledger *a = &world->entity_membership;
    const mc_world_entity_list_ledger *b = &world->entity_deferral;
    size_t a_bytes, b_bytes;
    if (!entity_ledger_valid(world, a, &a_bytes) || !entity_ledger_valid(world, b, &b_bytes) ||
        (a->list != NULL && a->list == b->list) || a_bytes > SIZE_MAX - b_bytes ||
        a_bytes + b_bytes > world->construction.direct_allocation_bytes)
        return false;
    const void *spans[] = {a->list == NULL ? NULL : (void *)a->list->items, (void *)a->entries,
                           b->list == NULL ? NULL : (void *)b->list->items, (void *)b->entries};
    for (size_t i = 0; i < sizeof(spans) / sizeof(spans[0]); ++i)
        for (size_t j = 0; j < i; ++j)
            if (spans[i] != NULL && spans[i] == spans[j])
                return false;
    return true;
}

static void entity_ledger_release(mc_world_runtime *world, mc_world_entity_list_ledger *ledger) {
    mc_world_reference_list *list = ledger->list;
    if (list == NULL)
        return;
    const size_t bytes =
        list->capacity * sizeof(*list->items) + ledger->capacity * sizeof(*ledger->entries);
    const size_t count = ledger->count;
    mc_entity **entries = ledger->entries;
    ledger->entries = NULL;
    ledger->count = 0;
    ledger->capacity = 0;
    for (size_t index = 0; index < count; ++index) {
        list->items[index] = NULL;
        if (entries[index] != NULL)
            mc_entity_release(entries[index]);
    }
    free(entries);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    world->construction.direct_allocation_bytes -= bytes;
}

void mc_world_runtime_entity_list_release(mc_world_runtime *world) {
    entity_ledger_release(world, &world->entity_membership);
    entity_ledger_release(world, &world->entity_deferral);
}

static bool append_entity(mc_world_runtime *world, mc_entity *entity,
                          mc_world_entity_error *frame) {
    mc_world_reference_list *list = world->fields.b; /* getfield91 */
    frame->bytecode = 95;
    if (!mc_world_runtime_entity_list_valid(world) || list == NULL ||
        list != world->entity_membership.list)
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

static bool observe_base_entity_at(mc_world_runtime *world, mc_entity *entity,
                                   mc_world_entity_error *frame, unsigned int caller_bytecode) {
    frame->bytecode = caller_bytecode;
    for (size_t index = 0;; ++index) {
        mc_world_access *world_access = NULL;
        bool present = false;
        mc_world_storage_error cause = {0};
        if (!mc_world_access_next(world, index, 7, 20, 25, &world_access, &present,
                                  &frame->world_access_bytecode, &cause))
            return entity_cause(frame, cause);
        if (!present)
            return true; /* Actual base fd/dj.c(Entity)V return40. */
        frame->world_access_bytecode = 29;
        if (world_access == NULL || world_access->callbacks.entity_added == NULL) {
            mc_world_access_release_held(world_access);
            return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "World Entity WorldAccess invocation");
        }
        if (entity != NULL) {
            const mc_entity_status held = mc_entity_retain(entity);
            if (held != MC_ENTITY_OK) {
                mc_world_access_release_held(world_access);
                frame->entity =
                    (mc_entity_error){.status = held, .operation = "WorldAccess Entity hold"};
                return entity_fail(frame, ownership_status(held), frame->entity.operation);
            }
        }
        const mc_world_storage_status status =
            world_access->callbacks.entity_added(world_access->context, world, entity, &cause);
        mc_entity_release(entity);
        mc_world_access_release_held(world_access);
        if (status != MC_WORLD_STORAGE_OK)
            return entity_cause(frame, mc_world_access_callback_error(status, cause));
        /* iinc34 follows the callback. Reload both current u and size7 on the
         * next iteration; synchronous removals/appends remain observable. */
    }
}

static bool observe_entity_at(mc_world_runtime *world, mc_entity *entity,
                              mc_world_entity_error *frame, unsigned int caller_bytecode) {
    if (!observe_base_entity_at(world, entity, frame, caller_bytecode))
        return false;
    if (world->construction.entry != MC_WORLD_RUNTIME_SERVER_DEDICATED)
        return true;
    mc_world_entity_index *index = world->dedicated.G; /* getfield6 */
    frame->dedicated_bytecode = 10;
    mc_entity_base_fields *fields = mc_entity_runtime_fields(entity);
    if (fields == NULL)
        return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Dedicated World added Entity ID receiver");
    const int32_t id = fields->id;
    frame->dedicated_bytecode = 14;
    mc_world_entity_index_error cause;
    if (mc_world_entity_index_put(index, id, entity, &cause) != MC_WORLD_STORAGE_OK) {
        frame->entity.status = cause.entity_status;
        frame->index_bytecode = cause.bytecode;
        return entity_cause(frame, cause.cause);
    }
    frame->dedicated_bytecode = frame->index_bytecode = 0;
    return true;
}

static bool append_entity_batch(mc_world_runtime *world, const mc_world_chunk_entity_list *source,
                                bool deferred, mc_world_entity_batch_error *frame) {
    mc_world_entity_error *publication = &frame->publication;
    publication->bytecode = 5;
    mc_world_reference_list *list = deferred ? world->fields.D : world->fields.b;
    if (list == NULL)
        return entity_fail(publication, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World addAll destination is null");
    if (list->count > list->capacity || list->capacity > SIZE_MAX / sizeof(*list->items) ||
        (list->capacity != 0 && list->items == NULL))
        return entity_fail(publication, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World addAll destination metadata");
    size_t count;
    if (mc_world_chunk_entity_count(source, &count) != MC_WORLD_STORAGE_OK)
        return entity_fail(publication, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World addAll Entity input is null");
    if (count == 0)
        return true; /* Preserve the existing finite empty-source no-op domain. */
    mc_world_entity_list_ledger *ledger =
        deferred ? &world->entity_deferral : &world->entity_membership;
    if (!mc_world_runtime_entity_list_valid(world) || list != ledger->list)
        return entity_fail(publication, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World addAll Entity ownership");
    if (count > (size_t)INT32_MAX - list->count || count > SIZE_MAX / sizeof(mc_entity *))
        return entity_fail(publication, MC_WORLD_STORAGE_LIMIT, "World addAll Entity size");
    const size_t target = list->count + count;
    size_t capacity = list->capacity;
    const bool grow = target > capacity;
    if (grow) {
        capacity = capacity == 0 ? 4u : capacity;
        while (capacity < target)
            capacity = capacity > (size_t)INT32_MAX / 2u ? (size_t)INT32_MAX : capacity * 2u;
    }
    if (capacity > SIZE_MAX / (sizeof(void *) + sizeof(mc_entity *)))
        return entity_fail(publication, MC_WORLD_STORAGE_LIMIT, "World addAll Entity capacity");
    const size_t new_bytes = grow ? capacity * (sizeof(void *) + sizeof(mc_entity *)) : 0;
    const size_t scratch_bytes = count * sizeof(mc_entity *);
    const size_t used = world->construction.direct_allocation_bytes;
    const size_t limit = world->construction_limits.max_direct_allocation_bytes;
    if (used > limit || scratch_bytes > limit - used || new_bytes > limit - used - scratch_bytes)
        return entity_fail(publication, MC_WORLD_STORAGE_LIMIT, "World addAll Entity peak budget");
    mc_entity **prepared = malloc(scratch_bytes);
    void **items = grow ? malloc(capacity * sizeof(*items)) : list->items;
    mc_entity **holds = grow ? malloc(capacity * sizeof(*holds)) : ledger->entries;
    if (prepared == NULL || items == NULL || holds == NULL) {
        free(prepared);
        if (grow) {
            free(items);
            free(holds);
        }
        return entity_fail(publication, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                           "World addAll Entity allocation");
    }
    const size_t temporary_bytes = scratch_bytes + new_bytes;
    world->construction.direct_allocation_bytes += temporary_bytes;
    if (grow && list->count != 0) {
        memcpy(items, list->items, list->count * sizeof(*items));
        memcpy(holds, ledger->entries, list->count * sizeof(*holds));
    }
    size_t prepared_count = 0;
    for (size_t index = 0; index < count; ++index) {
        frame->index = index;
        mc_entity *entity;
        if (mc_world_chunk_entity_at(source, index, &entity) != MC_WORLD_STORAGE_OK) {
            entity_fail(publication, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World addAll typed source occurrence");
            goto failed;
        }
        if (entity != NULL) {
            const mc_entity_status status = mc_entity_retain(entity);
            if (status != MC_ENTITY_OK) {
                publication->entity =
                    (mc_entity_error){.status = status, .operation = "World addAll Entity hold"};
                entity_fail(publication, ownership_status(status), "World addAll Entity hold");
                goto failed;
            }
        }
        prepared[prepared_count++] = entity;
    }
    /* All actual occurrence holds and capacity requests succeeded. Publish the
     * entire addAll before any source WorldAccess call, never one Entity at a time.
     * No Entity fields/World binding/class or Chunk location are inspected. */
    for (size_t index = 0; index < count; ++index) {
        items[list->count + index] = prepared[index];
        holds[list->count + index] = prepared[index];
    }
    if (grow) {
        const size_t old_bytes =
            list->capacity * sizeof(*list->items) + ledger->capacity * sizeof(*ledger->entries);
        free(list->items);
        free(ledger->entries);
        list->items = items;
        ledger->entries = holds;
        list->capacity = capacity;
        ledger->capacity = capacity;
        world->construction.direct_allocation_bytes -= old_bytes;
    }
    list->count = target;
    ledger->count = target;
    free(prepared);
    world->construction.direct_allocation_bytes -= scratch_bytes;
    frame->index = SIZE_MAX;
    return true;

failed:
    for (size_t index = 0; index < prepared_count; ++index)
        if (prepared[index] != NULL)
            mc_entity_release(prepared[index]);
    free(prepared);
    if (grow) {
        free(items);
        free(holds);
    }
    world->construction.direct_allocation_bytes -= temporary_bytes;
    return false;
}

mc_world_storage_status mc_world_runtime_entity_batch_held(mc_world_runtime *world,
                                                           const mc_world_chunk_entity_list *source,
                                                           bool deferred,
                                                           mc_world_entity_batch_error *error) {
    mc_world_entity_batch_error frame = mc_world_entity_batch_empty_error();
    bool complete = false;
    if (world == NULL)
        entity_fail(&frame.publication, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World batch receiver");
    else if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
             world->construction.phase == MC_WORLD_RUNTIME_RETIRED ||
             world->entity_containers_retired)
        entity_fail(&frame.publication, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World batch native phase");
    else if (append_entity_batch(world, source, deferred, &frame)) {
        complete = true;
        for (size_t index = 0; !deferred; ++index) {
            size_t count;
            frame.index = index;
            frame.publication.bytecode = 15;
            if (mc_world_chunk_entity_count(source, &count) != MC_WORLD_STORAGE_OK) {
                entity_fail(&frame.publication, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "World batch live input size");
                complete = false;
                break;
            }
            if (index >= count)
                break;
            frame.publication.bytecode = 26;
            mc_entity *entity;
            if (mc_world_chunk_entity_at(source, index, &entity) != MC_WORLD_STORAGE_OK) {
                entity_fail(&frame.publication, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "World batch live input get");
                complete = false;
                break;
            }
            /* Source cast31 accepts actual typed Entity/NULL; the dedicated
             * override reads its ID only after the base WorldAccess completes. */
            if (!observe_entity_at(world, entity, &frame.publication, 34)) {
                complete = false;
                break;
            }
        }
    }
    if (complete)
        frame = mc_world_entity_batch_empty_error();
    if (error != NULL)
        *error = frame;
    return frame.publication.status;
}

mc_world_storage_status mc_world_runtime_entity_by_id(mc_world_runtime *world, int32_t id,
                                                      mc_entity **out,
                                                      mc_world_entity_error *error) {
    mc_world_entity_error frame = {0};
    if (world == NULL || out == NULL)
        entity_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Dedicated Entity lookup arguments");
    else if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
             world->construction.entry != MC_WORLD_RUNTIME_SERVER_DEDICATED)
        entity_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Dedicated Entity lookup owner");
    else {
        mc_world_storage_error cause;
        if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK)
            entity_cause(&frame, cause);
        else {
            bool present;
            mc_entity *value;
            mc_world_entity_index_error index_error;
            frame.dedicated_bytecode = 5;
            if (mc_world_entity_index_get(world->dedicated.G, id, &present, &value, &index_error) !=
                MC_WORLD_STORAGE_OK) {
                frame.index_bytecode = index_error.bytecode;
                entity_cause(&frame, index_error.cause);
            } else {
                *out = value;
                frame = (mc_world_entity_error){0};
            }
            mc_world_runtime_call_leave(world);
        }
    }
    if (error != NULL)
        *error = frame;
    if (world != NULL)
        world->last_error = frame.cause;
    return frame.status;
}

static bool enter_entity(mc_world_runtime *world, mc_entity *entity, mc_world_entity_error *frame) {
    if (world == NULL)
        return entity_fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World Entity receiver");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return entity_fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Entity phase");
    if (entity != NULL && (world->classes == NULL || world->classes->entities != entity->classes))
        return entity_fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "World Entity application class");
    if (entity != NULL && !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_MINECART))
        return entity_fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World concrete Entity type");
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
                         mc_world_entity_error *frame) {
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
                          mc_world_entity_error *frame) {
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
    /* The admitted hl/ez, ju/gl and yl/pr classes are nonplayers. Their
     * actual immutable class identities make the two distinct source
     * instanceof gs/em tests28/52 false. Player/sleep descendants remain
     * separate implementations. */
    bool present;
    frame->bytecode = 45;
    frame->presence_bytecode = 6;
    frame->provider_bytecode = 6;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_chunk_provider_contains(world, chunk_x, chunk_z, &present, &cause) !=
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
        !observe_entity_at(world, entity, frame, 103))
        return false;
    *value = true;
    return true;
}

static mc_world_storage_status finish_entity(mc_world_runtime *world, mc_entity *entity,
                                             bool completed, mc_world_entity_error *frame,
                                             mc_world_entity_error *error) {
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
                                                       bool *out, mc_world_entity_error *error) {
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
                                                          mc_world_entity_error *error) {
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
