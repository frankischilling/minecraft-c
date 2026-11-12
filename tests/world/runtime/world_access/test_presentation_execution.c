/* Reuse actual SourceSaving owners; the imported program is never run. */
int pe_unused_scalar_main(int, char **);
#define MC_WORLD_BLOCK_SCALAR_MAIN pe_unused_scalar_main
#include "test_block_scalar_execution.c"
#undef MC_WORLD_BLOCK_SCALAR_MAIN

#include "block/registry/runtime_internal.h"
#include "mc/entity/arrow/construction.h"
#include "world/runtime/execution_children_internal.h"
#include "world/runtime/mutation/mutation_internal.h"
#include "world/runtime/presentation_execution_internal.h"

typedef struct pe_fixture pe_fixture;

typedef struct pe_observer {
    pe_fixture *fixture;
    size_t id;
} pe_observer;

struct pe_fixture {
    bs_fixture bs;
    mc_world_presentation_execution frame, probe;
    mc_world_runtime_execution_result owned, probe_owned;
    mc_world_mutation_error error, probe_error;
    mc_world_movement_error entity_error;
    mc_entity *actor;
    mc_world_storage_status raw, probe_raw;
    mc_world_access *observers[4];
    pe_observer contexts[4];
    mc_entity_tick_execution_world_token descendant;
    const mc_registry_string *name;
    const char *mode;
    size_t callbacks, entries, probes, order[8], saved_bindings;
    bool particle, valid, drift, native_stop, mutate, failure, capture_refusal, nodes;
    bool projected, held, legacy, bare, null_error, entity_sound;
};

static const uint64_t pe_arguments[] = {UINT64_C(0x8000000000000000), UINT64_C(0x7ff8000000001234),
                                        UINT64_C(0xfff0000000000000), UINT64_C(0x3fd0000000000000),
                                        UINT64_C(0x8000000000000000), UINT64_C(0x7ff8000000005678)};

static double pe_double(uint64_t word) {
    double value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static float pe_float(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static uint64_t pe_double_bits(double value) {
    uint64_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static uint32_t pe_float_bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool pe_call(pe_fixture *s, mc_world_presentation_execution *frame, mc_world_runtime *world,
                    mc_world_mutation_error *error, mc_world_storage_status *raw) {
    ++s->entries;
    const double x = pe_double(pe_arguments[0]), y = pe_double(pe_arguments[1]);
    const double z = pe_double(pe_arguments[2]), vx = pe_double(pe_arguments[3]);
    const double vy = pe_double(pe_arguments[4]), vz = pe_double(pe_arguments[5]);
    if (s->legacy || s->bare) {
        *raw = s->bare ? mc_world_runtime_particle_in_execution(&s->bs.cq.execution, world, s->name,
                                                                x, y, z, vx, vy, vz,
                                                                s->null_error ? NULL : error)
                       : mc_world_runtime_particle_detailed_in_execution(
                             &s->bs.cq.execution, world, s->name, x, y, z, vx, vy, vz, bs_site,
                             s->null_error ? NULL : error, &s->owned);
        return *raw == MC_WORLD_STORAGE_OK &&
               (s->bare || s->owned.ownership_status == MC_WORLD_STORAGE_OK);
    }
    if (s->particle)
        return s->held ? mc_world_mutation_particle_in_execution(frame, world, s->name, x, y, z, vx,
                                                                 vy, vz, error, raw)
                       : mc_world_runtime_particle_frame_in_execution(frame, world, s->name, x, y,
                                                                      z, vx, vy, vz, error, raw);
    return s->held
               ? mc_world_mutation_sound_in_execution(frame, world, x, y, z, s->name,
                                                      pe_float(UINT32_C(0x80000000)),
                                                      pe_float(UINT32_C(0x7fc01234)), error, raw)
               : mc_world_runtime_sound_in_execution(frame, world, x, y, z, s->name,
                                                     pe_float(UINT32_C(0x80000000)),
                                                     pe_float(UINT32_C(0x7fc01234)), error, raw);
}

static void pe_prepare(pe_fixture *s) {
    mc_world_presentation_execution_init(&s->frame, &s->bs.cq.execution, bs_site, &s->owned);
    mc_world_mutation_error_init(&s->error);
    s->raw = MC_WORLD_STORAGE_LIMIT;
}

static int pe_begin(pe_fixture *s) {
    VERIFY(bs_begin(&s->bs) == 0);
    pe_prepare(s);
    return 0;
}

static int pe_end(pe_fixture *s, bool failed) {
    /* Expire typed borrows before the independently owned producer drains. */
    s->error.world_access_failure = s->probe_error.world_access_failure =
        (mc_world_access_failure){0};
    s->frame = s->probe = (mc_world_presentation_execution){0};
    s->owned = s->probe_owned = (mc_world_runtime_execution_result){0};
    VERIFY(bs_end(&s->bs, failed) == 0);
    return 0;
}

static void pe_marker(mc_world_mutation_error *error) {
    mc_world_mutation_error_init(error);
    error->status = bs_error_marker.status;
    error->operation = bs_error_marker.operation;
    error->cause = bs_error_marker;
    error->world_access_bytecode = 211;
    error->wrapper_bytecode = 212;
}

static bool pe_marker_same(const mc_world_mutation_error *error) {
    return error->status == bs_error_marker.status &&
           error->operation == bs_error_marker.operation &&
           bs_error_same(error->cause, bs_error_marker) && error->world_access_bytecode == 211 &&
           error->wrapper_bytecode == 212 && error->world_access_failure.receiver == NULL &&
           error->world_access_failure.receipt == NULL;
}

static void pe_probe_node(pe_fixture *s) {
    cq_scope before = {0};
    const bool snapped = cq_scope_take(&s->bs.cq, &before);
    mc_world_presentation_execution_init(&s->probe, &s->bs.cq.execution, bs_site, &s->probe_owned);
    pe_marker(&s->probe_error);
    s->probe_raw = MC_WORLD_STORAGE_LIMIT;
    ++s->probes;
    const bool returned =
        pe_call(s, &s->probe, s->bs.cq.source.game.worlds[0], &s->probe_error, &s->probe_raw);
    s->valid &= !returned && !s->probe.entered && !s->probe.completed &&
                s->probe.callbacks_returned == 0 &&
                s->probe_owned.ownership_status == MC_WORLD_STORAGE_BUSY &&
                !s->probe_owned.selection_ready && !cq_pending(&s->probe_owned) &&
                pe_marker_same(&s->probe_error) && s->probe_raw == MC_WORLD_STORAGE_LIMIT &&
                snapped && cq_scope_same(&s->bs.cq, &before);
}

static mc_world_storage_status pe_observe(pe_observer *input, mc_world_runtime *world,
                                          const mc_registry_string *name, double x, double y,
                                          double z, mc_world_storage_error *cause,
                                          mc_world_access_failure *failure) {
    pe_fixture *s = input->fixture;
    const size_t index = s->callbacks++;
    if (index < COUNT(s->order))
        s->order[index] = input->id;
    s->valid &= world == s->bs.cq.source.game.worlds[0] && name == s->name &&
                index < COUNT(s->order) && s->observers[input->id]->active_holds == 1 &&
                pe_double_bits(x) == pe_arguments[0] && pe_double_bits(y) == pe_arguments[1] &&
                pe_double_bits(z) == pe_arguments[2] &&
                (s->legacy || s->bare ||
                 (s->frame.entered && !s->frame.completed && s->frame.callbacks_returned == index &&
                  s->frame.lease == &s->bs.cq.execution.leases[s->bs.outer.slot] &&
                  s->frame.lease->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED));
    if (!s->valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Presentation bounded callback admission", 0};
        return cause->status;
    }
    if (input->id != 0)
        return MC_WORLD_STORAGE_OK;
    if (s->nodes)
        pe_probe_node(s);
    if (s->failure || s->mutate) {
        bool removed = false;
        mc_world_storage_status status =
            mc_world_runtime_remove_world_access(world, s->observers[0], &removed, cause);
        if (status != MC_WORLD_STORAGE_OK || !removed)
            return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
        if (s->mutate)
            return mc_world_runtime_add_world_access(world, s->observers[3], cause);
        s->bs.cq.removed = true;
        status = mc_world_access_receipt_create(s->observers[0], NULL, &s->bs.cq.receipt, failure,
                                                cause);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        s->bs.cq.receipt->exception_class = "java/lang/IllegalStateException";
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "Presentation actual Source callback failure", 57};
    }
    if (s->native_stop || s->capture_refusal) {
        s->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Native integrity fault, no Source repair. */
        s->projected = true;
    }
    if (s->drift)
        return mc_entity_tick_execution_push_world(&s->bs.cq.execution, world, bs_site,
                                                   &s->descendant);
    return s->failure ? cause->status : MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status pe_entity_observe(pe_observer *input, mc_world_runtime *world,
                                                 const mc_registry_string *name, double x, double y,
                                                 double z, mc_world_storage_error *cause) {
    pe_fixture *s = input->fixture;
    const size_t index = s->callbacks++;
    if (index < COUNT(s->order))
        s->order[index] = input->id;
    const uint64_t expected_x = index == 0   ? pe_double_bits(8.5)
                                : index == 1 ? UINT64_C(0x8000000000000000)
                                             : pe_arguments[1];
    const uint64_t expected_z = index == 0   ? pe_double_bits(9.25)
                                : index == 1 ? pe_arguments[5]
                                             : pe_double_bits(-17.25);
    const double expected_y = index == 0 ? 65.5 : index == 1 ? 33.5 : 9.0;
    s->valid &=
        index < 3 && world == s->bs.cq.source.game.worlds[0] && name == s->name &&
        pe_double_bits(x) == expected_x && pe_double_bits(y) == pe_double_bits(expected_y) &&
        pe_double_bits(z) == expected_z && s->actor->holds == 2 &&
        s->observers[input->id]->active_holds == 1 &&
        s->bs.cq.execution.leases[s->bs.outer.slot].phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
        world->active_calls == 1;
    if (!s->valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Entity presentation bounded callback admission", 0};
        return cause->status;
    }
    if (s->mutate && input->id == 0) {
        /* Real Source edits stay in place through the next List/Entity reads. */
        s->actor->base.x = pe_double(UINT64_C(0x8000000000000000));
        s->actor->base.y = 33.75;
        s->actor->base.offset = .25f;
        s->actor->base.z = pe_double(pe_arguments[5]);
        bool removed = false;
        const mc_world_storage_status status =
            mc_world_runtime_remove_world_access(world, s->observers[0], &removed, cause);
        if (status != MC_WORLD_STORAGE_OK || !removed)
            return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
        return mc_world_runtime_add_world_access(world, s->observers[3], cause);
    }
    if (s->mutate && input->id == 2) {
        s->actor->base.x = pe_double(pe_arguments[1]);
        s->actor->base.y = 9.5;
        s->actor->base.offset = .5f;
        s->actor->base.z = -17.25;
    }
    if (index == 0 && s->native_stop) {
        s->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Native integrity control only. */
        s->projected = true;
    }
    if (index == 0 && s->drift)
        return mc_entity_tick_execution_push_world(&s->bs.cq.execution, world, bs_site,
                                                   &s->descendant);
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status pe_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    pe_observer *input = context;
    input->fixture->valid &= !input->fixture->particle &&
                             pe_float_bits(volume) == UINT32_C(0x80000000) &&
                             pe_float_bits(pitch) == UINT32_C(0x7fc01234);
    if (input->fixture->entity_sound)
        return pe_entity_observe(input, world, name, x, y, z, cause);
    return pe_observe(input, world, name, x, y, z, cause, failure);
}

static mc_world_storage_status pe_particle(void *context, mc_world_runtime *world,
                                           const mc_registry_string *name, double x, double y,
                                           double z, double vx, double vy, double vz,
                                           mc_world_storage_error *cause,
                                           mc_world_access_failure *failure) {
    pe_observer *input = context;
    input->fixture->valid &= input->fixture->particle && pe_double_bits(vx) == pe_arguments[3] &&
                             pe_double_bits(vy) == pe_arguments[4] &&
                             pe_double_bits(vz) == pe_arguments[5];
    return pe_observe(input, world, name, x, y, z, cause, failure);
}

static int pe_observers(pe_fixture *s, size_t count) {
    const mc_world_access_source_callbacks callbacks = {.sound = pe_sound, .particle = pe_particle};
    for (size_t i = 0; i < COUNT(s->observers); ++i) {
        s->contexts[i] = (pe_observer){s, i};
        VERIFY(mc_world_access_source_create(&callbacks, &s->contexts[i], &s->observers[i], NULL) ==
               MC_WORLD_STORAGE_OK);
        if (i < count)
            VERIFY(mc_world_runtime_add_world_access(s->bs.cq.source.game.worlds[0],
                                                     s->observers[i], NULL) == MC_WORLD_STORAGE_OK);
    }
    s->bs.cq.receiver = s->observers[0];
    return 0;
}

static int pe_observers_free(pe_fixture *s) {
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

static int pe_reenter(pe_fixture *s) {
    const size_t callbacks = s->callbacks, returned = s->frame.callbacks_returned;
    const mc_entity_tick_execution_error first = s->owned.ownership_error;
    pe_marker(&s->probe_error);
    s->probe_raw = MC_WORLD_STORAGE_LIMIT;
    VERIFY(!pe_call(s, &s->frame, s->bs.cq.source.game.worlds[0], &s->probe_error, &s->probe_raw) &&
           pe_marker_same(&s->probe_error) && s->probe_raw == MC_WORLD_STORAGE_LIMIT &&
           s->callbacks == callbacks && s->frame.callbacks_returned == returned &&
           s->frame.entered && !s->frame.completed &&
           (first.status == MC_WORLD_STORAGE_OK
                ? s->owned.ownership_status == MC_WORLD_STORAGE_INVALID_ARGUMENT
                : cq_owner_error_same(first, s->owned.ownership_error)));
    return 0;
}

static int pe_live(pe_fixture *s, bool empty) {
    fixture *f = &s->bs.cq.source.game;
    s->valid = true;
    if (!empty)
        VERIFY(pe_observers(s, 3) == 0);
    VERIFY(pe_begin(s) == 0);
    const mc_java_random random = *f->worlds[0]->fields.r;
    const size_t bindings = f->worlds[0]->persistent_bindings;
    if (s->held)
        VERIFY(mc_world_runtime_call_enter(f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    const bool returned = pe_call(s, &s->frame, f->worlds[0], &s->error, &s->raw);
    if (s->held)
        mc_world_runtime_call_leave(f->worlds[0]);
    VERIFY(
        returned && s->valid && s->callbacks == (empty ? 0u : 3u) &&
        s->raw == MC_WORLD_STORAGE_OK && s->error.status == MC_WORLD_STORAGE_OK &&
        s->error.world_access_bytecode == 0 && s->owned.ownership_status == MC_WORLD_STORAGE_OK &&
        !s->owned.selection_ready && !cq_pending(&s->owned) &&
        s->bs.cq.execution.receipt_count == 0 && s->bs.cq.execution.world_count == 1 &&
        s->bs.cq.execution.current_lease == s->bs.outer.slot &&
        s->bs.cq.execution.leases[s->bs.outer.slot].phase == MC_WORLD_ACCESS_FAILURE_LEASE_IDLE &&
        f->worlds[0]->persistent_bindings == bindings && f->worlds[0]->active_calls == 0 &&
        memcmp(&random, f->worlds[0]->fields.r, sizeof(random)) == 0);
    if (!s->legacy && !s->bare)
        VERIFY(s->frame.entered && s->frame.completed &&
               s->frame.callbacks_returned == (empty ? 0u : 3u) &&
               cq_token_same(s->frame.world, s->bs.outer));
    if (!empty)
        VERIFY(s->order[0] == 0 && s->order[1] == 2 && s->order[2] == 3 &&
               s->observers[0]->memberships == 0 && s->observers[1]->memberships == 1 &&
               s->observers[2]->memberships == 1 && s->observers[3]->memberships == 1 &&
               pe_observers_free(s) == 0);
    VERIFY(pe_end(s, false) == 0);
    return 0;
}

static int pe_native_stop(pe_fixture *s) {
    fixture *f = &s->bs.cq.source.game;
    s->valid = true;
    VERIFY(pe_observers(s, 2) == 0 && pe_begin(s) == 0);
    const mc_java_random random = *f->worlds[0]->fields.r;
    const bool returned = pe_call(s, &s->frame, f->worlds[0], &s->error, &s->raw);
    if (s->projected) {
        /* Restore only the native integrity control before checked cleanup. */
        f->worlds[0]->persistent_bindings = s->saved_bindings;
        s->projected = false;
    }
    const mc_world_storage_status expected =
        s->drift ? MC_WORLD_STORAGE_BUSY : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    VERIFY(!returned && s->valid && s->callbacks == 1 && s->order[0] == 0 &&
           s->raw == (s->bare ? expected : MC_WORLD_STORAGE_OK) &&
           (s->null_error || (s->error.status == (s->bare ? expected : MC_WORLD_STORAGE_OK) &&
                              s->error.world_access_bytecode == (s->particle ? 43u : 41u) &&
                              s->error.world_access_failure.receipt == NULL)) &&
           (s->bare || s->owned.ownership_status == expected) && !s->owned.selection_ready &&
           !cq_pending(&s->owned) && s->bs.cq.execution.receipt_count == 0 &&
           s->bs.cq.execution.leases[s->bs.outer.slot].phase ==
               MC_WORLD_ACCESS_FAILURE_LEASE_IDLE &&
           f->worlds[0]->active_calls == 0 &&
           memcmp(&random, f->worlds[0]->fields.r, sizeof(random)) == 0);
    if (!s->legacy && !s->bare)
        VERIFY(s->frame.entered && !s->frame.completed && s->frame.callbacks_returned == 1 &&
               s->frame.callback_status == MC_WORLD_STORAGE_OK &&
               cq_token_same(s->frame.world, s->bs.outer) && pe_reenter(s) == 0);
    if (s->drift)
        VERIFY(s->bs.cq.execution.current_lease == s->descendant.slot &&
               s->descendant.slot != s->bs.outer.slot &&
               mc_entity_tick_execution_pop_world(&s->bs.cq.execution, &s->descendant, false) ==
                   MC_WORLD_STORAGE_OK);
    VERIFY(pe_observers_free(s) == 0 && pe_end(s, true) == 0);
    return 0;
}

static int pe_lease(pe_fixture *s) {
    fixture *f = &s->bs.cq.source.game;
    s->valid = true;
    VERIFY(pe_observers(s, 2) == 0 && pe_begin(s) == 0);
    const mc_java_random random = *f->worlds[0]->fields.r;
    VERIFY(!pe_call(s, &s->frame, f->worlds[0], &s->error, &s->raw));
    if (s->projected) {
        f->worlds[0]->persistent_bindings = s->saved_bindings;
        s->projected = false;
    }
    mc_world_access_failure_lease *lease = &s->bs.cq.execution.leases[s->bs.outer.slot];
    VERIFY(s->valid && s->callbacks == 1 && s->bs.cq.removed &&
           s->raw == MC_WORLD_STORAGE_IO_ERROR &&
           (s->null_error ||
            (s->error.status == MC_WORLD_STORAGE_IO_ERROR && s->error.cause.system_error == 57 &&
             s->error.world_access_bytecode == (s->particle ? 43u : 41u) &&
             s->error.world_access_failure.receipt == s->bs.cq.receipt)) &&
           s->observers[0]->memberships == 0 && s->observers[0]->active_holds == 1 &&
           s->bs.cq.receipt->holds == 1 && s->bs.cq.receipt->leases == lease &&
           f->worlds[0]->active_calls == 0 &&
           memcmp(&random, f->worlds[0]->fields.r, sizeof(random)) == 0);
    if (!s->legacy && !s->bare)
        VERIFY(s->frame.entered && !s->frame.completed && s->frame.callbacks_returned == 1 &&
               s->frame.callback_status == MC_WORLD_STORAGE_IO_ERROR && s->frame.lease == lease &&
               cq_token_same(s->frame.world, s->bs.outer));
    const mc_world_access_failure *view = NULL;
    VERIFY(mc_world_access_failure_lease_inspect(lease, &view) &&
           view->receipt == s->bs.cq.receipt && view->receiver == s->observers[0] &&
           lease->callback_status == MC_WORLD_STORAGE_IO_ERROR &&
           lease->callback_error.system_error == 57);
    if (s->capture_refusal)
        VERIFY(s->owned.ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
               s->bs.cq.execution.receipt_count == 0 &&
               mc_entity_tick_execution_capture_receipt(&s->bs.cq.execution, view) ==
                   MC_WORLD_STORAGE_OK &&
               s->bs.cq.execution.receipt_count == 1 &&
               s->owned.ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    else
        VERIFY(s->owned.ownership_status == MC_WORLD_STORAGE_OK &&
               s->bs.cq.execution.receipt_count == 1);
    VERIFY(s->bs.cq.execution.receipts[0].receipt == s->bs.cq.receipt);
    if (!s->legacy && !s->bare)
        VERIFY(pe_reenter(s) == 0);
    if (s->nodes)
        pe_probe_node(s);
    s->error.world_access_failure = (mc_world_access_failure){0};
    view = NULL;
    VERIFY(s->valid &&
           mc_world_access_receipt_retire(&s->bs.cq.receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
           mc_world_access_free(s->observers[0], NULL) == MC_WORLD_STORAGE_BUSY &&
           pe_end(s, true) == 0 &&
           mc_world_access_receipt_retire(&s->bs.cq.receipt, NULL) == MC_WORLD_STORAGE_OK &&
           pe_observers_free(s) == 0);
    return 0;
}

static int pe_admission(pe_fixture *s) {
    fixture *f = &s->bs.cq.source.game;
    for (unsigned int mode = 0; mode < 5; ++mode) {
        mc_entity_tick_execution_init(&s->bs.cq.execution, NULL);
        if (mode >= 2)
            VERIFY(mc_entity_tick_execution_push_world(&s->bs.cq.execution,
                                                       mode == 3 ? NULL : f->worlds[0], bs_site,
                                                       &s->bs.outer) == MC_WORLD_STORAGE_OK);
        pe_prepare(s);
        if (mode == 0)
            s->frame.execution = NULL;
        if (mode == 4)
            s->frame.owned = NULL;
        pe_marker(&s->error);
        VERIFY(!pe_call(s, &s->frame, mode == 2 || mode == 3 ? NULL : f->worlds[0], &s->error,
                        &s->raw) &&
               pe_marker_same(&s->error) && s->raw == MC_WORLD_STORAGE_LIMIT && !s->frame.entered &&
               !s->frame.completed && s->frame.callbacks_returned == 0 && s->callbacks == 0 &&
               (mode == 4 || s->owned.ownership_status != MC_WORLD_STORAGE_OK));
        if (mode >= 2)
            VERIFY(mc_entity_tick_execution_pop_world(&s->bs.cq.execution, &s->bs.outer, false) ==
                   MC_WORLD_STORAGE_OK);
        s->frame = (mc_world_presentation_execution){0};
        s->owned = (mc_world_runtime_execution_result){0};
        VERIFY(mc_entity_tick_execution_retire(&s->bs.cq.execution) == MC_WORLD_STORAGE_OK);
    }
    return 0;
}

static int pe_legacy_admission(pe_fixture *s) {
    fixture *f = &s->bs.cq.source.game;
    mc_entity_tick_execution *copy = malloc(sizeof(*copy));
    VERIFY(copy != NULL);
    for (unsigned int mode = 0; mode < 7; ++mode) {
        mc_entity_tick_execution_init(&s->bs.cq.execution, NULL);
        if (mode >= 2)
            VERIFY(mc_entity_tick_execution_push_world(
                       &s->bs.cq.execution, mode == 4 || mode == 5 ? NULL : f->worlds[0], bs_site,
                       &s->bs.outer) == MC_WORLD_STORAGE_OK);
        pe_prepare(s);
        pe_marker(&s->error);
        const mc_java_random random = *f->worlds[0]->fields.r;
        const mc_world_storage_error prior = f->worlds[0]->last_error;
        const size_t bindings = f->worlds[0]->persistent_bindings;
        mc_entity_tick_execution *actual = &s->bs.cq.execution;
        if (mode == 0)
            actual = NULL;
        if (mode == 2) {
            memcpy(copy, actual, sizeof(*copy)); /* Invalid copied envelope owns no rights. */
            actual = copy;
        }
        if (mode == 6)
            f->worlds[0]->persistent_bindings = 0; /* Native-only integrity control. */
        ++s->entries;
        mc_world_runtime *world = mode == 3 || mode == 5 ? NULL : f->worlds[0];
        s->raw = s->bare ? mc_world_runtime_particle_in_execution(actual, world, NULL, 0, 0, 0, 0,
                                                                  0, 0, &s->error)
                         : mc_world_runtime_particle_detailed_in_execution(actual, world, NULL, 0,
                                                                           0, 0, 0, 0, 0, bs_site,
                                                                           &s->error, &s->owned);
        if (mode == 6)
            f->worlds[0]->persistent_bindings = bindings;
        const mc_world_storage_status expected = mode == 0 || mode == 2 || mode == 5
                                                     ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                     : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        const char *operation = mode == 0   ? s->bare
                                                  ? "World execution child context is required"
                                                  : "World particle execution/native result required"
                                : mode == 5 ? "World presentation arguments"
                                            : actual->admission.operation;
        VERIFY(s->raw == expected && s->error.status == expected &&
               s->error.cause.status == expected && s->error.operation != NULL &&
               strcmp(s->error.operation, operation) == 0 &&
               (mode == 0 || mode == 5 || s->error.operation == actual->admission.operation) &&
               s->error.cause.operation == s->error.operation && s->error.cause.system_error == 0 &&
               s->error.world_access_bytecode == 0 && s->error.wrapper_bytecode == 0 &&
               s->error.world_access_failure.receipt == NULL &&
               s->error.world_access_failure.receiver == NULL && s->callbacks == 0 &&
               bs_error_same(f->worlds[0]->last_error, prior) && f->worlds[0]->active_calls == 0 &&
               f->worlds[0]->persistent_bindings == bindings &&
               memcmp(&random, f->worlds[0]->fields.r, sizeof(random)) == 0 &&
               s->bs.cq.execution.receipt_count == 0);
        if (!s->bare)
            VERIFY(
                s->owned.ownership_status == (mode == 5 ? MC_WORLD_STORAGE_OK : expected) &&
                (mode == 5 || strcmp(s->owned.ownership_error.operation,
                                     mode == 0 ? "World particle execution/native result required"
                                               : "World particle actual owned admission") == 0));
        if (mode >= 2)
            VERIFY(mc_entity_tick_execution_pop_world(&s->bs.cq.execution, &s->bs.outer, false) ==
                   MC_WORLD_STORAGE_OK);
        s->frame = (mc_world_presentation_execution){0};
        s->owned = (mc_world_runtime_execution_result){0};
        VERIFY(mc_entity_tick_execution_retire(&s->bs.cq.execution) == MC_WORLD_STORAGE_OK);
    }
    free(copy); /* Never retire the copied envelope or infer any right from it. */
    return 0;
}

static bool pe_entity_call(pe_fixture *s, mc_entity_tick_execution *execution,
                           mc_world_runtime *world) {
    ++s->entries;
    mc_world_movement_error *error = s->null_error ? NULL : &s->entity_error;
    s->raw = s->bare
                 ? mc_world_runtime_entity_sound_in_execution(execution, world, s->actor, s->name,
                                                              pe_float(UINT32_C(0x80000000)),
                                                              pe_float(UINT32_C(0x7fc01234)), error)
                 : mc_world_runtime_entity_sound_detailed_in_execution(
                       execution, world, s->actor, s->name, pe_float(UINT32_C(0x80000000)),
                       pe_float(UINT32_C(0x7fc01234)), bs_site, error, &s->owned);
    return s->raw == MC_WORLD_STORAGE_OK &&
           (s->bare || s->owned.ownership_status == MC_WORLD_STORAGE_OK);
}

static int pe_entity_admission(pe_fixture *s) {
    fixture *f = &s->bs.cq.source.game;
    mc_entity_tick_execution *copy = malloc(sizeof(*copy));
    VERIFY(copy != NULL);
    for (unsigned int mode = 0; mode < 7; ++mode) {
        mc_entity_tick_execution_init(&s->bs.cq.execution, NULL);
        if (mode >= 2)
            VERIFY(mc_entity_tick_execution_push_world(
                       &s->bs.cq.execution, mode == 4 || mode == 5 ? NULL : f->worlds[0], bs_site,
                       &s->bs.outer) == MC_WORLD_STORAGE_OK);
        pe_prepare(s);
        mc_world_movement_error_init(&s->entity_error);
        s->entity_error.bytecode = s->entity_error.world_access_bytecode = 211;
        s->entity_error.status = bs_error_marker.status;
        s->entity_error.operation = bs_error_marker.operation;
        s->entity_error.cause = bs_error_marker;
        const mc_world_storage_error prior = f->worlds[0]->last_error;
        const size_t bindings = f->worlds[0]->persistent_bindings;
        mc_entity_tick_execution *actual = &s->bs.cq.execution;
        if (mode == 0)
            actual = NULL;
        if (mode == 2) {
            memcpy(copy, actual, sizeof(*copy)); /* This invalid copy owns no right. */
            actual = copy;
        }
        if (mode == 6)
            f->worlds[0]->persistent_bindings = 0; /* Native integrity control only. */
        VERIFY(!pe_entity_call(s, actual, mode == 3 || mode == 5 ? NULL : f->worlds[0]));
        if (mode == 6)
            f->worlds[0]->persistent_bindings = bindings;
        const mc_world_storage_status expected = mode == 0 || mode == 2 || mode == 5
                                                     ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                     : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        const char *operation =
            mode == 0   ? s->bare ? "World execution child context is required"
                                  : "World Entity sound execution/native result required"
            : mode == 5 ? "World movement arguments"
                        : actual->admission.operation;
        VERIFY(
            s->raw == expected && s->entity_error.status == expected &&
            s->entity_error.cause.status == expected && s->entity_error.operation != NULL &&
            strcmp(s->entity_error.operation, operation) == 0 &&
            (mode == 0 || mode == 5 || s->entity_error.operation == actual->admission.operation) &&
            s->entity_error.cause.operation == s->entity_error.operation &&
            s->entity_error.cause.system_error == 0 && s->entity_error.bytecode == 0 &&
            s->entity_error.world_access_bytecode == 0 &&
            s->entity_error.world_access_fault == MC_WORLD_ACCESS_SOURCE_FAULT_NONE &&
            s->entity_error.world_access_failure.receipt == NULL && s->callbacks == 0 &&
            bs_error_same(f->worlds[0]->last_error, prior) &&
            f->worlds[0]->persistent_bindings == bindings && f->worlds[0]->active_calls == 0 &&
            s->actor->holds == 1 && s->bs.cq.execution.receipt_count == 0);
        if (!s->bare)
            VERIFY(s->owned.ownership_status == (mode == 5 ? MC_WORLD_STORAGE_OK : expected) &&
                   (mode == 5 ||
                    strcmp(s->owned.ownership_error.operation,
                           mode == 0 ? "World Entity sound execution/native result required"
                                     : "World Entity sound actual owned admission") == 0));
        if (mode >= 2)
            VERIFY(mc_entity_tick_execution_pop_world(&s->bs.cq.execution, &s->bs.outer, false) ==
                   MC_WORLD_STORAGE_OK);
        s->frame = (mc_world_presentation_execution){0};
        s->owned = (mc_world_runtime_execution_result){0};
        VERIFY(mc_entity_tick_execution_retire(&s->bs.cq.execution) == MC_WORLD_STORAGE_OK);
    }
    free(copy); /* Never retire the invalid copied envelope. */
    return 0;
}

static int pe_entity(pe_fixture *s) {
    fixture *f = &s->bs.cq.source.game;
    const size_t live = f->entity_classes[0]->live_entities;
    VERIFY(mc_arrow_allocate(f->entity_classes[0], &s->actor, NULL) == MC_ENTITY_OK &&
           mc_arrow_construct_position(s->actor, f->worlds[0], 8.5, 65.75, 9.25, NULL) ==
               MC_ENTITY_OK &&
           s->actor->holds == 1 && s->actor->base.world == f->worlds[0]);
    s->actor->base.offset = .25f; /* Actual fixture argument, outside Source invocation. */
    const mc_java_random actor_random = *s->actor->random;
    const mc_java_random world_random = *f->worlds[0]->fields.r;
    if (strstr(s->mode, "admission") != NULL) {
        VERIFY(pe_entity_admission(s) == 0);
    } else {
        s->valid = true;
        VERIFY(pe_observers(s, s->mutate ? 3 : 2) == 0 && pe_begin(s) == 0);
        mc_world_movement_error_init(&s->entity_error);
        if (s->null_error) {
            s->entity_error.status = bs_error_marker.status;
            s->entity_error.operation = bs_error_marker.operation;
            s->entity_error.cause = bs_error_marker;
            s->entity_error.bytecode = s->entity_error.world_access_bytecode = 211;
        }
        if (s->mutate)
            VERIFY(mc_registry_runtime_literal_ascii(f->registry[0], "native.entity.sound.identity",
                                                     &s->name, NULL) == MC_REGISTRY_OK &&
                   mc_registry_runtime_literal_owned(f->registry[0], s->name));
        const size_t bindings = f->worlds[0]->persistent_bindings;
        const mc_world_storage_error prior = f->worlds[0]->last_error;
        const bool returned = pe_entity_call(s, &s->bs.cq.execution, f->worlds[0]);
        if (s->projected) {
            f->worlds[0]->persistent_bindings = s->saved_bindings;
            s->projected = false;
        }
        const mc_world_storage_status native = s->mutate  ? MC_WORLD_STORAGE_OK
                                               : s->drift ? MC_WORLD_STORAGE_BUSY
                                                          : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        VERIFY(returned == s->mutate && s->valid && s->callbacks == (s->mutate ? 3u : 1u) &&
               s->raw == (s->bare ? native : MC_WORLD_STORAGE_OK) &&
               (s->bare || s->owned.ownership_status == native) &&
               s->bs.cq.execution.receipt_count == 0 && !s->owned.selection_ready &&
               !cq_pending(&s->owned) && s->actor->holds == 1 && f->worlds[0]->active_calls == 0 &&
               s->bs.cq.execution.leases[s->bs.outer.slot].phase ==
                   MC_WORLD_ACCESS_FAILURE_LEASE_IDLE &&
               bs_error_same(f->worlds[0]->last_error, prior));
        if (s->null_error)
            VERIFY(s->entity_error.status == bs_error_marker.status &&
                   bs_error_same(s->entity_error.cause, bs_error_marker) &&
                   s->entity_error.bytecode == 211 && s->entity_error.world_access_bytecode == 211);
        else {
            VERIFY(s->entity_error.status == (s->bare ? native : MC_WORLD_STORAGE_OK) &&
                   s->entity_error.bytecode == (s->mutate ? 0u : 53u) &&
                   s->entity_error.world_access_bytecode == (s->mutate ? 0u : 53u) &&
                   s->entity_error.world_access_fault == MC_WORLD_ACCESS_SOURCE_FAULT_NONE &&
                   s->entity_error.world_access_failure.receipt == NULL &&
                   s->entity_error.world_access_failure.receiver == NULL);
            if (s->bare && !s->mutate)
                VERIFY(s->entity_error.cause.status == native &&
                       s->entity_error.cause.operation == s->entity_error.operation &&
                       s->entity_error.operation != NULL);
        }
        if (s->mutate)
            VERIFY(s->order[0] == 0 && s->order[1] == 2 && s->order[2] == 3 &&
                   pe_double_bits(s->actor->base.x) == pe_arguments[1] && s->actor->base.y == 9.5 &&
                   s->actor->base.offset == .5f && s->actor->base.z == -17.25);
        if (s->drift)
            VERIFY(s->bs.cq.execution.current_lease == s->descendant.slot &&
                   s->descendant.slot != s->bs.outer.slot &&
                   s->bs.cq.execution.world_slots[s->descendant.slot].parent == s->bs.outer.slot &&
                   mc_entity_tick_execution_pop_world(&s->bs.cq.execution, &s->descendant, false) ==
                       MC_WORLD_STORAGE_OK);
        VERIFY(f->worlds[0]->persistent_bindings == bindings && pe_observers_free(s) == 0 &&
               pe_end(s, !s->mutate) == 0);
    }
    VERIFY(memcmp(&world_random, f->worlds[0]->fields.r, sizeof(world_random)) == 0 &&
           memcmp(&actor_random, s->actor->random, sizeof(actor_random)) == 0);
    s->entity_error.world_access_failure = (mc_world_access_failure){0};
    mc_entity_release(s->actor);
    s->actor = NULL;
    VERIFY(f->entity_classes[0]->live_entities == live);
    return 0;
}

static bool pe_mode(const char *mode) {
    const char *const modes[] = {"sound-scope-drift",
                                 "particle-scope-drift",
                                 "sound-native-stop",
                                 "particle-native-stop",
                                 "sound-live",
                                 "particle-live",
                                 "sound-empty",
                                 "particle-empty",
                                 "sound-held-live",
                                 "particle-held-live",
                                 "sound-lease",
                                 "particle-lease",
                                 "sound-nodes",
                                 "particle-nodes",
                                 "sound-capture-refusal",
                                 "particle-capture-refusal",
                                 "sound-admission",
                                 "particle-admission",
                                 "particle-legacy-scope-drift",
                                 "particle-bare-scope-drift",
                                 "particle-legacy-live",
                                 "particle-bare-live",
                                 "particle-legacy-lease",
                                 "particle-bare-lease",
                                 "particle-legacy-null-lease",
                                 "particle-bare-null-native-stop",
                                 "particle-legacy-admission",
                                 "particle-bare-admission",
                                 "entity-detailed-scope-drift",
                                 "entity-bare-scope-drift",
                                 "entity-detailed-native-stop",
                                 "entity-bare-native-stop",
                                 "entity-detailed-live",
                                 "entity-bare-live",
                                 "entity-detailed-admission",
                                 "entity-bare-admission",
                                 "entity-detailed-null-scope-drift",
                                 "entity-bare-null-scope-drift"};
    for (size_t i = 0; i < COUNT(modes); ++i)
        if (!strcmp(mode, modes[i]))
            return true;
    return false;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && pe_mode(argv[1]));
    printf("PRESENTATION_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    pe_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL && ss_setup(&s->bs.cq.source, argv[2]) == 0 &&
           cq_files(&s->bs.cq, true) == 0 && ss_open(&s->bs.cq.source) == 0);
    printf("PRESENTATION_CASE %s BEGIN\n", argv[1]);
    fflush(stdout);
    s->mode = argv[1];
    s->entity_sound = !strncmp(argv[1], "entity-", 7);
    s->particle = !strncmp(argv[1], "particle-", 9);
    s->legacy = strstr(argv[1], "-legacy-") != NULL;
    s->bare = strstr(argv[1], "-bare-") != NULL;
    s->null_error = strstr(argv[1], "-null-") != NULL;
    s->drift = strstr(argv[1], "scope-drift") != NULL;
    s->native_stop = strstr(argv[1], "native-stop") != NULL;
    s->mutate = strstr(argv[1], "live") != NULL;
    s->held = strstr(argv[1], "-held-") != NULL;
    if (s->held)
        VERIFY(mc_registry_runtime_literal_ascii(s->bs.cq.source.game.registry[0],
                                                 "native.presentation.identity", &s->name,
                                                 NULL) == MC_REGISTRY_OK &&
               mc_registry_runtime_literal_owned(s->bs.cq.source.game.registry[0], s->name));
    s->nodes = strstr(argv[1], "nodes") != NULL;
    s->capture_refusal = strstr(argv[1], "capture-refusal") != NULL;
    s->failure = s->nodes || s->capture_refusal || strstr(argv[1], "lease") != NULL;
    const int status = s->entity_sound ? pe_entity(s)
                       : strstr(argv[1], "admission") && (s->legacy || s->bare)
                           ? pe_legacy_admission(s)
                       : strstr(argv[1], "admission") ? pe_admission(s)
                       : s->drift || s->native_stop   ? pe_native_stop(s)
                       : s->failure                   ? pe_lease(s)
                                    : pe_live(s, strstr(argv[1], "empty") != NULL);
    VERIFY(status == 0);
    printf("PRESENTATION_CASE %s END entries=%zu callbacks=%zu probes=%zu\n", argv[1], s->entries,
           s->callbacks, s->probes);
    fflush(stdout);
    VERIFY(cq_empty_stop(&s->bs.cq.source) == 0 && cq_files(&s->bs.cq, false) == 0 &&
           ss_finish(&s->bs.cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->bs.cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->bs.cq.inputs[i]);
    free(s);
    puts("PRESENTATION_OPEN recursive-mutation/full-wake/original-UI");
    printf("PASS: %u actual owned presentation checks;mode=%s\n", checks, argv[1]);
    return 0;
}
