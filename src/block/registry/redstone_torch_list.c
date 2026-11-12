#include "redstone_torch_list_internal.h"

#include "managed_internal.h"
#include "registry_internal.h"

#include "mc/core/java/java.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

struct mc_registry_redstone_torch_record {
    mc_registry_classes *classes;
    mc_registry_redstone_torch_record_view value;
    size_t holds, external;
};

struct mc_registry_redstone_torch_list {
    mc_registry_classes *classes;
    mc_registry_redstone_torch_list *next_owned;
    mc_registry_redstone_torch_record **elements;
    uint64_t identity, array_identity;
    int32_t size, capacity, mod_count;
    mc_registry_redstone_torch_backing backing;
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
struct mc_registry_redstone_torch_snapshot {
    mc_registry_classes *classes;
    mc_registry_redstone_torch_list_view view;
    mc_registry_redstone_torch_record *records[];
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

static mc_registry_status result(mc_registry_redstone_torch_error *error, mc_registry_status status,
                                 mc_registry_redstone_torch_failure failure,
                                 bool mod_count_stored) {
    if (error != NULL)
        *error = (mc_registry_redstone_torch_error){status, failure, mod_count_stored};
    return status;
}

static mc_registry_status native(mc_registry_redstone_torch_error *error, mc_registry_status status,
                                 bool mod_count_stored) {
    return result(error, status, MC_REGISTRY_REDSTONE_TORCH_NATIVE, mod_count_stored);
}

static mc_registry_status resolve(const mc_registry_redstone_torch_list_ref *reference,
                                  mc_registry_redstone_torch_list **out,
                                  mc_registry_redstone_torch_error *error) {
    if (reference == NULL || reference->classes == NULL)
        return native(error, MC_REGISTRY_INVALID_ARGUMENT, false);
    if (reference->object == NULL)
        return result(error, MC_REGISTRY_NULL_REFERENCE, MC_REGISTRY_REDSTONE_TORCH_LIST_NULL,
                      false);
    /* Compare only known live descriptors. Never dereference the captured raw
     * receiver merely to decide whether this module owns it. */
    for (mc_registry_redstone_torch_list *list = reference->classes->redstone_torch_lists;
         list != NULL; list = list->next_owned)
        if (list == reference->object) {
            *out = list;
            return MC_REGISTRY_OK;
        }
    return native(error, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY, false);
}

static void release_internal(mc_registry_redstone_torch_record *record) {
    if (record == NULL)
        return;
    assert(record->holds != 0);
    if (--record->holds == 0) {
        assert(record->external == 0);
        mc_registry_managed_release(record->classes, record);
    }
}

static mc_registry_redstone_torch_list_view list_view(const mc_registry_redstone_torch_list *list) {
    bool unused_slots_null = true;
    for (int32_t i = list->size; i < list->capacity; ++i)
        if (list->elements[i] != NULL)
            unused_slots_null = false;
    return (mc_registry_redstone_torch_list_view){
        .identity = list->identity,
        .array_identity = list->array_identity,
        .allocated_records = list->classes->redstone_torch_record_identities,
        .backing = list->backing,
        .size = list->size,
        .capacity = list->capacity,
        .mod_count = list->mod_count,
        .unused_slots_null = unused_slots_null};
}

mc_registry_status mc_registry_redstone_torch_list_create(mc_registry_classes *classes,
                                                          mc_registry_redstone_torch_list **out) {
    if (classes == NULL || out == NULL || *out != NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->redstone_torch_list_identities == UINT64_MAX)
        return mc_registry_fail(classes, MC_REGISTRY_LIMIT, "Redstone Torch native List identity",
                                NULL);
    mc_registry_redstone_torch_list *list = mc_registry_allocate(classes, sizeof(*list));
    if (list == NULL)
        return classes->last_error.status;
    list->classes = classes;
    list->identity = ++classes->redstone_torch_list_identities;
    if (classes->redstone_torch_array_identities == 0)
        classes->redstone_torch_array_identities = 1; /* Reserve default-empty token1. */
    list->array_identity = 1;
    list->backing = MC_REGISTRY_REDSTONE_TORCH_DEFAULT_EMPTY;
    list->next_owned = classes->redstone_torch_lists;
    classes->redstone_torch_lists = list;
    *out = list;
    return MC_REGISTRY_OK;
}

mc_registry_status
mc_registry_redstone_torch_list_capture(mc_registry_classes *classes,
                                        mc_registry_redstone_torch_list_ref *out) {
    if (classes == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    *out = (mc_registry_redstone_torch_list_ref){classes, classes->db_b};
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_redstone_torch_record_new(mc_registry_classes *classes,
                                                         mc_registry_redstone_torch_record **out,
                                                         mc_registry_redstone_torch_error *error) {
    if (classes == NULL || out == NULL || *out != NULL)
        return native(error, MC_REGISTRY_INVALID_ARGUMENT, false);
    if (classes->redstone_torch_external_occurrences == SIZE_MAX ||
        classes->redstone_torch_record_identities == UINT64_MAX)
        return native(error, MC_REGISTRY_LIMIT, false);
    void *storage = NULL;
    const mc_registry_status status =
        mc_registry_managed_allocate(classes, sizeof(mc_registry_redstone_torch_record), &storage);
    if (status != MC_REGISTRY_OK)
        return native(error, status, false);
    mc_registry_redstone_torch_record *record = storage;
    record->classes = classes;
    record->value.identity = ++classes->redstone_torch_record_identities;
    record->holds = record->external = 1;
    ++classes->redstone_torch_external_occurrences;
    *out = record;
    return result(error, MC_REGISTRY_OK, MC_REGISTRY_REDSTONE_TORCH_NONE, false);
}

mc_registry_status
mc_registry_redstone_torch_record_construct(mc_registry_redstone_torch_record *record, int32_t x,
                                            int32_t y, int32_t z, int64_t time,
                                            mc_registry_redstone_torch_error *error) {
    if (record == NULL || record->external == 0 || record->value.constructed)
        return native(error, MC_REGISTRY_INVALID_ARGUMENT, false);
    record->value.x = x;
    record->value.y = y;
    record->value.z = z;
    record->value.time = time;
    record->value.constructed = true;
    return result(error, MC_REGISTRY_OK, MC_REGISTRY_REDSTONE_TORCH_NONE, false);
}

mc_registry_status
mc_registry_redstone_torch_record_inspect(const mc_registry_redstone_torch_record *record,
                                          mc_registry_redstone_torch_record_view *out) {
    if (out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    *out =
        record == NULL ? (mc_registry_redstone_torch_record_view){.is_null = true} : record->value;
    return MC_REGISTRY_OK;
}

mc_registry_status
mc_registry_redstone_torch_record_retain(mc_registry_redstone_torch_record *record,
                                         mc_registry_redstone_torch_error *error) {
    if (record != NULL) {
        if (record->holds == SIZE_MAX || record->external == SIZE_MAX ||
            record->classes->redstone_torch_external_occurrences == SIZE_MAX)
            return native(error, MC_REGISTRY_LIMIT, false);
        ++record->holds;
        ++record->external;
        ++record->classes->redstone_torch_external_occurrences;
    }
    return result(error, MC_REGISTRY_OK, MC_REGISTRY_REDSTONE_TORCH_NONE, false);
}

void mc_registry_redstone_torch_record_release(mc_registry_redstone_torch_record *record) {
    if (record == NULL)
        return;
    assert(record->external != 0 && record->classes->redstone_torch_external_occurrences != 0);
    --record->external;
    --record->classes->redstone_torch_external_occurrences;
    release_internal(record);
}

/* Selected JDK17 ArrayList.grow/newLength policy. If its signed overflow would
 * require OOME, this native projection reports its distinct resource limit. */
static bool growth(const mc_registry_redstone_torch_list *list, int32_t *out) {
    const int32_t minimum = mc_java_i32_add(list->size, 1);
    if (list->backing == MC_REGISTRY_REDSTONE_TORCH_DEFAULT_EMPTY) {
        *out = minimum > 10 ? minimum : 10;
        return true;
    }
    const int32_t increment = mc_java_i32_sub(minimum, list->capacity);
    const int32_t preferred = list->capacity / 2;
    const int32_t amount = increment > preferred ? increment : preferred;
    const int32_t length = mc_java_i32_add(list->capacity, amount);
    const int32_t soft_maximum = INT32_MAX - 8;
    if (length > 0 && length <= soft_maximum) {
        *out = length;
        return true;
    }
    const int32_t min_length = mc_java_i32_add(list->capacity, increment);
    if (min_length < 0)
        return false;
    *out = min_length <= soft_maximum ? soft_maximum : min_length;
    return true;
}

static mc_registry_status grow(mc_registry_redstone_torch_list *list) {
    int32_t capacity = 0;
    if (!growth(list, &capacity) || capacity <= list->capacity ||
        (size_t)capacity > SIZE_MAX / sizeof(*list->elements) ||
        list->classes->redstone_torch_array_identities == UINT64_MAX)
        return MC_REGISTRY_LIMIT;
    void *storage = NULL;
    const mc_registry_status status = mc_registry_managed_allocate(
        list->classes, (size_t)capacity * sizeof(*list->elements), &storage);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_redstone_torch_record **elements = storage;
    if (list->capacity != 0)
        memcpy(elements, list->elements, (size_t)list->capacity * sizeof(*elements));
    mc_registry_redstone_torch_record **old = list->elements;
    list->elements = elements;
    list->capacity = capacity;
    list->array_identity = ++list->classes->redstone_torch_array_identities;
    list->backing = MC_REGISTRY_REDSTONE_TORCH_OWNED_ARRAY;
    mc_registry_managed_release(list->classes, old);
    return MC_REGISTRY_OK;
}

mc_registry_status
mc_registry_redstone_torch_list_add(const mc_registry_redstone_torch_list_ref *reference,
                                    mc_registry_redstone_torch_record *record,
                                    mc_registry_redstone_torch_error *error) {
    mc_registry_redstone_torch_list *list = NULL;
    mc_registry_status status = resolve(reference, &list, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (record != NULL && (record->classes != list->classes || !record->value.constructed))
        return native(error, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY, false);
    if (record != NULL && record->holds == SIZE_MAX)
        return native(error, MC_REGISTRY_LIMIT, false);
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    if (list->size == list->capacity) {
        status = grow(list);
        if (status != MC_REGISTRY_OK)
            return native(error, status, true);
    }
    if (record != NULL)
        ++record->holds;
    list->elements[list->size] = record;
    list->size = mc_java_i32_add(list->size, 1);
    return result(error, MC_REGISTRY_OK, MC_REGISTRY_REDSTONE_TORCH_NONE, true);
}

mc_registry_status
mc_registry_redstone_torch_list_size(const mc_registry_redstone_torch_list_ref *reference,
                                     int32_t *out, mc_registry_redstone_torch_error *error) {
    if (out == NULL)
        return native(error, MC_REGISTRY_INVALID_ARGUMENT, false);
    mc_registry_redstone_torch_list *list = NULL;
    const mc_registry_status status = resolve(reference, &list, error);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = list->size;
    return result(error, MC_REGISTRY_OK, MC_REGISTRY_REDSTONE_TORCH_NONE, false);
}

mc_registry_status
mc_registry_redstone_torch_list_get(const mc_registry_redstone_torch_list_ref *reference,
                                    int32_t index, mc_registry_redstone_torch_record **out,
                                    mc_registry_redstone_torch_error *error) {
    if (out == NULL)
        return native(error, MC_REGISTRY_INVALID_ARGUMENT, false);
    mc_registry_redstone_torch_list *list = NULL;
    const mc_registry_status status = resolve(reference, &list, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (index < 0 || index >= list->size)
        return result(error, MC_REGISTRY_INDEX_OUT_OF_RANGE, MC_REGISTRY_REDSTONE_TORCH_LIST_INDEX,
                      false);
    *out = list->elements[index];
    return result(error, MC_REGISTRY_OK, MC_REGISTRY_REDSTONE_TORCH_NONE, false);
}

mc_registry_status
mc_registry_redstone_torch_list_remove_first(const mc_registry_redstone_torch_list_ref *reference,
                                             mc_registry_redstone_torch_record **out,
                                             mc_registry_redstone_torch_error *error) {
    if (out == NULL || *out != NULL)
        return native(error, MC_REGISTRY_INVALID_ARGUMENT, false);
    mc_registry_redstone_torch_list *list = NULL;
    const mc_registry_status status = resolve(reference, &list, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (list->size == 0)
        return result(error, MC_REGISTRY_INDEX_OUT_OF_RANGE, MC_REGISTRY_REDSTONE_TORCH_LIST_INDEX,
                      false);
    mc_registry_redstone_torch_record *record = list->elements[0];
    if (record != NULL && (record->external == SIZE_MAX ||
                           list->classes->redstone_torch_external_occurrences == SIZE_MAX))
        return native(error, MC_REGISTRY_LIMIT, false);
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    const int32_t size = list->size - 1;
    if (size != 0)
        memmove(list->elements, list->elements + 1, (size_t)size * sizeof(*list->elements));
    list->elements[size] = NULL;
    list->size = size;
    if (record != NULL) {
        ++record->external;
        ++list->classes->redstone_torch_external_occurrences;
    }
    *out = record;
    return result(error, MC_REGISTRY_OK, MC_REGISTRY_REDSTONE_TORCH_NONE, true);
}

mc_registry_status
mc_registry_redstone_torch_list_inspect(const mc_registry_redstone_torch_list_ref *reference,
                                        mc_registry_redstone_torch_list_view *out,
                                        mc_registry_redstone_torch_error *error) {
    if (out == NULL)
        return native(error, MC_REGISTRY_INVALID_ARGUMENT, false);
    mc_registry_redstone_torch_list *list = NULL;
    const mc_registry_status status = resolve(reference, &list, error);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = list_view(list);
    return result(error, MC_REGISTRY_OK, MC_REGISTRY_REDSTONE_TORCH_NONE, false);
}

mc_registry_status
mc_registry_redstone_torch_snapshot_copy(const mc_registry_redstone_torch_list_ref *reference,
                                         mc_registry_redstone_torch_snapshot **out,
                                         mc_registry_redstone_torch_error *error) {
    if (out == NULL || *out != NULL)
        return native(error, MC_REGISTRY_INVALID_ARGUMENT, false);
    mc_registry_redstone_torch_list *list = NULL;
    mc_registry_status status = resolve(reference, &list, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (list->classes->redstone_torch_external_occurrences == SIZE_MAX ||
        (size_t)list->size > (SIZE_MAX - sizeof(mc_registry_redstone_torch_snapshot)) /
                                 sizeof(mc_registry_redstone_torch_record *))
        return native(error, MC_REGISTRY_LIMIT, false);
    void *storage = NULL;
    status = mc_registry_managed_allocate(list->classes,
                                          sizeof(mc_registry_redstone_torch_snapshot) +
                                              (size_t)list->size * sizeof(*list->elements),
                                          &storage);
    if (status != MC_REGISTRY_OK)
        return native(error, status, false);
    mc_registry_redstone_torch_snapshot *snapshot = storage;
    snapshot->classes = list->classes;
    snapshot->view = list_view(list);
    int32_t i = 0;
    for (; i < list->size; ++i) {
        mc_registry_redstone_torch_record *record = list->elements[i];
        if (record != NULL && record->holds == SIZE_MAX) {
            status = MC_REGISTRY_LIMIT;
            break;
        }
        if (record != NULL)
            ++record->holds;
        snapshot->records[i] = record;
    }
    if (status != MC_REGISTRY_OK) {
        while (i != 0)
            release_internal(snapshot->records[--i]);
        mc_registry_managed_release(list->classes, snapshot);
        return native(error, status, false);
    }
    ++list->classes->redstone_torch_external_occurrences;
    *out = snapshot;
    return result(error, MC_REGISTRY_OK, MC_REGISTRY_REDSTONE_TORCH_NONE, false);
}

mc_registry_status
mc_registry_redstone_torch_snapshot_inspect(const mc_registry_redstone_torch_snapshot *snapshot,
                                            mc_registry_redstone_torch_list_view *out) {
    if (snapshot == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    *out = snapshot->view;
    return MC_REGISTRY_OK;
}

mc_registry_status
mc_registry_redstone_torch_snapshot_at(const mc_registry_redstone_torch_snapshot *snapshot,
                                       size_t index, mc_registry_redstone_torch_record_view *out) {
    if (snapshot == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (index >= (size_t)snapshot->view.size)
        return MC_REGISTRY_INDEX_OUT_OF_RANGE;
    return mc_registry_redstone_torch_record_inspect(snapshot->records[index], out);
}

void mc_registry_redstone_torch_snapshot_release(mc_registry_redstone_torch_snapshot *snapshot) {
    if (snapshot == NULL)
        return;
    mc_registry_classes *classes = snapshot->classes;
    assert(classes->redstone_torch_external_occurrences != 0);
    for (int32_t i = 0; i < snapshot->view.size; ++i)
        release_internal(snapshot->records[i]);
    --classes->redstone_torch_external_occurrences;
    mc_registry_managed_release(classes, snapshot);
}

void mc_registry_redstone_torch_lists_retire(mc_registry_classes *classes) {
    if (classes == NULL)
        return;
    assert(classes->redstone_torch_external_occurrences == 0);
    mc_registry_redstone_torch_list *list = classes->redstone_torch_lists;
    classes->redstone_torch_lists = NULL;
    while (list != NULL) {
        mc_registry_redstone_torch_list *next = list->next_owned;
        for (int32_t i = 0; i < list->size; ++i)
            release_internal(list->elements[i]);
        mc_registry_managed_release(classes, list->elements);
        list->elements = NULL;
        list->size = list->capacity = 0;
        list->next_owned = NULL;
        list = next;
    }
}
