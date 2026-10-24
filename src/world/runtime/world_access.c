#include "world_access_internal.h"

#include "construction_internal.h"
#include "runtime_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static mc_world_storage_status result(mc_world_storage_error *error, mc_world_storage_status status,
                                      const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

mc_world_storage_error mc_world_access_callback_error(mc_world_storage_status status,
                                                      mc_world_storage_error cause) {
    if (status == MC_WORLD_STORAGE_OK)
        return (mc_world_storage_error){0};
    cause.status = status;
    if (cause.operation == NULL)
        cause.operation = "WorldAccess callback";
    return cause;
}

mc_world_storage_status mc_world_access_create(const mc_world_access_callbacks *callbacks,
                                               void *context, mc_world_access **out,
                                               mc_world_storage_error *error) {
    if (callbacks == NULL || out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "WorldAccess creation arguments");
    mc_world_access *world_access = calloc(1, sizeof(*world_access));
    if (world_access == NULL)
        return result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "WorldAccess allocation");
    world_access->callbacks = *callbacks;
    world_access->context = context;
    *out = world_access;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_access_free(mc_world_access *world_access,
                                             mc_world_storage_error *error) {
    if (world_access != NULL && (world_access->memberships != 0 || world_access->active_holds != 0))
        return result(error, MC_WORLD_STORAGE_BUSY, "WorldAccess is retained");
    free(world_access);
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

static bool shape(const mc_world_reference_list *list) {
    /* Source dispatch retains the existing finite void* List admission. Only
     * native ownership admission below imposes the owned int-sized capacity. */
    return list != NULL && list->count <= list->capacity &&
           list->capacity <= SIZE_MAX / sizeof(*list->items) &&
           (list->capacity == 0 || list->items != NULL);
}

bool mc_world_accesses_owned_valid(const mc_world_runtime *world) {
    if (world == NULL)
        return false;
    const mc_world_access_list_ledger *ledger = &world->world_accesses;
    const mc_world_reference_list *list = ledger->list;
    if (list == NULL)
        return ledger->items == NULL && ledger->entries == NULL && ledger->count == 0 &&
               ledger->capacity == 0;
    if (!shape(list) || ledger->count != list->count || ledger->capacity != list->capacity ||
        ledger->capacity > (size_t)INT32_MAX || list->items != ledger->items ||
        (ledger->capacity == 0) != (ledger->items == NULL) ||
        (ledger->capacity == 0) != (ledger->entries == NULL) ||
        (ledger->items != NULL && (void *)ledger->items == (void *)ledger->entries) ||
        ledger->capacity > SIZE_MAX / (sizeof(*ledger->items) + sizeof(*ledger->entries)))
        return false;
    const size_t bytes = ledger->capacity * (sizeof(*ledger->items) + sizeof(*ledger->entries));
    if (bytes > world->construction.direct_allocation_bytes)
        return false;
    for (size_t index = 0; index < ledger->count; ++index)
        if (ledger->items[index] != ledger->entries[index] ||
            (ledger->entries[index] != NULL && ledger->entries[index]->memberships == 0))
            return false;
    return true;
}

bool mc_world_accesses_retirement_valid(const mc_world_runtime *world) {
    if (!mc_world_accesses_owned_valid(world))
        return false;
    const mc_world_reference_list *current = world->fields.u;
    /* Only the captured constructor descriptor owns the typed occurrences.
     * A source replacement may be NULL or a shaped empty borrowed List; it is
     * never adopted, cleared or freed. Foreign nonempty contents retain their
     * explicit terminal admission failure before any owner is released. */
    return current == world->world_accesses.list || current == NULL ||
           (shape(current) && current->count == 0);
}

void mc_world_accesses_owned_release(mc_world_runtime *world) {
    mc_world_access_list_ledger *ledger = &world->world_accesses;
    if (ledger->list == NULL)
        return;
    /* Native terminal commit only: entries remain owned until each counter is
     * released. No user destructor/callback runs and the descriptor survives. */
    for (size_t index = 0; index < ledger->count; ++index)
        if (ledger->entries[index] != NULL)
            --ledger->entries[index]->memberships;
    const size_t bytes = ledger->capacity * (sizeof(*ledger->items) + sizeof(*ledger->entries));
    free(ledger->items);
    free(ledger->entries);
    ledger->items = NULL;
    ledger->entries = NULL;
    ledger->count = ledger->capacity = 0;
    ledger->list->items = NULL;
    ledger->list->count = ledger->list->capacity = 0;
    world->construction.direct_allocation_bytes -= bytes;
}

bool mc_world_access_next(mc_world_runtime *world, size_t index, unsigned int size_pc,
                          unsigned int get_pc, unsigned int cast_pc, mc_world_access **out,
                          bool *has, unsigned int *failed_pc, mc_world_storage_error *error) {
    const mc_world_reference_list *list = world->fields.u;
    *failed_pc = size_pc;
    if (!shape(list)) {
        result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldAccess size receiver metadata");
        return false;
    }
    if (index >= list->count) {
        *out = NULL;
        *has = false;
        *failed_pc = 0;
        result(error, MC_WORLD_STORAGE_OK, NULL);
        return true;
    }
    *failed_pc = get_pc;
    list = world->fields.u; /* Independent source receiver before List.get. */
    if (!shape(list) || index >= list->count) {
        result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldAccess get receiver or index");
        return false;
    }
    void *value = list->items[index];
    mc_world_access *world_access = NULL;
    if (value != NULL) {
        *failed_pc = cast_pc;
        const mc_world_access_list_ledger *ledger = &world->world_accesses;
        /* Compare raw addresses before dereferencing the trusted typed entry.
         * No later occurrence or foreign list is preflighted at this cast. */
        if (list != ledger->list || index >= ledger->count || index >= ledger->capacity ||
            ledger->entries == NULL || value != ledger->entries[index]) {
            result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "World concrete WorldAccess cast unavailable");
            return false;
        }
        world_access = ledger->entries[index];
        if (world_access->memberships == 0) {
            result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldAccess occurrence hold");
            return false;
        }
        if (world_access->active_holds == SIZE_MAX) {
            result(error, MC_WORLD_STORAGE_LIMIT, "WorldAccess active hold limit");
            return false;
        }
        ++world_access->active_holds;
    }
    *out = world_access;
    *has = true;
    *failed_pc = 0;
    result(error, MC_WORLD_STORAGE_OK, NULL);
    return true;
}

void mc_world_access_release_held(mc_world_access *world_access) {
    if (world_access != NULL)
        --world_access->active_holds;
}

static mc_world_storage_status admit(mc_world_runtime *world, mc_world_storage_error *error) {
    if (world == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess registration arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldAccess registration phase");
    return mc_world_runtime_call_enter(world, error);
}

static mc_world_storage_status finish(mc_world_runtime *world, mc_world_storage_status status,
                                      const char *operation, mc_world_storage_error *error) {
    mc_world_runtime_call_leave(world);
    return result(error, status, operation);
}

mc_world_storage_status mc_world_runtime_add_world_access(mc_world_runtime *world,
                                                          mc_world_access *world_access,
                                                          mc_world_storage_error *error) {
    /* fd.a(pm)/dj.a(jp): actual u at1, List.add5, discarded boolean10. The
     * native owning List permits NULL and every duplicate. No callback or
     * unrelated WorldAccess slot is inspected during this operation. */
    mc_world_storage_status status = admit(world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (!mc_world_accesses_owned_valid(world) || world->world_accesses.list == NULL ||
        world->fields.u != world->world_accesses.list)
        return finish(world, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldAccess List ownership",
                      error);
    mc_world_access_list_ledger *ledger = &world->world_accesses;
    if (ledger->count == (size_t)INT32_MAX ||
        (world_access != NULL && world_access->memberships == SIZE_MAX))
        return finish(world, MC_WORLD_STORAGE_LIMIT, "WorldAccess occurrence limit", error);
    if (ledger->count == ledger->capacity) {
        const size_t maximum = (size_t)INT32_MAX;
        size_t capacity = ledger->capacity == 0             ? 4u
                          : ledger->capacity > maximum / 2u ? maximum
                                                            : ledger->capacity * 2u;
        if (capacity <= ledger->count ||
            capacity > SIZE_MAX / (sizeof(*ledger->items) + sizeof(*ledger->entries)))
            return finish(world, MC_WORLD_STORAGE_LIMIT, "WorldAccess List capacity", error);
        const size_t bytes = capacity * (sizeof(*ledger->items) + sizeof(*ledger->entries));
        const size_t used = world->construction.direct_allocation_bytes;
        const size_t limit = world->construction_limits.max_direct_allocation_bytes;
        if (used > limit || bytes > limit - used)
            return finish(world, MC_WORLD_STORAGE_LIMIT, "WorldAccess List allocation budget",
                          error);
        /* The peak check includes the complete new pair while the old pair
         * remains live. Nothing is published or charged on allocation failure. */
        void **items = calloc(capacity, sizeof(*items));
        mc_world_access **entries = calloc(capacity, sizeof(*entries));
        if (items == NULL || entries == NULL) {
            free(items);
            free(entries);
            return finish(world, MC_WORLD_STORAGE_ALLOCATION_FAILED, "WorldAccess List allocation",
                          error);
        }
        if (ledger->count != 0) {
            memcpy(items, ledger->items, ledger->count * sizeof(*items));
            memcpy(entries, ledger->entries, ledger->count * sizeof(*entries));
        }
        const size_t old_bytes =
            ledger->capacity * (sizeof(*ledger->items) + sizeof(*ledger->entries));
        free(ledger->items);
        free(ledger->entries);
        ledger->items = ledger->list->items = items;
        ledger->entries = entries;
        ledger->capacity = ledger->list->capacity = capacity;
        world->construction.direct_allocation_bytes += bytes - old_bytes;
    }
    ledger->items[ledger->count] = world_access;
    ledger->entries[ledger->count] = world_access;
    if (world_access != NULL)
        ++world_access->memberships;
    ++ledger->count;
    ledger->list->count = ledger->count;
    return finish(world, MC_WORLD_STORAGE_OK, NULL, error);
}

mc_world_storage_status mc_world_runtime_remove_world_access(mc_world_runtime *world,
                                                             mc_world_access *world_access,
                                                             bool *out,
                                                             mc_world_storage_error *error) {
    /* Client fd.b(pm): actual u at1, List.remove5, discarded boolean10. This
     * concrete native handle has identity equality; remove the first matching
     * occurrence. No matching server World method exists. */
    if (out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "WorldAccess removal output");
    mc_world_storage_status status = admit(world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (world->classes->side != MC_WORLD_INFO_CLIENT)
        return finish(world, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Server WorldAccess removal is unavailable", error);
    if (!mc_world_accesses_owned_valid(world) || world->world_accesses.list == NULL ||
        world->fields.u != world->world_accesses.list)
        return finish(world, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldAccess List ownership",
                      error);
    mc_world_access_list_ledger *ledger = &world->world_accesses;
    bool found = false;
    for (size_t index = 0; index < ledger->count; ++index) {
        if (ledger->entries[index] != world_access)
            continue;
        const size_t tail = ledger->count - index - 1u;
        memmove(ledger->items + index, ledger->items + index + 1u, tail * sizeof(*ledger->items));
        memmove(ledger->entries + index, ledger->entries + index + 1u,
                tail * sizeof(*ledger->entries));
        --ledger->count;
        ledger->list->count = ledger->count;
        ledger->items[ledger->count] = NULL;
        ledger->entries[ledger->count] = NULL;
        if (world_access != NULL)
            --world_access->memberships;
        found = true;
        break;
    }
    *out = found;
    return finish(world, MC_WORLD_STORAGE_OK, NULL, error);
}
