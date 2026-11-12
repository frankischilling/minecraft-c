#include "execution_internal.h"

#include "../../world/runtime/runtime_internal.h"

static mc_entity_tick_execution_error empty_error(void) {
    return (mc_entity_tick_execution_error){MC_WORLD_STORAGE_OK, NULL, {NULL, NULL, NULL, -1}};
}

static bool valid(const mc_entity_tick_execution *e) {
    return e != NULL && e->identity == e;
}

static mc_world_storage_status admission(mc_entity_tick_execution *e,
                                         mc_world_storage_status status, const char *operation) {
    if (e != NULL)
        e->admission = (mc_entity_tick_execution_error){status, operation, e->site};
    return status;
}

static mc_world_storage_status capture_failed(mc_entity_tick_execution *e,
                                              mc_world_storage_status status, const char *operation,
                                              bool unprotected) {
    e->capture = (mc_entity_tick_execution_error){status, operation, e->site};
    if (e->first.status == MC_WORLD_STORAGE_OK)
        e->first = e->capture;
    e->capture_pending |= unprotected;
    e->phase = MC_ENTITY_TICK_EXECUTION_FAILED;
    return status;
}

static mc_world_storage_status
cleanup_failed(mc_entity_tick_execution *e, mc_world_storage_status status, const char *operation) {
    e->cleanup = (mc_entity_tick_execution_error){status, operation, e->site};
    return status;
}

static mc_entity_tick_execution_world_token world_token(const mc_entity_tick_execution *e,
                                                        size_t slot) {
    return (mc_entity_tick_execution_world_token){e, slot, e->world_slots[slot].generation};
}

static const mc_entity_tick_execution_world_slot *
world_slot(const mc_entity_tick_execution *e, const mc_entity_tick_execution_world_token *token) {
    if (!valid(e) || token == NULL || token->owner != e ||
        token->slot >= MC_ENTITY_TICK_EXECUTION_WORLDS)
        return NULL;
    const mc_entity_tick_execution_world_slot *s = &e->world_slots[token->slot];
    return s->phase != MC_ENTITY_TICK_EXECUTION_WORLD_FREE && s->generation == token->generation
               ? s
               : NULL;
}

static bool owns_world(const mc_entity_tick_execution *e, size_t slot,
                       const mc_entity_tick_execution_world_token *token) {
    return token->owner == e && token->slot == slot &&
           token->generation == e->world_slots[slot].generation;
}

/* Bounded native ownership validation only. Includes independently held alias
 * occurrences; two slots containing the same actual World still require two
 * real bindings. No mutable source World field or ambient failure is queried. */
static bool ledger_valid(const mc_entity_tick_execution *e) {
    if (!valid(e) || e->world_count > MC_ENTITY_TICK_EXECUTION_WORLDS ||
        e->lease_count > MC_ENTITY_TICK_EXECUTION_RECEIPTS ||
        e->receipt_count > MC_ENTITY_TICK_EXECUTION_RECEIPTS ||
        e->loader_count > MC_ENTITY_TICK_EXECUTION_LOADERS)
        return false;
    size_t worlds = 0, leases = 0, open = 0, copied = 0;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i) {
        const mc_entity_tick_execution_world_slot *s = &e->world_slots[i];
        leases += e->lease_used[i] ? 1u : 0u;
        if (s->phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE) {
            if (s->world != NULL || s->held || s->loader_open || e->lease_used[i])
                return false;
            continue;
        }
        if ((s->phase != MC_ENTITY_TICK_EXECUTION_WORLD_OPEN &&
             s->phase != MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED) ||
            s->generation == 0 || s->held != (s->world != NULL))
            return false;
        ++worlds;
        if (s->phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN) {
            ++open;
            if (!e->lease_used[i])
                return false;
            if (s->parent != SIZE_MAX &&
                (s->parent >= MC_ENTITY_TICK_EXECUTION_WORLDS || s->parent == i ||
                 e->world_slots[s->parent].phase != MC_ENTITY_TICK_EXECUTION_WORLD_OPEN))
                return false;
        } else if (s->parent != SIZE_MAX || s->loader_open)
            return false;
        if (s->loader_open) {
            if (s->loader_slot >= MC_ENTITY_TICK_EXECUTION_LOADERS)
                return false;
            const mc_entity_tick_execution_loader_slot *l = &e->loader_slots[s->loader_slot];
            if (l->phase != MC_ENTITY_TICK_EXECUTION_LOADER_RESERVED ||
                !owns_world(e, i, &l->world) || l->generation != s->selection_generation)
                return false;
        }
        if (s->held) {
            size_t occurrences = 0;
            for (size_t j = 0; j < MC_ENTITY_TICK_EXECUTION_WORLDS; ++j)
                if (e->world_slots[j].phase != MC_ENTITY_TICK_EXECUTION_WORLD_FREE &&
                    e->world_slots[j].held && e->world_slots[j].world == s->world)
                    ++occurrences;
            if (s->world->persistent_bindings < occurrences)
                return false;
        }
    }
    if (worlds != e->world_count || leases != e->lease_count || e->world_open != (open != 0))
        return false;
    size_t depth = 0, index = e->current_lease;
    bool seen[MC_ENTITY_TICK_EXECUTION_WORLDS] = {false};
    while (index != SIZE_MAX) {
        if (index >= MC_ENTITY_TICK_EXECUTION_WORLDS || seen[index] ||
            e->world_slots[index].phase != MC_ENTITY_TICK_EXECUTION_WORLD_OPEN)
            return false;
        seen[index] = true;
        ++depth;
        index = e->world_slots[index].parent;
    }
    if (depth != open)
        return false;
    if (open != 0) {
        const mc_entity_tick_execution_world_slot *s = &e->world_slots[e->current_lease];
        if (e->world != s->world || e->world_held != s->held)
            return false;
    } else if (e->world_held != (e->world != NULL))
        return false;
    else if (e->world != NULL) {
        bool found = false;
        for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
            found |= e->world_slots[i].phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED &&
                     e->world_slots[i].world == e->world && e->world_slots[i].held;
        if (!found)
            return false;
    }
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i) {
        const mc_entity_tick_execution_loader_slot *l = &e->loader_slots[i];
        if (l->phase == MC_ENTITY_TICK_EXECUTION_LOADER_FREE)
            continue;
        const mc_entity_tick_execution_world_slot *s = world_slot(e, &l->world);
        if (s == NULL)
            return false;
        if (l->phase == MC_ENTITY_TICK_EXECUTION_LOADER_RESERVED) {
            if (s->phase != MC_ENTITY_TICK_EXECUTION_WORLD_OPEN || !s->loader_open ||
                s->loader_slot != i || l->generation != s->selection_generation)
                return false;
        } else if (l->phase == MC_ENTITY_TICK_EXECUTION_LOADER_COPIED) {
            if (!s->held || e->loader_originals[i] == NULL)
                return false;
            ++copied;
        } else
            return false;
    }
    return copied == e->loader_count;
}

static void mirror_world(mc_entity_tick_execution *e, size_t fallback) {
    size_t slot = e->current_lease;
    e->world_open = slot != SIZE_MAX;
    if (slot == SIZE_MAX && fallback < MC_ENTITY_TICK_EXECUTION_WORLDS &&
        e->world_slots[fallback].phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED)
        slot = fallback;
    if (slot == SIZE_MAX) {
        e->world = NULL;
        e->world_held = false;
        return;
    }
    e->world = e->world_slots[slot].world;
    e->world_held = e->world_slots[slot].held;
    e->site = e->world_slots[slot].site;
}

static void free_world_slot(mc_entity_tick_execution *e, size_t index) {
    const uint64_t generation = e->world_slots[index].generation;
    e->world_slots[index] = (mc_entity_tick_execution_world_slot){
        .generation = generation, .parent = SIZE_MAX, .loader_slot = SIZE_MAX};
    --e->world_count;
}

static mc_world_storage_status capture_admission(mc_entity_tick_execution *e) {
    if (!valid(e))
        return admission(e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution capture owner identity");
    if (!e->world_open ||
        (e->phase != MC_ENTITY_TICK_EXECUTION_OPEN && e->phase != MC_ENTITY_TICK_EXECUTION_FAILED))
        return admission(e, MC_WORLD_STORAGE_BUSY,
                         "Entity execution capture outside current World call");
    if (!ledger_valid(e))
        return admission(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Entity execution capture World slot ownership");
    e->admission = empty_error();
    return MC_WORLD_STORAGE_OK;
}

void mc_entity_tick_execution_init(mc_entity_tick_execution *e, mc_math_execution *math) {
    if (e == NULL)
        return;
    e->identity = e;
    e->math = math;
    e->phase = MC_ENTITY_TICK_EXECUTION_OPEN;
    e->world = NULL;
    e->world_held = e->world_open = e->child_failed = e->capture_pending = false;
    e->world_count = e->loader_count = e->receipt_count = e->lease_count = 0;
    e->current_lease = SIZE_MAX;
    e->site = (mc_entity_tick_execution_site){NULL, NULL, NULL, -1};
    e->first = e->capture = e->admission = e->cleanup = empty_error();
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i) {
        e->loader_originals[i] = NULL;
        e->loader_slots[i] = (mc_entity_tick_execution_loader_slot){0};
    }
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_RECEIPTS; ++i) {
        e->receipts[i] = (mc_world_access_failure){0};
        e->lease_used[i] = false;
        mc_world_access_failure_lease_init(&e->leases[i]);
        e->world_slots[i] =
            (mc_entity_tick_execution_world_slot){.parent = SIZE_MAX, .loader_slot = SIZE_MAX};
    }
}

mc_world_storage_status
mc_entity_tick_execution_push_world(mc_entity_tick_execution *e, mc_world_runtime *world,
                                    mc_entity_tick_execution_site site,
                                    mc_entity_tick_execution_world_token *out) {
    if (!valid(e) || out == NULL)
        return admission(e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution nested World owner/token");
    if (e->phase != MC_ENTITY_TICK_EXECUTION_OPEN)
        return admission(e, MC_WORLD_STORAGE_BUSY, "Entity execution nested World after failure");
    if (!ledger_valid(e))
        return admission(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Entity execution nested World slot ownership");
    /* A returned failure in the CURRENT scope cannot be bypassed by pushing a
     * fresh node. Inspect without consuming its actual right; an ARMED parent
     * may still represent legitimate entered nested source work. */
    if (e->current_lease != SIZE_MAX) {
        const mc_world_access_failure *held = NULL;
        if (mc_world_access_failure_lease_inspect(&e->leases[e->current_lease], &held))
            return admission(e, MC_WORLD_STORAGE_BUSY,
                             "Entity execution nested World after current callback failure");
        /* False inspect also covers malformed HELD/IDLE nodes. With exclusive
         * access, checked release cannot consume a HELD right after that false
         * result: OK is an IDLE no-op; BUSY identifies an entered ARMED parent. */
        mc_world_storage_error cause = {0};
        const mc_world_storage_status ready =
            mc_world_access_failure_lease_release(&e->leases[e->current_lease], &cause);
        if (ready != MC_WORLD_STORAGE_OK && ready != MC_WORLD_STORAGE_BUSY)
            return admission(e, ready, cause.operation);
        for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
            if (e->loader_slots[i].phase == MC_ENTITY_TICK_EXECUTION_LOADER_COPIED &&
                owns_world(e, e->current_lease, &e->loader_slots[i].world))
                return admission(e, MC_WORLD_STORAGE_BUSY,
                                 "Entity execution nested World after current loader failure");
    }
    e->admission = empty_error();
    e->site = site;
    size_t index = SIZE_MAX;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (e->world_slots[i].phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE) {
            index = i;
            break;
        }
    if (index == SIZE_MAX || e->world_slots[index].generation == UINT64_MAX)
        return capture_failed(e, MC_WORLD_STORAGE_LIMIT,
                              "Entity execution precharged World/generation capacity", false);
    mc_world_storage_error cleanup = {0};
    const mc_world_storage_status ready =
        mc_world_access_failure_lease_release(&e->leases[index], &cleanup);
    if (ready != MC_WORLD_STORAGE_OK)
        return admission(e, ready, cleanup.operation);
    if (world != NULL && !mc_world_runtime_binding_retain(world)) {
        const mc_world_storage_error *cause = mc_world_runtime_last_error(world);
        return capture_failed(e, cause->status, cause->operation, false);
    }
    mc_entity_tick_execution_world_slot *s = &e->world_slots[index];
    const uint64_t generation = s->generation + 1u;
    *s = (mc_entity_tick_execution_world_slot){.phase = MC_ENTITY_TICK_EXECUTION_WORLD_OPEN,
                                               .generation = generation,
                                               .parent = e->current_lease,
                                               .loader_slot = SIZE_MAX,
                                               .world = world,
                                               .site = site,
                                               .held = world != NULL};
    e->lease_used[index] = true;
    ++e->lease_count;
    ++e->world_count;
    e->current_lease = index;
    mirror_world(e, SIZE_MAX);
    *out = world_token(e, index);
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_entity_tick_execution_begin_world(mc_entity_tick_execution *e,
                                                             mc_world_runtime *world,
                                                             mc_entity_tick_execution_site site) {
    if (!valid(e))
        return admission(e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution World owner identity");
    if (e->phase != MC_ENTITY_TICK_EXECUTION_OPEN || e->world_open || e->world_held ||
        e->world != NULL || e->current_lease != SIZE_MAX)
        return admission(e, MC_WORLD_STORAGE_BUSY, "Entity execution World call is not open");
    mc_entity_tick_execution_world_token token;
    return mc_entity_tick_execution_push_world(e, world, site, &token);
}

bool mc_entity_tick_execution_current_world(const mc_entity_tick_execution *e,
                                            mc_entity_tick_execution_world_token *out) {
    if (out == NULL || !ledger_valid(e) || !e->world_open)
        return false;
    *out = world_token(e, e->current_lease);
    return true;
}

bool mc_entity_tick_execution_read_world(const mc_entity_tick_execution *e,
                                         const mc_entity_tick_execution_world_token *token,
                                         mc_world_runtime **out) {
    if (out == NULL || !ledger_valid(e))
        return false;
    const mc_entity_tick_execution_world_slot *s = world_slot(e, token);
    if (s == NULL)
        return false;
    *out = s->world;
    return true;
}

mc_world_access_failure_lease *mc_entity_tick_execution_world_lease(mc_entity_tick_execution *e) {
    if (!valid(e)) {
        (void)admission(e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                        "Entity execution World lease owner identity");
        return NULL;
    }
    if (e->phase != MC_ENTITY_TICK_EXECUTION_OPEN || !e->world_open) {
        (void)admission(e, MC_WORLD_STORAGE_BUSY,
                        "Entity execution World lease outside current call");
        return NULL;
    }
    if (!ledger_valid(e)) {
        (void)admission(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Entity execution World lease slot ownership");
        return NULL;
    }
    e->admission = empty_error();
    return &e->leases[e->current_lease];
}

static size_t free_loader_slot(const mc_entity_tick_execution *e) {
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (e->loader_slots[i].phase == MC_ENTITY_TICK_EXECUTION_LOADER_FREE)
            return i;
    return SIZE_MAX;
}

mc_world_storage_status
mc_entity_tick_execution_loader_begin(mc_entity_tick_execution *e,
                                      const mc_entity_tick_execution_world_token *world,
                                      mc_entity_tick_execution_loader_attempt *out) {
    const mc_world_storage_status admitted = capture_admission(e);
    if (admitted != MC_WORLD_STORAGE_OK)
        return admitted;
    const mc_entity_tick_execution_world_slot *s = world_slot(e, world);
    if (out == NULL || s == NULL || world->slot != e->current_lease)
        return admission(e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution loader attempt current World token");
    if (e->phase != MC_ENTITY_TICK_EXECUTION_OPEN || s->loader_open)
        return admission(e, MC_WORLD_STORAGE_BUSY,
                         "Entity execution loader attempt already entered/failed");
    const size_t index = free_loader_slot(e);
    if (index == SIZE_MAX || s->selection_generation == UINT64_MAX)
        return capture_failed(e, MC_WORLD_STORAGE_LIMIT,
                              "Entity execution precharged loader attempt capacity", false);
    mc_entity_tick_execution_world_slot *current = &e->world_slots[world->slot];
    ++current->selection_generation;
    current->loader_open = true;
    current->selection_ready = false;
    current->loader_slot = index;
    e->loader_slots[index] = (mc_entity_tick_execution_loader_slot){
        MC_ENTITY_TICK_EXECUTION_LOADER_RESERVED, *world, current->selection_generation};
    *out = (mc_entity_tick_execution_loader_attempt){*world, index, current->selection_generation};
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_entity_tick_execution_loader_finish(
    mc_entity_tick_execution *e, const mc_entity_tick_execution_loader_attempt *attempt,
    mc_chunk_acquire_status actual_status, const mc_world_source_chunk_error *actual_failure,
    mc_entity_tick_execution_loader_selection *out) {
    const mc_world_storage_status admitted = capture_admission(e);
    if (admitted != MC_WORLD_STORAGE_OK)
        return admitted;
    if (attempt == NULL || out == NULL || attempt->slot >= MC_ENTITY_TICK_EXECUTION_LOADERS ||
        (unsigned int)actual_status > (unsigned int)MC_CHUNK_ACQUIRE_EMPTY)
        return admission(e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution loader finish attempt/outcome");
    const mc_entity_tick_execution_world_slot *s = world_slot(e, &attempt->world);
    mc_entity_tick_execution_loader_slot *l = &e->loader_slots[attempt->slot];
    if (s == NULL || attempt->world.slot != e->current_lease || !s->loader_open ||
        s->loader_slot != attempt->slot || s->selection_generation != attempt->generation ||
        l->phase != MC_ENTITY_TICK_EXECUTION_LOADER_RESERVED ||
        !owns_world(e, attempt->world.slot, &l->world) || l->generation != attempt->generation)
        return admission(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Entity execution loader finish exact reservation");
    if (actual_status != MC_CHUNK_ACQUIRE_OK && actual_failure != NULL && !s->held)
        return capture_failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution current loader World is not held", true);
    mc_entity_tick_execution_world_slot *current = &e->world_slots[attempt->world.slot];
    if (actual_status != MC_CHUNK_ACQUIRE_OK && actual_failure != NULL) {
        e->loaders[attempt->slot] = *actual_failure;
        e->loader_originals[attempt->slot] = actual_failure;
        l->phase = MC_ENTITY_TICK_EXECUTION_LOADER_COPIED;
        ++e->loader_count;
    } else {
        *l = (mc_entity_tick_execution_loader_slot){0};
        e->loader_originals[attempt->slot] = NULL;
        current->loader_slot = SIZE_MAX;
    }
    current->loader_open = false;
    current->selection_ready = true;
    current->selection_status = actual_status;
    *out = (mc_entity_tick_execution_loader_selection){attempt->world, attempt->generation,
                                                       actual_status};
    if (actual_status != MC_CHUNK_ACQUIRE_OK) {
        e->phase = MC_ENTITY_TICK_EXECUTION_FAILED;
        e->child_failed = true;
    }
    return MC_WORLD_STORAGE_OK;
}

bool mc_entity_tick_execution_loader_read(
    const mc_entity_tick_execution *e, const mc_entity_tick_execution_loader_selection *selection,
    mc_entity_tick_execution_loader_view *out) {
    if (selection == NULL || out == NULL || !ledger_valid(e))
        return false;
    const mc_entity_tick_execution_world_slot *s = world_slot(e, &selection->world);
    if (s == NULL || !s->selection_ready || s->loader_open ||
        s->selection_generation != selection->generation ||
        s->selection_status != selection->status)
        return false;
    const mc_world_source_chunk_error *failure = NULL;
    if (s->loader_slot != SIZE_MAX) {
        if (s->loader_slot >= MC_ENTITY_TICK_EXECUTION_LOADERS)
            return false;
        const mc_entity_tick_execution_loader_slot *l = &e->loader_slots[s->loader_slot];
        if (l->phase != MC_ENTITY_TICK_EXECUTION_LOADER_COPIED ||
            !owns_world(e, selection->world.slot, &l->world) ||
            l->generation != selection->generation)
            return false;
        failure = &e->loaders[s->loader_slot];
    }
    *out = (mc_entity_tick_execution_loader_view){s->selection_status, failure};
    return true;
}

mc_world_storage_status
mc_entity_tick_execution_copy_loader(mc_entity_tick_execution *e,
                                     const mc_world_source_chunk_error **selected_slot) {
    const mc_world_storage_status admitted = capture_admission(e);
    if (admitted != MC_WORLD_STORAGE_OK)
        return admitted;
    if (selected_slot == NULL)
        return capture_failed(e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                              "Entity execution loader selection slot", true);
    const mc_world_source_chunk_error *const borrowed = *selected_slot;
    if (borrowed == NULL)
        return MC_WORLD_STORAGE_OK;
    mc_entity_tick_execution_world_slot *s = &e->world_slots[e->current_lease];
    if (!s->held)
        return capture_failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution loader producer is not held", true);
    s->selection_ready = false; /* This compatibility helper mints no current token. */
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (e->loader_slots[i].phase == MC_ENTITY_TICK_EXECUTION_LOADER_COPIED &&
            (borrowed == &e->loaders[i] || borrowed == e->loader_originals[i])) {
            *selected_slot = &e->loaders[i];
            return MC_WORLD_STORAGE_OK;
        }
    const size_t index = free_loader_slot(e);
    if (index == SIZE_MAX)
        return capture_failed(e, MC_WORLD_STORAGE_LIMIT,
                              "Entity execution precharged loader capacity", true);
    e->loaders[index] = *borrowed;
    e->loader_originals[index] = borrowed;
    e->loader_slots[index] = (mc_entity_tick_execution_loader_slot){
        MC_ENTITY_TICK_EXECUTION_LOADER_COPIED, world_token(e, e->current_lease), 0};
    ++e->loader_count;
    *selected_slot = &e->loaders[index];
    return MC_WORLD_STORAGE_OK;
}

static bool same_failure(const mc_world_access_failure *a, const mc_world_access_failure *b) {
    return a->receiver == b->receiver && a->render_global == b->render_global &&
           a->receipt == b->receipt && a->source_null_receiver == b->source_null_receiver;
}

static const mc_world_access_failure *protected_failure(const mc_entity_tick_execution *e,
                                                        const mc_world_access_failure *borrowed) {
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_RECEIPTS; ++i) {
        const mc_world_access_failure *candidate = NULL;
        if (e->lease_used[i] && mc_world_access_failure_lease_inspect(&e->leases[i], &candidate) &&
            same_failure(candidate, borrowed))
            return candidate;
    }
    return NULL;
}

mc_world_storage_status
mc_entity_tick_execution_capture_receipt(mc_entity_tick_execution *e,
                                         const mc_world_access_failure *borrowed) {
    const mc_world_storage_status admitted = capture_admission(e);
    if (admitted != MC_WORLD_STORAGE_OK)
        return admitted;
    if (borrowed == NULL || (borrowed->receiver == NULL && borrowed->render_global == NULL &&
                             borrowed->receipt == NULL))
        return MC_WORLD_STORAGE_OK;
    if (borrowed->source_null_receiver || borrowed->receipt == NULL)
        return capture_failed(e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                              "Entity execution concrete receipt shape", true);
    const mc_world_access_failure *protected = protected_failure(e, borrowed);
    if (protected == NULL)
        return capture_failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution receipt lacks protected handoff", true);
    for (size_t i = 0; i < e->receipt_count; ++i)
        if (same_failure(&e->receipts[i], protected))
            return MC_WORLD_STORAGE_OK;
    if (e->receipt_count == MC_ENTITY_TICK_EXECUTION_RECEIPTS)
        return capture_failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution selected receipt occurrence invariant", false);
    e->receipts[e->receipt_count++] = *protected;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status
mc_entity_tick_execution_pop_world(mc_entity_tick_execution *e,
                                   const mc_entity_tick_execution_world_token *token,
                                   bool child_failed) {
    if (!valid(e))
        return admission(e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution World end owner identity");
    if (!e->world_open ||
        (e->phase != MC_ENTITY_TICK_EXECUTION_OPEN && e->phase != MC_ENTITY_TICK_EXECUTION_FAILED))
        return admission(e, MC_WORLD_STORAGE_BUSY,
                         "Entity execution World end outside current call");
    if (!ledger_valid(e))
        return cleanup_failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution World end slot ownership");
    const mc_entity_tick_execution_world_slot *read = world_slot(e, token);
    if (read == NULL || token->slot != e->current_lease)
        return admission(e, MC_WORLD_STORAGE_BUSY, "Entity execution World end noncurrent token");
    if (read->loader_open)
        return cleanup_failed(e, MC_WORLD_STORAGE_BUSY,
                              "Entity execution acquisition attempt is still entered");
    e->admission = e->cleanup = empty_error();
    mc_world_access_failure_lease *lease = &e->leases[token->slot];
    const mc_world_access_failure *held = NULL;
    const bool lease_held = mc_world_access_failure_lease_inspect(lease, &held);
    if (!lease_held) {
        mc_world_storage_error cause = {0};
        const mc_world_storage_status status = mc_world_access_failure_lease_release(lease, &cause);
        if (status != MC_WORLD_STORAGE_OK)
            return cleanup_failed(e, status, cause.operation);
    }
    bool copied_loader = false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        copied_loader |= e->loader_slots[i].phase == MC_ENTITY_TICK_EXECUTION_LOADER_COPIED &&
                         owns_world(e, token->slot, &e->loader_slots[i].world);
    const bool failed =
        child_failed || e->phase == MC_ENTITY_TICK_EXECUTION_FAILED || e->capture_pending;
    const bool unexpected = !failed && (lease_held || copied_loader);
    mc_entity_tick_execution_world_slot *s = &e->world_slots[token->slot];
    const size_t parent = s->parent;
    s->phase = MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED;
    s->parent = SIZE_MAX;
    e->current_lease = parent;
    if (!lease_held) {
        e->lease_used[token->slot] = false;
        --e->lease_count; /* Count this EXACT slot, never truncate an array prefix. */
    }
    if (failed || unexpected) {
        e->phase = MC_ENTITY_TICK_EXECUTION_FAILED;
        e->child_failed = true;
    } else {
        if (s->held)
            mc_world_runtime_binding_release(s->world);
        free_world_slot(e, token->slot);
    }
    mirror_world(e, token->slot);
    if (unexpected)
        return cleanup_failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution returned child retained a failure owner");
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_entity_tick_execution_end_world(mc_entity_tick_execution *e,
                                                           bool child_failed) {
    if (!valid(e))
        return admission(e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution World end owner identity");
    if (!e->world_open ||
        (e->phase != MC_ENTITY_TICK_EXECUTION_OPEN && e->phase != MC_ENTITY_TICK_EXECUTION_FAILED))
        return admission(e, MC_WORLD_STORAGE_BUSY,
                         "Entity execution World end outside current call");
    if (e->current_lease >= MC_ENTITY_TICK_EXECUTION_WORLDS)
        return cleanup_failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution current lease ownership extent");
    const mc_entity_tick_execution_world_token token = world_token(e, e->current_lease);
    return mc_entity_tick_execution_pop_world(e, &token, child_failed);
}

mc_world_storage_status mc_entity_tick_execution_release_receipts(mc_entity_tick_execution *e) {
    if (e == NULL)
        return MC_WORLD_STORAGE_OK;
    if (!valid(e))
        return admission(e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution receipt cleanup owner identity");
    if (e->phase == MC_ENTITY_TICK_EXECUTION_RETIRED)
        return MC_WORLD_STORAGE_OK;
    e->cleanup = empty_error();
    if (e->world_open)
        return cleanup_failed(e, MC_WORLD_STORAGE_BUSY,
                              "Entity execution World call is still entered");
    if (e->capture_pending)
        return cleanup_failed(e, MC_WORLD_STORAGE_BUSY,
                              "Entity execution unprotected capture integration is open");
    if (!ledger_valid(e))
        return cleanup_failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution receipt ownership slot ledger");
    e->phase = MC_ENTITY_TICK_EXECUTION_RETIRING;
    for (size_t i = 0; i < e->receipt_count; ++i)
        if (protected_failure(e, &e->receipts[i]) == NULL)
            return cleanup_failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Entity execution selected receipt lease ownership");
    while (e->receipt_count != 0)
        e->receipts[--e->receipt_count] = (mc_world_access_failure){0};
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_RECEIPTS; ++i) {
        if (!e->lease_used[i])
            continue;
        mc_world_storage_error cause = {0};
        const mc_world_storage_status status =
            mc_world_access_failure_lease_release(&e->leases[i], &cause);
        if (status != MC_WORLD_STORAGE_OK)
            return cleanup_failed(e, status, cause.operation);
        e->lease_used[i] = false;
        --e->lease_count;
    }
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_entity_tick_execution_retire(mc_entity_tick_execution *e) {
    const mc_world_storage_status status = mc_entity_tick_execution_release_receipts(e);
    if (status != MC_WORLD_STORAGE_OK || e == NULL || e->phase == MC_ENTITY_TICK_EXECUTION_RETIRED)
        return status;
    /* Receipt cleanup may have consumed a prefix of sparse used nodes on an
     * earlier try. Only the exact still-used nodes/Worlds are inspected here. */
    if (!ledger_valid(e))
        return cleanup_failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution terminal World binding ownership");
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i) {
        e->loader_originals[i] = NULL;
        e->loader_slots[i] = (mc_entity_tick_execution_loader_slot){0};
    }
    e->loader_count = 0;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i) {
        mc_entity_tick_execution_world_slot *s = &e->world_slots[i];
        if (s->phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE)
            continue;
        s->selection_ready = false; /* Invalidate exposed loader tokens FIRST. */
        if (s->held)
            mc_world_runtime_binding_release(s->world);
        free_world_slot(e, i);
    }
    e->current_lease = SIZE_MAX;
    mirror_world(e, SIZE_MAX);
    e->math = NULL;
    e->phase = MC_ENTITY_TICK_EXECUTION_RETIRED;
    return MC_WORLD_STORAGE_OK;
}
