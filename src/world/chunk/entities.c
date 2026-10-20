#include "object_state_internal.h"

#include "../../entity/entity_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64) && !(defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#error "Chunk Entity coordinates require SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Chunk Entity coordinates require strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(CHAR_BIT == 8 && sizeof(double) == sizeof(uint64_t),
               "Chunk Entity coordinates require binary64 storage");

static mc_world_storage_status insertion_result(mc_world_chunk_entity_error *out,
                                                const mc_world_chunk_entity_error *frame,
                                                mc_world_storage_status status,
                                                const char *operation, unsigned int pc)
{
    if (out != NULL) {
        *out = *frame;
        out->status = status;
        out->operation = operation;
        out->bytecode = pc;
        out->cause = (mc_world_storage_error){status, operation, 0};
    }
    return status;
}

static bool list_valid(const mc_world_chunk_entity_list *list)
{
    return list != NULL && list->count <= list->capacity && list->capacity <= (size_t)INT32_MAX &&
           list->capacity <= SIZE_MAX / sizeof(*list->items) &&
           ((list->capacity == 0 && list->items == NULL) ||
            (list->capacity != 0 && list->items != NULL));
}

static mc_world_storage_status hold_status(mc_entity_status status)
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

static mc_world_storage_status append(mc_world_chunk *chunk, mc_world_chunk_entity_list *list,
                                      mc_entity *entity, mc_world_chunk_entity_error *frame)
{
    if (!list_valid(list) || chunk->allocation_bytes > chunk->max_allocation_bytes ||
        (chunk->release_entity != NULL && chunk->release_entity != mc_entity_release))
        return insertion_result(frame, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity List storage", 148);
    if (list->count == (size_t)INT32_MAX)
        return insertion_result(frame, frame, MC_WORLD_STORAGE_LIMIT, "Chunk Entity List size",
                                148);

    mc_entity **items = list->items;
    size_t capacity = list->capacity;
    size_t new_bytes = 0;
    if (list->count == capacity) {
        /* Concrete native capacity policy, not JDK ArrayList allocation/OOM
         * parity. Old AND new capacities are charged while both are live. */
        capacity = capacity == 0                       ? 4u
                   : capacity > (size_t)INT32_MAX / 2u ? (size_t)INT32_MAX
                                                       : capacity * 2u;
        if (capacity <= list->count || capacity > SIZE_MAX / sizeof(*items))
            return insertion_result(frame, frame, MC_WORLD_STORAGE_LIMIT,
                                    "Chunk Entity List capacity", 148);
        new_bytes = capacity * sizeof(*items);
        if (new_bytes > chunk->max_allocation_bytes - chunk->allocation_bytes)
            return insertion_result(frame, frame, MC_WORLD_STORAGE_LIMIT,
                                    "Chunk Entity List allocation budget", 148);
        items = malloc(new_bytes);
        if (items == NULL)
            return insertion_result(frame, frame, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                    "Chunk Entity List allocation", 148);
        chunk->allocation_bytes += new_bytes;
        if (list->count != 0)
            memcpy(items, list->items, list->count * sizeof(*items));
    }

    /* One new native hold for this occurrence, including when the same pointer
     * is already present. No equality/hash or membership deduplication. */
    const mc_entity_status held = mc_entity_retain(entity);
    if (held != MC_ENTITY_OK) {
        if (new_bytes != 0) {
            free(items);
            chunk->allocation_bytes -= new_bytes;
        }
        frame->entity =
            (mc_entity_error){.status = held, .operation = "Chunk List Entity ownership"};
        return insertion_result(frame, frame, hold_status(held), "Chunk List Entity ownership",
                                148);
    }

    chunk->release_entity = mc_entity_release;
    items[list->count] = entity;
    if (new_bytes != 0) {
        mc_entity **const old_items = list->items;
        const size_t old_bytes = list->capacity * sizeof(*old_items);
        list->items = items;
        list->capacity = capacity;
        free(old_items);
        chunk->allocation_bytes -= old_bytes;
    }
    ++list->count;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_chunk_insert_entity(mc_world_chunk *chunk, mc_entity *entity,
                                                     mc_world_chunk_entity_error *error)
{
    mc_world_chunk_entity_error frame = {0};
    if (chunk == NULL)
        return insertion_result(error, &frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                "Chunk Entity insertion arguments", 0);
    if (chunk->state == NULL)
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity state binding", 0);
    if (!mc_world_chunk_source_enabled(chunk))
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity source operations are disabled", 0);

    /* lm/hi.a(Entity)V: this precedes even the first nullable Entity read. */
    chunk->state->has_entities = true; /* q2 */
    mc_entity_base_fields *fields = mc_entity_runtime_fields(entity);
    if (fields == NULL)
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity x receiver", 6);
    const double x = fields->x; /* sn.aM / lq.aP6 */
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity coordinate FP admission", 12);
    const int32_t chunk_x = mc_math_floor_double(x / 16.0); /* ddiv12/floor13 */

    fields = mc_entity_runtime_fields(entity);
    if (fields == NULL)
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity z receiver", 18);
    const int32_t chunk_z = mc_math_floor_double(fields->z / 16.0); /* 18/24/25 */
    if (chunk_x != chunk->state->x || chunk_z != chunk->state->z)
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk wrong-location diagnostic is not implemented", 45);

    fields = mc_entity_runtime_fields(entity);
    if (fields == NULL)
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity y receiver", 74);
    int32_t slice = mc_math_floor_double(fields->y / 16.0); /* 74/80/81 */
    if (slice < 0)
        slice = 0; /* 91/92 */

    mc_world_chunk_list_array *array = chunk->m; /* 97 */
    if (array == NULL)
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity slice array length", 100);
    if (array->length > (size_t)INT32_MAX)
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity slice array native shape", 100);
    if (slice >= (int32_t)array->length) {
        array = chunk->m; /* Actual second array receiver105/length108. */
        if (array == NULL || array->length > (size_t)INT32_MAX)
            return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                    "Chunk Entity clamp array length", 108);
        slice = mc_java_i32_sub((int32_t)array->length, 1); /* 110 */
    }

    /* These source stores precede the actual List selection and any native
     * append allocation/hold failure. Do not combine into a snapshot update. */
    mc_entity_runtime_fields(entity)->bF = true;            /* sn.bF / lq.bG115 */
    mc_entity_runtime_fields(entity)->bG = chunk->state->x; /* sn.bG / lq.bH123 */
    mc_entity_runtime_fields(entity)->bH = slice;           /* sn.bH / lq.bI129 */
    mc_entity_runtime_fields(entity)->bI = chunk->state->z; /* sn.bI / lq.bJ137 */

    array = chunk->m; /* getfield141, NOT the earlier length receiver */
    if (array == NULL || slice < 0 || (uint32_t)slice >= array->length ||
        (uint32_t)slice >= CHUNK_ENTITY_SLICES)
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity selected slice", 146);
    mc_world_chunk_entity_list *const list = array->items[(uint32_t)slice];
    if (list == NULL)
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity List receiver", 148);
    if (list != &chunk->entities[(uint32_t)slice])
        return insertion_result(error, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Chunk Entity List native owner", 148);
    const mc_world_storage_status status = append(chunk, list, entity, &frame);
    if (status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = frame;
        return status;
    }
    return insertion_result(error, &frame, MC_WORLD_STORAGE_OK, NULL, 0);
}
