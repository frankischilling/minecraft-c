/* Reuse the genuine SourceSaving setup and checked terminal bodies, not main. */
int si_unused_query_main(int, char **);
#define MC_WORLD_COLLISION_QUERY_MAIN si_unused_query_main
#include "test_collision_query_execution.c"
#undef MC_WORLD_COLLISION_QUERY_MAIN

#include "../../../support/java_random.h"
#include "world/runtime/entity_insertion_execution_internal.h"

typedef struct insert_guard {
    unsigned char world[sizeof(mc_world_runtime)], entity[sizeof(mc_entity)];
    unsigned char profile[sizeof(mc_java_random_profile)];
    unsigned char execution[sizeof(mc_entity_tick_execution)];
    unsigned char error[sizeof(mc_world_entity_error)];
    mc_java_random world_random, entity_random;
    mc_world_source_chunk_loader_state loader;
    size_t active_calls, bindings, entity_holds, world_entries, cache_count, insertions;
} insert_guard;

typedef struct insert_fixture {
    cq_fixture cq;
    mc_math_classes *math_owner;
    mc_math_execution math;
    mc_world_entity_insert_execution frame;
    mc_world_entity_error error;
    mc_world_storage_status status;
    mc_entity_tick_execution_world_token outer;
    mc_entity *item;
    mc_item_stack *stack;
    insert_guard before;
    size_t entries, primes;
    bool scope_open, completed, out;
} insert_fixture;

static const mc_entity_tick_execution_site si_site = {"native.insert.control", "run", "()V", 0};

static bool si_mode(const char *mode) {
    return strcmp(mode, "preflight") == 0 || strcmp(mode, "success") == 0 ||
           strcmp(mode, "absent") == 0 || strcmp(mode, "math-missing") == 0 ||
           strcmp(mode, "math-limit") == 0;
}

static bool si_quiet(const mc_world_runtime_execution_result *result) {
    if (result->selection_ready || result->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (result->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (result->closes[i].used)
            return false;
    return true;
}

static int si_take(insert_fixture *s) {
    fixture *f = &s->cq.source.game;
    mc_world_runtime *world = f->worlds[0];
    insert_guard *g = &s->before;
    mc_world_runtime_source_construction_state construction;
    VERIFY(world->fields.r != NULL && s->item->random != NULL &&
           mc_world_runtime_source_construction_inspect(world, &construction) &&
           mc_world_source_chunk_loader_inspect(construction.loader, &g->loader));
    memcpy(g->world, world, sizeof(*world));
    memcpy(g->entity, s->item, sizeof(*s->item));
    memcpy(g->profile, f->profile, sizeof(*f->profile));
    memcpy(g->execution, &s->cq.execution, sizeof(s->cq.execution));
    memcpy(g->error, &s->error, sizeof(s->error));
    g->world_random = *world->fields.r;
    g->entity_random = *s->item->random;
    g->active_calls = world->active_calls;
    g->bindings = world->persistent_bindings;
    g->entity_holds = s->item->holds;
    g->world_entries = world->fields.b->count;
    g->cache_count = f->providers[0].cache->count;
    g->insertions = f->providers[0].cache->insertion_count;
    return 0;
}

static bool si_inputs_same(const insert_fixture *s) {
    const fixture *f = &s->cq.source.game;
    return mc_test_java_random_equal(f->worlds[0]->fields.r, &s->before.world_random) &&
           mc_test_java_random_equal(s->item->random, &s->before.entity_random) &&
           memcmp(s->before.profile, f->profile, sizeof(*f->profile)) == 0;
}

static bool si_call(insert_fixture *s, bool null_out) {
    ++s->entries; /* Native C entry attempts, not a Source method count. */
    s->completed = mc_world_runtime_insert_entity_in_execution(
        &s->frame, s->cq.source.game.worlds[0], s->item, null_out ? NULL : &s->out, &s->error,
        &s->status);
    return s->completed;
}

static int si_preflight(insert_fixture *s) {
    fixture *f = &s->cq.source.game;
    mc_world_runtime *world = f->worlds[0];
    memset(&s->error, 0, sizeof(s->error));
    s->error.status = MC_WORLD_STORAGE_IO_ERROR;
    s->error.operation = "Insert required-output untouched diagnostic";
    s->error.bytecode = 29;
    s->error.cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                              "Insert required-output untouched cause", 17};
    s->status = MC_WORLD_STORAGE_IO_ERROR;
    s->out = true;
    VERIFY(si_take(s) == 0 && !si_call(s, true) && s->entries == 1 && s->out &&
           s->status == MC_WORLD_STORAGE_IO_ERROR &&
           memcmp(s->before.error, &s->error, sizeof(s->error)) == 0 &&
           s->cq.result.ownership_status == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
           s->cq.result.ownership_error.operation != NULL &&
           strcmp(s->cq.result.ownership_error.operation,
                  "World single insertion actual execution/code/outputs required") == 0 &&
           s->cq.result.ownership_error.site.class_name == si_site.class_name &&
           s->cq.result.ownership_error.site.method == si_site.method &&
           s->cq.result.ownership_error.site.descriptor == si_site.descriptor &&
           s->cq.result.ownership_error.site.pc == si_site.pc && si_quiet(&s->cq.result));
    mc_math_class_state math = {0};
    VERIFY(!s->frame.entered && !s->frame.body_entered && !s->frame.body_completed &&
           !s->frame.completed && !s->frame.get_returned && !s->frame.chunk_returned &&
           !s->frame.observer_returned && !s->math.selected && !s->math.failed &&
           s->math.owned == NULL &&
           mc_math_classes_state(s->math_owner, &math, NULL) == MC_MATH_CLASS_OK &&
           math.phase == MC_MATH_CLASS_COLD && math.holds == 1 && math.source_a == NULL &&
           memcmp(s->before.world, world, sizeof(*world)) == 0 &&
           memcmp(s->before.entity, s->item, sizeof(*s->item)) == 0 &&
           memcmp(s->before.execution, &s->cq.execution, sizeof(s->cq.execution)) == 0 &&
           si_inputs_same(s) && cq_loader_same(&s->cq, &s->before.loader) &&
           f->providers[0].cache->count == 0 && f->providers[0].cache->insertion_count == 0 &&
           world->fields.u->count == 0);
    return 0;
}

static int si_success(insert_fixture *s) {
    fixture *f = &s->cq.source.game;
    mc_world_runtime *world = f->worlds[0];
    s->out = false;
    s->status = MC_WORLD_STORAGE_IO_ERROR;
    VERIFY(si_take(s) == 0 && si_call(s, false) && s->entries == 1 && s->out &&
           s->status == MC_WORLD_STORAGE_OK && s->error.status == MC_WORLD_STORAGE_OK &&
           s->error.bytecode == 0 && s->error.cause.status == MC_WORLD_STORAGE_OK &&
           s->frame.entered && s->frame.body_entered && s->frame.body_completed &&
           s->frame.completed && s->frame.get_returned && s->frame.chunk_returned &&
           s->frame.observer_returned && s->frame.acquisition.status == MC_CHUNK_ACQUIRE_OK &&
           s->frame.acquisition.source_chunk == NULL &&
           s->frame.chunk_status == MC_WORLD_STORAGE_OK &&
           s->cq.result.ownership_status == MC_WORLD_STORAGE_OK && si_quiet(&s->cq.result));
    const mc_world_chunk_entity_list *slice = NULL;
    mc_world_chunk_fields chunk;
    size_t count = SIZE_MAX;
    mc_entity *entry = NULL;
    mc_math_class_state math = {0};
    VERIFY(mc_world_chunk_fields_get(f->chunks[0], &chunk) == MC_WORLD_STORAGE_OK && chunk.q &&
           chunk.d == world && mc_world_chunk_slice_at(chunk.m, 5, &slice) == MC_WORLD_STORAGE_OK &&
           mc_world_chunk_entity_count(slice, &count) == MC_WORLD_STORAGE_OK && count == 1 &&
           mc_world_chunk_entity_at(slice, 0, &entry) == MC_WORLD_STORAGE_OK && entry == s->item &&
           world->fields.b->count == s->before.world_entries + 1 &&
           world->fields.b->items[s->before.world_entries] == s->item &&
           world->entity_membership.entries[s->before.world_entries] == s->item &&
           world->entity_membership.count == world->fields.b->count &&
           mc_world_runtime_entity_list_valid(world) &&
           s->item->holds == s->before.entity_holds + 2 && s->item->base.world == world &&
           s->item->base.bF && s->item->base.bG == 0 && s->item->base.bH == 5 &&
           s->item->base.bI == 0 &&
           mc_math_classes_state(s->math_owner, &math, NULL) == MC_MATH_CLASS_OK &&
           math.phase == MC_MATH_CLASS_READY && math.holds == 2 && math.source_a != NULL &&
           !s->math.failed && si_inputs_same(s) && cq_loader_same(&s->cq, &s->before.loader) &&
           world->active_calls == s->before.active_calls &&
           world->persistent_bindings == s->before.bindings &&
           f->providers[0].cache->count == s->before.cache_count &&
           f->providers[0].cache->insertion_count == s->before.insertions &&
           s->cq.execution.loader_count == 0 && s->cq.execution.receipt_count == 0 &&
           world->fields.u->count == 0);
    return 0;
}

static int si_absent(insert_fixture *s) {
    fixture *f = &s->cq.source.game;
    s->out = true;
    s->status = MC_WORLD_STORAGE_IO_ERROR;
    VERIFY(si_take(s) == 0 && si_call(s, false) && !s->out && s->entries == 1 &&
           s->status == MC_WORLD_STORAGE_OK && s->error.status == MC_WORLD_STORAGE_OK &&
           s->error.bytecode == 0 && s->frame.entered && s->frame.body_entered &&
           s->frame.body_completed && s->frame.completed && !s->frame.get_returned &&
           !s->frame.chunk_returned && !s->frame.observer_returned &&
           s->cq.result.ownership_status == MC_WORLD_STORAGE_OK && si_quiet(&s->cq.result));
    mc_math_class_state math = {0};
    VERIFY(mc_math_classes_state(s->math_owner, &math, NULL) == MC_MATH_CLASS_OK &&
           math.phase == MC_MATH_CLASS_READY && !s->math.failed &&
           memcmp(s->before.entity, s->item, sizeof(*s->item)) == 0 && si_inputs_same(s) &&
           cq_loader_same(&s->cq, &s->before.loader) && f->providers[0].cache->count == 0 &&
           f->providers[0].cache->insertion_count == 0 && f->worlds[0]->fields.b->count == 0 &&
           f->worlds[0]->fields.u->count == 0 &&
           f->worlds[0]->active_calls == s->before.active_calls &&
           f->worlds[0]->persistent_bindings == s->before.bindings);
    return 0;
}

static int si_math_failure(insert_fixture *s, bool missing) {
    fixture *f = &s->cq.source.game;
    s->out = true;
    s->status = MC_WORLD_STORAGE_IO_ERROR;
    VERIFY(si_take(s) == 0 && !si_call(s, false) && s->entries == 1 && s->out &&
           s->status == MC_WORLD_STORAGE_OK && s->error.status == MC_WORLD_STORAGE_OK &&
           s->error.cause.status == MC_WORLD_STORAGE_OK && s->error.bytecode == 8 &&
           s->error.operation == NULL && s->frame.entered && s->frame.body_entered &&
           !s->frame.body_completed && !s->frame.completed && !s->frame.get_returned &&
           !s->frame.chunk_returned && !s->frame.observer_returned &&
           s->cq.result.ownership_status == MC_WORLD_STORAGE_OK && si_quiet(&s->cq.result) &&
           s->math.failed && s->math.selected && s->math.first.caller.pc == 8 &&
           strcmp(s->math.first.caller.class_name, "fd") == 0 &&
           strcmp(s->math.first.caller.method, "b") == 0 &&
           strcmp(s->math.first.caller.descriptor, "(Lsn;)Z") == 0 &&
           s->math.first.cause.status ==
               (missing ? MC_MATH_CLASS_INVALID_ARGUMENT : MC_MATH_CLASS_LIMIT));
    if (missing)
        VERIFY(s->math.owned == NULL && s->math.first.cause.source_pc == -1);
    else {
        mc_math_class_state math = {0};
        VERIFY(s->math.owned == s->math_owner &&
               mc_math_classes_state(s->math_owner, &math, NULL) == MC_MATH_CLASS_OK &&
               math.phase == MC_MATH_CLASS_FAILED && math.source_a == NULL && math.holds == 2 &&
               s->math.first.cause.source_pc == 2);
    }
    VERIFY(memcmp(s->before.entity, s->item, sizeof(*s->item)) == 0 && si_inputs_same(s) &&
           cq_loader_same(&s->cq, &s->before.loader) && f->providers[0].cache->count == 0 &&
           f->providers[0].cache->insertion_count == 0 && f->worlds[0]->fields.b->count == 0 &&
           f->worlds[0]->fields.u->count == 0 &&
           f->worlds[0]->active_calls == s->before.active_calls &&
           f->worlds[0]->persistent_bindings == s->before.bindings);
    return 0;
}

static int si_stop(insert_fixture *s) {
    /* Only the exact created caller scope, once. No Source retry/get/cancel or
     * ambient unwind. Unexpected pending children refuse and retain the graph. */
    VERIFY(si_quiet(&s->cq.result));
    if (s->scope_open) {
        VERIFY(mc_entity_tick_execution_pop_world(&s->cq.execution, &s->outer, !s->completed) ==
               MC_WORLD_STORAGE_OK);
        s->scope_open = false;
    }
    s->error = (mc_world_entity_error){0};
    s->frame.acquisition = (mc_world_runtime_acquisition_error){0};
    s->frame.chunk = (mc_world_chunk_entity_error){0};
    s->cq.result = (mc_world_runtime_execution_result){0};
    VERIFY(mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK &&
           mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK &&
           mc_math_classes_release(&s->math_owner, NULL) == MC_MATH_CLASS_OK);
    /* Actual constructor/Chunk/World holds are consumed by the existing real
     * terminal bodies; no membership, Chunk.d or Source field is repaired. */
    mc_entity_release(s->item);
    s->item = NULL;
    mc_item_stack_release(s->stack);
    s->stack = NULL;
    VERIFY((s->cq.source.game.states[0] == NULL ? cq_empty_stop(&s->cq.source)
                                                : ss_stop(&s->cq.source)) == 0 &&
           cq_files(&s->cq, false) == 0 && ss_finish(&s->cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->cq.inputs[i]);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && si_mode(argv[1]));
    printf("ENTITY_INSERT_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    insert_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->cq.mode = argv[1];
    VERIFY(ss_setup(&s->cq.source, argv[2]) == 0 && cq_files(&s->cq, true) == 0 &&
           ss_open(&s->cq.source) == 0);
    const bool missing = strcmp(argv[1], "math-missing") == 0;
    VERIFY(missing || mc_math_classes_create(strcmp(argv[1], "math-limit") == 0 ? 1024 : SIZE_MAX,
                                             &s->math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&s->math, s->math_owner);
    mc_entity_tick_execution_init(&s->cq.execution, &s->math);
    mc_world_entity_insert_execution_init(&s->frame, &s->cq.execution, s->cq.source.game.classes[0],
                                          cq_code, si_site, &s->cq.result);
    VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, s->cq.source.game.worlds[0],
                                               si_site, &s->outer) == MC_WORLD_STORAGE_OK);
    s->scope_open = true;
    if (strcmp(argv[1], "success") == 0) {
        mc_chunk_state *loaded = NULL;
        ++s->primes;
        VERIFY(mc_world_runtime_get(s->cq.source.game.worlds[0], 0, 0, &loaded) ==
                   MC_CHUNK_ACQUIRE_OK &&
               loaded != NULL && cq_remember(&s->cq) == 0 && loaded == s->cq.source.game.states[0]);
    }
    VERIFY(mc_item_stack_allocate(&s->stack) == MC_ITEM_STACK_OK &&
           mc_item_stack_construct(s->stack, 262, 5, 7) == MC_ITEM_STACK_OK &&
           mc_item_entity_allocate(s->cq.source.game.entity_classes[0], &s->item, NULL) ==
               MC_ENTITY_OK &&
           mc_item_entity_construct(s->item, s->cq.source.game.worlds[0], 8.5, 80.0, 8.5, s->stack,
                                    NULL) == MC_ENTITY_OK);
    printf("ENTITY_INSERT_CASE %s BEGIN\n", argv[1]);
    fflush(stdout);
    const int result = strcmp(argv[1], "preflight") == 0 ? si_preflight(s)
                       : strcmp(argv[1], "success") == 0 ? si_success(s)
                       : strcmp(argv[1], "absent") == 0  ? si_absent(s)
                                                         : si_math_failure(s, missing);
    const size_t entries = s->entries, primes = s->primes;
    const int stopped = si_stop(s); /* Reached native failures still use checked cleanup. */
    if (stopped != 0) {
        fputs("Entity insert cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    free(s);
    VERIFY(result == 0 && entries == 1);
    printf("ENTITY_INSERT_CASE %s END entries=%zu primes=%zu\n", argv[1], entries, primes);
    printf("ENTITY_INSERT_OPEN malformed-GET/Blank/callback-native-fault/parent/full-SL\n");
    printf("PASS: %u actual owned single-Entity insertion checks;mode=%s\n", checks, argv[1]);
    return 0;
}
