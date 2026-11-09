#include "../execution_children_internal.h"
#include "class_entities_execution_internal.h"

#include "../../../entity/entity_internal.h"
#include "../../chunk/object_state_internal.h"
#include "../../storage/objects_internal.h"
#include "../chunk_provider_internal.h"
#include "../construction_internal.h"
#include "entities_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64) && !(defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#error "World Class query arithmetic requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "World Class queries require strict floating transformations"
#endif
#include <emmintrin.h>

/* One actual fresh source List and an independent hold per occurrence. This
 * owner is never installed in a World source field or its captured R ledger. */
struct mc_world_class_entity_list {
    mc_world_runtime *world;
    mc_world_reference_list source;
    mc_world_entity_list_ledger ledger;
    size_t charged_bytes;
    bool leased;
};

static mc_world_storage_status fail(mc_world_nearby_error *frame, mc_world_storage_status status,
                                    const char *operation) {
    frame->query.status = status;
    frame->query.operation = operation;
    frame->query.cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status cause(mc_world_nearby_error *frame, mc_world_storage_error child) {
    frame->query.status = child.status;
    frame->query.operation = child.operation;
    frame->query.cause = child;
    return child.status;
}

static mc_world_storage_status entity_status(mc_entity_status status) {
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

static mc_world_storage_status entity_fail(mc_world_nearby_error *frame, mc_entity_status status,
                                           const char *operation) {
    frame->entity = (mc_entity_error){.status = status, .operation = operation};
    return fail(frame, entity_status(status), operation);
}

/* Constant-time descriptor checks. The opaque owner never adopts an unknown
 * caller List; per-occurrence identity is checked at the actual reached get. */
static bool result_shape(const mc_world_class_entity_list *result) {
    if (result == NULL || result->world == NULL)
        return false;
    const mc_world_reference_list *const list = &result->source;
    const mc_world_entity_list_ledger *const ledger = &result->ledger;
    if (ledger->list != list || list->count != ledger->count ||
        list->capacity != ledger->capacity || list->count > list->capacity ||
        list->capacity > (size_t)INT32_MAX ||
        list->capacity >
            (SIZE_MAX - sizeof(*result)) / (sizeof(*list->items) + sizeof(*ledger->entries)) ||
        ((list->capacity == 0) != (list->items == NULL)) ||
        ((ledger->capacity == 0) != (ledger->entries == NULL)) ||
        (list->items != NULL && (void *)list->items == (void *)ledger->entries))
        return false;
    const size_t bytes =
        sizeof(*result) + list->capacity * (sizeof(*list->items) + sizeof(*ledger->entries));
    return bytes == result->charged_bytes &&
           bytes <= result->world->construction.direct_allocation_bytes;
}

static mc_world_class_entity_list *result_create(mc_world_runtime *world,
                                                 mc_world_nearby_error *frame) {
    frame->query.bytecode = 67;
    const size_t used = world->construction.direct_allocation_bytes;
    const size_t limit = world->construction_limits.max_direct_allocation_bytes;
    if (used > limit || sizeof(mc_world_class_entity_list) > limit - used) {
        fail(frame, MC_WORLD_STORAGE_LIMIT, "Class query List header budget");
        return NULL;
    }
    mc_world_class_entity_list *result = calloc(1, sizeof(*result));
    if (result == NULL) {
        fail(frame, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Class query List header allocation");
        return NULL;
    }
    world->construction.direct_allocation_bytes += sizeof(*result);
    frame->query.bytecode = 71;
    result->world = world;
    result->ledger.list = &result->source;
    result->charged_bytes = sizeof(*result);
    return result;
}

void mc_world_class_entity_list_release(mc_world_class_entity_list *result) {
    if (result == NULL)
        return;
    mc_world_runtime *const world = result->world;
    const bool leased = result->leased;
    const size_t charged = result->charged_bytes;
    for (size_t i = 0; i < result->ledger.count; ++i)
        mc_entity_release(result->ledger.entries[i]);
    free(result->source.items);
    free(result->ledger.entries);
    free(result);
    world->construction.direct_allocation_bytes -= charged;
    if (leased)
        --world->local_leases;
}

static mc_world_storage_status append(mc_world_runtime *world, mc_world_class_entity_list *result,
                                      mc_entity *entity, mc_world_nearby_error *frame) {
    if (!result_shape(result) || result->world != world)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Class query destination List owner");
    mc_world_reference_list *const list = &result->source;
    mc_world_entity_list_ledger *const ledger = &result->ledger;
    if (list->count == (size_t)INT32_MAX)
        return fail(frame, MC_WORLD_STORAGE_LIMIT, "Class query List size");
    void **items = list->items;
    mc_entity **entries = ledger->entries;
    size_t capacity = list->capacity;
    size_t bytes = 0;
    if (list->count == capacity) {
        capacity = capacity == 0                       ? 4u
                   : capacity > (size_t)INT32_MAX / 2u ? (size_t)INT32_MAX
                                                       : capacity * 2u;
        if (capacity <= list->count ||
            capacity > (SIZE_MAX - sizeof(*result)) / (sizeof(*items) + sizeof(*entries)))
            return fail(frame, MC_WORLD_STORAGE_LIMIT, "Class query List capacity");
        bytes = capacity * (sizeof(*items) + sizeof(*entries));
        const size_t used = world->construction.direct_allocation_bytes;
        const size_t limit = world->construction_limits.max_direct_allocation_bytes;
        if (used > limit || bytes > limit - used)
            return fail(frame, MC_WORLD_STORAGE_LIMIT, "Class query List allocation budget");
        items = malloc(capacity * sizeof(*items));
        entries = malloc(capacity * sizeof(*entries));
        if (items == NULL || entries == NULL) {
            free(items);
            free(entries);
            return fail(frame, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Class query List allocation");
        }
        world->construction.direct_allocation_bytes += bytes;
        if (list->count != 0) {
            memcpy(items, list->items, list->count * sizeof(*items));
            memcpy(entries, ledger->entries, list->count * sizeof(*entries));
        }
    }
    const mc_entity_status held = mc_entity_retain(entity);
    if (held != MC_ENTITY_OK) {
        if (bytes != 0) {
            free(items);
            free(entries);
            world->construction.direct_allocation_bytes -= bytes;
        }
        return entity_fail(frame, held, "Class query occurrence ownership");
    }
    items[list->count] = entity;
    entries[list->count] = entity;
    if (bytes != 0) {
        const size_t old_bytes = list->capacity * (sizeof(*items) + sizeof(*entries));
        free(list->items);
        free(ledger->entries);
        list->items = items;
        list->capacity = capacity;
        ledger->entries = entries;
        ledger->capacity = capacity;
        world->construction.direct_allocation_bytes -= old_bytes;
        result->charged_bytes = sizeof(*result) + bytes;
    }
    ++list->count;
    ++ledger->count;
    return MC_WORLD_STORAGE_OK;
}

static bool slice_array(const mc_world_chunk *chunk, const mc_world_chunk_list_array *array) {
    return array == &chunk->slices && array->length <= CHUNK_ENTITY_SLICES;
}

static bool slice_list(const mc_world_chunk *chunk, const mc_world_chunk_entity_list *list) {
    if (list == NULL)
        return false;
    bool known = false;
    for (size_t i = 0; i < CHUNK_ENTITY_SLICES; ++i)
        if (list == &chunk->entities[i])
            known = true;
    return known && list->count <= list->capacity && list->capacity <= (size_t)INT32_MAX &&
           list->capacity <= SIZE_MAX / sizeof(*list->items) &&
           ((list->capacity == 0) == (list->items == NULL));
}

typedef struct class_query_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_code code;
    mc_world_runtime_execution_result *owned;
} class_query_execution;

static bool class_floor(const class_query_execution *query, double value, bool chunk,
                        unsigned int pc, int32_t *out, mc_world_nearby_error *frame) {
    if (query != NULL) {
        if (chunk)
            frame->query.chunk_bytecode = pc;
        else
            frame->query.bytecode = pc;
        const mc_math_class_status status = mc_math_execution_require(
            query->execution->math,
            (mc_math_execution_site){chunk ? "lm" : "fd", "a",
                                     chunk ? "(Ljava/lang/Class;Leq;Ljava/util/List;)V"
                                           : "(Ljava/lang/Class;Leq;)Ljava/util/List;",
                                     (int)pc},
            "b", "(D)I");
        if (status != MC_MATH_CLASS_OK) {
            return false; /* Exact Math channel; do not invent a World Source status. */
        }
    }
    *out = mc_math_floor_double(value);
    return true;
}

static bool class_stopped(mc_world_nearby_error *frame, mc_world_storage_status status,
                          const char *operation) {
    (void)fail(frame, status, operation);
    return false;
}

static bool class_entity_stopped(mc_world_nearby_error *frame, mc_entity_status status,
                                 const char *operation) {
    (void)entity_fail(frame, status, operation);
    return false;
}

static bool gather(mc_world_runtime *world, mc_world_chunk *chunk,
                   const mc_entity_class_literal *target, const mc_entity_bounds *bounds,
                   mc_world_class_entity_list *destination, mc_world_nearby_error *frame,
                   const class_query_execution *query) {
    frame->query.chunk_bytecode = 1;
    if (bounds == NULL)
        return class_stopped(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Class gather query bounds");
    frame->query.chunk_bytecode = 7;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return class_stopped(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Class gather FP admission");
    int32_t low, high;
    if (!class_floor(query, (bounds->min_y - 2.0) / 16.0, true, 12, &low, frame) ||
        !class_floor(query, (bounds->max_y + 2.0) / 16.0, true, 29, &high, frame))
        return false;
    if (low < 0)
        low = 0;
    frame->query.chunk_bytecode = 48;
    mc_world_chunk_list_array *array = chunk->m;
    if (!slice_array(chunk, array))
        return class_stopped(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Class gather slice array length");
    if (high >= (int32_t)array->length) {
        frame->query.chunk_bytecode = 56;
        array = chunk->m;
        if (!slice_array(chunk, array))
            return class_stopped(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "Class gather clamp array length");
        high = mc_java_i32_sub((int32_t)array->length, 1);
    }
    for (int32_t slice = low; slice <= high; slice = mc_java_i32_add(slice, 1)) {
        frame->slice = (size_t)slice;
        frame->query.chunk_bytecode = 78;
        array = chunk->m;
        if (!slice_array(chunk, array) || slice < 0 || (uint32_t)slice >= array->length)
            return class_stopped(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "Class gather selected slice");
        mc_world_chunk_entity_list *const list = array->items[(uint32_t)slice];
        for (size_t index = 0;; ++index) {
            frame->index = index;
            frame->query.chunk_bytecode = 88;
            if (!slice_list(chunk, list))
                return class_stopped(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "Class gather List size");
            if (index >= list->count)
                break;
            frame->query.chunk_bytecode = 100;
            mc_entity *candidate;
            if (mc_world_chunk_entity_at(list, index, &candidate) != MC_WORLD_STORAGE_OK)
                return class_stopped(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "Class gather List get");
            /* The actual typed Chunk occurrence supplies checkcast105. NULL
             * passes that cast but fails getClass113 before target is used. */
            frame->query.chunk_bytecode = 113;
            if (candidate == NULL || candidate->class_literal == NULL)
                return class_entity_stopped(frame, MC_ENTITY_STATE_ERROR,
                                            "Class gather Entity getClass receiver");
            const mc_entity_class_literal *const actual = candidate->class_literal;
            frame->query.chunk_bytecode = 116;
            if (target == NULL)
                return class_entity_stopped(frame, MC_ENTITY_STATE_ERROR,
                                            "Class gather Class receiver");
            bool matches;
            const mc_entity_status matched =
                mc_entity_class_literal_is_assignable_from(target, actual, &matches);
            if (matched != MC_ENTITY_OK)
                return class_entity_stopped(frame, matched,
                                            "Class gather canonical Class matching");
            if (!matches)
                continue;
            frame->query.chunk_bytecode = 124;
            const mc_entity_bounds *const candidate_bounds = candidate->bounds;
            /* getfield124 can read NULL; only invoke128 rejects that receiver. */
            frame->query.chunk_bytecode = 128;
            if (candidate_bounds == NULL)
                return class_stopped(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "Class gather bounds invocation receiver");
            if (mc_entity_bounds_intersects(candidate_bounds, bounds)) {
                frame->query.chunk_bytecode = 137;
                const mc_world_storage_status status = append(world, destination, candidate, frame);
                if (status != MC_WORLD_STORAGE_OK)
                    return false;
            }
        }
    }
    return true;
}

static mc_world_storage_status
class_gather_run(mc_world_runtime *world, mc_chunk_state *state,
                 const mc_entity_class_literal *target, const mc_entity_bounds *bounds,
                 mc_world_class_entity_list *destination, mc_world_nearby_error *error,
                 const class_query_execution *query, bool *continued) {
    if (continued != NULL)
        *continued = false;
    mc_world_nearby_error local;
    mc_world_nearby_error *frame = error;
    if (query == NULL) {
        if (error == NULL)
            mc_world_nearby_error_init(&local);
        else
            local = *error;
        frame = &local;
    }
    mc_world_storage_status status;
    bool completed = false;
    frame->query.chunk_bytecode = 0;
    if (state == NULL)
        status =
            fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Class query acquired Chunk receiver");
    else if (state->blank) {
        status = MC_WORLD_STORAGE_OK; /* Actual li/hg override return0. */
        completed = true;
    } else if (world == NULL)
        status = fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Class gather native World owner");
    else {
        mc_world_store *const store = world->fields.w;
        mc_world_chunk *const chunk = mc_world_store_chunk_object(store, state);
        if (chunk == NULL || !mc_world_chunk_source_enabled(chunk))
            status =
                fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Class query concrete Chunk owner");
        else {
            mc_world_storage_error child = {0};
            status = mc_world_store_lease_chunk(store, state, &child);
            if (status != MC_WORLD_STORAGE_OK)
                (void)cause(frame, child);
            else {
                completed = gather(world, chunk, target, bounds, destination, frame, query);
                status = frame->query.status;
                (void)mc_world_store_release_chunk(store, state, NULL);
            }
        }
    }
    if (continued != NULL)
        *continued = completed;
    if (query == NULL && error != NULL)
        *error = *frame;
    return status;
}

mc_world_storage_status mc_world_class_entities_gather_held(mc_world_runtime *world,
                                                            mc_chunk_state *state,
                                                            const mc_entity_class_literal *target,
                                                            const mc_entity_bounds *bounds,
                                                            mc_world_class_entity_list *destination,
                                                            mc_world_nearby_error *error) {
    return class_gather_run(world, state, target, bounds, destination, error, NULL, NULL);
}

static bool query(mc_world_runtime *world, const mc_entity_class_literal *target,
                  const mc_entity_bounds *bounds, mc_world_class_entity_list **out,
                  mc_world_nearby_error *frame, const class_query_execution *owned) {
    frame->query.bytecode = 1;
    if (bounds == NULL) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Class query bounds receiver");
        return false;
    }
    frame->query.bytecode = 7;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Class query coordinate FP admission");
        return false;
    }
    int32_t low_x, high_x, low_z, high_z;
    if (!class_floor(owned, (bounds->min_x - 2.0) / 16.0, false, 12, &low_x, frame) ||
        !class_floor(owned, (bounds->max_x + 2.0) / 16.0, false, 28, &high_x, frame) ||
        !class_floor(owned, (bounds->min_z - 2.0) / 16.0, false, 45, &low_z, frame) ||
        !class_floor(owned, (bounds->max_z + 2.0) / 16.0, false, 62, &high_z, frame))
        return false;
    mc_world_class_entity_list *const result = result_create(world, frame);
    if (result == NULL)
        return false;
    for (int32_t x = low_x; x <= high_x; x = mc_java_i32_add(x, 1)) {
        for (int32_t z = low_z; z <= high_z; z = mc_java_i32_add(z, 1)) {
            frame->query.bytecode = 102;
            frame->query.provider_bytecode = 6;
            bool present;
            mc_world_storage_error child = {0};
            if (mc_world_runtime_chunk_provider_contains(world, x, z, &present, &child) !=
                MC_WORLD_STORAGE_OK) {
                cause(frame, child);
                goto failed;
            }
            frame->query.provider_bytecode = 0;
            if (!present)
                continue;
            frame->query.bytecode = 113;
            mc_chunk_state *state = NULL;
            const bool acquired =
                owned == NULL
                    ? mc_world_entity_query_acquire_held(world, x, z, &state, frame)
                    : mc_world_entity_query_acquire_in_execution(
                          owned->execution, world, owned->code, x, z, &state, frame, owned->owned);
            if (!acquired)
                goto failed;
            frame->query.bytecode = 120;
            bool gathered = false;
            (void)class_gather_run(world, state, target, bounds, result, frame, owned, &gathered);
            if (!gathered)
                goto failed;
            frame->query.chunk_bytecode = 0;
            frame->slice = frame->index = SIZE_MAX;
        }
    }
    frame->query.bytecode = 137;
    if (world->local_leases == SIZE_MAX) {
        fail(frame, MC_WORLD_STORAGE_LIMIT, "Class query returned List lease");
        goto failed;
    }
    ++world->local_leases;
    result->leased = true;
    *out = result;
    return true;

failed:
    mc_world_class_entity_list_release(result);
    return false;
}

static bool class_entities_run(mc_world_runtime *world, const mc_entity_class_literal *target,
                               const mc_entity_bounds *bounds, mc_world_class_entity_list **out,
                               mc_world_nearby_error *frame, const class_query_execution *owned) {
    if (world == NULL || out == NULL) {
        fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Class query arguments");
        return false;
    }
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Class query World phase");
        return false;
    }
    mc_world_storage_error child = {0};
    if (mc_world_runtime_call_enter(world, &child) != MC_WORLD_STORAGE_OK) {
        cause(frame, child);
        return false;
    }
    const bool completed = query(world, target, bounds, out, frame, owned);
    if (completed)
        mc_world_nearby_error_init(frame);
    if (frame->query.status != MC_WORLD_STORAGE_OK)
        world->last_error = frame->query.cause;
    mc_world_runtime_call_leave(world);
    return completed;
}

mc_world_storage_status mc_world_runtime_class_entities(mc_world_runtime *world,
                                                        const mc_entity_class_literal *target,
                                                        const mc_entity_bounds *bounds,
                                                        mc_world_class_entity_list **out,
                                                        mc_world_nearby_error *error) {
    mc_world_nearby_error frame;
    mc_world_nearby_error_init(&frame);
    (void)class_entities_run(world, target, bounds, out, &frame, NULL);
    if (error != NULL)
        *error = frame;
    return frame.query.status;
}

bool mc_world_runtime_class_entities_in_execution(
    mc_entity_tick_execution *execution, mc_world_runtime *world,
    mc_world_runtime_execution_code code, const mc_entity_class_literal *target,
    const mc_entity_bounds *bounds, mc_world_class_entity_list **out, mc_world_nearby_error *error,
    mc_world_storage_status *status, mc_world_runtime_execution_result *owned) {
    if (owned != NULL)
        mc_world_runtime_execution_result_init(owned);
    if (error != NULL)
        mc_world_nearby_error_init(error);
    const mc_entity_tick_execution_site site =
        execution != NULL && execution->identity == execution
            ? execution->site
            : (mc_entity_tick_execution_site){NULL, NULL, NULL, -1};
    if (execution == NULL || execution->identity != execution || execution->math == NULL ||
        error == NULL || status == NULL || owned == NULL) {
        mc_world_runtime_execution_result_note(owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "Class query execution/Math/outputs required", site);
        return false;
    }
    mc_world_access_failure_lease *unused = NULL;
    const mc_world_storage_status admitted =
        mc_world_execution_child_lease(execution, world, &unused);
    if (admitted != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(owned, admitted,
                                               "Class query evaluated World ownership", site);
        return false;
    }
    const class_query_execution query = {execution, code, owned};
    const bool completed = class_entities_run(world, target, bounds, out, error, &query);
    *status = error->query.status;
    return completed && owned->ownership_status == MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_class_entity_list_count(const mc_world_class_entity_list *result,
                                                         size_t *out) {
    if (result == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (!result_shape(result))
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    *out = result->source.count;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_class_entity_list_get(const mc_world_class_entity_list *result,
                                                       size_t index, mc_entity **out) {
    size_t count;
    if (out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    const mc_world_storage_status status = mc_world_class_entity_list_count(result, &count);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (index >= count)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (result->ledger.entries[index] == NULL ||
        result->source.items[index] != result->ledger.entries[index])
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    *out = result->ledger.entries[index];
    return MC_WORLD_STORAGE_OK;
}

const mc_world_reference_list *
mc_world_class_entity_list_identity(const mc_world_class_entity_list *result) {
    return result == NULL ? NULL : &result->source;
}
