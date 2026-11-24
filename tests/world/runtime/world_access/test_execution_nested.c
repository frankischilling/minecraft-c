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
    mc_world_runtime *expected_world;
    bool remove_self, removed, move_once, reenter, bad, saturated, inject_extent, extent_projected;
    bool nested;
    mc_world_storage_status nested_push, nested_child, nested_pop;
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
    size_t current, leases, views, bindings, worlds, loaders;
    unsigned char slots[sizeof(((mc_entity_tick_execution *)0)->world_slots)];
    unsigned char nodes[sizeof(((mc_entity_tick_execution *)0)->leases)];
    unsigned char used[sizeof(((mc_entity_tick_execution *)0)->lease_used)];
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
    sp_scope value = {.world = e->world,
                      .math = e->math,
                      .phase = e->phase,
                      .first = e->first,
                      .capture = e->capture,
                      .current = e->current_lease,
                      .leases = e->lease_count,
                      .views = e->receipt_count,
                      .bindings = e->world == NULL ? 0 : e->world->persistent_bindings,
                      .worlds = e->world_count,
                      .loaders = e->loader_count,
                      .held = e->world_held,
                      .open = e->world_open,
                      .failed = e->child_failed,
                      .pending = e->capture_pending};
    memcpy(value.slots, e->world_slots, sizeof(value.slots));
    memcpy(value.nodes, e->leases, sizeof(value.nodes));
    memcpy(value.used, e->lease_used, sizeof(value.used));
    return value;
}

static bool sp_scope_same(const sp_fixture *s, const sp_scope *before) {
    const sp_scope now = sp_scope_take(s);
    return now.world == before->world && now.math == before->math && now.phase == before->phase &&
           now.current == before->current && now.leases == before->leases &&
           now.views == before->views && now.bindings == before->bindings &&
           now.worlds == before->worlds && now.loaders == before->loaders &&
           memcmp(now.slots, before->slots, sizeof(now.slots)) == 0 &&
           memcmp(now.nodes, before->nodes, sizeof(now.nodes)) == 0 &&
           memcmp(now.used, before->used, sizeof(now.used)) == 0 && now.held == before->held &&
           now.open == before->open && now.failed == before->failed &&
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
    for (size_t i = 0; i < WORLD_COUNT; ++i) {
        if (s->base->worlds[i] == NULL)
            continue;
        bool removed;
        do {
            removed = false;
            if (mc_world_runtime_remove_world_access(s->base->worlds[i], access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
        } while (removed);
    }
    return true;
}

static bool sp_fresh(sp_fixture *s) {
    sp_clear_views(s);
    if (s->execution_initialized) {
        while (s->execution.world_open)
            if (mc_entity_tick_execution_end_world(&s->execution, true) != MC_WORLD_STORAGE_OK)
                return false;
        if (mc_entity_tick_execution_retire(&s->execution) != MC_WORLD_STORAGE_OK)
            return false;
    }
    mc_entity_tick_execution_init(&s->execution, NULL);
    s->execution_initialized = true;
    return true;
}

static mc_world_storage_status sp_observe(sp_receiver *r, mc_world_runtime *world,
                                          enum sp_kind kind, mc_world_storage_error *error,
                                          mc_world_access_failure *failure) {
    sp_fixture *s = r->fixture;
    const size_t index = r->calls++;
    if (index >= COUNT(r->events) || world != r->expected_world || world->active_calls == 0 ||
        r->self->active_holds == 0) {
        r->bad = true;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "execution child observer", 0};
        return error->status;
    }
    r->events[index].kind = kind;
    if (r->nested) {
        r->nested = false;
        mc_entity_tick_execution *e = &s->execution;
        const size_t parent = e->current_lease;
        const size_t worlds = e->world_count, leases = e->lease_count;
        mc_world_runtime *b = s->base->worlds[2];
        const size_t ab = world->persistent_bindings, bb = b->persistent_bindings;
        unsigned char parent_slot[sizeof(e->world_slots[parent])];
        unsigned char parent_node[sizeof(e->leases[parent])];
        memcpy(parent_slot, &e->world_slots[parent], sizeof(parent_slot));
        memcpy(parent_node, &e->leases[parent], sizeof(parent_node));
        mc_entity_tick_execution_world_token inner = {0};
        r->nested_push = mc_entity_tick_execution_push_world(
            e, b, (mc_entity_tick_execution_site){"native.callback", "nested", "()V", 1}, &inner);
        r->nested_child = MC_WORLD_STORAGE_BUSY;
        r->nested_pop = MC_WORLD_STORAGE_BUSY;
        if (r->nested_push == MC_WORLD_STORAGE_OK) {
            r->nested_child =
                kind == SP_SOUND
                    ? mc_world_runtime_particle_in_execution(e, b, s->name, .5, -0.0, 7.25, -.125,
                                                             0, 3, s->probe_particle)
                    : mc_world_runtime_entity_sound_in_execution(e, b, s->actor, s->name, .75f,
                                                                 1.25f, s->probe_sound);
            r->nested_pop = mc_entity_tick_execution_pop_world(
                e, &inner, r->nested_child != MC_WORLD_STORAGE_OK);
        }
        r->scope_preserved =
            e->world == world && e->world_open && e->current_lease == parent &&
            e->world_count == worlds && e->lease_count == leases &&
            world->persistent_bindings == ab && b->persistent_bindings == bb &&
            memcmp(parent_slot, &e->world_slots[parent], sizeof(parent_slot)) == 0 &&
            memcmp(parent_node, &e->leases[parent], sizeof(parent_node)) == 0;
    }
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
        s->receivers[i].expected_world = f->worlds[0];
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
        while (s->execution.world_open)
            if (mc_entity_tick_execution_end_world(&s->execution, true) != MC_WORLD_STORAGE_OK)
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

/* New native OWNER controls. These source sites identify the component input;
 * they do not claim an SL/Chunk activation parent was executed. */
static mc_entity_tick_execution_site np_site(int pc) {
    return (mc_entity_tick_execution_site){"native.nested.owner", "control", "()V", pc};
}

static bool np_token_equal(const mc_entity_tick_execution_world_token *a,
                           const mc_entity_tick_execution_world_token *b) {
    return a->owner == b->owner && a->slot == b->slot && a->generation == b->generation;
}

static bool np_success(sp_fixture *s) {
    bool ok = false;
    mc_entity_tick_execution *e = &s->execution;
    mc_world_runtime *a = s->base->worlds[0], *distinct = s->base->worlds[2];
    sp_receiver *r = &s->receivers[0];
    CHECK(sp_fresh(s));
    for (int kind_index = SP_SOUND; kind_index <= SP_PARTICLE; ++kind_index) {
        const enum sp_kind kind = (enum sp_kind)kind_index;
        for (int alias = 0; alias < 2; ++alias) {
            mc_world_runtime *b = alias ? a : distinct;
            const size_t ab = a->persistent_bindings, bb = b->persistent_bindings;
            const sp_random_state random = sp_random_take(s);
            mc_entity_tick_execution_world_token outer = {0}, inner = {0}, current = {0};
            mc_world_runtime *selected = NULL;
            r->calls = 0;
            r->mode = SP_RETURN;
            r->expected_world = b;
            CHECK(mc_world_runtime_add_world_access(b, r->self, NULL) == MC_WORLD_STORAGE_OK &&
                  mc_entity_tick_execution_push_world(e, a, np_site(1), &outer) ==
                      MC_WORLD_STORAGE_OK &&
                  mc_entity_tick_execution_push_world(e, b, np_site(2), &inner) ==
                      MC_WORLD_STORAGE_OK &&
                  outer.slot == 0 && inner.slot == 1 && e->world_count == 2 &&
                  e->lease_count == 2 && a->persistent_bindings == ab + (alias ? 2u : 1u) &&
                  b->persistent_bindings == bb + (alias ? 2u : 1u) &&
                  mc_entity_tick_execution_read_world(e, &outer, &selected) && selected == a &&
                  mc_entity_tick_execution_current_world(e, &current) &&
                  np_token_equal(&current, &inner) && e->world == b && e->world_open &&
                  sp_call(s, kind, e, b, kind == SP_SOUND) == MC_WORLD_STORAGE_OK &&
                  r->calls == 1 && !r->bad && r->self->active_holds == 0 &&
                  r->events[0].kind == kind && r->events[0].name == s->name);
            if (kind == SP_SOUND)
                CHECK(sp_double(r->events[0].numbers[0], 1.5) &&
                      sp_double(r->events[0].numbers[1], 9.75 - (double)s->actor->base.offset) &&
                      sp_double(r->events[0].numbers[2], -2.25) && r->events[0].volume == .75f &&
                      r->events[0].pitch == 1.25f);
            else
                CHECK(sp_double(r->events[0].numbers[0], .5) &&
                      sp_double(r->events[0].numbers[1], -0.0) &&
                      sp_double(r->events[0].numbers[2], 7.25) &&
                      sp_double(r->events[0].numbers[3], -.125) &&
                      sp_double(r->events[0].numbers[4], 0.0) &&
                      sp_double(r->events[0].numbers[5], 3.0));
            CHECK(sp_unsubscribe(s, r->self) &&
                  mc_entity_tick_execution_pop_world(e, &inner, false) == MC_WORLD_STORAGE_OK &&
                  e->world_count == 1 && e->lease_count == 1 && e->world == a &&
                  a->persistent_bindings == ab + 1u && (alias || b->persistent_bindings == bb) &&
                  mc_entity_tick_execution_current_world(e, &current) &&
                  np_token_equal(&current, &outer));
            selected = distinct;
            CHECK(!mc_entity_tick_execution_read_world(e, &inner, &selected) &&
                  selected == distinct &&
                  mc_entity_tick_execution_pop_world(e, &outer, false) == MC_WORLD_STORAGE_OK &&
                  e->world_count == 0 && e->lease_count == 0 && !e->world_open &&
                  a->persistent_bindings == ab && b->persistent_bindings == bb &&
                  sp_random_same(s, &random));
        }
    }
    CHECK(mc_entity_tick_execution_retire(e) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool np_sparse_failure(sp_fixture *s) {
    bool ok = false;
    mc_entity_tick_execution *e = &s->execution;
    mc_world_runtime *a = s->base->worlds[0], *distinct = s->base->worlds[2];
    sp_receiver *r = &s->receivers[0];
    for (int kind_index = SP_SOUND; kind_index <= SP_PARTICLE; ++kind_index) {
        const enum sp_kind kind = (enum sp_kind)kind_index;
        for (int alias = 0; alias < 2; ++alias) {
            for (int native = 0; native < 2; ++native) {
                CHECK(sp_fresh(s));
                mc_world_runtime *b = alias ? a : distinct;
                const size_t ab = a->persistent_bindings, bb = b->persistent_bindings;
                const sp_random_state random = sp_random_take(s);
                mc_entity_tick_execution_world_token outer = {0}, inner = {0};
                r->expected_world = b;
                r->calls = 0;
                r->mode = native ? SP_NATIVE : SP_SOURCE;
                r->remove_self = true;
                r->removed = false;
                r->saturated = !native;
                CHECK(mc_world_runtime_add_world_access(b, r->self, NULL) == MC_WORLD_STORAGE_OK &&
                      mc_entity_tick_execution_push_world(e, a, np_site(10), &outer) ==
                          MC_WORLD_STORAGE_OK &&
                      mc_entity_tick_execution_push_world(e, b, np_site(11), &inner) ==
                          MC_WORLD_STORAGE_OK);
                const bool output = kind == SP_SOUND ? !native : native != 0;
                const mc_world_storage_status returned = sp_call(s, kind, e, b, output);
                sp_restore_counter(r); /* Authored receipt counter only, before any CHECK. */
                const mc_world_access_failure *view = NULL;
                const char *exception = NULL;
                CHECK(returned == (native ? MC_WORLD_STORAGE_LIMIT : MC_WORLD_STORAGE_IO_ERROR) &&
                      outer.slot == 0 && inner.slot == 1 && r->calls == 1 && !r->bad &&
                      r->removed && r->self->memberships == 0 && r->self->active_holds == 1 &&
                      r->free_during == MC_WORLD_STORAGE_BUSY && r->receipt != NULL &&
                      r->receipt->holds == 1 && (native || r->projected_holds == SIZE_MAX) &&
                      !e->capture_pending && e->receipt_count == 1 && e->lease_count == 2 &&
                      !mc_world_access_failure_lease_inspect(&e->leases[outer.slot], &view) &&
                      view == NULL &&
                      mc_world_access_failure_lease_inspect(&e->leases[inner.slot], &view) &&
                      view->receiver == r->self && view->receipt == r->receipt &&
                      mc_world_access_failure_source_exception(view, &exception) == !native &&
                      (native || strcmp(exception, "java/lang/IllegalStateException") == 0));
                const mc_world_storage_error first = e->leases[inner.slot].callback_error;
                CHECK(first.status == returned && first.system_error == 71 &&
                      first.operation != NULL);
                view = NULL;
                sp_clear_views(s);
                CHECK(mc_entity_tick_execution_pop_world(e, &inner, true) == MC_WORLD_STORAGE_OK &&
                      e->world == a && e->world_open && e->current_lease == outer.slot &&
                      e->world_count == 2 && e->lease_count == 2 &&
                      mc_entity_tick_execution_pop_world(e, &outer, true) == MC_WORLD_STORAGE_OK &&
                      !e->world_open && e->world_count == 2 && e->lease_count == 1 &&
                      !e->lease_used[0] && e->lease_used[1] &&
                      mc_world_access_failure_lease_inspect(&e->leases[1], &view) &&
                      view->receipt == r->receipt && r->self->active_holds == 1 &&
                      mc_world_access_receipt_retire(&r->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
                      mc_world_access_free(r->self, NULL) == MC_WORLD_STORAGE_BUSY);
                view = NULL;
                /* A native downward-counter fault must refuse at sparse slot1.
                 * Restore that authored scalar before assertions or retry. */
                const size_t active = r->self->active_holds;
                r->self->active_holds = 0;
                const mc_world_storage_status refused =
                    mc_entity_tick_execution_release_receipts(e);
                r->self->active_holds = active;
                CHECK(refused == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e->lease_used[1] &&
                      e->lease_count == 1 && e->receipt_count == 1 && e->world_count == 2 &&
                      e->phase == MC_ENTITY_TICK_EXECUTION_RETIRING &&
                      a->persistent_bindings == ab + (alias ? 2u : 1u) &&
                      b->persistent_bindings == bb + (alias ? 2u : 1u) &&
                      mc_entity_tick_execution_release_receipts(e) == MC_WORLD_STORAGE_OK &&
                      e->lease_count == 0 && e->receipt_count == 0 && !e->lease_used[1] &&
                      r->self->active_holds == 0 && e->world_count == 2 && r->receipt->holds == 1 &&
                      r->calls == 1 && mc_entity_tick_execution_retire(e) == MC_WORLD_STORAGE_OK &&
                      e->world_count == 0 && a->persistent_bindings == ab &&
                      b->persistent_bindings == bb &&
                      mc_world_access_receipt_retire(&r->receipt, NULL) == MC_WORLD_STORAGE_OK &&
                      b->last_error.status == first.status &&
                      b->last_error.operation == first.operation &&
                      b->last_error.system_error == first.system_error &&
                      sp_random_same(s, &random));
                if (output) {
                    const mc_world_storage_error *after =
                        kind == SP_SOUND ? &s->sound->cause : &s->particle->cause;
                    CHECK(after->status == first.status && after->operation == first.operation &&
                          after->system_error == first.system_error);
                }
            }
        }
    }
    ok = true;
cleanup:
    return ok;
}

static bool np_reentry(sp_fixture *s) {
    bool ok = false;
    mc_entity_tick_execution *e = &s->execution;
    mc_world_runtime *a = s->base->worlds[0], *b = s->base->worlds[2];
    sp_receiver *r = &s->receivers[0];
    for (int kind_index = SP_SOUND; kind_index <= SP_PARTICLE; ++kind_index) {
        const enum sp_kind kind = (enum sp_kind)kind_index;
        CHECK(sp_fresh(s));
        const size_t ab = a->persistent_bindings, bb = b->persistent_bindings;
        const sp_random_state random = sp_random_take(s);
        mc_entity_tick_execution_world_token outer = {0}, inner = {0};
        r->expected_world = b;
        r->mode = SP_RETURN;
        r->calls = 0;
        r->reenter = true;
        CHECK(
            mc_world_runtime_add_world_access(b, r->self, NULL) == MC_WORLD_STORAGE_OK &&
            mc_entity_tick_execution_push_world(e, a, np_site(20), &outer) == MC_WORLD_STORAGE_OK &&
            mc_entity_tick_execution_push_world(e, b, np_site(21), &inner) == MC_WORLD_STORAGE_OK &&
            sp_call(s, kind, e, b, true) == MC_WORLD_STORAGE_OK && r->calls == 1 && !r->bad &&
            r->scope_preserved && r->inner_status == MC_WORLD_STORAGE_BUSY &&
            r->end_false == MC_WORLD_STORAGE_BUSY && r->end_true == MC_WORLD_STORAGE_BUSY &&
            r->retire_active == MC_WORLD_STORAGE_BUSY && e->current_lease == inner.slot &&
            e->world_count == 2 && e->lease_count == 2 &&
            e->phase == MC_ENTITY_TICK_EXECUTION_OPEN && r->self->active_holds == 0 &&
            mc_entity_tick_execution_pop_world(e, &inner, false) == MC_WORLD_STORAGE_OK &&
            mc_entity_tick_execution_pop_world(e, &outer, false) == MC_WORLD_STORAGE_OK &&
            mc_entity_tick_execution_retire(e) == MC_WORLD_STORAGE_OK &&
            sp_unsubscribe(s, r->self) && a->persistent_bindings == ab &&
            b->persistent_bindings == bb && sp_random_same(s, &random));
    }
    /* A genuinely ARMED callback may enter a different evaluated World. Only
     * same-node reuse/close is refused: no blanket active-parent rejection. */
    sp_receiver *child = &s->receivers[1];
    for (int kind_index = SP_SOUND; kind_index <= SP_PARTICLE; ++kind_index) {
        const enum sp_kind kind = (enum sp_kind)kind_index;
        CHECK(sp_fresh(s));
        const size_t ab = a->persistent_bindings, bb = b->persistent_bindings;
        const sp_random_state random = sp_random_take(s);
        mc_entity_tick_execution_world_token outer = {0};
        r->expected_world = a;
        r->mode = SP_RETURN;
        r->calls = 0;
        r->nested = true;
        child->expected_world = b;
        child->mode = SP_RETURN;
        child->calls = 0;
        CHECK(mc_world_runtime_add_world_access(a, r->self, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(b, child->self, NULL) == MC_WORLD_STORAGE_OK &&
              mc_entity_tick_execution_push_world(e, a, np_site(22), &outer) ==
                  MC_WORLD_STORAGE_OK &&
              sp_call(s, kind, e, a, true) == MC_WORLD_STORAGE_OK && r->calls == 1 &&
              child->calls == 1 && !r->bad && !child->bad && r->scope_preserved &&
              r->nested_push == MC_WORLD_STORAGE_OK && r->nested_child == MC_WORLD_STORAGE_OK &&
              r->nested_pop == MC_WORLD_STORAGE_OK && r->self->active_holds == 0 &&
              child->self->active_holds == 0 && e->world_count == 1 && e->lease_count == 1 &&
              r->events[0].kind == kind && child->events[0].kind != kind &&
              mc_entity_tick_execution_pop_world(e, &outer, false) == MC_WORLD_STORAGE_OK &&
              mc_entity_tick_execution_retire(e) == MC_WORLD_STORAGE_OK &&
              sp_unsubscribe(s, r->self) && sp_unsubscribe(s, child->self) &&
              a->persistent_bindings == ab && b->persistent_bindings == bb &&
              sp_random_same(s, &random));
    }
    ok = true;
cleanup:
    return ok;
}

typedef struct np_guard_observation {
    mc_world_storage_status valid, malformed;
    bool valid_preserved, malformed_preserved;
} np_guard_observation;

static bool np_held_push(sp_fixture *s, np_guard_observation *observation) {
    bool ok = false;
    mc_entity_tick_execution *e = &s->execution;
    mc_world_runtime *a = s->base->worlds[0], *b = s->base->worlds[2];
    sp_receiver *r = &s->receivers[0];
    for (int kind_index = SP_SOUND; kind_index <= SP_PARTICLE; ++kind_index) {
        const enum sp_kind kind = (enum sp_kind)kind_index;
        CHECK(sp_fresh(s));
        const size_t ab = a->persistent_bindings, bb = b->persistent_bindings;
        mc_entity_tick_execution_world_token outer = {0}, current = {0};
        const mc_entity_tick_execution_world_token sentinel = {NULL, 713, UINT64_C(173)};
        mc_entity_tick_execution_world_token output = sentinel;
        r->calls = 0;
        r->expected_world = a;
        r->mode = SP_SOURCE;
        r->remove_self = true;
        CHECK(mc_world_runtime_add_world_access(a, r->self, NULL) == MC_WORLD_STORAGE_OK &&
              mc_entity_tick_execution_push_world(e, a, np_site(25), &outer) ==
                  MC_WORLD_STORAGE_OK &&
              sp_call(s, kind, e, a, true) == MC_WORLD_STORAGE_IO_ERROR && r->calls == 1 &&
              !r->bad && e->phase == MC_ENTITY_TICK_EXECUTION_OPEN);
        const sp_scope before = sp_scope_take(s);
        const mc_entity_tick_execution_site first_site = e->site;
        const mc_world_storage_error first = e->leases[outer.slot].callback_error;
        const mc_world_access_failure *borrow = NULL;
        mc_world_access_failure_lease *const node = &e->leases[outer.slot];
        CHECK(mc_world_access_failure_lease_inspect(node, &borrow) &&
              borrow->receipt == r->receipt);
        observation->valid = mc_entity_tick_execution_push_world(e, b, np_site(26), &output);
        observation->valid_preserved =
            observation->valid == MC_WORLD_STORAGE_BUSY && np_token_equal(&output, &sentinel) &&
            sp_scope_same(s, &before) && e->site.class_name == first_site.class_name &&
            e->site.method == first_site.method && e->site.descriptor == first_site.descriptor &&
            e->site.pc == first_site.pc && mc_entity_tick_execution_current_world(e, &current) &&
            np_token_equal(&current, &outer) && a->persistent_bindings == ab + 1u &&
            b->persistent_bindings == bb && node == &e->leases[e->current_lease] &&
            mc_world_access_failure_lease_inspect(node, &borrow) && borrow->receipt == r->receipt &&
            r->self->active_holds == 1 && r->receipt->holds == 1 &&
            e->leases[outer.slot].callback_error.status == first.status &&
            e->leases[outer.slot].callback_error.operation == first.operation &&
            e->leases[outer.slot].callback_error.system_error == first.system_error &&
            (kind == SP_SOUND ? s->sound->cause.operation : s->particle->cause.operation) ==
                first.operation;
        /* A wrong successful admission is still a real owning scope. Close its
         * exact returned token normally before asserting this red result. */
        if (observation->valid == MC_WORLD_STORAGE_OK)
            CHECK(mc_entity_tick_execution_pop_world(e, &output, true) == MC_WORLD_STORAGE_OK);
        /* A failed inspect is not proof of IDLE. Project only the real native
         * active counter downward, then restore it BEFORE any assertion. */
        if (observation->valid_preserved) {
            borrow = NULL;
            const size_t active = r->self->active_holds;
            r->self->active_holds = 0;
            output = sentinel;
            observation->malformed =
                mc_entity_tick_execution_push_world(e, b, np_site(27), &output);
            r->self->active_holds = active;
            observation->malformed_preserved =
                observation->malformed == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                np_token_equal(&output, &sentinel) && sp_scope_same(s, &before) &&
                e->site.pc == first_site.pc && a->persistent_bindings == ab + 1u &&
                b->persistent_bindings == bb &&
                mc_world_access_failure_lease_inspect(node, &borrow) &&
                borrow->receipt == r->receipt && r->self->active_holds == 1 &&
                e->leases[outer.slot].callback_error.operation == first.operation;
            if (observation->malformed == MC_WORLD_STORAGE_OK)
                CHECK(mc_entity_tick_execution_pop_world(e, &output, true) == MC_WORLD_STORAGE_OK);
        }
        borrow = NULL;
        sp_clear_views(s);
        CHECK(mc_entity_tick_execution_pop_world(e, &outer, true) == MC_WORLD_STORAGE_OK &&
              mc_entity_tick_execution_retire(e) == MC_WORLD_STORAGE_OK &&
              mc_world_access_receipt_retire(&r->receipt, NULL) == MC_WORLD_STORAGE_OK &&
              a->persistent_bindings == ab && b->persistent_bindings == bb && r->calls == 1);
        if (!observation->valid_preserved || !observation->malformed_preserved)
            return false; /* Main checks the recorded result AFTER real fixture cleanup. */
    }
    ok = true;
cleanup:
    return ok;
}

static bool np_tokens(sp_fixture *s) {
    bool ok = false;
    mc_entity_tick_execution *e = &s->execution;
    mc_world_runtime *a = s->base->worlds[0], *b = s->base->worlds[2];
    const size_t ab = a->persistent_bindings, bb = b->persistent_bindings;
    const mc_entity_tick_execution_world_token sentinel = {NULL, 531, UINT64_C(937)};
    mc_entity_tick_execution_world_token out = sentinel, outer = {0}, inner = {0}, stale = {0},
                                         wrong = {0};
    mc_world_runtime *read = b;
    CHECK(sp_fresh(s) && !mc_entity_tick_execution_current_world(e, &out) &&
          np_token_equal(&out, &sentinel) &&
          mc_entity_tick_execution_push_world(e, a, np_site(30), NULL) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e->world_count == 0 && a->persistent_bindings == ab &&
          mc_entity_tick_execution_push_world(e, a, np_site(31), &outer) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_push_world(e, b, np_site(32), &inner) == MC_WORLD_STORAGE_OK);
    const sp_scope before = sp_scope_take(s);
    CHECK(mc_entity_tick_execution_pop_world(e, &outer, false) == MC_WORLD_STORAGE_BUSY &&
          sp_scope_same(s, &before) &&
          mc_entity_tick_execution_begin_world(e, a, np_site(33)) == MC_WORLD_STORAGE_BUSY &&
          sp_scope_same(s, &before));
    wrong = inner;
    ++wrong.generation;
    CHECK(!mc_entity_tick_execution_read_world(e, &wrong, &read) && read == b &&
          mc_entity_tick_execution_pop_world(e, &wrong, true) == MC_WORLD_STORAGE_BUSY &&
          sp_scope_same(s, &before));
    wrong = inner;
    wrong.owner = NULL;
    CHECK(!mc_entity_tick_execution_read_world(e, &wrong, &read) && read == b &&
          mc_entity_tick_execution_pop_world(e, &inner, false) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_pop_world(e, &outer, false) == MC_WORLD_STORAGE_OK);
    stale = outer;
    CHECK(mc_entity_tick_execution_push_world(e, a, np_site(34), &outer) == MC_WORLD_STORAGE_OK &&
          outer.slot == stale.slot && outer.generation == stale.generation + 1u &&
          !mc_entity_tick_execution_read_world(e, &stale, &read) && read == b &&
          mc_entity_tick_execution_pop_world(e, &stale, false) == MC_WORLD_STORAGE_BUSY &&
          e->world_open && e->current_lease == outer.slot &&
          mc_entity_tick_execution_pop_world(e, &outer, false) == MC_WORLD_STORAGE_OK);
    /* Nullable evaluated receivers are independent legal native occurrences. */
    CHECK(
        mc_entity_tick_execution_push_world(e, NULL, np_site(35), &outer) == MC_WORLD_STORAGE_OK &&
        mc_entity_tick_execution_push_world(e, NULL, np_site(36), &inner) == MC_WORLD_STORAGE_OK &&
        mc_entity_tick_execution_read_world(e, &inner, &read) && read == NULL &&
        e->world_count == 2 && e->lease_count == 2 && !e->world_held &&
        mc_entity_tick_execution_pop_world(e, &inner, false) == MC_WORLD_STORAGE_OK &&
        mc_entity_tick_execution_pop_world(e, &outer, false) == MC_WORLD_STORAGE_OK &&
        a->persistent_bindings == ab && b->persistent_bindings == bb);
    mc_entity_tick_execution_world_token tokens[MC_ENTITY_TICK_EXECUTION_WORLDS] = {0};
    for (size_t i = 0; i < COUNT(tokens); ++i)
        CHECK(mc_entity_tick_execution_push_world(e, a, np_site(40), &tokens[i]) ==
              MC_WORLD_STORAGE_OK);
    out = sentinel;
    CHECK(mc_entity_tick_execution_push_world(e, b, np_site(41), &out) == MC_WORLD_STORAGE_LIMIT &&
          np_token_equal(&out, &sentinel) && e->world_count == COUNT(tokens) &&
          e->lease_count == COUNT(tokens) && a->persistent_bindings == ab + COUNT(tokens) &&
          b->persistent_bindings == bb && e->first.status == MC_WORLD_STORAGE_LIMIT &&
          !e->capture_pending);
    for (size_t i = COUNT(tokens); i != 0; --i)
        CHECK(mc_entity_tick_execution_pop_world(e, &tokens[i - 1], true) == MC_WORLD_STORAGE_OK);
    CHECK(e->world_count == COUNT(tokens) && e->lease_count == 0 && !e->world_open &&
          mc_entity_tick_execution_retire(e) == MC_WORLD_STORAGE_OK &&
          a->persistent_bindings == ab && e->first.status == MC_WORLD_STORAGE_LIMIT);
    out = sentinel;
    CHECK(mc_entity_tick_execution_push_world(e, a, np_site(42), &out) == MC_WORLD_STORAGE_BUSY &&
          np_token_equal(&out, &sentinel) && !mc_entity_tick_execution_current_world(e, &out) &&
          np_token_equal(&out, &sentinel));
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    sp_fixture *s = NULL;
    bool guard_checked = false, guard_expected = true;
    np_guard_observation guard = {MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                  MC_WORLD_STORAGE_INVALID_ARGUMENT, false, false};
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "nested-owner") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    s = calloc(1, sizeof(*s));
    CHECK(s != NULL);
    s->base = calloc(1, sizeof(*s->base));
    CHECK(s->base != NULL && sp_setup(s, root));
    puts("NESTED_OWNER_CASE id=0");
    CHECK(np_success(s));
    puts("NESTED_OWNER_CASE id=1");
    CHECK(np_sparse_failure(s));
    puts("NESTED_OWNER_CASE id=2");
    CHECK(np_reentry(s));
    puts("NESTED_OWNER_CASE id=3");
    guard_checked = true;
    guard_expected = np_held_push(s, &guard);
    if (!guard_expected)
        goto cleanup;
    puts("NESTED_OWNER_CASE id=4");
    CHECK(np_tokens(s));
    result = 0;
cleanup:
    if (!sp_cleanup(s)) {
        fputs("Nested execution owner cleanup refused; remaining graph retained\n", stderr);
        return 1;
    }
    if (guard_checked && !guard_expected) {
        fprintf(stderr,
                "Nested owner guard assertion AFTER checked cleanup: valid=%d preserved=%d "
                "malformed=%d preserved=%d\n",
                guard.valid, guard.valid_preserved, guard.malformed, guard.malformed_preserved);
        return 1;
    }
    if (result == 0 && printf("PASS: %u actual nested World owner checks;5 groups\n", checks) < 0)
        result = 1;
    return result;
}
