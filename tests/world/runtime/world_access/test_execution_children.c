/* Prospective native component controls, not original observations or SL.
 * Imported publication main is renamed and NEVER executed. Actual constructed
 * Worlds, source callbacks, registration and next acquire all owning rights.
 * Native counter faults are restored before CHECK; no source pointer/phase is
 * repaired for cleanup. The real N case uses its genuine initial App=NULL. */
int sp_unused_publication_fixture_main(int, char **);
#define main sp_unused_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "block/registry/runtime_internal.h"
#include "client/world/render_global_access.h"
#include "client/world/render_global_internal.h"
#include "core/java/random_profile_internal.h"
#include "entity/tick/execution_internal.h"
#include "world/runtime/execution_children_internal.h"
#include "world/runtime/mutation/mutation_internal.h"
#include "world/runtime/world_access_internal.h"
#include "world/runtime/world_access_lease_internal.h"

#include <xmmintrin.h>

enum sp_kind { SP_SOUND, SP_PARTICLE };

enum sp_mode { SP_RETURN, SP_SOURCE, SP_NATIVE, SP_ADAPTER, SP_FOREIGN };
typedef struct sp_fixture sp_fixture;

typedef struct sp_event {
    enum sp_kind kind;
    const mc_registry_string *name;
    double numbers[6];
    float volume, pitch;
} sp_event;

typedef struct sp_receiver {
    sp_fixture *fixture;
    mc_world_access *self;
    mc_world_access_receipt *receipt;
    enum sp_mode mode;
    bool remove_self, removed, move_once, reenter, bad, saturated, inject_extent, extent_projected;
    size_t calls, original_holds, projected_holds, original_views, projected_views;
    mc_world_storage_status free_during, inner_status, end_false, end_true, retire_active;
    bool scope_preserved;
    sp_event events[8];
} sp_receiver;

struct sp_fixture {
    fixture *base;
    mc_entity *actor; /* Borrowed from base->entities, not another occurrence. */
    const mc_registry_string *name;
    sp_receiver receivers[2];
    mc_entity_tick_execution execution;
    bool execution_initialized;
    mc_world_movement_error *sound, *probe_sound;
    mc_world_mutation_error *particle, *probe_particle;
    mc_client_render_engine_classes *render_classes;
    mc_client_render_global *renderer;
    mc_client_render_global_error *render_error;
};

typedef struct sp_random_state {
    mc_java_random actor, world;
    uint64_t uniquifier, math_state, math_gaussian;
    unsigned int holder_phase;
    const mc_java_math_random_owner *math;
    bool math_have;
    size_t bytes;
} sp_random_state;

typedef struct sp_scope {
    mc_world_runtime *world;
    mc_math_execution *math;
    mc_entity_tick_execution_phase phase;
    mc_entity_tick_execution_error first, capture;
    size_t current, leases, views, bindings;
    bool held, open, failed, pending;
} sp_scope;

static bool sp_error_equal(const mc_entity_tick_execution_error *a,
                           const mc_entity_tick_execution_error *b) {
    return a->status == b->status && a->operation == b->operation &&
           a->site.class_name == b->site.class_name && a->site.method == b->site.method &&
           a->site.descriptor == b->site.descriptor && a->site.pc == b->site.pc;
}

static sp_scope sp_scope_take(const sp_fixture *s) {
    const mc_entity_tick_execution *e = &s->execution;
    return (sp_scope){
        e->world,          e->math,          e->phase,
        e->first,          e->capture,       e->current_lease,
        e->lease_count,    e->receipt_count, e->world == NULL ? 0 : e->world->persistent_bindings,
        e->world_held,     e->world_open,    e->child_failed,
        e->capture_pending};
}

static bool sp_scope_same(const sp_fixture *s, const sp_scope *before) {
    const sp_scope now = sp_scope_take(s);
    return now.world == before->world && now.math == before->math && now.phase == before->phase &&
           now.current == before->current && now.leases == before->leases &&
           now.views == before->views && now.bindings == before->bindings &&
           now.held == before->held && now.open == before->open && now.failed == before->failed &&
           now.pending == before->pending && sp_error_equal(&now.first, &before->first) &&
           sp_error_equal(&now.capture, &before->capture);
}

static sp_random_state sp_random_take(const sp_fixture *s) {
    const mc_java_random_profile *p = s->base->profile;
    sp_random_state value = {
        .actor = *s->actor->random,
        .world = *s->base->worlds[0]->fields.r,
        .uniquifier = atomic_load_explicit(&p->uniquifier, memory_order_relaxed),
        .holder_phase = atomic_load_explicit(&p->holder_phase, memory_order_relaxed),
        .math = p->math,
        .bytes = p->allocated_bytes};
    if (p->math != NULL) {
        value.math_state = atomic_load_explicit(&p->math->state, memory_order_relaxed);
        value.math_have = p->math->have_next_next_gaussian;
        memcpy(&value.math_gaussian, &p->math->next_next_gaussian, sizeof(value.math_gaussian));
    }
    return value;
}

static bool sp_random_same(const sp_fixture *s, const sp_random_state *before) {
    const sp_random_state now = sp_random_take(s);
    return mc_test_java_random_equal(&now.actor, &before->actor) &&
           mc_test_java_random_equal(&now.world, &before->world) &&
           now.uniquifier == before->uniquifier && now.holder_phase == before->holder_phase &&
           now.math == before->math && now.math_state == before->math_state &&
           now.math_gaussian == before->math_gaussian && now.math_have == before->math_have &&
           now.bytes == before->bytes;
}

static bool sp_double(double a, double b) {
    uint64_t x, y;
    memcpy(&x, &a, sizeof(x));
    memcpy(&y, &b, sizeof(y));
    return x == y;
}

static mc_world_storage_status sp_call(sp_fixture *s, enum sp_kind kind,
                                       mc_entity_tick_execution *execution, mc_world_runtime *world,
                                       bool output) {
    return kind == SP_SOUND
               ? mc_world_runtime_entity_sound_in_execution(execution, world, s->actor, s->name,
                                                            .75f, 1.25f, output ? s->sound : NULL)
               : mc_world_runtime_particle_in_execution(execution, world, s->name, .5, -0.0, 7.25,
                                                        -.125, 0.0, 3.0,
                                                        output ? s->particle : NULL);
}

static void sp_clear_views(sp_fixture *s) {
    if (s->sound != NULL) {
        s->sound->world_access_failure = (mc_world_access_failure){0};
        s->sound->presentation.world_access_failure = (mc_world_access_failure){0};
    }
    if (s->particle != NULL)
        s->particle->world_access_failure = (mc_world_access_failure){0};
    if (s->probe_sound != NULL)
        s->probe_sound->world_access_failure = (mc_world_access_failure){0};
    if (s->probe_particle != NULL)
        s->probe_particle->world_access_failure = (mc_world_access_failure){0};
}

static void sp_restore_counter(sp_receiver *r) {
    if (r->saturated && r->receipt != NULL) {
        r->projected_holds = r->receipt->holds;
        r->receipt->holds = r->original_holds;
        r->saturated = false;
    }
    if (r->extent_projected) {
        r->projected_views = r->fixture->execution.receipt_count;
        r->fixture->execution.receipt_count = r->original_views;
        r->extent_projected = false;
    }
}

static bool sp_unsubscribe(sp_fixture *s, mc_world_access *access) {
    if (access == NULL || s->base == NULL || s->base->worlds[0] == NULL)
        return true;
    bool removed;
    do {
        removed = false;
        if (mc_world_runtime_remove_world_access(s->base->worlds[0], access, &removed, NULL) !=
            MC_WORLD_STORAGE_OK)
            return false;
    } while (removed);
    return true;
}

static bool sp_fresh(sp_fixture *s) {
    sp_clear_views(s);
    if (s->execution_initialized) {
        if (s->execution.world_open &&
            mc_entity_tick_execution_end_world(&s->execution, true) != MC_WORLD_STORAGE_OK)
            return false;
        if (mc_entity_tick_execution_retire(&s->execution) != MC_WORLD_STORAGE_OK)
            return false;
    }
    mc_entity_tick_execution_init(&s->execution, NULL);
    s->execution_initialized = true;
    return true;
}

static bool sp_begin(sp_fixture *s, mc_world_runtime *world, enum sp_kind kind) {
    return mc_entity_tick_execution_begin_world(
               &s->execution, world,
               (mc_entity_tick_execution_site){"sl", "w_", "()V", kind == SP_SOUND ? 748 : 1403}) ==
           MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status sp_observe(sp_receiver *r, mc_world_runtime *world,
                                          enum sp_kind kind, mc_world_storage_error *error,
                                          mc_world_access_failure *failure) {
    sp_fixture *s = r->fixture;
    const size_t index = r->calls++;
    if (index >= COUNT(r->events) || world != s->base->worlds[0] || world->active_calls == 0 ||
        r->self->active_holds == 0) {
        r->bad = true;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "execution child observer", 0};
        return error->status;
    }
    r->events[index].kind = kind;
    if (r->move_once) {
        r->move_once = false;
        if (mc_entity_set_position(s->actor, 4.5, 12.75, -6.25) != MC_ENTITY_OK)
            r->bad = true;
    }
    if (r->remove_self) {
        r->remove_self = false;
        bool removed = false;
        if (mc_world_runtime_remove_world_access(world, r->self, &removed, error) !=
                MC_WORLD_STORAGE_OK ||
            !removed) {
            r->bad = true;
            return error->status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                                        : error->status;
        }
        r->removed = true;
        r->free_during = mc_world_access_free(r->self, NULL);
    }
    if (r->reenter) {
        r->reenter = false;
        const sp_scope scope = sp_scope_take(s);
        const size_t active = r->self->active_holds, calls = r->calls;
        r->inner_status =
            kind == SP_SOUND
                ? mc_world_runtime_entity_sound_in_execution(&s->execution, world, s->actor,
                                                             s->name, 1, 1, s->probe_sound)
                : mc_world_runtime_particle_in_execution(&s->execution, world, s->name, 0, 0, 0, 0,
                                                         0, 0, s->probe_particle);
        r->scope_preserved =
            sp_scope_same(s, &scope) && r->calls == calls && r->self->active_holds == active;
        r->end_false = mc_entity_tick_execution_end_world(&s->execution, false);
        r->scope_preserved &= sp_scope_same(s, &scope);
        r->end_true = mc_entity_tick_execution_end_world(&s->execution, true);
        r->scope_preserved &= sp_scope_same(s, &scope);
        r->retire_active = mc_entity_tick_execution_retire(&s->execution);
        r->scope_preserved &= sp_scope_same(s, &scope) && r->self->active_holds == active;
    }
    *failure = (mc_world_access_failure){0};
    *error = (mc_world_storage_error){0};
    if (r->mode == SP_RETURN)
        return MC_WORLD_STORAGE_OK;
    if (r->mode == SP_FOREIGN) {
        sp_receiver *foreign = &s->receivers[1];
        *failure =
            (mc_world_access_failure){.receiver = foreign->self, .receipt = foreign->receipt};
    } else if (r->mode != SP_ADAPTER) {
        if (r->receipt != NULL) {
            r->bad = true;
            *error =
                (mc_world_storage_error){MC_WORLD_STORAGE_BUSY, "observer receipt still owned", 0};
            return error->status;
        }
        if (mc_world_access_receipt_create(r->self, NULL, &r->receipt, failure, error) !=
            MC_WORLD_STORAGE_OK)
            return error->status;
        r->receipt->exception_class =
            r->mode == SP_SOURCE ? "java/lang/IllegalStateException" : NULL;
        if (r->saturated) {
            r->original_holds = r->receipt->holds;
            r->receipt->holds = SIZE_MAX; /* Authored native counter fault, not Java OOM. */
        }
    }
    if (r->inject_extent) {
        r->inject_extent = false;
        r->original_views = s->execution.receipt_count;
        s->execution.receipt_count = MC_ENTITY_TICK_EXECUTION_RECEIPTS;
        r->extent_projected = true; /* Authored native extent fault, not source state. */
    }
    *error = (mc_world_storage_error){r->mode == SP_NATIVE ? MC_WORLD_STORAGE_LIMIT
                                                           : MC_WORLD_STORAGE_IO_ERROR,
                                      "execution child authored callback failure", 71};
    return error->status;
}

static mc_world_storage_status sp_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *error,
                                        mc_world_access_failure *failure) {
    sp_receiver *r = context;
    if (r->calls < COUNT(r->events))
        r->events[r->calls] = (sp_event){SP_SOUND, name, {x, y, z, 0, 0, 0}, volume, pitch};
    return sp_observe(r, world, SP_SOUND, error, failure);
}

static mc_world_storage_status sp_particle(void *context, mc_world_runtime *world,
                                           const mc_registry_string *name, double x, double y,
                                           double z, double vx, double vy, double vz,
                                           mc_world_storage_error *error,
                                           mc_world_access_failure *failure) {
    sp_receiver *r = context;
    if (r->calls < COUNT(r->events))
        r->events[r->calls] = (sp_event){SP_PARTICLE, name, {x, y, z, vx, vy, vz}, 0, 0};
    return sp_observe(r, world, SP_PARTICLE, error, failure);
}

static const mc_world_access_source_callbacks sp_callbacks = {.sound = sp_sound,
                                                              .particle = sp_particle};

static bool sp_empty_and_arguments(sp_fixture *s) {
    bool ok = false;
    mc_world_runtime *w = s->base->worlds[0];
    sp_receiver *r = &s->receivers[0];
    CHECK(w->fields.u->count == 0 && sp_fresh(s));
    const sp_random_state random = sp_random_take(s);
    const size_t bindings = w->persistent_bindings;
    for (size_t i = 0; i < 3; ++i) {
        CHECK(sp_begin(s, w, SP_SOUND));
        const unsigned saved = _mm_getcsr();
        _mm_setcsr(saved | 0x8000u);
        const mc_world_storage_status status = mc_world_runtime_entity_sound_in_execution(
            &s->execution, w, NULL, NULL, 1, 1, s->sound);
        _mm_setcsr(saved);
        CHECK(status == MC_WORLD_STORAGE_OK && s->sound->status == MC_WORLD_STORAGE_OK &&
              s->execution.receipt_count == 0 && s->execution.world_open &&
              mc_entity_tick_execution_end_world(&s->execution, false) == MC_WORLD_STORAGE_OK &&
              s->execution.lease_count == 0 && w->persistent_bindings == bindings);
    }
    r->mode = SP_RETURN;
    r->calls = 0;
    CHECK(mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_entity_sound(w, s->actor, s->name, .75f, 1.25f, s->sound) ==
              MC_WORLD_STORAGE_OK &&
          r->calls == 1 && !r->bad && r->self->active_holds == 0 && sp_unsubscribe(s, r->self));
    const sp_event legacy_sound = r->events[0];
    r->calls = 0;
    r->move_once = true;
    const mc_entity_base_fields before = s->actor->base;
    CHECK(mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          sp_begin(s, w, SP_SOUND));
    CHECK(sp_call(s, SP_SOUND, &s->execution, w, true) == MC_WORLD_STORAGE_OK && r->calls == 2 &&
          !r->bad && r->events[0].name == s->name && sp_double(r->events[0].numbers[0], before.x) &&
          sp_double(r->events[0].numbers[1], before.y - (double)before.offset) &&
          sp_double(r->events[0].numbers[2], before.z) && r->events[0].volume == .75f &&
          r->events[0].pitch == 1.25f && legacy_sound.name == r->events[0].name &&
          sp_double(legacy_sound.numbers[0], r->events[0].numbers[0]) &&
          sp_double(legacy_sound.numbers[1], r->events[0].numbers[1]) &&
          sp_double(legacy_sound.numbers[2], r->events[0].numbers[2]) &&
          sp_double(r->events[1].numbers[0], s->actor->base.x) &&
          sp_double(r->events[1].numbers[1], s->actor->base.y - (double)s->actor->base.offset) &&
          sp_double(r->events[1].numbers[2], s->actor->base.z) &&
          mc_entity_tick_execution_end_world(&s->execution, false) == MC_WORLD_STORAGE_OK &&
          r->self->active_holds == 0 && r->self->memberships == 2 && sp_random_same(s, &random));
    CHECK(sp_unsubscribe(s, r->self));
    r->calls = 0;
    CHECK(mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_particle(w, s->name, .5, -0.0, 7.25, -.125, 0.0, 3.0, s->particle) ==
              MC_WORLD_STORAGE_OK &&
          r->calls == 1 && !r->bad && sp_unsubscribe(s, r->self));
    const sp_event legacy_particle = r->events[0];
    r->calls = 0;
    r->remove_self = true;
    CHECK(mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          sp_begin(s, w, SP_PARTICLE));
    CHECK(sp_call(s, SP_PARTICLE, &s->execution, w, true) == MC_WORLD_STORAGE_OK && r->calls == 1 &&
          r->removed && r->free_during == MC_WORLD_STORAGE_BUSY && r->self->memberships == 1 &&
          r->events[0].name == s->name && sp_double(r->events[0].numbers[0], .5) &&
          sp_double(r->events[0].numbers[1], -0.0) && sp_double(r->events[0].numbers[2], 7.25) &&
          sp_double(r->events[0].numbers[3], -.125) && sp_double(r->events[0].numbers[4], 0.0) &&
          sp_double(r->events[0].numbers[5], 3.0) && legacy_particle.name == r->events[0].name &&
          sp_double(legacy_particle.numbers[0], r->events[0].numbers[0]) &&
          sp_double(legacy_particle.numbers[1], r->events[0].numbers[1]) &&
          sp_double(legacy_particle.numbers[2], r->events[0].numbers[2]) &&
          sp_double(legacy_particle.numbers[3], r->events[0].numbers[3]) &&
          sp_double(legacy_particle.numbers[4], r->events[0].numbers[4]) &&
          sp_double(legacy_particle.numbers[5], r->events[0].numbers[5]) &&
          mc_entity_tick_execution_end_world(&s->execution, false) == MC_WORLD_STORAGE_OK &&
          sp_random_same(s, &random) && sp_unsubscribe(s, r->self));
    CHECK(mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          sp_begin(s, w, SP_SOUND));
    const size_t calls = r->calls;
    CHECK(mc_world_runtime_entity_sound_in_execution(&s->execution, w, NULL, s->name, 1, 1,
                                                     s->sound) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          s->sound->bytecode == 33 && r->calls == calls && r->self->active_holds == 0 &&
          mc_entity_tick_execution_end_world(&s->execution, true) == MC_WORLD_STORAGE_OK &&
          sp_fresh(s) && sp_begin(s, w, SP_SOUND));
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(saved | 0x8000u);
    const mc_world_storage_status bad_fp = sp_call(s, SP_SOUND, &s->execution, w, true);
    _mm_setcsr(saved);
    CHECK(bad_fp == MC_WORLD_STORAGE_UNSUPPORTED_STATE && s->sound->bytecode == 44 &&
          r->calls == calls && r->self->active_holds == 0 && sp_unsubscribe(s, r->self) &&
          mc_entity_tick_execution_end_world(&s->execution, true) == MC_WORLD_STORAGE_OK &&
          sp_fresh(s) && sp_begin(s, NULL, SP_SOUND));
    CHECK(sp_call(s, SP_SOUND, &s->execution, NULL, true) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          !s->sound->world_access_failure.source_null_receiver &&
          mc_entity_tick_execution_end_world(&s->execution, true) == MC_WORLD_STORAGE_OK &&
          sp_random_same(s, &random));
    ok = true;
cleanup:
    return ok;
}

static bool sp_failures(sp_fixture *s) {
    bool ok = false;
    mc_world_runtime *w = s->base->worlds[0];
    sp_receiver *r = &s->receivers[0];
    for (int kind = SP_SOUND; kind <= SP_PARTICLE; ++kind) {
        for (int native = 0; native < 2; ++native) {
            CHECK(sp_fresh(s));
            r->calls = 0;
            r->mode = native ? SP_NATIVE : SP_SOURCE;
            r->remove_self = true;
            r->removed = false;
            r->saturated = !native;
            const size_t bindings = w->persistent_bindings;
            CHECK(mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
                  sp_begin(s, w, (enum sp_kind)kind));
            const sp_random_state random = sp_random_take(s);
            const bool output = kind == SP_SOUND ? !native : native != 0;
            const mc_world_storage_status status =
                sp_call(s, (enum sp_kind)kind, &s->execution, w, output);
            sp_restore_counter(r); /* Correct the authored counter before CHECK/cleanup. */
            const mc_world_access_failure *view = NULL;
            const char *exception = NULL;
            CHECK(status == (native ? MC_WORLD_STORAGE_LIMIT : MC_WORLD_STORAGE_IO_ERROR) &&
                  r->calls == 1 && !r->bad && r->removed &&
                  r->free_during == MC_WORLD_STORAGE_BUSY && r->self->memberships == 0 &&
                  r->self->active_holds == 1 && r->receipt != NULL && r->receipt->holds == 1 &&
                  (!native ? r->projected_holds == SIZE_MAX : true) &&
                  s->execution.receipt_count == 1 && !s->execution.capture_pending &&
                  mc_world_access_failure_lease_inspect(
                      &s->execution.leases[s->execution.current_lease], &view) &&
                  view->receiver == r->self && view->receipt == r->receipt &&
                  mc_world_access_failure_source_exception(view, &exception) == !native &&
                  (native || strcmp(exception, "java/lang/IllegalStateException") == 0) &&
                  mc_world_access_receipt_retire(&r->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
                  mc_world_access_free(r->self, NULL) == MC_WORLD_STORAGE_BUSY &&
                  sp_random_same(s, &random));
            if (output) {
                const mc_world_access_failure *actual = kind == SP_SOUND
                                                            ? &s->sound->world_access_failure
                                                            : &s->particle->world_access_failure;
                CHECK(actual->receipt == r->receipt && actual->receiver == r->self);
            }
            const sp_scope scope = sp_scope_take(s);
            const size_t calls = r->calls;
            const mc_world_storage_error first =
                !output            ? s->execution.leases[s->execution.current_lease].callback_error
                : kind == SP_SOUND ? s->sound->cause
                                   : s->particle->cause;
            CHECK(first.status == status && first.operation != NULL && first.system_error == 71);
            CHECK((kind == SP_SOUND
                       ? mc_world_runtime_entity_sound_in_execution(&s->execution, w, s->actor,
                                                                    s->name, 1, 1, s->probe_sound)
                       : mc_world_runtime_particle_in_execution(&s->execution, w, s->name, 0, 0, 0,
                                                                0, 0, 0, s->probe_particle)) ==
                      MC_WORLD_STORAGE_BUSY &&
                  sp_scope_same(s, &scope) && r->calls == calls && r->self->active_holds == 1 &&
                  (kind == SP_SOUND ? s->probe_sound->world_access_failure.receipt == NULL
                                    : s->probe_particle->world_access_failure.receipt == NULL));
            view = NULL; /* Consume no producer while any selected outer borrow survives. */
            sp_clear_views(s);
            CHECK(mc_entity_tick_execution_end_world(&s->execution, true) == MC_WORLD_STORAGE_OK);
            const size_t active = r->self->active_holds;
            r->self->active_holds = 0; /* Explicit bounded native metadata fault. */
            const mc_world_storage_status refused =
                mc_entity_tick_execution_release_receipts(&s->execution);
            r->self->active_holds = active;
            CHECK(refused == MC_WORLD_STORAGE_UNSUPPORTED_STATE && s->execution.world_held &&
                  s->execution.lease_count == 1 && r->self->active_holds == 1 &&
                  r->receipt->holds == 1 &&
                  mc_entity_tick_execution_release_receipts(&s->execution) == MC_WORLD_STORAGE_OK &&
                  s->execution.receipt_count == 0 && s->execution.lease_count == 0 &&
                  s->execution.world_held && r->self->active_holds == 0 &&
                  mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
                  mc_world_access_receipt_retire(&r->receipt, NULL) == MC_WORLD_STORAGE_OK &&
                  r->receipt == NULL && r->calls == calls && w->persistent_bindings == bindings &&
                  w->last_error.status == first.status &&
                  w->last_error.operation == first.operation &&
                  w->last_error.system_error == first.system_error && sp_random_same(s, &random));
            if (output) {
                const mc_world_storage_error *after =
                    kind == SP_SOUND ? &s->sound->cause : &s->particle->cause;
                CHECK(after->status == first.status && after->operation == first.operation &&
                      after->system_error == first.system_error);
            }
        }
    }
    /* Force only the native view-count invariant while its actual lease
     * remains owned. Restore that injected counter, not a source pointer or
     * a produced failure/phase, before CHECK. Original callback cause wins. */
    CHECK(sp_fresh(s));
    r->mode = SP_SOURCE;
    r->calls = 0;
    r->inject_extent = true;
    CHECK(mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          sp_begin(s, w, SP_SOUND));
    const mc_world_storage_status extent_status = sp_call(s, SP_SOUND, &s->execution, w, true);
    sp_restore_counter(r);
    CHECK(extent_status == MC_WORLD_STORAGE_IO_ERROR &&
          r->projected_views == MC_ENTITY_TICK_EXECUTION_RECEIPTS && r->original_views == 0 &&
          s->sound->cause.status == MC_WORLD_STORAGE_IO_ERROR &&
          s->sound->cause.system_error == 71 &&
          s->sound->world_access_failure.receipt == r->receipt &&
          s->execution.capture.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          s->execution.first.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !s->execution.capture_pending && s->execution.phase == MC_ENTITY_TICK_EXECUTION_FAILED &&
          r->receipt->holds == 1 && r->self->active_holds == 1 && sp_unsubscribe(s, r->self));
    sp_clear_views(s);
    CHECK(mc_entity_tick_execution_end_world(&s->execution, true) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
          s->execution.first.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          s->sound->cause.status == MC_WORLD_STORAGE_IO_ERROR &&
          mc_world_access_receipt_retire(&r->receipt, NULL) == MC_WORLD_STORAGE_OK);
    /* A real foreign receipt is malformed output for this invocation. The
     * handoff rejects it without adopting that producer or publishing a view. */
    mc_world_access_failure foreign = {0};
    sp_receiver *other = &s->receivers[1];
    CHECK(mc_world_access_receipt_create(other->self, NULL, &other->receipt, &foreign, NULL) ==
              MC_WORLD_STORAGE_OK &&
          foreign.receiver == other->self && foreign.receipt == other->receipt && sp_fresh(s));
    r->mode = SP_FOREIGN;
    r->calls = 0;
    CHECK(mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          sp_begin(s, w, SP_PARTICLE) &&
          sp_call(s, SP_PARTICLE, &s->execution, w, true) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          s->particle->world_access_failure.receipt == NULL &&
          s->particle->world_access_failure.receiver == NULL && other->receipt->holds == 1 &&
          r->self->active_holds == 0 && other->self->active_holds == 0 &&
          s->execution.receipt_count == 0 && sp_unsubscribe(s, r->self) &&
          mc_entity_tick_execution_end_world(&s->execution, true) == MC_WORLD_STORAGE_OK);
    foreign = (mc_world_access_failure){0};
    CHECK(mc_world_access_receipt_retire(&other->receipt, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(sp_fresh(s));
    r->mode = SP_ADAPTER;
    r->calls = 0;
    CHECK(mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          sp_begin(s, w, SP_SOUND) &&
          sp_call(s, SP_SOUND, &s->execution, w, true) == MC_WORLD_STORAGE_IO_ERROR &&
          s->sound->world_access_failure.receipt == NULL && s->execution.receipt_count == 0 &&
          r->self->active_holds == 0 && sp_unsubscribe(s, r->self) &&
          mc_entity_tick_execution_end_world(&s->execution, true) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool sp_reentry_and_admission(sp_fixture *s) {
    bool ok = false;
    mc_entity_tick_execution *copy = malloc(sizeof(*copy));
    mc_world_runtime *w = s->base->worlds[0];
    sp_receiver *r = &s->receivers[0];
    CHECK(copy != NULL && sp_fresh(s));
    const sp_random_state random = sp_random_take(s);
    for (int kind = SP_SOUND; kind <= SP_PARTICLE; ++kind) {
        r->mode = SP_RETURN;
        r->calls = 0;
        r->reenter = true;
        r->scope_preserved = false;
        CHECK(mc_world_runtime_add_world_access(w, r->self, NULL) == MC_WORLD_STORAGE_OK &&
              sp_begin(s, w, (enum sp_kind)kind));
        const sp_scope scope = sp_scope_take(s);
        CHECK(sp_call(s, (enum sp_kind)kind, &s->execution, w, true) == MC_WORLD_STORAGE_OK &&
              r->calls == 1 && !r->bad && r->inner_status == MC_WORLD_STORAGE_BUSY &&
              r->end_false == MC_WORLD_STORAGE_BUSY && r->end_true == MC_WORLD_STORAGE_BUSY &&
              r->retire_active == MC_WORLD_STORAGE_BUSY && r->scope_preserved &&
              sp_scope_same(s, &scope) && r->self->active_holds == 0 &&
              mc_entity_tick_execution_end_world(&s->execution, false) == MC_WORLD_STORAGE_OK &&
              sp_unsubscribe(s, r->self));
    }
    CHECK(sp_begin(s, w, SP_SOUND));
    const sp_scope scope = sp_scope_take(s);
    const mc_world_storage_error first_world = w->last_error;
    const mc_world_storage_error other_world = s->base->worlds[2]->last_error;
    const size_t other_bindings = s->base->worlds[2]->persistent_bindings;
    const mc_java_random other_random = *s->base->worlds[2]->fields.r;
    CHECK(sp_call(s, SP_SOUND, &s->execution, s->base->worlds[2], true) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          sp_scope_same(s, &scope) && w->last_error.status == first_world.status &&
          w->last_error.operation == first_world.operation &&
          s->base->worlds[2]->last_error.status == other_world.status &&
          s->base->worlds[2]->last_error.operation == other_world.operation &&
          s->base->worlds[2]->last_error.system_error == other_world.system_error &&
          s->base->worlds[2]->persistent_bindings == other_bindings &&
          s->base->worlds[2]->active_calls == 0 && w->active_calls == 0 &&
          mc_test_java_random_equal(s->base->worlds[2]->fields.r, &other_random) &&
          sp_call(s, SP_PARTICLE, &s->execution, NULL, true) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          sp_scope_same(s, &scope));
    memcpy(copy, &s->execution, sizeof(*copy)); /* Invalid copied native envelope owns NO right. */
    CHECK(sp_call(s, SP_SOUND, copy, w, true) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          sp_scope_same(s, &scope) &&
          sp_call(s, SP_PARTICLE, NULL, w, true) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          s->particle->world_access_failure.receipt == NULL && sp_scope_same(s, &scope) &&
          mc_entity_tick_execution_end_world(&s->execution, false) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
          sp_call(s, SP_SOUND, &s->execution, w, true) != MC_WORLD_STORAGE_OK &&
          s->sound->world_access_failure.receipt == NULL && sp_random_same(s, &random));
    ok = true;
cleanup:
    free(copy); /* Never retire/capture from the unowned copy. */
    return ok;
}

static bool sp_null_interface(sp_fixture *s) {
    bool ok = false, registered = false;
    mc_world_runtime *w = s->base->worlds[0];
    CHECK(sp_fresh(s) && mc_world_runtime_add_world_access(w, NULL, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    CHECK(sp_begin(s, w, SP_SOUND) &&
          sp_call(s, SP_SOUND, &s->execution, w, true) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          s->sound->world_access_bytecode == 53 &&
          s->sound->world_access_failure.source_null_receiver &&
          s->sound->world_access_failure.receipt == NULL && s->execution.receipt_count == 0 &&
          mc_entity_tick_execution_end_world(&s->execution, true) == MC_WORLD_STORAGE_OK);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(w, NULL, &removed, NULL) == MC_WORLD_STORAGE_OK &&
          removed);
    registered = false;
    ok = true;
cleanup:
    if (registered) {
        bool removed_cleanup = false;
        if (mc_world_runtime_remove_world_access(w, NULL, &removed_cleanup, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed_cleanup)
            ok = false;
    }
    return ok;
}

static bool sp_actual_n(sp_fixture *s) {
    bool ok = false;
    mc_world_runtime *w = s->base->worlds[0];
    CHECK(sp_fresh(s) && w->fields.u->count == 0);
    const size_t bindings = w->persistent_bindings;
    CHECK(mc_client_render_engine_classes_create(NULL, &s->render_classes) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_global_allocate(s->render_classes, NULL, &s->renderer,
                                           s->render_error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          s->renderer->fields.t == NULL &&
          mc_render_global_access_create(s->renderer, &s->renderer->access, s->render_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_source_add(s->renderer->access, w, s->render_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    mc_world_access *identity = w->world_accesses.entries[0];
    const sp_random_state random = sp_random_take(s);
    CHECK(sp_begin(s, w, SP_SOUND) &&
          sp_call(s, SP_SOUND, &s->execution, w, true) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          s->sound->world_access_failure.receiver == identity &&
          s->sound->world_access_failure.render_global != NULL &&
          s->sound->world_access_failure.render_global->source_pc == 22 &&
          s->sound->world_access_failure.render_global->status ==
              MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          !s->sound->world_access_failure.render_global->native_policy &&
          identity->active_holds == 1 && s->sound->world_access_failure.receipt->holds == 1 &&
          s->execution.receipt_count == 1 && !s->execution.capture_pending);
    const char *exception = NULL;
    CHECK(mc_world_access_failure_source_exception(&s->sound->world_access_failure, &exception) &&
          strcmp(exception, "java/lang/NullPointerException") == 0 &&
          mc_render_global_access_detach_all(s->renderer->access, s->render_error) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          identity->active_holds == 1 && w->fields.u->count == 1);
    const mc_world_storage_error first = s->sound->cause;
    sp_clear_views(s);
    CHECK(mc_entity_tick_execution_end_world(&s->execution, true) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
          identity->active_holds == 0 && s->sound->cause.status == first.status &&
          s->sound->cause.operation == first.operation &&
          mc_render_global_access_detach_all(s->renderer->access, s->render_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          w->fields.u->count == 0 &&
          mc_render_global_access_retire(&s->renderer->access, s->render_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          w->persistent_bindings == bindings && sp_random_same(s, &random));
    /* Actual particle guard returns with this genuine initial N graph. A
     * deeper N particle failure needs real App/i/effect_renderer and is OPEN. */
    CHECK(mc_render_global_access_create(s->renderer, &s->renderer->access, s->render_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_source_add(s->renderer->access, w, s->render_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          sp_fresh(s) && sp_begin(s, w, SP_PARTICLE));
    identity = w->world_accesses.entries[0];
    CHECK(sp_call(s, SP_PARTICLE, &s->execution, w, true) == MC_WORLD_STORAGE_OK &&
          identity->active_holds == 0 && s->execution.receipt_count == 0 &&
          mc_entity_tick_execution_end_world(&s->execution, false) == MC_WORLD_STORAGE_OK &&
          w->persistent_bindings == bindings + 1 && sp_random_same(s, &random));
    ok = true;
cleanup:
    return ok;
}

static bool sp_setup(sp_fixture *s, const char *root) {
    bool ok = false;
    fixture *f = s->base;
    const uint16_t title[] = {'C', 'h', 'i', 'l', 'd'};
    s->sound = malloc(sizeof(*s->sound));
    s->probe_sound = malloc(sizeof(*s->probe_sound));
    s->particle = malloc(sizeof(*s->particle));
    s->probe_particle = malloc(sizeof(*s->probe_particle));
    s->render_error = calloc(1, sizeof(*s->render_error));
    CHECK(s->sound != NULL && s->probe_sound != NULL && s->particle != NULL &&
          s->probe_particle != NULL && s->render_error != NULL);
    mc_world_movement_error_init(s->sound);
    mc_world_movement_error_init(s->probe_sound);
    mc_world_mutation_error_init(s->particle);
    mc_world_mutation_error_init(s->probe_particle);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK &&
          mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK &&
              mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
                  MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
    }
    CHECK(open_store(f, 0, root) && create_world(f, 0) && open_store(f, 2, root) &&
          create_world(f, 2) && make_entity(f, 0, f->worlds[0], 1.5, 9.75, -2.25, &s->actor));
    mc_registry_error registry;
    CHECK(mc_registry_runtime_literal_ascii(f->registry[0], "execution.child", &s->name,
                                            &registry) == MC_REGISTRY_OK &&
          sp_fresh(s));
    for (size_t i = 0; i < COUNT(s->receivers); ++i) {
        s->receivers[i].fixture = s;
        CHECK(mc_world_access_source_create(&sp_callbacks, &s->receivers[i], &s->receivers[i].self,
                                            NULL) == MC_WORLD_STORAGE_OK);
    }
    ok = true;
cleanup:
    return ok;
}

static bool sp_cleanup(sp_fixture *s) {
    if (s == NULL)
        return true;
    for (size_t i = 0; i < COUNT(s->receivers); ++i)
        sp_restore_counter(&s->receivers[i]);
    sp_clear_views(s);
    if (s->execution_initialized) {
        if (s->execution.world_open &&
            mc_entity_tick_execution_end_world(&s->execution, true) != MC_WORLD_STORAGE_OK)
            return false;
        if (mc_entity_tick_execution_retire(&s->execution) != MC_WORLD_STORAGE_OK)
            return false;
    }
    for (size_t i = 0; i < COUNT(s->receivers); ++i) {
        sp_receiver *r = &s->receivers[i];
        if (!sp_unsubscribe(s, r->self) ||
            mc_world_access_receipt_retire(&r->receipt, NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_access_free(r->self, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        r->self = NULL;
    }
    if (s->renderer != NULL && s->renderer->access != NULL &&
        (mc_render_global_access_detach_all(s->renderer->access, s->render_error) !=
             MC_CLIENT_RENDER_GLOBAL_OK ||
         mc_render_global_access_retire(&s->renderer->access, s->render_error) !=
             MC_CLIENT_RENDER_GLOBAL_OK))
        return false;
    if (s->renderer != NULL && mc_client_render_global_release(&s->renderer, s->render_error) !=
                                   MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    if (s->render_classes != NULL &&
        mc_client_render_engine_classes_release(s->render_classes) != MC_CLIENT_RENDER_ENGINE_OK)
        return false;
    s->render_classes = NULL;
    /* Retire the actual unpublished caller occurrences BEFORE World teardown;
     * no source field/pointer is cleared to make checked retirement pass. */
    if (s->base != NULL) {
        for (size_t i = 0; i < s->base->entity_count; ++i) {
            mc_entity_release(s->base->entities[i]);
            s->base->entities[i] = NULL;
        }
        s->actor = NULL;
    }
    if (!cleanup_fixture(s->base))
        return false;
    free(s->sound);
    free(s->probe_sound);
    free(s->particle);
    free(s->probe_particle);
    free(s->render_error);
    free(s);
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    sp_fixture *s = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "sound-particle") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    s = calloc(1, sizeof(*s));
    CHECK(s != NULL);
    s->base = calloc(1, sizeof(*s->base));
    CHECK(s->base != NULL && sp_setup(s, root));
    puts("EXECUTION_CHILD_CASE id=0");
    CHECK(sp_empty_and_arguments(s));
    puts("EXECUTION_CHILD_CASE id=1");
    CHECK(sp_failures(s));
    puts("EXECUTION_CHILD_CASE id=2");
    CHECK(sp_reentry_and_admission(s));
    puts("EXECUTION_CHILD_CASE id=3");
    CHECK(sp_null_interface(s));
    puts("EXECUTION_CHILD_CASE id=4");
    CHECK(sp_actual_n(s));
    result = 0;
cleanup:
    if (!sp_cleanup(s)) {
        fputs("Execution children cleanup refused; actual remaining graph retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u actual World execution-child checks;5 groups\n", checks) < 0)
        result = 1;
    return result;
}
