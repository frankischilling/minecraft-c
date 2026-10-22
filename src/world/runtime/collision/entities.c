#include "entities_internal.h"

#include "../../../entity/entity_internal.h"
#include "../../chunk/object_state_internal.h"
#include "../../storage/objects_internal.h"
#include "../construction_internal.h"
#include "../provider_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64) && !(defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#error "World nearby arithmetic requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "World nearby arithmetic requires strict floating transformations"
#endif
#include <emmintrin.h>

mc_world_nearby_error mc_world_nearby_empty_error(void) {
    mc_world_nearby_error result = {.slice = SIZE_MAX, .index = SIZE_MAX};
    result.tile = mc_live_tiles_empty_error();
    result.batch = mc_world_entity_batch_empty_error();
    result.load_entity = mc_world_entity_load_empty_error();
    result.query.registry = result.tile.registry;
    return result;
}

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

/* Constant-time descriptor admission. The separately held typed occurrences
 * are inspected only at a reached get, ordinary clear or terminal validation.
 * Append does not rescan every previous occurrence on every addition. */
static bool owned_shape(const mc_world_runtime *world) {
    const mc_world_entity_list_ledger *ledger = &world->nearby_entities;
    const mc_world_reference_list *list = ledger->list;
    if (list == NULL)
        return ledger->entries == NULL && ledger->count == 0 && ledger->capacity == 0;
    if (list == world->entity_membership.list || list == world->entity_deferral.list ||
        list == world->collision_boxes.list || list->count != ledger->count ||
        list->capacity != ledger->capacity || list->count > list->capacity ||
        list->capacity > (size_t)INT32_MAX ||
        list->capacity > SIZE_MAX / (sizeof(*list->items) + sizeof(*ledger->entries)) ||
        ((list->capacity == 0) != (list->items == NULL)) ||
        ((ledger->capacity == 0) != (ledger->entries == NULL)) ||
        (list->items != NULL && (void *)list->items == (void *)ledger->entries))
        return false;
    const size_t bytes = list->capacity * (sizeof(*list->items) + sizeof(*ledger->entries));
    if (bytes > world->construction.direct_allocation_bytes)
        return false;
    const mc_world_entity_list_ledger *others[] = {&world->entity_membership,
                                                   &world->entity_deferral};
    for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); ++i) {
        const mc_world_entity_list_ledger *other = others[i];
        const void *a[] = {list->items, ledger->entries};
        const void *b[] = {other->list == NULL ? NULL : other->list->items, other->entries};
        for (size_t j = 0; j < 2; ++j)
            for (size_t k = 0; k < 2; ++k)
                if (a[j] != NULL && a[j] == b[k])
                    return false;
    }
    const mc_world_collision_list_ledger *boxes = &world->collision_boxes;
    const void *box_items = boxes->list == NULL ? NULL : boxes->list->items;
    if ((list->items != NULL &&
         ((void *)list->items == box_items || (void *)list->items == (void *)boxes->entries)) ||
        (ledger->entries != NULL && ((void *)ledger->entries == box_items ||
                                     (void *)ledger->entries == (void *)boxes->entries)))
        return false;
    return true;
}

bool mc_world_nearby_owned_valid(const mc_world_runtime *world) {
    if (world == NULL || !owned_shape(world))
        return false;
    const mc_world_entity_list_ledger *ledger = &world->nearby_entities;
    for (size_t i = 0; i < ledger->count; ++i)
        if (ledger->entries[i] == NULL || ledger->list->items[i] != ledger->entries[i])
            return false;
    return true;
}

static void clear_owned(mc_world_runtime *world) {
    mc_world_entity_list_ledger *ledger = &world->nearby_entities;
    const size_t count = ledger->count;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *entity = ledger->entries[i];
        ledger->entries[i] = NULL;
        ledger->list->items[i] = NULL;
        mc_entity_release(entity);
    }
    ledger->count = 0;
    if (ledger->list != NULL)
        ledger->list->count = 0;
}

void mc_world_nearby_owned_release(mc_world_runtime *world) {
    mc_world_entity_list_ledger *ledger = &world->nearby_entities;
    clear_owned(world);
    if (ledger->list == NULL)
        return;
    const size_t bytes =
        ledger->capacity * (sizeof(*ledger->list->items) + sizeof(*ledger->entries));
    free(ledger->entries);
    free(ledger->list->items);
    ledger->entries = NULL;
    ledger->capacity = 0;
    ledger->list->items = NULL;
    ledger->list->capacity = 0;
    world->construction.direct_allocation_bytes -= bytes;
}

static mc_world_storage_status append(mc_world_runtime *world, mc_world_reference_list *list,
                                      mc_entity *entity, mc_world_nearby_error *frame) {
    if (list == NULL || list != world->nearby_entities.list || !owned_shape(world))
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearby destination List owner");
    mc_world_entity_list_ledger *ledger = &world->nearby_entities;
    if (list->count == (size_t)INT32_MAX)
        return fail(frame, MC_WORLD_STORAGE_LIMIT, "Nearby List size");
    void **items = list->items;
    mc_entity **entries = ledger->entries;
    size_t capacity = list->capacity;
    size_t bytes = 0;
    if (list->count == capacity) {
        capacity = capacity == 0                       ? 4u
                   : capacity > (size_t)INT32_MAX / 2u ? (size_t)INT32_MAX
                                                       : capacity * 2u;
        if (capacity <= list->count || capacity > SIZE_MAX / (sizeof(*items) + sizeof(*entries)))
            return fail(frame, MC_WORLD_STORAGE_LIMIT, "Nearby List capacity");
        bytes = capacity * (sizeof(*items) + sizeof(*entries));
        const size_t used = world->construction.direct_allocation_bytes;
        const size_t limit = world->construction_limits.max_direct_allocation_bytes;
        if (used > limit || bytes > limit - used)
            return fail(frame, MC_WORLD_STORAGE_LIMIT, "Nearby List allocation budget");
        items = malloc(capacity * sizeof(*items));
        entries = malloc(capacity * sizeof(*entries));
        if (items == NULL || entries == NULL) {
            free(items);
            free(entries);
            return fail(frame, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Nearby List allocation");
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
        frame->entity =
            (mc_entity_error){.status = held, .operation = "Nearby occurrence ownership"};
        return fail(frame, entity_status(held), "Nearby occurrence ownership");
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

static mc_world_storage_status gather(mc_world_runtime *world, mc_world_chunk *chunk,
                                      mc_entity *excluded, const mc_entity_bounds *bounds,
                                      mc_world_reference_list *destination,
                                      mc_world_nearby_error *frame) {
    frame->query.chunk_bytecode = 1;
    if (bounds == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk gather query bounds");
    frame->query.chunk_bytecode = 7;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk gather FP admission");
    int32_t low = mc_math_floor_double((bounds->min_y - 2.0) / 16.0);  /* floor12 */
    int32_t high = mc_math_floor_double((bounds->max_y + 2.0) / 16.0); /* floor29 */
    if (low < 0)
        low = 0;
    frame->query.chunk_bytecode = 48;
    mc_world_chunk_list_array *array = chunk->m;
    if (!slice_array(chunk, array))
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk gather slice array length");
    if (high >= (int32_t)array->length) {
        frame->query.chunk_bytecode = 56;
        array = chunk->m;
        if (!slice_array(chunk, array))
            return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Chunk gather clamp array length");
        high = mc_java_i32_sub((int32_t)array->length, 1);
    }
    for (int32_t slice = low; slice <= high; slice = mc_java_i32_add(slice, 1)) {
        frame->slice = (size_t)slice;
        frame->query.chunk_bytecode = 78;
        array = chunk->m; /* current m73, indexed78 */
        if (!slice_array(chunk, array) || slice < 0 || (uint32_t)slice >= array->length)
            return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk gather selected slice");
        mc_world_chunk_entity_list *const list = array->items[(uint32_t)slice]; /* local7 */
        for (size_t index = 0;; ++index) {
            frame->index = index;
            frame->query.chunk_bytecode = 88;
            if (!slice_list(chunk, list))
                return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk gather List size");
            if (index >= list->count)
                break;
            frame->query.chunk_bytecode = 100;
            mc_entity *candidate;
            if (mc_world_chunk_entity_at(list, index, &candidate) != MC_WORLD_STORAGE_OK)
                return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk gather List get");
            if (candidate == excluded) /* actual reference comparison113 */
                continue;
            frame->query.chunk_bytecode = 118;
            if (candidate == NULL || candidate->bounds == NULL)
                return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "Chunk gather Entity bounds");
            frame->query.chunk_bytecode = 122;
            if (mc_entity_bounds_intersects(candidate->bounds, bounds)) {
                frame->query.chunk_bytecode = 131;
                const mc_world_storage_status status = append(world, destination, candidate, frame);
                if (status != MC_WORLD_STORAGE_OK)
                    return status;
            }
        }
    }
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_nearby_gather_held(mc_world_runtime *world, mc_chunk_state *state,
                                                    mc_entity *excluded,
                                                    const mc_entity_bounds *bounds,
                                                    mc_world_reference_list *destination,
                                                    mc_world_nearby_error *error) {
    mc_world_nearby_error frame = error == NULL ? mc_world_nearby_empty_error() : *error;
    mc_world_storage_status status;
    frame.query.chunk_bytecode = 0;
    if (state == NULL)
        status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearby acquired Chunk receiver");
    else if (state->blank)
        status = MC_WORLD_STORAGE_OK; /* actual li/hg override return0 */
    else {
        mc_world_store *store = world->fields.w;
        mc_world_chunk *chunk = mc_world_store_chunk_object(store, state);
        if (chunk == NULL || !mc_world_chunk_source_enabled(chunk))
            status =
                fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearby concrete Chunk owner");
        else {
            mc_world_storage_error child = {0};
            status = mc_world_store_lease_chunk(store, state, &child);
            if (status != MC_WORLD_STORAGE_OK)
                (void)cause(&frame, child);
            else {
                status = gather(world, chunk, excluded, bounds, destination, &frame);
                (void)mc_world_store_release_chunk(store, state, NULL);
            }
        }
    }
    if (error != NULL)
        *error = frame;
    return status;
}

bool mc_world_entity_query_acquire_held(mc_world_runtime *world, int32_t x, int32_t z,
                                        mc_chunk_state **out, mc_world_nearby_error *frame) {
    frame->query.provider_bytecode = 6;
    if (world->fields.v == NULL) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearby provider receiver");
        return false;
    }
    /* Detailed acquisition belongs to the installed concrete coordinator.
     * A different live provider may answer presence, but must not accidentally
     * dispatch this getter through the old provider's cached contexts. */
    mc_world_provider_fields provider;
    mc_world_storage_error native = {0};
    if (mc_world_provider_bind_fields(world->fields.v, &provider, &native) != MC_WORLD_STORAGE_OK) {
        cause(frame, native);
        return false;
    }
    if (provider.world != world || provider.cache != world->acquisition.cache ||
        provider.pending != world->acquisition.pending || provider.store != world->options.store) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearby replacement provider binding");
        return false;
    }
    mc_world_runtime_acquisition_error child;
    frame->query.acquisition = mc_world_runtime_get_detailed(world, x, z, out, &child);
    if (frame->query.acquisition != MC_CHUNK_ACQUIRE_OK) {
        frame->query.registry = child.registry;
        frame->query.population = child.population;
        frame->tile = child.tile;
        frame->batch = child.batch;
        frame->load_entity = child.load_entity;
        frame->activation_bytecode = child.activation_bytecode;
        frame->activation_world_bytecode = child.activation_world_bytecode;
        frame->load_bytecode = child.load_bytecode;
        frame->load_builder_bytecode = child.load_builder_bytecode;
        if (child.cause.status != MC_WORLD_STORAGE_OK)
            cause(frame, child.cause);
        else
            fail(frame,
                 frame->query.acquisition == MC_CHUNK_ACQUIRE_LIMIT ||
                         frame->query.acquisition == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT
                     ? MC_WORLD_STORAGE_LIMIT
                     : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 "Nearby Chunk acquisition");
        return false;
    }
    frame->query.provider_bytecode = 0;
    return true;
}

static bool query(mc_world_runtime *world, mc_entity *excluded, const mc_entity_bounds *bounds,
                  mc_world_entity_list_view *value, mc_world_nearby_error *frame) {
    frame->query.bytecode = 4;
    if (world->fields.R == NULL || world->fields.R != world->nearby_entities.list ||
        !mc_world_nearby_owned_valid(world)) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearby clear List owner");
        return false;
    }
    clear_owned(world); /* ordinary source clear, capacity/descriptor unchanged */
    frame->query.bytecode = 10;
    if (bounds == NULL) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearby query bounds");
        return false;
    }
    frame->query.bytecode = 16;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearby coordinate FP admission");
        return false;
    }
    const int32_t low_x = mc_math_floor_double((bounds->min_x - 2.0) / 16.0);
    const int32_t high_x = mc_math_floor_double((bounds->max_x + 2.0) / 16.0);
    const int32_t low_z = mc_math_floor_double((bounds->min_z - 2.0) / 16.0);
    const int32_t high_z = mc_math_floor_double((bounds->max_z + 2.0) / 16.0);
    for (int32_t x = low_x; x <= high_x; x = mc_java_i32_add(x, 1)) {
        for (int32_t z = low_z; z <= high_z; z = mc_java_i32_add(z, 1)) {
            frame->query.bytecode = 102;
            frame->query.provider_bytecode = 6;
            bool present;
            mc_world_storage_error child = {0};
            if (mc_world_runtime_provider_contains(world, x, z, &present, &child) !=
                MC_WORLD_STORAGE_OK) {
                cause(frame, child);
                return false;
            }
            frame->query.provider_bytecode = 0;
            if (!present)
                continue;
            frame->query.bytecode = 113;
            mc_chunk_state *state = NULL;
            if (!mc_world_entity_query_acquire_held(world, x, z, &state, frame))
                return false;
            /* Actual load of current R119 occurs AFTER acquiring getter113.
             * The Chunk call captures this destination without another clear. */
            frame->query.bytecode = 122;
            if (mc_world_nearby_gather_held(world, state, excluded, bounds, world->fields.R,
                                            frame) != MC_WORLD_STORAGE_OK)
                return false;
            frame->query.chunk_bytecode = 0;
            frame->slice = frame->index = SIZE_MAX;
        }
    }
    frame->query.bytecode = 141;
    const mc_world_reference_list *const result = world->fields.R; /* getfield138 */
    if (result != NULL && (result != world->nearby_entities.list || !owned_shape(world))) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearby returned List owner");
        return false;
    }
    if (world->local_leases == SIZE_MAX) {
        fail(frame, MC_WORLD_STORAGE_LIMIT, "Nearby returned List lease");
        return false;
    }
    ++world->local_leases;
    *value = (mc_world_entity_list_view){world, result};
    return true;
}

mc_world_storage_status mc_world_runtime_nearby_entities(mc_world_runtime *world,
                                                         mc_entity *excluded,
                                                         const mc_entity_bounds *bounds,
                                                         mc_world_entity_list_view *out,
                                                         mc_world_nearby_error *error) {
    mc_world_nearby_error frame = mc_world_nearby_empty_error();
    if (world == NULL || out == NULL)
        fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Nearby arguments");
    else if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
             world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearby World phase");
    else {
        mc_world_storage_error child = {0};
        if (mc_world_runtime_call_enter(world, &child) != MC_WORLD_STORAGE_OK)
            cause(&frame, child);
        else {
            const mc_entity_status held =
                excluded == NULL ? MC_ENTITY_OK : mc_entity_retain(excluded);
            if (held != MC_ENTITY_OK) {
                frame.entity =
                    (mc_entity_error){.status = held, .operation = "Nearby excluded Entity hold"};
                fail(&frame, entity_status(held), "Nearby excluded Entity hold");
            } else {
                mc_world_entity_list_view value = {0};
                if (query(world, excluded, bounds, &value, &frame)) {
                    *out = value;
                    frame = mc_world_nearby_empty_error();
                }
                mc_entity_release(excluded);
            }
            if (frame.query.status != MC_WORLD_STORAGE_OK)
                world->last_error = frame.query.cause;
            mc_world_runtime_call_leave(world);
        }
    }
    if (error != NULL)
        *error = frame;
    return frame.query.status;
}

mc_world_storage_status mc_world_entity_list_view_count(const mc_world_entity_list_view *view,
                                                        size_t *out) {
    if (view == NULL || view->owner == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (view->identity == NULL || view->identity != view->owner->nearby_entities.list ||
        !owned_shape(view->owner))
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    *out = view->identity->count;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_entity_list_view_get(const mc_world_entity_list_view *view,
                                                      size_t index, mc_entity **out) {
    size_t count;
    if (out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    const mc_world_storage_status status = mc_world_entity_list_view_count(view, &count);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (index >= count)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    const mc_world_entity_list_ledger *ledger = &view->owner->nearby_entities;
    if (ledger->entries[index] == NULL || view->identity->items[index] != ledger->entries[index])
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    *out = ledger->entries[index];
    return MC_WORLD_STORAGE_OK;
}

void mc_world_entity_list_view_release(mc_world_entity_list_view *view) {
    if (view == NULL || view->owner == NULL)
        return;
    --view->owner->local_leases;
    *view = (mc_world_entity_list_view){0};
}
