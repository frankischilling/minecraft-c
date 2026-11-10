/* Reuse real SourceSaving owners and the scalar fixture's checked teardown.
 * No imported main or fake loader/receipt producer is executed. */
int ne_unused_scalar_main(int, char **);
#define MC_WORLD_BLOCK_SCALAR_MAIN ne_unused_scalar_main
#include "test_block_scalar_execution.c"
#undef MC_WORLD_BLOCK_SCALAR_MAIN

#include "world/runtime/mutation/mutation_internal.h"
#include "world/runtime/mutation/notifications_execution_internal.h"

struct ne_fixture;

typedef struct ne_context {
    struct ne_fixture *fixture;
    size_t id;
} ne_context;

typedef struct ne_fixture {
    bs_fixture bs;
    mc_world_notification_execution frame, probe;
    mc_world_runtime_execution_result owned, probe_owned;
    mc_world_mutation_error error, probe_error;
    mc_world_storage_status raw, probe_raw, retire_observed;
    mc_world_access *observers[4];
    ne_context contexts[4];
    mc_entity_tick_execution_world_token descendant;
    size_t entries, callbacks, cell_callbacks, range_callbacks, probes, order[8], saved_bindings;
    bool callback_valid, range, equal, mutate, probe_nodes;
    bool capture_refusal, native_success, projected, drift;
} ne_fixture;

static bool ne_call(ne_fixture *s, mc_world_notification_execution *frame, mc_world_runtime *world,
                    bool range, mc_world_mutation_error *error, mc_world_storage_status *raw) {
    ++s->entries;
    return range
               ? mc_world_mutation_observe_range_in_execution(
                     frame, world, 17, -5, 91, s->equal ? 17 : -12, s->equal ? -5 : 19,
                     s->equal ? 91 : -4, error, raw)
               : mc_world_mutation_observe_cell_in_execution(frame, world, 17, -5, 91, error, raw);
}

static void ne_prepare(ne_fixture *s) {
    mc_world_notification_execution_init(&s->frame, &s->bs.cq.execution, bs_site, &s->owned);
    mc_world_mutation_error_init(&s->error);
    s->raw = MC_WORLD_STORAGE_LIMIT;
}

static int ne_begin(ne_fixture *s) {
    VERIFY(bs_begin(&s->bs) == 0);
    ne_prepare(s);
    return 0;
}

static int ne_end(ne_fixture *s, bool failed) {
    /* Every typed protected view and frame borrow expires before the actual
     * execution retires its independently owned producer occurrence. */
    s->error.world_access_failure = s->probe_error.world_access_failure =
        (mc_world_access_failure){0};
    s->frame = s->probe = (mc_world_notification_execution){0};
    s->owned = s->probe_owned = (mc_world_runtime_execution_result){0};
    VERIFY(bs_end(&s->bs, failed) == 0);
    return 0;
}

static bool ne_marker_same(const mc_world_mutation_error *error) {
    return error->status == MC_WORLD_STORAGE_IO_ERROR &&
           error->operation == bs_error_marker.operation &&
           error->cause.status == bs_error_marker.status &&
           error->cause.operation == bs_error_marker.operation &&
           error->cause.system_error == bs_error_marker.system_error &&
           error->world_access_bytecode == 211 && error->wrapper_bytecode == 212 &&
           error->world_access_failure.receiver == NULL &&
           error->world_access_failure.receipt == NULL;
}

static void ne_marker(mc_world_mutation_error *error) {
    mc_world_mutation_error_init(error);
    error->status = bs_error_marker.status;
    error->operation = bs_error_marker.operation;
    error->cause = bs_error_marker;
    error->world_access_bytecode = 211;
    error->wrapper_bytecode = 212;
}

static void ne_probe_node(ne_fixture *s) {
    cq_scope before = {0};
    const bool snapped = cq_scope_take(&s->bs.cq, &before);
    for (unsigned int range = 0; range < 2; ++range) {
        mc_world_notification_execution_init(&s->probe, &s->bs.cq.execution, bs_site,
                                             &s->probe_owned);
        ne_marker(&s->probe_error);
        s->probe_raw = MC_WORLD_STORAGE_LIMIT;
        ++s->probes;
        const bool returned = ne_call(s, &s->probe, s->bs.cq.source.game.worlds[0], range != 0,
                                      &s->probe_error, &s->probe_raw);
        s->callback_valid &= !returned && !s->probe.entered && !s->probe.completed &&
                             s->probe.callbacks_returned == 0 &&
                             s->probe_owned.ownership_status == MC_WORLD_STORAGE_BUSY &&
                             !s->probe_owned.selection_ready && !cq_pending(&s->probe_owned) &&
                             ne_marker_same(&s->probe_error) &&
                             s->probe_raw == MC_WORLD_STORAGE_LIMIT && snapped &&
                             cq_scope_same(&s->bs.cq, &before);
    }
}

static mc_world_storage_status ne_failure_body(void *context, mc_world_runtime *world, int32_t x,
                                               int32_t y, int32_t z, mc_world_storage_error *cause,
                                               mc_world_access_failure *failure) {
    ne_fixture *s = context;
    ++s->callbacks;
    mc_world_access *receiver = s->bs.cq.receiver;
    s->callback_valid &= world == s->bs.cq.source.game.worlds[0] && x == 17 && y == -5 && z == 91 &&
                         s->callbacks == 1 && receiver->active_holds == 1 && s->frame.entered &&
                         !s->frame.completed && s->frame.callbacks_returned == 0 &&
                         s->frame.lease == &s->bs.cq.execution.leases[s->bs.outer.slot] &&
                         s->frame.lease->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                         s->frame.lease->receipt == NULL;
    if (!s->callback_valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Notification bounded callback admission", 0};
        return cause->status;
    }
    if (s->probe_nodes)
        ne_probe_node(s);
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, receiver, &removed, cause);
    if (status != MC_WORLD_STORAGE_OK || !removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    s->bs.cq.removed = true;
    status = mc_world_access_receipt_create(receiver, NULL, &s->bs.cq.receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    s->bs.cq.receipt->exception_class = "java/lang/IllegalStateException";
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Notification actual Source callback failure", 57};
    if (s->capture_refusal) {
        s->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Explicit native integrity control only. */
        s->projected = true;
    }
    return cause->status;
}

static mc_world_storage_status ne_failure(void *context, mc_world_runtime *world, int32_t x,
                                          int32_t y, int32_t z, mc_world_storage_error *cause,
                                          mc_world_access_failure *failure) {
    ne_fixture *s = context;
    ++s->cell_callbacks;
    return ne_failure_body(context, world, x, y, z, cause, failure);
}

static mc_world_storage_status ne_failure_range(void *context, mc_world_runtime *world, int32_t x0,
                                                int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                                                int32_t z1, mc_world_storage_error *cause,
                                                mc_world_access_failure *failure) {
    ne_fixture *s = context;
    ++s->range_callbacks;
    s->callback_valid &= x1 == -12 && y1 == 19 && z1 == -4;
    return ne_failure_body(context, world, x0, y0, z0, cause, failure);
}

static int ne_reenter(ne_fixture *s);

static int ne_lease(ne_fixture *s) {
    fixture *f = &s->bs.cq.source.game;
    const mc_world_access_source_callbacks callbacks = {.cell = ne_failure,
                                                        .range = ne_failure_range};
    s->callback_valid = true;
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->bs.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->bs.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           ne_begin(s) == 0);
    const bool returned = ne_call(s, &s->frame, f->worlds[0], s->range, &s->error, &s->raw);
    if (s->projected) {
        f->worlds[0]->persistent_bindings = s->saved_bindings;
        s->projected = false;
    }
    VERIFY(!returned && s->callback_valid && s->callbacks == 1 && s->bs.cq.removed &&
           s->raw == MC_WORLD_STORAGE_IO_ERROR && s->error.status == MC_WORLD_STORAGE_IO_ERROR &&
           s->error.cause.system_error == 57 &&
           s->error.world_access_bytecode == (s->range ? 40u : 34u) &&
           s->error.world_access_failure.receipt == s->bs.cq.receipt && s->frame.entered &&
           !s->frame.completed && s->frame.callbacks_returned == 1 &&
           s->frame.callback_status == MC_WORLD_STORAGE_IO_ERROR &&
           cq_token_same(s->frame.world, s->bs.outer) &&
           s->frame.lease == &s->bs.cq.execution.leases[s->bs.outer.slot] &&
           s->bs.cq.receiver->memberships == 0 && s->bs.cq.receiver->active_holds == 1 &&
           s->bs.cq.receipt->holds == 1 && s->bs.cq.receipt->leases == s->frame.lease);
    const mc_world_access_failure *view = NULL;
    VERIFY(mc_world_access_failure_lease_inspect(s->frame.lease, &view) &&
           view->receipt == s->bs.cq.receipt && view->receiver == s->bs.cq.receiver &&
           s->frame.lease->callback_status == MC_WORLD_STORAGE_IO_ERROR &&
           s->frame.lease->callback_error.system_error == 57);
    if (s->capture_refusal) {
        VERIFY(s->owned.ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
               s->bs.cq.execution.receipt_count == 0 &&
               mc_entity_tick_execution_capture_receipt(&s->bs.cq.execution, view) ==
                   MC_WORLD_STORAGE_OK &&
               s->bs.cq.execution.receipt_count == 1 && s->callbacks == 1 &&
               s->raw == MC_WORLD_STORAGE_IO_ERROR && s->error.cause.system_error == 57 &&
               s->owned.ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    } else
        VERIFY(s->owned.ownership_status == MC_WORLD_STORAGE_OK &&
               s->bs.cq.execution.receipt_count == 1);
    VERIFY(s->bs.cq.execution.receipts[0].receipt == s->bs.cq.receipt);
    VERIFY(ne_reenter(s) == 0 && s->raw == MC_WORLD_STORAGE_IO_ERROR &&
           s->error.status == MC_WORLD_STORAGE_IO_ERROR && s->error.cause.system_error == 57 &&
           s->callbacks == 1);
    if (s->probe_nodes)
        ne_probe_node(s);
    /* Clear all actual borrowed views before attempting retirement. The exact
     * HELD lease, independent of receipt.holds, must still refuse that attempt. */
    s->error.world_access_failure = (mc_world_access_failure){0};
    view = NULL;
    s->retire_observed = mc_world_access_receipt_retire(&s->bs.cq.receipt, NULL);
    VERIFY(s->retire_observed == MC_WORLD_STORAGE_BUSY && s->callback_valid &&
           mc_world_access_free(s->bs.cq.receiver, NULL) == MC_WORLD_STORAGE_BUSY &&
           ne_end(s, true) == 0 &&
           mc_world_access_receipt_retire(&s->bs.cq.receipt, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_access_free(s->bs.cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->bs.cq.receiver = NULL;
    return 0;
}

static mc_world_storage_status ne_observe_body(void *context, mc_world_runtime *world, int32_t x,
                                               int32_t y, int32_t z, mc_world_storage_error *cause,
                                               mc_world_access_failure *failure) {
    (void)failure;
    ne_context *input = context;
    ne_fixture *s = input->fixture;
    const size_t index = s->callbacks++;
    if (index < COUNT(s->order))
        s->order[index] = input->id;
    s->callback_valid &= index < COUNT(s->order) && world == s->bs.cq.source.game.worlds[0] &&
                         x == 17 && y == -5 && z == 91 && s->frame.entered &&
                         s->frame.lease->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                         s->observers[input->id]->active_holds == 1;
    if (!s->callback_valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Notification bounded observer admission", 0};
        return cause->status;
    }
    if (input->id != 0)
        return MC_WORLD_STORAGE_OK;
    if (s->mutate) {
        bool removed = false;
        mc_world_storage_status status =
            mc_world_runtime_remove_world_access(world, s->observers[0], &removed, cause);
        if (status != MC_WORLD_STORAGE_OK || !removed)
            return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
        return mc_world_runtime_add_world_access(world, s->observers[3], cause);
    }
    if (s->native_success) {
        s->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Downward native fault; no Source field changes. */
        s->projected = true;
    }
    if (s->drift)
        return mc_entity_tick_execution_push_world(&s->bs.cq.execution, world, bs_site,
                                                   &s->descendant);
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ne_observe(void *context, mc_world_runtime *world, int32_t x,
                                          int32_t y, int32_t z, mc_world_storage_error *cause,
                                          mc_world_access_failure *failure) {
    ne_context *input = context;
    ++input->fixture->cell_callbacks;
    return ne_observe_body(context, world, x, y, z, cause, failure);
}

static mc_world_storage_status ne_observe_range(void *context, mc_world_runtime *world, int32_t x0,
                                                int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                                                int32_t z1, mc_world_storage_error *cause,
                                                mc_world_access_failure *failure) {
    ne_context *input = context;
    ne_fixture *s = input->fixture;
    ++s->range_callbacks;
    s->callback_valid &=
        x1 == (s->equal ? 17 : -12) && y1 == (s->equal ? -5 : 19) && z1 == (s->equal ? 91 : -4);
    return ne_observe_body(context, world, x0, y0, z0, cause, failure);
}

static int ne_observers(ne_fixture *s, size_t count) {
    const mc_world_access_source_callbacks callbacks = {.cell = ne_observe,
                                                        .range = ne_observe_range};
    for (size_t i = 0; i < COUNT(s->observers); ++i) {
        s->contexts[i] = (ne_context){s, i};
        VERIFY(mc_world_access_source_create(&callbacks, &s->contexts[i], &s->observers[i], NULL) ==
               MC_WORLD_STORAGE_OK);
        if (i < count)
            VERIFY(mc_world_runtime_add_world_access(s->bs.cq.source.game.worlds[0],
                                                     s->observers[i], NULL) == MC_WORLD_STORAGE_OK);
    }
    s->bs.cq.receiver = s->observers[0];
    return 0;
}

static int ne_observers_free(ne_fixture *s) {
    for (size_t i = 0; i < COUNT(s->observers); ++i) {
        if (s->observers[i]->memberships != 0) {
            bool removed = false;
            VERIFY(mc_world_runtime_remove_world_access(s->bs.cq.source.game.worlds[0],
                                                        s->observers[i], &removed,
                                                        NULL) == MC_WORLD_STORAGE_OK &&
                   removed);
        }
        VERIFY(s->observers[i]->active_holds == 0 &&
               mc_world_access_free(s->observers[i], NULL) == MC_WORLD_STORAGE_OK);
        s->observers[i] = NULL;
    }
    s->bs.cq.receiver = NULL;
    return 0;
}

static int ne_live(ne_fixture *s, bool empty) {
    fixture *f = &s->bs.cq.source.game;
    s->callback_valid = true;
    if (!empty)
        VERIFY(ne_observers(s, 3) == 0);
    VERIFY(ne_begin(s) == 0);
    const size_t bindings = f->worlds[0]->persistent_bindings;
    VERIFY(ne_call(s, &s->frame, f->worlds[0], s->range, &s->error, &s->raw) &&
           s->raw == MC_WORLD_STORAGE_OK && s->error.status == MC_WORLD_STORAGE_OK &&
           s->error.world_access_bytecode == 0 && s->frame.entered && s->frame.completed &&
           s->frame.callback_status == MC_WORLD_STORAGE_OK &&
           s->frame.callbacks_returned == (empty ? 0u : 3u) && s->callbacks == (empty ? 0u : 3u) &&
           s->callback_valid && s->owned.ownership_status == MC_WORLD_STORAGE_OK &&
           !s->owned.selection_ready && !cq_pending(&s->owned) &&
           s->bs.cq.execution.receipt_count == 0 && s->bs.cq.execution.world_count == 1 &&
           s->bs.cq.execution.current_lease == s->bs.outer.slot &&
           cq_token_same(s->frame.world, s->bs.outer) &&
           s->frame.lease == &s->bs.cq.execution.leases[s->bs.outer.slot] &&
           s->frame.lease->phase == MC_WORLD_ACCESS_FAILURE_LEASE_IDLE &&
           f->worlds[0]->persistent_bindings == bindings && f->worlds[0]->active_calls == 0);
    if (!empty)
        VERIFY(s->order[0] == 0 && s->order[1] == 2 && s->order[2] == 3 &&
               s->observers[0]->memberships == 0 && s->observers[1]->memberships == 1 &&
               s->observers[2]->memberships == 1 && s->observers[3]->memberships == 1 &&
               ne_observers_free(s) == 0);
    VERIFY(ne_end(s, false) == 0);
    return 0;
}

static int ne_reenter(ne_fixture *s) {
    const size_t callbacks = s->callbacks;
    const mc_entity_tick_execution_error first = s->owned.ownership_error;
    const size_t returned = s->frame.callbacks_returned;
    ne_marker(&s->probe_error);
    s->probe_raw = MC_WORLD_STORAGE_LIMIT;
    VERIFY(!ne_call(s, &s->frame, s->bs.cq.source.game.worlds[0], s->range, &s->probe_error,
                    &s->probe_raw) &&
           ne_marker_same(&s->probe_error) && s->probe_raw == MC_WORLD_STORAGE_LIMIT &&
           s->callbacks == callbacks && s->frame.callbacks_returned == returned &&
           s->frame.entered && !s->frame.completed &&
           (first.status == MC_WORLD_STORAGE_OK
                ? s->owned.ownership_status == MC_WORLD_STORAGE_INVALID_ARGUMENT
                : cq_owner_error_same(first, s->owned.ownership_error)));
    return 0;
}

static int ne_native_stop(ne_fixture *s) {
    fixture *f = &s->bs.cq.source.game;
    s->callback_valid = true;
    VERIFY(ne_observers(s, 2) == 0 && ne_begin(s) == 0);
    const bool returned = ne_call(s, &s->frame, f->worlds[0], s->range, &s->error, &s->raw);
    if (s->projected) {
        f->worlds[0]->persistent_bindings = s->saved_bindings;
        s->projected = false;
    }
    VERIFY(!returned && s->callback_valid && s->callbacks == 1 && s->order[0] == 0 &&
           s->raw == MC_WORLD_STORAGE_OK && s->error.status == MC_WORLD_STORAGE_OK &&
           s->error.world_access_bytecode == (s->range ? 40u : 34u) && s->frame.entered &&
           !s->frame.completed && s->frame.callbacks_returned == 1 &&
           s->frame.callback_status == MC_WORLD_STORAGE_OK &&
           s->owned.ownership_status != MC_WORLD_STORAGE_OK && !s->owned.selection_ready &&
           !cq_pending(&s->owned) && s->error.world_access_failure.receipt == NULL &&
           s->bs.cq.execution.receipt_count == 0 &&
           s->frame.lease == &s->bs.cq.execution.leases[s->bs.outer.slot] &&
           s->frame.lease->phase == MC_WORLD_ACCESS_FAILURE_LEASE_IDLE &&
           cq_token_same(s->frame.world, s->bs.outer) && ne_reenter(s) == 0);
    if (s->drift) {
        VERIFY(s->bs.cq.execution.current_lease == s->descendant.slot &&
               s->descendant.slot != s->bs.outer.slot &&
               s->bs.cq.execution.world_slots[s->descendant.slot].parent == s->bs.outer.slot &&
               mc_entity_tick_execution_pop_world(&s->bs.cq.execution, &s->descendant, false) ==
                   MC_WORLD_STORAGE_OK);
    }
    VERIFY(s->callbacks == 1 && ne_observers_free(s) == 0 && ne_end(s, true) == 0);
    return 0;
}

static int ne_admission(ne_fixture *s) {
    fixture *f = &s->bs.cq.source.game;
    for (unsigned int mode = 0; mode < 4; ++mode) {
        mc_entity_tick_execution_init(&s->bs.cq.execution, NULL);
        if (mode >= 2)
            VERIFY(mc_entity_tick_execution_push_world(&s->bs.cq.execution,
                                                       mode == 3 ? NULL : f->worlds[0], bs_site,
                                                       &s->bs.outer) == MC_WORLD_STORAGE_OK);
        for (unsigned int range = 0; range < 2; ++range) {
            ne_prepare(s);
            if (mode == 0)
                s->frame.execution = NULL;
            ne_marker(&s->error);
            VERIFY(!ne_call(s, &s->frame, mode >= 2 ? NULL : f->worlds[0], range != 0, &s->error,
                            &s->raw) &&
                   ne_marker_same(&s->error) && s->raw == MC_WORLD_STORAGE_LIMIT &&
                   !s->frame.entered && !s->frame.completed && s->frame.callbacks_returned == 0 &&
                   s->owned.ownership_status != MC_WORLD_STORAGE_OK && !s->owned.selection_ready &&
                   !cq_pending(&s->owned) && s->callbacks == 0);
        }
        if (mode >= 2)
            VERIFY(mc_entity_tick_execution_pop_world(&s->bs.cq.execution, &s->bs.outer, false) ==
                   MC_WORLD_STORAGE_OK);
        s->frame = (mc_world_notification_execution){0};
        s->owned = (mc_world_runtime_execution_result){0};
        VERIFY(mc_entity_tick_execution_retire(&s->bs.cq.execution) == MC_WORLD_STORAGE_OK);
    }
    return 0;
}

static bool ne_mode(const char *mode) {
    return strcmp(mode, "lease-cell") == 0 || strcmp(mode, "lease-range") == 0 ||
           strcmp(mode, "live-cell") == 0 || strcmp(mode, "live-range") == 0 ||
           strcmp(mode, "equal-range") == 0 || strcmp(mode, "empty-cell") == 0 ||
           strcmp(mode, "empty-range") == 0 || strcmp(mode, "admission") == 0 ||
           strcmp(mode, "nodes") == 0 || strcmp(mode, "capture-refusal") == 0 ||
           strcmp(mode, "native-stop-cell") == 0 || strcmp(mode, "native-stop-range") == 0 ||
           strcmp(mode, "scope-drift") == 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && ne_mode(argv[1]));
    printf("NOTIFICATION_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    ne_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL && ss_setup(&s->bs.cq.source, argv[2]) == 0 &&
           cq_files(&s->bs.cq, true) == 0 && ss_open(&s->bs.cq.source) == 0);
    printf("NOTIFICATION_CASE %s BEGIN\n", argv[1]);
    fflush(stdout);
    s->range = strstr(argv[1], "range") != NULL;
    s->equal = strcmp(argv[1], "equal-range") == 0;
    s->mutate = strncmp(argv[1], "live-", 5) == 0 || s->equal;
    s->native_success = strncmp(argv[1], "native-stop-", 12) == 0;
    s->drift = strcmp(argv[1], "scope-drift") == 0;
    s->probe_nodes = strcmp(argv[1], "nodes") == 0;
    s->capture_refusal = strcmp(argv[1], "capture-refusal") == 0;
    const int status = strcmp(argv[1], "admission") == 0    ? ne_admission(s)
                       : s->mutate                          ? ne_live(s, false)
                       : strncmp(argv[1], "empty-", 6) == 0 ? ne_live(s, true)
                       : s->native_success || s->drift      ? ne_native_stop(s)
                                                            : ne_lease(s);
    VERIFY(status == 0 && s->cell_callbacks == (s->range ? 0u : s->callbacks) &&
           s->range_callbacks == (s->range ? s->callbacks : 0u));
    printf("NOTIFICATION_CASE %s END entries=%zu callbacks=%zu probes=%zu\n", argv[1], s->entries,
           s->callbacks, s->probes);
    fflush(stdout);
    VERIFY(cq_empty_stop(&s->bs.cq.source) == 0 && cq_files(&s->bs.cq, false) == 0 &&
           ss_finish(&s->bs.cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->bs.cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->bs.cq.inputs[i]);
    free(s);
    puts("NOTIFICATION_OPEN neighbors/hooks/full-notify/original-UI");
    printf("PASS: %u actual owned notification checks;mode=%s\n", checks, argv[1]);
    return 0;
}
