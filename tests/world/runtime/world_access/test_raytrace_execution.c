/* Reuse the genuine SourceSaving bootstrap; no imported main is executed. */
int re_unused_query_main(int, char **);
#define MC_WORLD_COLLISION_QUERY_MAIN re_unused_query_main
#include "test_collision_query_execution.c"
#undef MC_WORLD_COLLISION_QUERY_MAIN

#include "mc/entity/hit_result.h"
#include "world/runtime/raytrace_execution_internal.h"
#include <math.h>

typedef struct re_fixture {
    cq_fixture cq;
    mc_math_classes *math_owner;
    mc_math_execution math;
    mc_world_raytrace_call *ray;
    mc_world_raytrace_state state;
    mc_world_raytrace_execution_state entered;
    mc_world_raytrace_status status;
    mc_entity_tick_execution_world_token world;
    mc_world_runtime *evaluated_world; /* Borrowed under this exact created scope. */
    mc_entity_vector *start, *end;
    mc_entity_hit_result *hit;
    mc_entity_vector_snapshot vectors_before, vectors_entered;
    mc_math_class_error observed_math_error;
    mc_math_execution_site observed_math_child;
    size_t bindings, calls, bounds_before;
    int32_t bounds_cursor_entered;
    bool completed;
    bool observed_math_failure;
} re_fixture;

static bool re_quiet(const mc_world_runtime_execution_result *r) {
    if (r->ownership_status != MC_WORLD_STORAGE_OK || r->selection_ready || r->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (r->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (r->closes[i].used)
            return false;
    return true;
}

static bool re_no_continuation(const mc_world_runtime_execution_result *r) {
    if (r->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (r->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (r->closes[i].used)
            return false;
    return true;
}

static bool re_math(re_fixture *s, mc_math_class_phase phase, size_t holds) {
    mc_math_class_state view;
    return mc_math_classes_state(s->math_owner, &view, NULL) == MC_MATH_CLASS_OK &&
           view.phase == phase && view.holds == holds &&
           (view.source_a != NULL) == (phase == MC_MATH_CLASS_READY);
}

static bool re_random(const mc_java_random *a, const mc_java_random *b) {
    uint64_t ga, gb;
    memcpy(&ga, &a->next_next_gaussian, sizeof(ga));
    memcpy(&gb, &b->next_next_gaussian, sizeof(gb));
    return a->state == b->state && ga == gb &&
           a->have_next_next_gaussian == b->have_next_next_gaussian;
}

static int re_enter(re_fixture *s) {
    fixture *f = &s->cq.source.game;
    const char *mode = s->cq.mode;
    const bool clear = strcmp(mode, "clear") == 0;
    const bool null_end = strcmp(mode, "null-end") == 0;
    const bool shape = strcmp(mode, "state-shape") == 0;
    s->evaluated_world = f->worlds[0];
    s->bindings = s->evaluated_world->persistent_bindings;
    VERIFY(mc_entity_tick_execution_push_world(
               &s->cq.execution, s->evaluated_world,
               (mc_entity_tick_execution_site){"native.owned.ray", "trace", "()V", 0},
               &s->world) == MC_WORLD_STORAGE_OK &&
           s->evaluated_world->persistent_bindings == s->bindings + 1);
    /* Actual receiver is owned before the genuine later vector factories. */
    VERIFY(mc_entity_vector_get_snapshot(f->entity_classes[0], &s->vectors_before) == MC_ENTITY_OK);
    s->bounds_before = f->entity_classes[0]->bounds_holds;
    VERIFY(mc_entity_vector_create(f->entity_classes[0], clear ? NAN : 2.5, shape ? 64.25 : 64.5,
                                   4.5, &s->start, NULL) == MC_ENTITY_OK);
    if (!clear && !null_end)
        VERIFY(mc_entity_vector_create(f->entity_classes[0], 6.5, shape ? 64.25 : 64.5, 4.5,
                                       &s->end, NULL) == MC_ENTITY_OK);
    const mc_world_collision_block_execution_code code = {
        cq_code, f->registry[0], f->classes[0], {"lm", "d", "(III)Low;", 67}};
    s->status = MC_WORLD_RAYTRACE_OK; /* A number alone is never a return witness. */
    ++s->calls;
    s->completed = mc_world_raytrace_flags_begin_in_execution(
        &s->cq.execution, f->classes[0], s->evaluated_world, code, s->start, s->end, false,
        strcmp(mode, "block") == 0, &s->ray, &s->status, &s->cq.result);
    VERIFY(
        s->ray != NULL && mc_world_raytrace_inspect(s->ray, &s->state) &&
        mc_world_raytrace_execution_inspect(s->ray, &s->entered) && !s->state.active &&
        !s->state.retiring && s->state.world == f->worlds[0] && s->state.start == s->start &&
        s->state.end == s->end && s->entered.owned == &s->cq.result &&
        f->worlds[0]->active_calls == 0 && f->worlds[0]->persistent_bindings == s->bindings + 2 &&
        mc_entity_vector_get_snapshot(f->entity_classes[0], &s->vectors_entered) == MC_ENTITY_OK);
    s->bounds_cursor_entered = f->entity_classes[0]->bounds_cursor;
    return 0;
}

static int re_success(re_fixture *s) {
    fixture *f = &s->cq.source.game;
    const bool clear = strcmp(s->cq.mode, "clear") == 0;
    const bool miss = strcmp(s->cq.mode, "miss") == 0;
    const bool shape = strcmp(s->cq.mode, "state-shape") == 0;
    VERIFY(s->completed && s->status == MC_WORLD_RAYTRACE_OK && s->state.source_returned &&
           s->state.phase == MC_WORLD_RAYTRACE_RETURNED && s->state.failure == NULL &&
           !s->entered.query_failed && !s->entered.native_stopped && !s->entered.math_failed &&
           s->entered.collision == NULL && re_quiet(&s->cq.result) &&
           s->cq.execution.first.status == MC_WORLD_STORAGE_OK &&
           s->cq.execution.phase == MC_ENTITY_TICK_EXECUTION_OPEN &&
           !s->cq.execution.capture_pending && s->cq.execution.receipt_count == 0);
    if (clear) {
        VERIFY(s->state.source_pc == 30 && s->state.cells_started == 0 &&
               s->state.cells_read == 0 && s->state.steps_started == 0 && s->state.result == NULL &&
               isnan(s->start->x) && s->state.end == NULL && s->math.owned == NULL &&
               !s->math.failed && re_math(s, MC_MATH_CLASS_COLD, 1) &&
               f->providers[0].cache->count == 0);
        return 0;
    }
    VERIFY(s->math.owned == s->math_owner && !s->math.failed && !s->math.selected &&
           re_math(s, MC_MATH_CLASS_READY, 2) && cq_remember(&s->cq) == 0 &&
           f->providers[0].cache->count == 1 && f->providers[0].cache->insertion_count == 1);
    if (miss) {
        VERIFY(s->state.source_pc == 284 && s->state.result == NULL &&
               s->state.steps_started == 5 && s->state.cells_started == 5 &&
               s->state.cells_read == 5 && s->state.block_ray_calls == 0 &&
               s->state.predicate_calls == 0 && s->start->x == 6 && s->start->y == 64.5 &&
               s->start->z == 4.5);
        return 0;
    }
    VERIFY(s->state.source_pc == 930 && s->state.steps_started == 2 &&
           s->state.cells_started == 3 && s->state.cells_read == 3 &&
           s->state.block_ray_calls == 1 && s->state.predicate_calls == 1 &&
           s->state.movement_box_calls == (shape ? 0u : 1u) && s->state.result != NULL);
    mc_entity_hit_result_view hit;
    VERIFY(mc_entity_hit_result_inspect(s->state.result, &hit) == MC_ENTITY_HIT_OK &&
           hit.classes == f->entity_classes[0] && hit.phase == MC_ENTITY_HIT_READY &&
           hit.fields.b == 4 && hit.fields.c == 64 && hit.fields.d == 4 && hit.fields.e == 4 &&
           hit.fields.g == NULL && hit.fields.f != NULL && hit.fields.f->x == 4 &&
           hit.fields.f->y == (shape ? 64.25 : 64.5) && hit.fields.f->z == 4.5 &&
           s->start->x == 4 && s->start->y == (shape ? 64.25 : 64.5) && s->start->z == 4.5);
    VERIFY(mc_entity_hit_result_retain(s->state.result, NULL) == MC_ENTITY_HIT_OK);
    s->hit = s->state.result; /* A genuine independent occurrence, not a copied pointer owner. */
    if (shape) {
        const mc_block *rail = NULL;
        VERIFY(mc_registry_block_get(f->registry[0], 66, &rail, NULL) == MC_REGISTRY_OK &&
               rail != NULL && rail->kind == MC_BLOCK_RAIL && rail->fields.bs == 0 &&
               rail->fields.bt == 0 && rail->fields.bu == 0 && rail->fields.bv == 1 &&
               rail->fields.bw == .625 && rail->fields.bx == 1);
    }
    return 0;
}

static int re_prefix_failure(re_fixture *s) {
    fixture *f = &s->cq.source.game;
    const bool null_end = strcmp(s->cq.mode, "null-end") == 0;
    const bool storage = strcmp(s->cq.mode, "math-storage") == 0;
    VERIFY(!s->completed && !s->state.source_returned &&
           s->state.phase == MC_WORLD_RAYTRACE_FAILED && s->state.result == NULL &&
           s->state.cells_started == 0 && s->state.steps_started == 0 && !s->entered.query_failed &&
           s->entered.collision == NULL && re_quiet(&s->cq.result) &&
           f->providers[0].cache->count == 0 && f->providers[0].cache->insertion_count == 0 &&
           s->cq.execution.phase == MC_ENTITY_TICK_EXECUTION_OPEN && s->math.owned == NULL &&
           re_math(s, MC_MATH_CLASS_COLD, 1) && s->start->x == 2.5 && s->start->y == 64.5 &&
           s->start->z == 4.5);
    if (null_end) {
        VERIFY(s->status == MC_WORLD_RAYTRACE_SOURCE_EXCEPTION && s->state.source_pc == 33 &&
               s->state.failure != NULL && s->state.failure->source_exception &&
               !s->state.failure->native_policy && s->state.failure->exception_class != NULL &&
               strcmp(s->state.failure->exception_class, "java.lang.NullPointerException") == 0 &&
               !s->entered.math_failed && !s->entered.native_stopped && !s->math.failed);
    } else {
        VERIFY(s->status == MC_WORLD_RAYTRACE_STATE && s->state.source_pc == 68 &&
               s->state.failure != NULL && s->state.failure->native_policy &&
               !s->state.failure->source_exception && s->entered.native_stopped &&
               s->entered.math_failed &&
               s->entered.math_error.status ==
                   (storage ? MC_MATH_CLASS_STATE_ERROR : MC_MATH_CLASS_INVALID_ARGUMENT) &&
               s->entered.math_error.operation != NULL &&
               s->entered.math_caller.class_name != NULL &&
               strcmp(s->entered.math_caller.class_name, "fd") == 0 &&
               s->entered.math_caller.method != NULL &&
               strcmp(s->entered.math_caller.method, "a") == 0 &&
               s->entered.math_caller.descriptor != NULL &&
               strcmp(s->entered.math_caller.descriptor, "(Lbt;Lbt;ZZ)Lvf;") == 0 &&
               s->entered.math_caller.pc == 68 && s->entered.math_child.class_name != NULL &&
               strcmp(s->entered.math_child.class_name, "in") == 0 &&
               s->entered.math_child.method != NULL && s->entered.math_child.descriptor != NULL &&
               strcmp(s->entered.math_child.method, "b") == 0 &&
               strcmp(s->entered.math_child.descriptor, "(D)I") == 0 &&
               s->entered.math_child.pc == -1 && s->math.failed == !storage);
        if (!storage)
            VERIFY(s->math.first.caller.pc == 68 &&
                   s->math.first.cause.status == s->entered.math_error.status &&
                   s->math.first.cause.operation == s->entered.math_error.operation);
        s->observed_math_failure = true;
        s->observed_math_error = s->entered.math_error;
        s->observed_math_child = s->entered.math_child;
    }
    return 0;
}

static int re_loader_failure(re_fixture *s) {
    fixture *f = &s->cq.source.game;
    VERIFY(!s->completed && s->status == MC_WORLD_RAYTRACE_SOURCE_EXCEPTION &&
           !s->state.source_returned && s->state.source_pc == 125 && s->state.cells_started == 1 &&
           s->state.cells_read == 0 && s->state.steps_started == 0 &&
           s->state.block_ray_calls == 0 && s->state.failure != NULL &&
           s->state.failure->source_exception && !s->state.failure->native_policy &&
           s->entered.query_failed && !s->entered.math_failed && !s->entered.native_stopped &&
           s->entered.collision != NULL &&
           s->entered.collision->nearby.query.status == MC_WORLD_STORAGE_INVALID_DATA &&
           s->entered.collision->nearby.query.acquisition == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
           s->cq.result.ownership_status == MC_WORLD_STORAGE_OK && s->cq.result.selection_ready &&
           re_no_continuation(&s->cq.result) &&
           cq_token_same(s->cq.result.selection.world, s->world) &&
           mc_entity_tick_execution_loader_read(&s->cq.execution, &s->cq.result.selection,
                                                &s->cq.view) &&
           s->cq.view.status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED && s->cq.view.failure != NULL &&
           s->entered.collision->nearby.query.source_chunk == s->cq.view.failure &&
           s->state.failure->source_chunk == s->cq.view.failure);
    s->cq.selected = s->cq.result.selection;
    /* Only AFTER explicit token selection, validate the actual producer graph. */
    mc_world_runtime_source_construction_state construction;
    mc_world_source_chunk_loader_state producer;
    VERIFY(mc_world_runtime_source_construction_inspect(f->worlds[0], &construction) &&
           construction.loader != NULL &&
           mc_world_source_chunk_loader_inspect(construction.loader, &producer) &&
           producer.phase == MC_WORLD_SOURCE_FAILED && !producer.active && producer.last_pc == 18 &&
           producer.reader != NULL && producer.record != NULL && producer.failure != NULL &&
           s->cq.view.failure != producer.failure &&
           kl_error_same(s->cq.view.failure, producer.failure) &&
           s->cq.view.failure->source.source_exception &&
           !s->cq.view.failure->source.native_policy &&
           s->cq.view.failure->input.source_fault == MC_NBT_SOURCE_ROOT &&
           s->cq.view.failure->input.source_class != NULL &&
           strcmp(s->cq.view.failure->input.source_class, "as") == 0 &&
           s->cq.view.failure->input.source_method != NULL &&
           strcmp(s->cq.view.failure->input.source_method, "a(Ljava/io/DataInput;)Lnu;") == 0 &&
           s->cq.view.failure->input.source_pc == 23 &&
           s->cq.view.failure->storage.status == MC_WORLD_STORAGE_INVALID_DATA &&
           s->cq.execution.loader_count == 1 &&
           s->cq.execution.phase == MC_ENTITY_TICK_EXECUTION_FAILED &&
           s->cq.execution.first.status == MC_WORLD_STORAGE_OK &&
           f->providers[0].cache->count == 0 && f->providers[0].cache->insertion_count == 0 &&
           s->math.owned == s->math_owner && !s->math.failed && re_math(s, MC_MATH_CLASS_READY, 2));
    return 0;
}

static int re_drain(re_fixture *s) {
    fixture *f = &s->cq.source.game;
    const bool failed = !s->completed;
    const bool loader = strcmp(s->cq.mode, "loader-error") == 0;
    /* Borrowed Ray views expire at actual call retirement; copied native Math
     * observations and the independently retained VF remain separate. */
    s->state = (mc_world_raytrace_state){0};
    s->entered = (mc_world_raytrace_execution_state){0};
    VERIFY(mc_world_raytrace_retire(&s->ray) == MC_WORLD_RAYTRACE_OK && s->ray == NULL &&
           f->worlds[0]->persistent_bindings == s->bindings + 1);
    if (s->observed_math_failure)
        VERIFY(s->observed_math_error.status != MC_MATH_CLASS_OK &&
               s->observed_math_error.operation != NULL &&
               s->observed_math_child.class_name != NULL &&
               strcmp(s->observed_math_child.class_name, "in") == 0 &&
               s->observed_math_child.pc == -1);
    mc_entity_vector_snapshot after;
    VERIFY(mc_entity_vector_get_snapshot(f->entity_classes[0], &after) == MC_ENTITY_OK &&
           after.pool_identity == s->vectors_entered.pool_identity &&
           after.cursor == s->vectors_entered.cursor &&
           f->entity_classes[0]->bounds_holds == s->bounds_before &&
           f->entity_classes[0]->bounds_cursor == s->bounds_cursor_entered);
    if (s->hit != NULL) {
        mc_entity_hit_result_view view;
        VERIFY(mc_entity_hit_result_inspect(s->hit, &view) == MC_ENTITY_HIT_OK &&
               view.phase == MC_ENTITY_HIT_READY && view.native_holds == 1 && view.fields.b == 4 &&
               view.fields.c == 64 && view.fields.d == 4 && view.fields.e == 4 &&
               view.fields.f != NULL && view.fields.f->x == 4);
    }
    VERIFY(mc_entity_hit_result_release(&s->hit, NULL) == MC_ENTITY_HIT_OK);
    VERIFY(mc_entity_vector_release(f->entity_classes[0], s->start) == MC_ENTITY_OK);
    s->start = NULL;
    if (s->end != NULL) {
        VERIFY(mc_entity_vector_release(f->entity_classes[0], s->end) == MC_ENTITY_OK);
        s->end = NULL;
    }
    VERIFY(mc_entity_vector_get_snapshot(f->entity_classes[0], &after) == MC_ENTITY_OK &&
           after.holds == s->vectors_before.holds &&
           mc_entity_tick_execution_pop_world(&s->cq.execution, &s->world, failed) ==
               MC_WORLD_STORAGE_OK &&
           !s->cq.execution.world_open &&
           f->worlds[0]->persistent_bindings == s->bindings + (failed ? 1u : 0u));
    if (loader)
        VERIFY(
            mc_entity_tick_execution_loader_read(&s->cq.execution, &s->cq.selected, &s->cq.view) &&
            s->cq.view.failure != NULL && s->cq.view.failure->input.source_pc == 23);
    VERIFY(re_no_continuation(&s->cq.result));
    s->cq.view = (mc_entity_tick_execution_loader_view){0};
    s->cq.result =
        (mc_world_runtime_execution_result){0}; /* Ordinary completed VALUEs, no rights. */
    VERIFY(mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK &&
           f->worlds[0]->persistent_bindings == s->bindings &&
           mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK &&
           mc_math_classes_release(&s->math_owner, NULL) == MC_MATH_CLASS_OK);
    s->evaluated_world = NULL;
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 &&
           (strcmp(argv[1], "clear") == 0 || strcmp(argv[1], "miss") == 0 ||
            strcmp(argv[1], "block") == 0 || strcmp(argv[1], "state-shape") == 0 ||
            strcmp(argv[1], "null-end") == 0 || strcmp(argv[1], "math-missing") == 0 ||
            strcmp(argv[1], "math-storage") == 0 || strcmp(argv[1], "loader-error") == 0));
    printf("RAY_EXECUTION_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    re_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->cq.mode = argv[1];
    VERIFY(ss_setup(&s->cq.source, argv[2]) == 0 && cq_files(&s->cq, true) == 0 &&
           ss_open(&s->cq.source) == 0 &&
           mc_math_classes_create(SIZE_MAX, &s->math_owner, NULL) == MC_MATH_CLASS_OK &&
           re_math(s, MC_MATH_CLASS_COLD, 1));
    mc_math_execution_init(&s->math, strcmp(argv[1], "math-missing") == 0 ? NULL : s->math_owner);
    const bool no_storage = strcmp(argv[1], "math-storage") == 0 || strcmp(argv[1], "clear") == 0;
    mc_entity_tick_execution_init(&s->cq.execution, no_storage ? NULL : &s->math);
    const mc_java_random random = *s->cq.source.game.worlds[0]->fields.r;
    printf("RAY_EXECUTION_CASE %s BEGIN\n", argv[1]);
    fflush(stdout);
    VERIFY(re_enter(s) == 0);
    if (strcmp(argv[1], "loader-error") == 0)
        VERIFY(re_loader_failure(s) == 0);
    else if (strcmp(argv[1], "null-end") == 0 || strncmp(argv[1], "math-", 5) == 0)
        VERIFY(re_prefix_failure(s) == 0);
    else
        VERIFY(re_success(s) == 0);
    VERIFY(s->calls == 1 && re_random(s->cq.source.game.worlds[0]->fields.r, &random) &&
           re_drain(s) == 0);
    printf("RAY_EXECUTION_CASE %s END calls=%zu\n", argv[1], s->calls);
    fflush(stdout);
    VERIFY((s->cq.source.game.states[0] == NULL ? cq_empty_stop(&s->cq.source)
                                                : ss_stop(&s->cq.source)) == 0 &&
           cq_files(&s->cq, false) == 0 && ss_finish(&s->cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->cq.inputs[i]);
    free(s);
    puts("RAY_EXECUTION_OPEN "
         "GET-OK-native-refusal/callbacks/unpublished-close/foreign-graph/full-Arrow");
    printf("PASS: %u actual owned Ray checks;mode=%s\n", checks, argv[1]);
    return 0;
}
