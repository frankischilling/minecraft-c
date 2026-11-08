#include "world_access_internal.h"
#include "world_access_lease_internal.h"

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

static bool same_failure(const mc_world_access_failure *value, const mc_world_access *receiver,
                         const mc_world_access_receipt *receipt) {
    return !value->source_null_receiver && value->receiver == receiver &&
           value->receipt == receipt && value->render_global == receipt->render_global;
}

static bool empty_failure(const mc_world_access_failure *value) {
    return value->receiver == NULL && value->receipt == NULL && value->render_global == NULL &&
           !value->source_null_receiver;
}

static bool receipt_registry(const mc_world_access *receiver, const mc_world_access_receipt *wanted,
                             bool *found) {
    bool present = false;
    const mc_world_access_receipt *previous = NULL;
    for (const mc_world_access_receipt *r = receiver->receipts; r != NULL; r = r->owner_next) {
        if (r->owner_previous != previous || r->receiver != receiver)
            return false;
        if (r == wanted)
            present = true;
        previous = r;
    }
    if (found != NULL)
        *found = present;
    return true;
}

/* Each node owns one existing active right. Subtract from the real available
 * rights, never increment a counter at failure handoff. Back-links detect a
 * repeated/cyclic record without a counter that can wrap. */
static bool receipt_graph(const mc_world_access *receiver,
                          const mc_world_access_receipt *wanted_receipt,
                          const mc_world_access_failure_lease *wanted_lease, bool *receipt_found,
                          bool *lease_found, size_t *available) {
    bool rf = false, lf = false;
    size_t rights = receiver->active_holds;
    const mc_world_access_receipt *previous_receipt = NULL;
    for (const mc_world_access_receipt *r = receiver->receipts; r != NULL; r = r->owner_next) {
        if (r->owner_previous != previous_receipt || r->receiver != receiver || r->holds == 0)
            return false;
        if (r == wanted_receipt)
            rf = true;
        const mc_world_access_failure_lease *previous_lease = NULL;
        for (const mc_world_access_failure_lease *n = r->leases; n != NULL; n = n->next) {
            if (n->identity != n || n->phase != MC_WORLD_ACCESS_FAILURE_LEASE_HELD ||
                n->previous != previous_lease || n->receiver != receiver || n->receipt != r ||
                !same_failure(&n->failure, receiver, r) || rights == 0)
                return false;
            --rights;
            if (n == wanted_lease)
                lf = true;
            previous_lease = n;
        }
        previous_receipt = r;
    }
    if (receipt_found != NULL)
        *receipt_found = rf;
    if (lease_found != NULL)
        *lease_found = lf;
    if (available != NULL)
        *available = rights;
    return true;
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
    if (world_access != NULL &&
        (world_access->memberships != 0 || world_access->active_holds != 0 ||
         world_access->receipt_holds != 0 || world_access->receipts != NULL))
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
        return ledger->items == NULL && ledger->entries == NULL && ledger->identities == NULL &&
               ledger->count == 0 && ledger->capacity == 0;
    if (!shape(list) || ledger->count != list->count || ledger->capacity != list->capacity ||
        ledger->capacity > (size_t)INT32_MAX || list->items != ledger->items ||
        (ledger->capacity == 0) != (ledger->items == NULL) ||
        (ledger->capacity == 0) != (ledger->entries == NULL) ||
        (ledger->capacity == 0) != (ledger->identities == NULL) ||
        (ledger->items != NULL && (void *)ledger->items == (void *)ledger->entries) ||
        (ledger->items != NULL && (void *)ledger->items == (void *)ledger->identities) ||
        (ledger->entries != NULL && (void *)ledger->entries == (void *)ledger->identities) ||
        ledger->capacity > SIZE_MAX / (sizeof(*ledger->items) + sizeof(*ledger->entries) +
                                       sizeof(*ledger->identities)))
        return false;
    const size_t bytes = ledger->capacity * (sizeof(*ledger->items) + sizeof(*ledger->entries) +
                                             sizeof(*ledger->identities));
    if (bytes > world->construction.direct_allocation_bytes)
        return false;
    for (size_t index = 0; index < ledger->count; ++index)
        if (ledger->identities[index] == 0 || ledger->identities[index] > ledger->last_identity ||
            (index != 0 && ledger->identities[index] <= ledger->identities[index - 1]) ||
            ledger->items[index] != ledger->entries[index] ||
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
    const size_t bytes = ledger->capacity * (sizeof(*ledger->items) + sizeof(*ledger->entries) +
                                             sizeof(*ledger->identities));
    free(ledger->items);
    free(ledger->entries);
    free(ledger->identities);
    ledger->identities = NULL;
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

mc_world_storage_status mc_world_access_add_receipted(mc_world_runtime *world,
                                                      mc_world_access *world_access,
                                                      uint64_t *identity,
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
    if (ledger->last_identity == UINT64_MAX || ledger->count == (size_t)INT32_MAX ||
        (world_access != NULL && world_access->memberships == SIZE_MAX))
        return finish(world, MC_WORLD_STORAGE_LIMIT, "WorldAccess occurrence limit", error);
    if (ledger->count == ledger->capacity) {
        const size_t maximum = (size_t)INT32_MAX;
        size_t capacity = ledger->capacity == 0             ? 4u
                          : ledger->capacity > maximum / 2u ? maximum
                                                            : ledger->capacity * 2u;
        if (capacity <= ledger->count ||
            capacity > SIZE_MAX / (sizeof(*ledger->items) + sizeof(*ledger->entries) +
                                   sizeof(*ledger->identities)))
            return finish(world, MC_WORLD_STORAGE_LIMIT, "WorldAccess List capacity", error);
        const size_t bytes = capacity * (sizeof(*ledger->items) + sizeof(*ledger->entries) +
                                         sizeof(*ledger->identities));
        const size_t used = world->construction.direct_allocation_bytes;
        const size_t limit = world->construction_limits.max_direct_allocation_bytes;
        if (used > limit || bytes > limit - used)
            return finish(world, MC_WORLD_STORAGE_LIMIT, "WorldAccess List allocation budget",
                          error);
        /* The peak check includes all three new spans while the old spans
         * remain live. Nothing is published or charged on allocation failure. */
        void **items = calloc(capacity, sizeof(*items));
        mc_world_access **entries = calloc(capacity, sizeof(*entries));
        uint64_t *identities = calloc(capacity, sizeof(*identities));
        if (items == NULL || entries == NULL || identities == NULL) {
            free(items);
            free(entries);
            free(identities);
            return finish(world, MC_WORLD_STORAGE_ALLOCATION_FAILED, "WorldAccess List allocation",
                          error);
        }
        if (ledger->count != 0) {
            memcpy(items, ledger->items, ledger->count * sizeof(*items));
            memcpy(entries, ledger->entries, ledger->count * sizeof(*entries));
            memcpy(identities, ledger->identities, ledger->count * sizeof(*identities));
        }
        const size_t old_bytes =
            ledger->capacity *
            (sizeof(*ledger->items) + sizeof(*ledger->entries) + sizeof(*ledger->identities));
        free(ledger->items);
        free(ledger->entries);
        free(ledger->identities);
        ledger->identities = identities;
        ledger->items = ledger->list->items = items;
        ledger->entries = entries;
        ledger->capacity = ledger->list->capacity = capacity;
        world->construction.direct_allocation_bytes += bytes - old_bytes;
    }
    ledger->items[ledger->count] = world_access;
    ledger->entries[ledger->count] = world_access;
    ledger->identities[ledger->count] = ++ledger->last_identity;
    if (world_access != NULL)
        ++world_access->memberships;
    ++ledger->count;
    ledger->list->count = ledger->count;
    if (identity != NULL)
        *identity = ledger->last_identity;
    return finish(world, MC_WORLD_STORAGE_OK, NULL, error);
}

mc_world_storage_status mc_world_runtime_add_world_access(mc_world_runtime *world,
                                                          mc_world_access *access,
                                                          mc_world_storage_error *error) {
    return mc_world_access_add_receipted(world, access, NULL, error);
}

mc_world_storage_status mc_world_access_remove_receipted(mc_world_runtime *world,
                                                         mc_world_access *world_access, bool *out,
                                                         uint64_t *identity,
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
    uint64_t removed_identity = 0;
    for (size_t index = 0; index < ledger->count; ++index) {
        if (ledger->entries[index] != world_access)
            continue;
        removed_identity = ledger->identities[index];
        const size_t tail = ledger->count - index - 1u;
        memmove(ledger->items + index, ledger->items + index + 1u, tail * sizeof(*ledger->items));
        memmove(ledger->entries + index, ledger->entries + index + 1u,
                tail * sizeof(*ledger->entries));
        memmove(ledger->identities + index, ledger->identities + index + 1u,
                tail * sizeof(*ledger->identities));
        --ledger->count;
        ledger->identities[ledger->count] = 0;
        ledger->list->count = ledger->count;
        ledger->items[ledger->count] = NULL;
        ledger->entries[ledger->count] = NULL;
        if (world_access != NULL)
            --world_access->memberships;
        found = true;
        break;
    }
    *out = found;
    if (identity != NULL)
        *identity = removed_identity;
    return finish(world, MC_WORLD_STORAGE_OK, NULL, error);
}

mc_world_storage_status mc_world_runtime_remove_world_access(mc_world_runtime *world,
                                                             mc_world_access *access, bool *out,
                                                             mc_world_storage_error *error) {
    return mc_world_access_remove_receipted(world, access, out, NULL, error);
}

mc_world_storage_status
mc_world_access_source_create(const mc_world_access_source_callbacks *callbacks, void *context,
                              mc_world_access **out, mc_world_storage_error *error) {
    if (callbacks == NULL || out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "source WorldAccess creation arguments");
    const mc_world_access_callbacks empty = {0};
    mc_world_access *access = NULL;
    mc_world_storage_status status = mc_world_access_create(&empty, context, &access, error);
    if (status == MC_WORLD_STORAGE_OK) {
        access->source_callbacks = *callbacks;
        *out = access;
    }
    return status;
}

mc_world_storage_status mc_world_access_occurrence_present(const mc_world_runtime *world,
                                                           const mc_world_access *access,
                                                           uint64_t identity, bool *out,
                                                           mc_world_storage_error *error) {
    if (world == NULL || identity == 0 || out == NULL ||
        world->construction.mode != MC_WORLD_RUNTIME_GAME)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "WorldAccess occurrence arguments");
    if (!mc_world_accesses_owned_valid(world))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "WorldAccess occurrence ownership");
    const mc_world_access_list_ledger *ledger = &world->world_accesses;
    bool present = false;
    for (size_t i = 0; i < ledger->count; ++i) {
        if (ledger->identities[i] != identity)
            continue;
        if (ledger->entries[i] != access)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "WorldAccess occurrence identity");
        present = true;
        break;
    }
    *out = present;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_access_detach_occurrence(mc_world_runtime *world,
                                                          mc_world_access *access,
                                                          uint64_t identity,
                                                          mc_world_storage_error *error) {
    bool present = false;
    mc_world_storage_status status =
        mc_world_access_occurrence_present(world, access, identity, &present, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (world->active_calls != 0 || world->local_leases != 0 ||
        (access != NULL && access->active_holds != 0))
        return result(error, MC_WORLD_STORAGE_BUSY, "WorldAccess entered occurrence detach");
    if (!present)
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    mc_world_access_list_ledger *ledger = &world->world_accesses;
    for (size_t i = 0; i < ledger->count; ++i) {
        if (ledger->identities[i] != identity)
            continue;
        const size_t tail = ledger->count - i - 1u;
        memmove(ledger->items + i, ledger->items + i + 1u, tail * sizeof(*ledger->items));
        memmove(ledger->entries + i, ledger->entries + i + 1u, tail * sizeof(*ledger->entries));
        memmove(ledger->identities + i, ledger->identities + i + 1u,
                tail * sizeof(*ledger->identities));
        --ledger->count;
        ledger->list->count = ledger->count;
        ledger->items[ledger->count] = NULL;
        ledger->entries[ledger->count] = NULL;
        ledger->identities[ledger->count] = 0;
        if (access != NULL)
            --access->memberships;
        break;
    }
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_storage_status call_cell_body(mc_world_access *access, mc_world_runtime *world,
                                              int32_t x, int32_t y, int32_t z,
                                              mc_world_storage_error *error,
                                              mc_world_access_failure *failure) {
    *failure = (mc_world_access_failure){0};
    if (access != NULL && access->source_callbacks.cell != NULL) {
        const mc_world_storage_status status =
            access->source_callbacks.cell(access->context, world, x, y, z, error, failure);
        return status;
    }
    if (access != NULL && access->callbacks.cell != NULL)
        return access->callbacks.cell(access->context, world, x, y, z, error);
    return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World cell WorldAccess invocation");
}

static mc_world_storage_status call_range_body(mc_world_access *access, mc_world_runtime *world,
                                               int32_t x0, int32_t y0, int32_t z0, int32_t x1,
                                               int32_t y1, int32_t z1,
                                               mc_world_storage_error *error,
                                               mc_world_access_failure *failure) {
    *failure = (mc_world_access_failure){0};
    if (access != NULL && access->source_callbacks.range != NULL) {
        const mc_world_storage_status status = access->source_callbacks.range(
            access->context, world, x0, y0, z0, x1, y1, z1, error, failure);
        return status;
    }
    if (access != NULL && access->callbacks.range != NULL)
        return access->callbacks.range(access->context, world, x0, y0, z0, x1, y1, z1, error);
    return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World range WorldAccess invocation");
}

static mc_world_storage_status call_sound_body(mc_world_access *access, mc_world_runtime *world,
                                               const mc_registry_string *name, double x, double y,
                                               double z, float volume, float pitch,
                                               mc_world_storage_error *error,
                                               mc_world_access_failure *failure) {
    *failure = (mc_world_access_failure){0};
    if (access == NULL) {
        failure->source_null_receiver = true;
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World sound WorldAccess invocation");
    }
    if (access != NULL && access->source_callbacks.sound != NULL) {
        const mc_world_storage_status status = access->source_callbacks.sound(
            access->context, world, name, x, y, z, volume, pitch, error, failure);
        return status;
    }
    if (access != NULL && access->callbacks.sound != NULL)
        return access->callbacks.sound(access->context, world, name, x, y, z, volume, pitch, error);
    return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World sound WorldAccess invocation");
}

static mc_world_storage_status call_particle_body(mc_world_access *access, mc_world_runtime *world,
                                                  const mc_registry_string *name, double x,
                                                  double y, double z, double vx, double vy,
                                                  double vz, mc_world_storage_error *error,
                                                  mc_world_access_failure *failure) {
    *failure = (mc_world_access_failure){0};
    if (access != NULL && access->source_callbacks.particle != NULL) {
        const mc_world_storage_status status = access->source_callbacks.particle(
            access->context, world, name, x, y, z, vx, vy, vz, error, failure);
        return status;
    }
    if (access != NULL && access->callbacks.particle != NULL)
        return access->callbacks.particle(access->context, world, name, x, y, z, vx, vy, vz, error);
    return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "World particle WorldAccess invocation");
}

static mc_world_storage_status call_entity_added_body(mc_world_access *access,
                                                      mc_world_runtime *world, mc_entity *entity,
                                                      mc_world_storage_error *error,
                                                      mc_world_access_failure *failure) {
    *failure = (mc_world_access_failure){0};
    if (access != NULL && access->source_callbacks.entity_added != NULL) {
        const mc_world_storage_status status =
            access->source_callbacks.entity_added(access->context, world, entity, error, failure);
        return status;
    }
    if (access != NULL && access->callbacks.entity_added != NULL)
        return access->callbacks.entity_added(access->context, world, entity, error);
    return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Entity WorldAccess invocation");
}

static mc_world_storage_status call_entity_removed_body(mc_world_access *access,
                                                        mc_world_runtime *world, mc_entity *entity,
                                                        mc_world_storage_error *error,
                                                        mc_world_access_failure *failure) {
    *failure = (mc_world_access_failure){0};
    if (access != NULL && access->source_callbacks.entity_removed != NULL) {
        const mc_world_storage_status status =
            access->source_callbacks.entity_removed(access->context, world, entity, error, failure);
        return status;
    }
    if (access != NULL && access->callbacks.entity_removed != NULL)
        return access->callbacks.entity_removed(access->context, world, entity, error);
    return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "World removed Entity WorldAccess invocation");
}

static mc_world_storage_status call_sky_changed_body(mc_world_access *access,
                                                     mc_world_runtime *world,
                                                     mc_world_storage_error *error,
                                                     mc_world_access_failure *failure) {
    *failure = (mc_world_access_failure){0};
    if (access != NULL && access->source_callbacks.sky_changed != NULL) {
        const mc_world_storage_status status =
            access->source_callbacks.sky_changed(access->context, world, error, failure);
        return status;
    }
    if (access != NULL && access->callbacks.sky_changed != NULL)
        return access->callbacks.sky_changed(access->context, world, error);
    return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World sky WorldAccess invocation");
}

static mc_world_storage_status
call_tile_entity_dirty_body(mc_world_access *access, mc_world_runtime *world, int32_t x, int32_t y,
                            int32_t z, mc_tile_entity *tile, mc_world_storage_error *error,
                            mc_world_access_failure *failure) {
    *failure = (mc_world_access_failure){0};
    if (access != NULL && access->source_callbacks.tile_entity_dirty != NULL) {
        const mc_world_storage_status status = access->source_callbacks.tile_entity_dirty(
            access->context, world, x, y, z, tile, error, failure);
        return status;
    }
    if (access != NULL && access->callbacks.tile_entity_dirty != NULL)
        return access->callbacks.tile_entity_dirty(access->context, world, x, y, z, tile, error);
    return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "TileEntity dirty WorldAccess invocation");
}

static mc_world_storage_status call_auxiliary_body(mc_world_access *access, mc_world_runtime *world,
                                                   mc_entity *player, int32_t effect, int32_t x,
                                                   int32_t y, int32_t z, int32_t data,
                                                   mc_world_storage_error *error,
                                                   mc_world_access_failure *failure) {
    *failure = (mc_world_access_failure){0};
    if (access != NULL && access->source_callbacks.auxiliary != NULL) {
        const mc_world_storage_status status = access->source_callbacks.auxiliary(
            access->context, world, player, effect, x, y, z, data, error, failure);
        return status;
    }
    if (access != NULL && access->callbacks.auxiliary != NULL)
        return access->callbacks.auxiliary(access->context, world, player, effect, x, y, z, data,
                                           error);
    return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "World auxiliary WorldAccess invocation");
}

mc_world_storage_status mc_world_access_receipt_create(mc_world_access *receiver,
                                                       const mc_client_render_global_error *cause,
                                                       mc_world_access_receipt **out,
                                                       mc_world_access_failure *view,
                                                       mc_world_storage_error *error) {
    if (receiver == NULL || out == NULL || view == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "WorldAccess receipt arguments");
    if (receiver->receipt_holds == SIZE_MAX)
        return result(error, MC_WORLD_STORAGE_LIMIT, "WorldAccess receipt owner limit");
    if (!receipt_registry(receiver, NULL, NULL))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "WorldAccess actual receipt ownership graph");
    mc_world_access_receipt *receipt = calloc(1, sizeof(*receipt));
    if (receipt == NULL)
        return result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "WorldAccess receipt allocation");
    receipt->receiver = receiver;
    receipt->render_global = cause;
    receipt->holds = 1;
    receipt->owner_next = receiver->receipts;
    if (receipt->owner_next != NULL)
        receipt->owner_next->owner_previous = receipt;
    receiver->receipts = receipt;
    ++receiver->receipt_holds;
    *out = receipt;
    *view = (mc_world_access_failure){receiver, cause, receipt, false};
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

static bool null_receiver_failure(const mc_world_access_failure *failure) {
    return failure->source_null_receiver && failure->receiver == NULL &&
           failure->render_global == NULL && failure->receipt == NULL;
}

mc_world_storage_status mc_world_access_failure_retain(const mc_world_access_failure *borrowed,
                                                       mc_world_access_failure *out) {
    if (borrowed == NULL || out == NULL || borrowed == out)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (null_receiver_failure(borrowed)) {
        *out = *borrowed;
        return MC_WORLD_STORAGE_OK;
    }
    if (borrowed->source_null_receiver || borrowed->receipt == NULL ||
        borrowed->receiver != borrowed->receipt->receiver ||
        borrowed->render_global != borrowed->receipt->render_global)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (borrowed->receipt->holds == SIZE_MAX)
        return MC_WORLD_STORAGE_LIMIT;
    ++borrowed->receipt->holds;
    *out = *borrowed;
    return MC_WORLD_STORAGE_OK;
}

void mc_world_access_failure_release(mc_world_access_failure *owned) {
    if (owned == NULL)
        return;
    if (null_receiver_failure(owned)) {
        *owned = (mc_world_access_failure){0};
        return;
    }
    if (owned->receipt == NULL)
        return;
    if (owned->receipt->holds <= 1)
        return;
    --owned->receipt->holds;
    *owned = (mc_world_access_failure){0};
}

mc_world_storage_status mc_world_access_receipt_retire(mc_world_access_receipt **slot,
                                                       mc_world_storage_error *error) {
    if (slot == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess receipt retirement slot");
    mc_world_access_receipt *receipt = *slot;
    if (receipt == NULL)
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    if (receipt->leases != NULL || receipt->holds != 1)
        return result(error, MC_WORLD_STORAGE_BUSY, "WorldAccess captured callback receipt");
    bool found = false;
    if (receipt->receiver == NULL || receipt->receiver->receipt_holds == 0 ||
        !receipt_registry(receipt->receiver, receipt, &found) || !found)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "WorldAccess actual receipt retirement ownership");
    if (receipt->owner_previous != NULL)
        receipt->owner_previous->owner_next = receipt->owner_next;
    else
        receipt->receiver->receipts = receipt->owner_next;
    if (receipt->owner_next != NULL)
        receipt->owner_next->owner_previous = receipt->owner_previous;
    --receipt->receiver->receipt_holds;
    free(receipt);
    *slot = NULL;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

bool mc_world_access_failure_source_exception(const mc_world_access_failure *failure,
                                              const char **exception_class) {
    if (failure == NULL)
        return false;
    if (null_receiver_failure(failure)) {
        if (exception_class != NULL)
            *exception_class = "java/lang/NullPointerException";
        return true;
    }
    if (failure->source_null_receiver || failure->receipt == NULL ||
        failure->receiver != failure->receipt->receiver ||
        failure->render_global != failure->receipt->render_global ||
        failure->receipt->exception_class == NULL)
        return false;
    if (exception_class != NULL)
        *exception_class = failure->receipt->exception_class;
    return true;
}

static bool lease_idle(const mc_world_access_failure_lease *node) {
    return node->identity == node && node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_IDLE &&
           node->receiver == NULL && node->receipt == NULL && node->previous == NULL &&
           node->next == NULL && empty_failure(&node->failure);
}

static void lease_empty(mc_world_access_failure_lease *node) {
    node->phase = MC_WORLD_ACCESS_FAILURE_LEASE_IDLE;
    node->receiver = NULL;
    node->receipt = NULL;
    node->previous = node->next = NULL;
    node->failure = (mc_world_access_failure){0};
}

void mc_world_access_failure_lease_init(mc_world_access_failure_lease *node) {
    if (node == NULL)
        return;
    *node = (mc_world_access_failure_lease){.identity = node};
}

bool mc_world_access_failure_lease_inspect(const mc_world_access_failure_lease *node,
                                           const mc_world_access_failure **out) {
    if (node == NULL || out == NULL || node->identity != node ||
        node->phase != MC_WORLD_ACCESS_FAILURE_LEASE_HELD || node->receiver == NULL ||
        node->receipt == NULL)
        return false;
    bool receipt_found = false, lease_found = false;
    if (!receipt_graph(node->receiver, node->receipt, node, &receipt_found, &lease_found, NULL) ||
        !receipt_found || !lease_found)
        return false;
    *out = &node->failure;
    return true;
}

mc_world_storage_status mc_world_access_failure_lease_release(mc_world_access_failure_lease *node,
                                                              mc_world_storage_error *cleanup) {
    if (node == NULL || node->identity != node)
        return result(cleanup, MC_WORLD_STORAGE_INVALID_ARGUMENT, "WorldAccess lease identity");
    if (lease_idle(node))
        return result(cleanup, MC_WORLD_STORAGE_OK, NULL);
    if (node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED)
        return result(cleanup, MC_WORLD_STORAGE_BUSY, "WorldAccess lease callback entered");
    const mc_world_access_failure *view = NULL;
    if (!mc_world_access_failure_lease_inspect(node, &view))
        return result(cleanup, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "WorldAccess lease actual retirement graph");
    mc_world_access *receiver = node->receiver;
    mc_world_access_receipt *receipt = node->receipt;
    /* All structural/rights admission precedes mutation. Clear the protected
     * view while BOTH the receipt guard and receiver active right still exist. */
    node->failure = (mc_world_access_failure){0};
    if (node->previous != NULL)
        node->previous->next = node->next;
    else
        receipt->leases = node->next;
    if (node->next != NULL)
        node->next->previous = node->previous;
    lease_empty(node);
    --receiver->active_holds;
    return result(cleanup, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_storage_status lease_prepare(mc_world_access **held_slot,
                                             mc_world_access_failure_lease *node,
                                             mc_world_storage_error *error,
                                             mc_world_access_failure *failure) {
    if (held_slot == NULL || error == NULL || failure == NULL || failure == &node->failure ||
        error == &node->callback_error || held_slot == &node->receiver)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess lease invocation storage");
    *failure = (mc_world_access_failure){0};
    if (!lease_idle(node))
        return result(error, MC_WORLD_STORAGE_BUSY, "WorldAccess lease fresh invocation slot");
    mc_world_access *receiver = *held_slot;
    size_t available = 0;
    if (receiver != NULL &&
        (!receipt_graph(receiver, NULL, NULL, NULL, NULL, &available) || available == 0))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "WorldAccess lease actual current active occurrence");
    node->phase = MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
    node->receiver = receiver; /* Borrowed until the actual post-call transfer. */
    node->callback_status = MC_WORLD_STORAGE_OK;
    node->callback_error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status lease_complete(mc_world_access **held_slot,
                                              mc_world_access_failure_lease *node,
                                              mc_world_storage_status status,
                                              mc_world_storage_error *error,
                                              mc_world_access_failure *failure) {
    node->callback_status = status;
    node->callback_error = *error; /* Value-only first callback status/cause. */
    mc_world_access *receiver = node->receiver;
    if (status == MC_WORLD_STORAGE_OK) {
        *failure = (mc_world_access_failure){0};
        lease_empty(node);
        return status;
    }
    if (empty_failure(failure) ||
        (receiver == NULL && *held_slot == NULL && null_receiver_failure(failure))) {
        lease_empty(node); /* Actual null receiver / ordinary no-receipt failure. */
        return status;
    }
    bool found = false;
    size_t available = 0;
    /* Never infer a producer from a foreign pointer or normalize an identity
     * mismatch. Walk managed receiver-owned identities before dereferencing
     * the returned receipt; no receipt.holds increment or allocation occurs. */
    if (*held_slot != receiver || receiver == NULL || failure->source_null_receiver ||
        failure->receiver != receiver || failure->receipt == NULL ||
        !receipt_graph(receiver, failure->receipt, NULL, &found, NULL, &available) || !found ||
        available == 0 || !same_failure(failure, receiver, failure->receipt)) {
        *failure = (mc_world_access_failure){0}; /* No unowned foreign view escapes. */
        lease_empty(node);
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "WorldAccess returned failure actual producer identity");
    }
    node->receipt = failure->receipt;
    node->failure = *failure;
    node->phase = MC_WORLD_ACCESS_FAILURE_LEASE_HELD;
    node->next = node->receipt->leases;
    if (node->next != NULL)
        node->next->previous = node;
    node->receipt->leases = node;
    *held_slot = NULL; /* MOVE this invocation's existing active occurrence. */
    return status;
}

mc_world_storage_status mc_world_access_call_cell(mc_world_access *access, mc_world_runtime *world,
                                                  int32_t x, int32_t y, int32_t z,
                                                  mc_world_storage_error *error,
                                                  mc_world_access_failure *failure) {
    const mc_world_storage_status status = call_cell_body(access, world, x, y, z, error, failure);
    if (failure->render_global != NULL || failure->receipt != NULL)
        failure->receiver = access; /* Exact legacy borrowed-view contract. */
    return status;
}

mc_world_storage_status mc_world_access_call_cell_with_lease(mc_world_access **held_slot,
                                                             mc_world_runtime *world, int32_t x,
                                                             int32_t y, int32_t z,
                                                             mc_world_access_failure_lease *node,
                                                             mc_world_storage_error *error,
                                                             mc_world_access_failure *failure) {
    if (held_slot == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess invocation owning slot");
    if (node == NULL)
        return mc_world_access_call_cell(*held_slot, world, x, y, z, error, failure);
    const mc_world_storage_status admission = lease_prepare(held_slot, node, error, failure);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    const mc_world_storage_status status =
        call_cell_body(*held_slot, world, x, y, z, error, failure);
    return lease_complete(held_slot, node, status, error, failure);
}

mc_world_storage_status mc_world_access_call_range(mc_world_access *access, mc_world_runtime *world,
                                                   int32_t x0, int32_t y0, int32_t z0, int32_t x1,
                                                   int32_t y1, int32_t z1,
                                                   mc_world_storage_error *error,
                                                   mc_world_access_failure *failure) {
    const mc_world_storage_status status =
        call_range_body(access, world, x0, y0, z0, x1, y1, z1, error, failure);
    if (failure->render_global != NULL || failure->receipt != NULL)
        failure->receiver = access; /* Exact legacy borrowed-view contract. */
    return status;
}

mc_world_storage_status mc_world_access_call_range_with_lease(
    mc_world_access **held_slot, mc_world_runtime *world, int32_t x0, int32_t y0, int32_t z0,
    int32_t x1, int32_t y1, int32_t z1, mc_world_access_failure_lease *node,
    mc_world_storage_error *error, mc_world_access_failure *failure) {
    if (held_slot == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess invocation owning slot");
    if (node == NULL)
        return mc_world_access_call_range(*held_slot, world, x0, y0, z0, x1, y1, z1, error,
                                          failure);
    const mc_world_storage_status admission = lease_prepare(held_slot, node, error, failure);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    const mc_world_storage_status status =
        call_range_body(*held_slot, world, x0, y0, z0, x1, y1, z1, error, failure);
    return lease_complete(held_slot, node, status, error, failure);
}

mc_world_storage_status mc_world_access_call_sound(mc_world_access *access, mc_world_runtime *world,
                                                   const mc_registry_string *name, double x,
                                                   double y, double z, float volume, float pitch,
                                                   mc_world_storage_error *error,
                                                   mc_world_access_failure *failure) {
    const mc_world_storage_status status =
        call_sound_body(access, world, name, x, y, z, volume, pitch, error, failure);
    if (failure->render_global != NULL || failure->receipt != NULL)
        failure->receiver = access; /* Exact legacy borrowed-view contract. */
    return status;
}

mc_world_storage_status mc_world_access_call_sound_with_lease(
    mc_world_access **held_slot, mc_world_runtime *world, const mc_registry_string *name, double x,
    double y, double z, float volume, float pitch, mc_world_access_failure_lease *node,
    mc_world_storage_error *error, mc_world_access_failure *failure) {
    if (held_slot == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess invocation owning slot");
    if (node == NULL)
        return mc_world_access_call_sound(*held_slot, world, name, x, y, z, volume, pitch, error,
                                          failure);
    const mc_world_storage_status admission = lease_prepare(held_slot, node, error, failure);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    const mc_world_storage_status status =
        call_sound_body(*held_slot, world, name, x, y, z, volume, pitch, error, failure);
    return lease_complete(held_slot, node, status, error, failure);
}

mc_world_storage_status mc_world_access_call_particle(mc_world_access *access,
                                                      mc_world_runtime *world,
                                                      const mc_registry_string *name, double x,
                                                      double y, double z, double vx, double vy,
                                                      double vz, mc_world_storage_error *error,
                                                      mc_world_access_failure *failure) {
    const mc_world_storage_status status =
        call_particle_body(access, world, name, x, y, z, vx, vy, vz, error, failure);
    if (failure->render_global != NULL || failure->receipt != NULL)
        failure->receiver = access; /* Exact legacy borrowed-view contract. */
    return status;
}

mc_world_storage_status mc_world_access_call_particle_with_lease(
    mc_world_access **held_slot, mc_world_runtime *world, const mc_registry_string *name, double x,
    double y, double z, double vx, double vy, double vz, mc_world_access_failure_lease *node,
    mc_world_storage_error *error, mc_world_access_failure *failure) {
    if (held_slot == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess invocation owning slot");
    if (node == NULL)
        return mc_world_access_call_particle(*held_slot, world, name, x, y, z, vx, vy, vz, error,
                                             failure);
    const mc_world_storage_status admission = lease_prepare(held_slot, node, error, failure);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    const mc_world_storage_status status =
        call_particle_body(*held_slot, world, name, x, y, z, vx, vy, vz, error, failure);
    return lease_complete(held_slot, node, status, error, failure);
}

mc_world_storage_status mc_world_access_call_entity_added(mc_world_access *access,
                                                          mc_world_runtime *world,
                                                          mc_entity *entity,
                                                          mc_world_storage_error *error,
                                                          mc_world_access_failure *failure) {
    const mc_world_storage_status status =
        call_entity_added_body(access, world, entity, error, failure);
    if (failure->render_global != NULL || failure->receipt != NULL)
        failure->receiver = access; /* Exact legacy borrowed-view contract. */
    return status;
}

mc_world_storage_status
mc_world_access_call_entity_added_with_lease(mc_world_access **held_slot, mc_world_runtime *world,
                                             mc_entity *entity, mc_world_access_failure_lease *node,
                                             mc_world_storage_error *error,
                                             mc_world_access_failure *failure) {
    if (held_slot == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess invocation owning slot");
    if (node == NULL)
        return mc_world_access_call_entity_added(*held_slot, world, entity, error, failure);
    const mc_world_storage_status admission = lease_prepare(held_slot, node, error, failure);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    const mc_world_storage_status status =
        call_entity_added_body(*held_slot, world, entity, error, failure);
    return lease_complete(held_slot, node, status, error, failure);
}

mc_world_storage_status mc_world_access_call_entity_removed(mc_world_access *access,
                                                            mc_world_runtime *world,
                                                            mc_entity *entity,
                                                            mc_world_storage_error *error,
                                                            mc_world_access_failure *failure) {
    const mc_world_storage_status status =
        call_entity_removed_body(access, world, entity, error, failure);
    if (failure->render_global != NULL || failure->receipt != NULL)
        failure->receiver = access; /* Exact legacy borrowed-view contract. */
    return status;
}

mc_world_storage_status mc_world_access_call_entity_removed_with_lease(
    mc_world_access **held_slot, mc_world_runtime *world, mc_entity *entity,
    mc_world_access_failure_lease *node, mc_world_storage_error *error,
    mc_world_access_failure *failure) {
    if (held_slot == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess invocation owning slot");
    if (node == NULL)
        return mc_world_access_call_entity_removed(*held_slot, world, entity, error, failure);
    const mc_world_storage_status admission = lease_prepare(held_slot, node, error, failure);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    const mc_world_storage_status status =
        call_entity_removed_body(*held_slot, world, entity, error, failure);
    return lease_complete(held_slot, node, status, error, failure);
}

mc_world_storage_status mc_world_access_call_sky_changed(mc_world_access *access,
                                                         mc_world_runtime *world,
                                                         mc_world_storage_error *error,
                                                         mc_world_access_failure *failure) {
    const mc_world_storage_status status = call_sky_changed_body(access, world, error, failure);
    if (failure->render_global != NULL || failure->receipt != NULL)
        failure->receiver = access; /* Exact legacy borrowed-view contract. */
    return status;
}

mc_world_storage_status mc_world_access_call_sky_changed_with_lease(
    mc_world_access **held_slot, mc_world_runtime *world, mc_world_access_failure_lease *node,
    mc_world_storage_error *error, mc_world_access_failure *failure) {
    if (held_slot == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess invocation owning slot");
    if (node == NULL)
        return mc_world_access_call_sky_changed(*held_slot, world, error, failure);
    const mc_world_storage_status admission = lease_prepare(held_slot, node, error, failure);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    const mc_world_storage_status status = call_sky_changed_body(*held_slot, world, error, failure);
    return lease_complete(held_slot, node, status, error, failure);
}

mc_world_storage_status mc_world_access_call_tile_entity_dirty(
    mc_world_access *access, mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
    mc_tile_entity *tile, mc_world_storage_error *error, mc_world_access_failure *failure) {
    const mc_world_storage_status status =
        call_tile_entity_dirty_body(access, world, x, y, z, tile, error, failure);
    if (failure->render_global != NULL || failure->receipt != NULL)
        failure->receiver = access; /* Exact legacy borrowed-view contract. */
    return status;
}

mc_world_storage_status mc_world_access_call_tile_entity_dirty_with_lease(
    mc_world_access **held_slot, mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
    mc_tile_entity *tile, mc_world_access_failure_lease *node, mc_world_storage_error *error,
    mc_world_access_failure *failure) {
    if (held_slot == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess invocation owning slot");
    if (node == NULL)
        return mc_world_access_call_tile_entity_dirty(*held_slot, world, x, y, z, tile, error,
                                                      failure);
    const mc_world_storage_status admission = lease_prepare(held_slot, node, error, failure);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    const mc_world_storage_status status =
        call_tile_entity_dirty_body(*held_slot, world, x, y, z, tile, error, failure);
    return lease_complete(held_slot, node, status, error, failure);
}

mc_world_storage_status
mc_world_access_call_auxiliary(mc_world_access *access, mc_world_runtime *world, mc_entity *player,
                               int32_t effect, int32_t x, int32_t y, int32_t z, int32_t data,
                               mc_world_storage_error *error, mc_world_access_failure *failure) {
    const mc_world_storage_status status =
        call_auxiliary_body(access, world, player, effect, x, y, z, data, error, failure);
    if (failure->render_global != NULL || failure->receipt != NULL)
        failure->receiver = access; /* Exact legacy borrowed-view contract. */
    return status;
}

mc_world_storage_status mc_world_access_call_auxiliary_with_lease(
    mc_world_access **held_slot, mc_world_runtime *world, mc_entity *player, int32_t effect,
    int32_t x, int32_t y, int32_t z, int32_t data, mc_world_access_failure_lease *node,
    mc_world_storage_error *error, mc_world_access_failure *failure) {
    if (held_slot == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "WorldAccess invocation owning slot");
    if (node == NULL)
        return mc_world_access_call_auxiliary(*held_slot, world, player, effect, x, y, z, data,
                                              error, failure);
    const mc_world_storage_status admission = lease_prepare(held_slot, node, error, failure);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    const mc_world_storage_status status =
        call_auxiliary_body(*held_slot, world, player, effect, x, y, z, data, error, failure);
    return lease_complete(held_slot, node, status, error, failure);
}
