#include "../../entity/entity_internal.h"
#include "mc/core/java/java.h"
#include "object_state_internal.h"
#include <string.h>

static mc_world_storage_status failed(mc_world_chunk_entity_error *out,
                                      mc_world_storage_status status, unsigned int pc,
                                      const char *operation) {
    if (out != NULL)
        *out = (mc_world_chunk_entity_error){.status = status,
                                             .operation = operation,
                                             .bytecode = pc,
                                             .cause = {status, operation, 0}};
    return status;
}

/* All represented concrete Entity classes inherit the pinned sn.equals body.
 * Java instanceof is confined to this exact defining Entity namespace. No
 * READY gate is added to the passive source field reads. */
static bool source_equals(const mc_entity *key, const mc_entity *value) {
    if (key == NULL)
        return value == NULL;
    return value != NULL && key->classes == value->classes && key->base.id == value->base.id;
}

mc_world_storage_status mc_world_chunk_remove_entity(mc_world_chunk *chunk, mc_entity *entity,
                                                     int32_t slice, size_t *work, size_t max_work,
                                                     mc_world_chunk_entity_error *error) {
    if (chunk == NULL || !mc_world_chunk_source_enabled(chunk))
        return failed(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, 0, "Chunk remove native owner");
    if (slice < 0)
        slice = 0;
    mc_world_chunk_list_array *array = chunk->m;
    if (array == NULL || array->length > (size_t)INT32_MAX)
        return failed(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, 11, "Chunk remove array length");
    if (slice >= (int32_t)array->length) {
        array = chunk->m;
        if (array == NULL || array->length > (size_t)INT32_MAX)
            return failed(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, 19,
                          "Chunk remove clamp array length");
        slice = mc_java_i32_sub((int32_t)array->length, 1);
    }
    array = chunk->m;
    if (array == NULL || slice < 0 || (size_t)slice >= array->length ||
        slice >= CHUNK_ENTITY_SLICES)
        return failed(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, 28,
                      "Chunk remove selected array element");
    mc_world_chunk_entity_list *list = array->items[(size_t)slice];
    if (list == NULL || list != &chunk->entities[(size_t)slice] || list->count > list->capacity ||
        list->capacity > (size_t)INT32_MAX || list->capacity > SIZE_MAX / sizeof(*list->items) ||
        ((list->capacity == 0) != (list->items == NULL)) ||
        (list->count != 0 && chunk->release_entity != mc_entity_release))
        return failed(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, 30,
                      "Chunk remove actual List owner");
    for (size_t index = 0; index < list->count; ++index) {
        if (work != NULL) {
            if (*work == max_work)
                return failed(error, MC_WORLD_STORAGE_LIMIT, 30,
                              "Chunk pending Entity.equals search ceiling");
            ++*work;
        }
        mc_entity *candidate = list->items[index];
        if (!source_equals(entity, candidate))
            continue;
        memmove(list->items + index, list->items + index + 1,
                (list->count - index - 1) * sizeof(*list->items));
        --list->count;
        list->items[list->count] = NULL;
        /* Retire the matched occurrence, which can have the argument's ID
         * without being the same Entity. The caller holds its own argument. */
        if (candidate != NULL)
            chunk->release_entity(candidate);
        break;
    }
    if (error != NULL)
        *error = (mc_world_chunk_entity_error){0};
    return MC_WORLD_STORAGE_OK;
}
