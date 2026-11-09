/* Genuine current fixture; its program is renamed and never executed here. */
int nq_unused_collision_main(int, char **);
#define MC_WORLD_COLLISION_QUERY_MAIN nq_unused_collision_main
#include "test_collision_query_execution.c"
#undef MC_WORLD_COLLISION_QUERY_MAIN

/* The genuine current CQ/SourceSaving fixture above is included unchanged. Its
 * renamed main and all older program paths are never executed by this main. */
#include "world/runtime/collision/entities_execution_internal.h"
#include "world/runtime/collision/entities_internal.h"
#include <xmmintrin.h>

typedef struct nq_fixture {
    cq_fixture cq;
    mc_math_classes *math_owner;
    mc_math_execution math;
    mc_world_nearby_error error;
    mc_world_runtime_execution_result result;
    mc_world_storage_status status;
    mc_entity *items[2];
    size_t queries, acquires, gathers, setup_gets;
    bool completed, callbacks_ok;
} nq_fixture;

static const mc_entity_tick_execution_site nq_site = {"native.nearby.control", "query", "()V", 0};
static const mc_entity_bounds nq_bounds = {2, 60, 2, 13, 70, 13};
static const mc_entity_bounds nq_empty = {34, 60, 2, 0, 70, 13};

/* Nonnull initialized Random VALUES. Cache bits remain observable even
 * while invalid; compare no native struct padding and perform no draw/getter. */
static bool nq_random_same(const mc_java_random *left, const mc_java_random *right) {
    uint64_t left_bits, right_bits;
    _Static_assert(sizeof(left_bits) == sizeof(left->next_next_gaussian),
                   "binary64 Random cache storage required");
    memcpy(&left_bits, &left->next_next_gaussian, sizeof(left_bits));
    memcpy(&right_bits, &right->next_next_gaussian, sizeof(right_bits));
    return left->state == right->state &&
           left->have_next_next_gaussian == right->have_next_next_gaussian &&
           left_bits == right_bits;
}

static bool nq_quiet(const mc_world_runtime_execution_result *result) {
    return result->ownership_status == MC_WORLD_STORAGE_OK && !result->selection_ready &&
           !cq_pending(result) && mg_closes_empty(result);
}

static bool nq_values(const mc_world_entity_list_view *view, mc_entity *const *items, size_t n) {
    size_t actual = SIZE_MAX;
    if (mc_world_entity_list_view_count(view, &actual) != MC_WORLD_STORAGE_OK || actual != n)
        return false;
    for (size_t i = 0; i < n; ++i) {
        mc_entity *entity = NULL;
        if (mc_world_entity_list_view_get(view, i, &entity) != MC_WORLD_STORAGE_OK ||
            entity != items[i])
            return false;
    }
    return true;
}

static bool nq_call(nq_fixture *s, mc_entity *excluded, const mc_entity_bounds *bounds,
                    mc_world_entity_list_view *out) {
    ++s->queries;
    return mc_world_runtime_nearby_entities_in_execution(
        &s->cq.execution, s->cq.source.game.worlds[0], cq_code, excluded, bounds, out, &s->error,
        &s->status, &s->result);
}

/* These calls cannot leave an acquisition/activation descendant: the complete
 * query's nonloading presence is actual map-cache membership. Source failure
 * and actual outputs remain visible in the fixture after ordinary owner close. */
static int nq_scoped(nq_fixture *s, mc_entity *excluded, const mc_entity_bounds *bounds,
                     mc_world_entity_list_view *out, mc_math_execution *math) {
    mc_world_runtime *world = s->cq.source.game.worlds[0];
    const size_t bindings = world->persistent_bindings, active = world->active_calls;
    mc_entity_tick_execution_init(&s->cq.execution, math);
    mc_entity_tick_execution_world_token outer;
    VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, world, nq_site, &outer) ==
           MC_WORLD_STORAGE_OK);
    s->completed = nq_call(s, excluded, bounds, out);
    VERIFY(nq_quiet(&s->result) && world->active_calls == active &&
           mc_entity_tick_execution_pop_world(&s->cq.execution, &outer, !s->completed) ==
               MC_WORLD_STORAGE_OK &&
           mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK &&
           world->persistent_bindings == bindings);
    return 0;
}

static int nq_load(nq_fixture *s) {
    fixture *f = &s->cq.source.game;
    mc_chunk_state *state = NULL;
    /* Genuine setup acquisition, separate from every complete nearby query. */
    ++s->setup_gets;
    VERIFY(mc_world_runtime_get(f->worlds[0], 0, 0, &state) == MC_CHUNK_ACQUIRE_OK &&
           state != NULL && cq_remember(&s->cq) == 0 && state == f->states[0] && !state->blank &&
           f->worlds[0]->fields.R->count == 0);
    return 0;
}

static int nq_items(nq_fixture *s) {
    fixture *f = &s->cq.source.game;
    for (size_t i = 0; i < COUNT(s->items); ++i) {
        VERIFY(mc_item_entity_allocate(f->entity_classes[0], &s->items[i], NULL) == MC_ENTITY_OK &&
               mc_item_entity_construct(s->items[i], f->worlds[0], 8.0 + (double)i * .5, 64.0, 8.0,
                                        NULL, NULL) == MC_ENTITY_OK);
    }
    VERIFY(
        mc_world_runtime_chunk_add_entity(f->chunks[0], s->items[0], NULL) == MC_WORLD_STORAGE_OK &&
        mc_world_runtime_chunk_add_entity(f->chunks[0], s->items[0], NULL) == MC_WORLD_STORAGE_OK &&
        mc_world_runtime_chunk_add_entity(f->chunks[0], s->items[1], NULL) == MC_WORLD_STORAGE_OK &&
        s->items[0]->holds == 3 && s->items[1]->holds == 2);
    return 0;
}

static int nq_query_case(nq_fixture *s) {
    fixture *f = &s->cq.source.game;
    mc_world_runtime *world = f->worlds[0];
    VERIFY(nq_load(s) == 0 && nq_items(s) == 0);
    const mc_java_random randoms[] = {*s->items[0]->random, *s->items[1]->random};
    const mc_java_random world_random = *world->fields.r;
    const bool dirty = f->states[0]->dirty;
    const mc_entity_bounds bounds[] = {*s->items[0]->bounds, *s->items[1]->bounds};
    mc_entity *all[] = {s->items[0], s->items[0], s->items[1]};
    mc_entity *without_a[] = {s->items[1]};
    const mc_world_reference_list *identity = world->fields.R;
    mc_world_entity_list_view first = {0}, second = {0};
    VERIFY(nq_scoped(s, NULL, &nq_bounds, &first, &s->math) == 0 && s->completed &&
           s->status == MC_WORLD_STORAGE_OK && nq_values(&first, all, COUNT(all)) &&
           first.owner == world && first.identity == identity && world->local_leases == 1 &&
           s->items[0]->holds == 5 && s->items[1]->holds == 3 && s->error.query.bytecode == 0 &&
           s->error.slice == SIZE_MAX);
    VERIFY(nq_scoped(s, s->items[0], &nq_bounds, &second, &s->math) == 0 && s->completed &&
           nq_values(&first, without_a, COUNT(without_a)) &&
           nq_values(&second, without_a, COUNT(without_a)) && world->local_leases == 2 &&
           s->items[0]->holds == 3 && s->items[1]->holds == 3 &&
           mc_world_runtime_retire_entity_containers(world, NULL) == MC_WORLD_STORAGE_BUSY &&
           mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_BUSY);
    /* Fresh output has no owning lease; its real identity remains held above. */
    mc_world_entity_list_view untouched = {NULL, identity};
    mc_math_execution absent;
    mc_math_execution_init(&absent, NULL);
    VERIFY(nq_scoped(s, NULL, NULL, &untouched, &absent) == 0 && !s->completed &&
           s->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && s->error.query.bytecode == 10 &&
           untouched.owner == NULL && untouched.identity == identity &&
           nq_values(&first, NULL, 0) && nq_values(&second, NULL, 0) && s->items[0]->holds == 3 &&
           s->items[1]->holds == 2 && absent.owned == NULL && !absent.selected && !absent.failed &&
           mc_math_execution_retire(&absent) == MC_MATH_CLASS_OK);
    mc_world_entity_list_view_release(&second);
    mc_world_entity_list_view_release(&first);
    VERIFY(first.owner == NULL && second.owner == NULL && world->local_leases == 0 &&
           mc_world_nearby_owned_valid(world) && f->states[0]->dirty == dirty &&
           nq_random_same(world->fields.r, &world_random) &&
           nq_random_same(s->items[0]->random, &randoms[0]) &&
           nq_random_same(s->items[1]->random, &randoms[1]) &&
           memcmp(s->items[0]->bounds, &bounds[0], sizeof(bounds[0])) == 0 &&
           memcmp(s->items[1]->bounds, &bounds[1], sizeof(bounds[1])) == 0);
    return 0;
}

static int nq_prefix_case(nq_fixture *s) {
    mc_world_runtime *world = s->cq.source.game.worlds[0];
    VERIFY(nq_load(s) == 0);
    mc_world_entity_list_view out = {0};
    mc_math_execution absent;
    mc_math_execution_init(&absent, NULL);
    VERIFY(nq_scoped(s, NULL, NULL, &out, &absent) == 0 && !s->completed &&
           s->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && s->error.query.bytecode == 10 &&
           !absent.failed && !absent.selected && absent.owned == NULL && out.owner == NULL);
    /* Genuine native FP input; ONE actual entry, then restore before any
     * assertion, owner close or native cleanup. */
    mc_entity_tick_execution_init(&s->cq.execution, &absent);
    mc_entity_tick_execution_world_token fp_world;
    VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, world, nq_site, &fp_world) ==
           MC_WORLD_STORAGE_OK);
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(saved | 0x8000u);
    s->completed = nq_call(s, NULL, &nq_bounds, &out);
    _mm_setcsr(saved);
    VERIFY(!s->completed && s->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
           s->error.query.bytecode == 16 && !absent.failed && !absent.selected &&
           absent.owned == NULL && out.owner == NULL && nq_quiet(&s->result) &&
           mc_entity_tick_execution_pop_world(&s->cq.execution, &fp_world, true) ==
               MC_WORLD_STORAGE_OK &&
           mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK);
    VERIFY(nq_scoped(s, NULL, &nq_bounds, &out, &absent) == 0 && !s->completed &&
           s->status == MC_WORLD_STORAGE_OK && s->error.query.bytecode == 21 &&
           s->error.query.provider_bytecode == 0 && absent.failed && absent.owned == NULL &&
           absent.first.caller.pc == 21 && strcmp(absent.first.caller.class_name, "fd") == 0 &&
           absent.first.cause.status == MC_MATH_CLASS_INVALID_ARGUMENT && out.owner == NULL &&
           mc_math_execution_retire(&absent) == MC_MATH_CLASS_OK);
    mc_math_class_state cold, ready;
    VERIFY(mc_math_classes_state(s->math_owner, &cold, NULL) == MC_MATH_CLASS_OK &&
           cold.phase == MC_MATH_CLASS_COLD && cold.source_a == NULL && cold.holds == 1 &&
           nq_scoped(s, NULL, &nq_empty, &out, &s->math) == 0 && s->completed &&
           s->status == MC_WORLD_STORAGE_OK && nq_values(&out, NULL, 0) &&
           mc_math_classes_state(s->math_owner, &ready, NULL) == MC_MATH_CLASS_OK &&
           ready.phase == MC_MATH_CLASS_READY && ready.source_a != NULL && ready.holds == 2 &&
           s->math.owned == s->math_owner && !s->math.failed && !s->math.selected);
    mc_world_entity_list_view_release(&out);
    /* Real constructor-created Blank component, NOT a top-query Blank route.
     * FD top-level Math is not bypassed by claiming this direct LM override. */
    mc_entity_tick_execution_init(&s->cq.execution, &s->math);
    mc_entity_tick_execution_world_token outer;
    VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, world, nq_site, &outer) ==
               MC_WORLD_STORAGE_OK &&
           world->fallback.blank &&
           mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    const size_t count = world->fields.R->count, capacity = world->fields.R->capacity;
    mc_world_nearby_error_init(&s->error);
    const unsigned blank_saved = _mm_getcsr();
    _mm_setcsr(blank_saved | 0x8000u);
    const mc_world_storage_status blank = mc_world_nearby_gather_held(
        world, &world->fallback, NULL, NULL, world->fields.R, &s->error);
    _mm_setcsr(blank_saved);
    ++s->gathers;
    mc_world_runtime_call_leave(world);
    VERIFY(blank == MC_WORLD_STORAGE_OK && s->error.query.chunk_bytecode == 0 &&
           world->fields.R->count == count && world->fields.R->capacity == capacity &&
           world->local_leases == 0 && mc_world_nearby_owned_valid(world) &&
           mc_entity_tick_execution_pop_world(&s->cq.execution, &outer, false) ==
               MC_WORLD_STORAGE_OK &&
           mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK);
    return 0;
}

static void nq_probe(nq_fixture *s) {
    cq_scope before = {0};
    mc_world_runtime *world = s->cq.source.game.worlds[0];
    const bool taken = cq_scope_take(&s->cq, &before);
    const size_t r_count = world->fields.R->count;
    const mc_world_storage_error world_cause = world->last_error;
    void **const r_items = world->fields.R->items;
    const size_t a_holds = s->items[0]->holds, b_holds = s->items[1]->holds;
    mc_world_entity_list_view out = {NULL, world->fields.R};
    const bool completed = nq_call(s, NULL, NULL, &out);
    s->callbacks_ok &=
        taken && !completed && s->status == MC_WORLD_STORAGE_BUSY &&
        s->result.ownership_status == MC_WORLD_STORAGE_BUSY &&
        s->result.ownership_error.operation != NULL &&
        strcmp(s->result.ownership_error.operation, "Nearby exact evaluated World ownership") ==
            0 &&
        !s->result.selection_ready && !cq_pending(&s->result) && mg_closes_empty(&s->result) &&
        s->error.query.bytecode == 0 && s->error.query.provider_bytecode == 0 &&
        s->error.query.status == MC_WORLD_STORAGE_BUSY && s->error.query.source_chunk == NULL &&
        out.owner == NULL && out.identity == world->fields.R && world->fields.R->count == r_count &&
        world->fields.R->items == r_items && s->items[0]->holds == a_holds &&
        s->items[1]->holds == b_holds && cq_scope_same(&s->cq, &before) &&
        world->last_error.status == world_cause.status &&
        world->last_error.operation == world_cause.operation &&
        world->last_error.system_error == world_cause.system_error;
}

static mc_world_storage_status nq_dirty(void *context, mc_world_runtime *world, int32_t x,
                                        int32_t y, int32_t z, mc_tile_entity *tile,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    nq_fixture *s = context;
    ++s->cq.callbacks;
    s->cq.node = mc_entity_tick_execution_world_lease(&s->cq.execution);
    s->callbacks_ok &= world == s->cq.source.game.worlds[0] && x == 8 && y == 64 && z == 8 &&
                       tile == NULL && s->cq.callbacks == 1 && s->cq.node != NULL &&
                       s->cq.node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                       s->cq.receiver->active_holds == 1;
    if (strcmp(s->cq.mode, "armed") == 0) {
        nq_probe(s);
        /* A genuine new B occurrence (alias of A), with an independent IDLE
         * node below the still-ARMED A call. No Source receiver field changes. */
        mc_entity_tick_execution_world_token b;
        mc_world_entity_list_view view = {0};
        const mc_world_storage_status pushed =
            mc_entity_tick_execution_push_world(&s->cq.execution, world, nq_site, &b);
        if (pushed != MC_WORLD_STORAGE_OK) {
            s->callbacks_ok = false;
            return pushed;
        }
        const bool completed = nq_call(s, NULL, &nq_empty, &view);
        s->callbacks_ok &= completed && s->status == MC_WORLD_STORAGE_OK && nq_quiet(&s->result) &&
                           nq_values(&view, NULL, 0);
        if (completed)
            mc_world_entity_list_view_release(&view);
        const mc_world_storage_status popped =
            mc_entity_tick_execution_pop_world(&s->cq.execution, &b, !completed);
        s->callbacks_ok &= popped == MC_WORLD_STORAGE_OK &&
                           s->cq.node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
        return popped;
    }
    bool removed = false;
    const mc_world_storage_status removal =
        mc_world_runtime_remove_world_access(world, s->cq.receiver, &removed, cause);
    s->cq.removed = removal == MC_WORLD_STORAGE_OK && removed;
    if (!s->cq.removed)
        return removal == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : removal;
    s->cq.free_during = mc_world_access_free(s->cq.receiver, NULL);
    const mc_world_storage_status made =
        mc_world_access_receipt_create(s->cq.receiver, NULL, &s->cq.receipt, failure, cause);
    if (made != MC_WORLD_STORAGE_OK)
        return made;
    s->cq.receipt->exception_class = "java/lang/IllegalStateException";
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Nearby control actual callback failure", 77};
    s->cq.callback_cause = *cause;
    return cause->status;
}

static int nq_node_case(nq_fixture *s) {
    fixture *f = &s->cq.source.game;
    mc_world_runtime *world = f->worlds[0];
    VERIFY(nq_load(s) == 0 && nq_items(s) == 0);
    mc_world_entity_list_view view = {0};
    VERIFY(nq_scoped(s, NULL, &nq_bounds, &view, &s->math) == 0 && s->completed);
    mc_world_entity_list_view_release(&view);
    s->callbacks_ok = true;
    const mc_world_access_source_callbacks callbacks = {.tile_entity_dirty = nq_dirty};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(world, s->cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    mc_entity_tick_execution_init(&s->cq.execution, &s->math);
    mc_entity_tick_execution_world_token outer;
    VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, world, nq_site, &outer) ==
           MC_WORLD_STORAGE_OK);
    const mc_world_storage_status status = mc_world_runtime_tile_entity_dirty_in_execution(
        &s->cq.execution, world, cq_code, 8, 64, 8, NULL, &s->cq.dirty, &s->cq.result);
    VERIFY(s->cq.callbacks == 1 && s->callbacks_ok);
    if (strcmp(s->cq.mode, "held") == 0) {
        const mc_world_access_failure *protected = NULL;
        VERIFY(status == MC_WORLD_STORAGE_IO_ERROR && s->cq.removed &&
               s->cq.free_during == MC_WORLD_STORAGE_BUSY && s->cq.receipt != NULL &&
               s->cq.dirty.cause.status == s->cq.callback_cause.status &&
               s->cq.dirty.cause.operation == s->cq.callback_cause.operation &&
               mc_world_access_failure_lease_inspect(s->cq.node, &protected) &&
               protected->receipt == s->cq.receipt && protected->receiver == s->cq.receiver);
        nq_probe(s);
        VERIFY(s->callbacks_ok && s->cq.dirty.cause.status == s->cq.callback_cause.status &&
               s->cq.dirty.cause.operation == s->cq.callback_cause.operation &&
               s->cq.dirty.cause.system_error == s->cq.callback_cause.system_error &&
               s->error.query.status == MC_WORLD_STORAGE_BUSY &&
               mc_entity_tick_execution_capture_receipt(&s->cq.execution, protected) ==
                   MC_WORLD_STORAGE_OK &&
               mc_entity_tick_execution_pop_world(&s->cq.execution, &outer, true) ==
                   MC_WORLD_STORAGE_OK &&
               mc_world_access_receipt_retire(&s->cq.receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
               mc_world_access_free(s->cq.receiver, NULL) == MC_WORLD_STORAGE_BUSY);
        protected = NULL;
        s->cq.dirty.world_access_failure = (mc_world_access_failure){0};
        mc_world_nearby_error_init(&s->error);
        s->result = s->cq.result = (mc_world_runtime_execution_result){0};
        VERIFY(mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK &&
               mc_world_access_receipt_retire(&s->cq.receipt, NULL) == MC_WORLD_STORAGE_OK);
    } else {
        bool removed = false;
        VERIFY(status == MC_WORLD_STORAGE_OK && s->queries == 3 &&
               s->cq.execution.receipt_count == 0 &&
               mc_world_runtime_remove_world_access(world, s->cq.receiver, &removed, NULL) ==
                   MC_WORLD_STORAGE_OK &&
               removed &&
               mc_entity_tick_execution_pop_world(&s->cq.execution, &outer, false) ==
                   MC_WORLD_STORAGE_OK &&
               mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK);
    }
    VERIFY(mc_world_access_free(s->cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->cq.receiver = NULL;
    s->cq.node = NULL;
    return 0;
}

static int nq_loader_case(nq_fixture *s) {
    fixture *f = &s->cq.source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_world_runtime_source_construction_state construction;
    VERIFY(mc_world_runtime_source_construction_inspect(world, &construction) &&
           construction.loader != NULL && f->providers[0].cache->count == 0);
    mc_entity_tick_execution_init(&s->cq.execution, &s->math);
    mc_entity_tick_execution_world_token outer;
    const size_t bindings = world->persistent_bindings;
    VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, world, nq_site, &outer) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_nearby_error_init(&s->error);
    mc_world_runtime_execution_result_init(&s->result);
    mc_chunk_state sentinel = {0};
    mc_chunk_state *out = &sentinel;
    /* One reached shared acquiring component, NOT the whole cache-gated query.
     * Prefix0 is an honest native component caller, not fabricated FD113. */
    const bool completed = mc_world_entity_query_acquire_in_execution(
        &s->cq.execution, world, cq_code, 0, 0, &out, &s->error, &s->result);
    ++s->acquires;
    mc_world_runtime_call_leave(world);
    mc_entity_tick_execution_loader_view selected = {0};
    VERIFY(
        !completed && out == &sentinel && s->error.query.status == MC_WORLD_STORAGE_INVALID_DATA &&
        s->error.query.bytecode == 0 && s->error.query.provider_bytecode == 6 &&
        s->error.query.acquisition == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
        s->result.ownership_status == MC_WORLD_STORAGE_OK && s->result.selection_ready &&
        !cq_pending(&s->result) && mg_closes_empty(&s->result) &&
        mc_entity_tick_execution_loader_read(&s->cq.execution, &s->result.selection, &selected) &&
        selected.status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED && selected.failure != NULL &&
        s->error.query.source_chunk == selected.failure);
    mc_world_source_chunk_loader_state producer;
    VERIFY(mc_world_source_chunk_loader_inspect(construction.loader, &producer) &&
           producer.phase == MC_WORLD_SOURCE_FAILED && producer.failure != NULL &&
           selected.failure != producer.failure &&
           selected.failure == &s->cq.execution.loaders[0] &&
           kl_error_same(selected.failure, producer.failure) &&
           selected.failure->source.source_exception && !selected.failure->source.native_policy &&
           selected.failure->input.source_fault == MC_NBT_SOURCE_ROOT &&
           s->error.query.cause.operation == producer.failure->storage.operation);
    const mc_entity_tick_execution_loader_selection token = s->result.selection;
    mc_world_nearby_error_init(&s->error); /* End this real caller's typed borrows. */
    selected = (mc_entity_tick_execution_loader_view){0};
    VERIFY(mc_entity_tick_execution_pop_world(&s->cq.execution, &outer, true) ==
               MC_WORLD_STORAGE_OK &&
           world->persistent_bindings == bindings + 1 &&
           mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_BUSY &&
           mc_entity_tick_execution_loader_read(&s->cq.execution, &token, &selected) &&
           kl_error_same(selected.failure, producer.failure));
    selected = (mc_entity_tick_execution_loader_view){0};
    producer = (mc_world_source_chunk_loader_state){0};
    construction = (mc_world_runtime_source_construction_state){0};
    s->result = (mc_world_runtime_execution_result){0};
    VERIFY(mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK &&
           world->persistent_bindings == bindings && s->cq.execution.loader_count == 0 &&
           s->math.owned == NULL && !s->math.failed && !s->math.selected);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && (strcmp(argv[1], "query") == 0 || strcmp(argv[1], "prefix") == 0 ||
                         strcmp(argv[1], "armed") == 0 || strcmp(argv[1], "held") == 0 ||
                         strcmp(argv[1], "acquire-loader") == 0));
    printf("NEARBY_OWNED_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    nq_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->cq.mode = argv[1];
    VERIFY(ss_setup(&s->cq.source, argv[2]) == 0 && cq_files(&s->cq, true) == 0 &&
           ss_open(&s->cq.source) == 0 &&
           mc_math_classes_create(SIZE_MAX, &s->math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&s->math, s->math_owner);
    printf("NEARBY_OWNED_CASE %s BEGIN\n", s->cq.mode);
    fflush(stdout);
    const int result = strcmp(s->cq.mode, "query") == 0            ? nq_query_case(s)
                       : strcmp(s->cq.mode, "prefix") == 0         ? nq_prefix_case(s)
                       : strcmp(s->cq.mode, "acquire-loader") == 0 ? nq_loader_case(s)
                                                                   : nq_node_case(s);
    VERIFY(result == 0 &&
           mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK &&
           mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK &&
           mc_math_classes_release(&s->math_owner, NULL) == MC_MATH_CLASS_OK);
    printf("NEARBY_OWNED_CASE %s END queries=%zu acquires=%zu gathers=%zu setup_gets=%zu\n",
           s->cq.mode, s->queries, s->acquires, s->gathers, s->setup_gets);
    fflush(stdout);
    for (size_t i = 0; i < COUNT(s->items); ++i) {
        mc_entity_release(s->items[i]); /* Actual caller occurrence, before terminal World graph. */
        s->items[i] = NULL;
    }
    mc_world_nearby_error_init(&s->error);
    s->result = (mc_world_runtime_execution_result){0};
    VERIFY((s->cq.source.game.states[0] == NULL ? cq_empty_stop(&s->cq.source)
                                                : ss_stop(&s->cq.source)) == 0 &&
           cq_files(&s->cq, false) == 0 && ss_finish(&s->cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->cq.inputs[i]);
    free(s);
    printf("NEARBY_OWNED_OPEN "
           "top-query-cold/owned-Blank/get-ok-native-completion/foreign-Store/full-SL\n");
    printf("PASS: %u actual owned nearby checks;mode=%s\n", checks, argv[1]);
    return 0;
}
