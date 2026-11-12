/* Actual SourceSaving/Registry/Entity setup and checked terminal bodies. */
int drop_unused_query_main(int, char **);
#define MC_WORLD_COLLISION_QUERY_MAIN drop_unused_query_main
#include "../../world/runtime/world_access/test_collision_query_execution.c"
#undef MC_WORLD_COLLISION_QUERY_MAIN

#include "entity/drop/internal.h"

typedef struct drop_fixture {
    cq_fixture cq;
    mc_math_classes *math_owner;
    mc_math_execution math;
    mc_entity_tick_execution_world_token outer;
    mc_entity_tick_execution_world_token extra[MC_ENTITY_TICK_EXECUTION_WORLDS];
    size_t extra_count;
    mc_entity *actor;
    mc_entity_drop_call **call;
    size_t callbacks, saved_bindings;
    bool valid;
} drop_fixture;

static bool drop_is(const drop_fixture *s, const char *mode) {
    return strcmp(s->cq.mode, mode) == 0;
}

static bool drop_math_ready(const drop_fixture *s) {
    mc_math_class_state state;
    return s->math.owned == s->math_owner && s->math.declared == s->math_owner &&
           !s->math.selected && !s->math.failed &&
           mc_math_classes_state(s->math_owner, &state, NULL) == MC_MATH_CLASS_OK &&
           state.phase == MC_MATH_CLASS_READY && state.holds == 2 && state.source_a != NULL &&
           mc_math_classes_table(s->math_owner) == state.source_a;
}

static mc_world_storage_status drop_added(void *context, mc_world_runtime *world, mc_entity *item,
                                          mc_world_storage_error *cause,
                                          mc_world_access_failure *failure) {
    drop_fixture *s = context;
    ++s->callbacks;
    mc_entity_drop_state state;
    mc_entity_drop_execution_view view;
    mc_entity_drop_error cleanup;
    mc_entity_tick_execution_world_token current;
    s->valid &= s->callbacks == 1 && mc_entity_drop_inspect(*s->call, &state) && state.active &&
                state.item == item && state.item_constructed && state.pickup_delay_stored &&
                state.publication_entered && !state.publication_returned &&
                state.constructor_world == world && state.publication_world == world &&
                item->base.bF && item->base.bH == 5 && world->fields.b->count == 1 &&
                world->fields.b->items[0] == item &&
                mc_entity_drop_execution_inspect(*s->call, &view) &&
                view.constructor_world.consumed && view.publication_world.open &&
                mc_entity_tick_execution_current_world(&s->cq.execution, &current) &&
                cq_token_same(current, view.publication_world.world) &&
                s->cq.execution.world_slots[current.slot].site.pc == 35 &&
                mc_entity_drop_retire(s->call, &cleanup) == MC_ENTITY_BUSY;
    if (drop_is(s, "callback-failure") || drop_is(s, "callback-capture")) {
        bool removed = false;
        const mc_world_storage_status status =
            mc_world_runtime_remove_world_access(world, s->cq.receiver, &removed, cause);
        if (status != MC_WORLD_STORAGE_OK || !removed)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        s->cq.removed = true;
        const mc_world_storage_status created =
            mc_world_access_receipt_create(s->cq.receiver, NULL, &s->cq.receipt, failure, cause);
        if (created != MC_WORLD_STORAGE_OK)
            return created;
        s->cq.receipt->exception_class = "java/lang/IllegalStateException";
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "Owned drop actual WorldAccess failure", 57};
    }
    if (drop_is(s, "native-close") || drop_is(s, "callback-capture")) {
        s->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Temporary downward native-counter fault. */
    }
    return drop_is(s, "callback-failure") || drop_is(s, "callback-capture")
               ? MC_WORLD_STORAGE_IO_ERROR
               : MC_WORLD_STORAGE_OK;
}

static int drop_stop(drop_fixture *s, bool completed) {
    while (s->extra_count != 0) {
        --s->extra_count;
        VERIFY(mc_entity_tick_execution_pop_world(&s->cq.execution, &s->extra[s->extra_count],
                                                  true) == MC_WORLD_STORAGE_OK);
    }
    VERIFY(mc_entity_tick_execution_pop_world(&s->cq.execution, &s->outer, !completed) ==
               MC_WORLD_STORAGE_OK &&
           mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK &&
           mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK &&
           mc_math_classes_release(&s->math_owner, NULL) == MC_MATH_CLASS_OK);
    if (s->cq.receipt != NULL)
        VERIFY(mc_world_access_receipt_retire(&s->cq.receipt, NULL) == MC_WORLD_STORAGE_OK);
    if (s->cq.receiver != NULL) {
        if (!s->cq.removed) {
            bool removed = false;
            VERIFY(mc_world_runtime_remove_world_access(s->cq.source.game.worlds[0], s->cq.receiver,
                                                        &removed, NULL) == MC_WORLD_STORAGE_OK &&
                   removed);
        }
        VERIFY(mc_world_access_free(s->cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
        s->cq.receiver = NULL;
    }
    mc_entity_release(s->actor);
    s->actor = NULL;
    VERIFY(ss_stop(&s->cq.source) == 0 && cq_files(&s->cq, false) == 0 &&
           ss_finish(&s->cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->cq.inputs[i]);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3);
    drop_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->cq.mode = argv[1];
    s->valid = true;
    VERIFY(ss_setup(&s->cq.source, argv[2]) == 0 && cq_files(&s->cq, true) == 0 &&
           ss_open(&s->cq.source) == 0);
    fixture *f = &s->cq.source.game;
    VERIFY(drop_is(s, "math-missing") ||
           mc_math_classes_create(drop_is(s, "math-limit") ? 1024 : SIZE_MAX, &s->math_owner,
                                  NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&s->math, s->math_owner);
    mc_entity_tick_execution_init(&s->cq.execution, &s->math);
    const mc_entity_tick_execution_site site = {"native.drop.control", "run", "()V", 0};
    VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, f->worlds[0], site, &s->outer) ==
           MC_WORLD_STORAGE_OK);
    mc_chunk_state *loaded = NULL;
    VERIFY(mc_world_runtime_get(f->worlds[0], 0, 0, &loaded) == MC_CHUNK_ACQUIRE_OK &&
           loaded != NULL && cq_remember(&s->cq) == 0);
    VERIFY(mc_falling_entity_allocate(f->entity_classes[0], &s->actor, NULL) == MC_ENTITY_OK &&
           mc_falling_entity_construct(s->actor, f->worlds[0], 8.5, 80, 8.5, 12, NULL) ==
               MC_ENTITY_OK);
    const size_t members = f->worlds[0]->fields.b->count;
    mc_entity_drop_call *call = NULL;
    mc_entity_drop_error error = {0};
    mc_entity_drop_state state = {0};
    mc_entity_drop_execution_view execution;
    s->call = &call;
    if (drop_is(s, "preflight")) {
        const mc_entity_drop_error before = {.status = MC_ENTITY_LIMIT, .stack_pc = 73};
        error = before;
        mc_entity_status raw = MC_ENTITY_WORLD_FAILED;
        const size_t live = f->entity_classes[0]->live_entities;
        const size_t bindings = f->worlds[0]->persistent_bindings;
        VERIFY(!mc_entity_drop_numeric_begin_in_execution(NULL, f->classes[0], cq_code,
                                                          f->registry[0], s->actor, 35, 1, 0, &call,
                                                          &raw, &error, &s->cq.result) &&
               call == NULL && raw == MC_ENTITY_WORLD_FAILED &&
               memcmp(&error, &before, sizeof(error)) == 0 &&
               s->cq.result.ownership_status == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
               f->entity_classes[0]->live_entities == live &&
               f->worlds[0]->persistent_bindings == bindings && !s->math.selected &&
               !s->math.failed);
        s->cq.result = (mc_world_runtime_execution_result){0};
        VERIFY(drop_stop(s, true) == 0);
        free(s);
        printf("PASS: %u actual execution-owned Entity drop checks;mode=%s\n", checks, argv[1]);
        return 0;
    }
    if (drop_is(s, "capture-capacity"))
        while (s->extra_count + 1 < MC_ENTITY_TICK_EXECUTION_WORLDS) {
            VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, f->worlds[0], cq_site,
                                                       &s->extra[s->extra_count]) ==
                   MC_WORLD_STORAGE_OK);
            ++s->extra_count;
        }
    if (drop_is(s, "absent"))
        VERIFY(mc_entity_set_position(s->actor, 10000, 80, 10000) == MC_ENTITY_OK);
    if (drop_is(s, "nullable"))
        s->actor->base.world = NULL; /* Declared input; fixture's owning binding stays live. */
    const bool callback = drop_is(s, "success") || drop_is(s, "callback-failure") ||
                          drop_is(s, "callback-capture") || drop_is(s, "native-close");
    if (callback) {
        const mc_world_access_source_callbacks callbacks = {.entity_added = drop_added};
        VERIFY(mc_world_access_source_create(&callbacks, s, &s->cq.receiver, NULL) ==
                   MC_WORLD_STORAGE_OK &&
               mc_world_runtime_add_world_access(f->worlds[0], s->cq.receiver, NULL) ==
                   MC_WORLD_STORAGE_OK);
    }
    /* The owned entry must still construct Item, then stop at actual FD.b8
     * before Chunk/World publication, with raw Source status still OK. */
    mc_entity_status raw = MC_ENTITY_WORLD_FAILED;
    const bool completed =
        drop_is(s, "absent")
            ? mc_entity_drop_stack_begin_in_execution(&s->cq.execution, f->classes[0], cq_code,
                                                      f->registry[0], s->actor, NULL, .25f, &call,
                                                      &raw, &error, &s->cq.result)
            : mc_entity_drop_numeric_begin_in_execution(&s->cq.execution, f->classes[0], cq_code,
                                                        f->registry[0], s->actor, 35, 1, .25f,
                                                        &call, &raw, &error, &s->cq.result);
    if (s->saved_bindings != 0)
        f->worlds[0]->persistent_bindings = s->saved_bindings;
    if (drop_is(s, "nullable"))
        s->actor->base.world = f->worlds[0];
    VERIFY(mc_entity_drop_inspect(call, &state) &&
           mc_entity_drop_execution_inspect(call, &execution));
    bool passed;
    if (drop_is(s, "capture-capacity"))
        passed = !completed && raw == MC_ENTITY_OK && state.item_allocated &&
                 !state.item_constructed && state.item->phase == MC_ENTITY_RAW &&
                 state.stack_pc == 5 && state.x == 0 && !execution.constructor_world.created &&
                 !execution.publication_world.created && !s->math.selected &&
                 execution.result->ownership_status == MC_WORLD_STORAGE_LIMIT;
    else if (drop_is(s, "math-missing") || drop_is(s, "math-limit"))
        passed = !completed && raw == MC_ENTITY_OK && state.item_constructed &&
                 state.pickup_delay_stored && !state.returned && !state.publication_returned &&
                 !state.inserted && state.stack_pc == 39 && s->math.failed &&
                 s->math.first.caller.pc == 8 &&
                 strcmp(s->math.first.caller.class_name, "fd") == 0 &&
                 f->worlds[0]->fields.b->count == members && execution.publication_returned &&
                 execution.publication_status == MC_WORLD_STORAGE_OK &&
                 execution.result->ownership_status == MC_WORLD_STORAGE_OK;
    else if (drop_is(s, "absent"))
        passed = completed && raw == MC_ENTITY_OK && state.returned && state.item_constructed &&
                 state.pickup_delay_stored && state.publication_returned && !state.inserted &&
                 state.item->stack == NULL && state.y == 80.25 && state.stack_pc == 44 &&
                 !execution.publication->get_returned && drop_math_ready(s) &&
                 f->worlds[0]->fields.b->count == members;
    else if (drop_is(s, "nullable"))
        passed = !completed && raw == MC_ENTITY_STATE_ERROR && state.item_constructed &&
                 state.pickup_delay_stored && state.constructor_world == NULL &&
                 state.publication_world == NULL && state.item->base.world == NULL &&
                 error.source_exception && !error.native_policy && error.site.pc == 39 &&
                 !strcmp(error.exception_class, "java/lang/NullPointerException") &&
                 !execution.publication_returned && !s->math.selected;
    else {
        passed = callback && s->callbacks == 1 && s->valid && state.item_constructed &&
                 state.pickup_delay_stored && drop_math_ready(s) &&
                 f->worlds[0]->fields.b->count == members + 1 &&
                 f->worlds[0]->fields.b->items[members] == state.item && state.item->base.bF;
        if (drop_is(s, "success"))
            passed &= completed && raw == MC_ENTITY_OK && state.returned && state.inserted &&
                      state.publication_returned && execution.completed;
        else if (drop_is(s, "native-close")) {
            const mc_entity_drop_world_execution_view before = execution.publication_world;
            mc_entity_drop_call *same = call;
            passed &=
                !completed && raw == MC_ENTITY_OK && !state.returned && state.inserted &&
                state.publication_returned && before.open && before.close_attempted &&
                before.locals_drained && !before.failed && !before.consumed &&
                before.initial_close_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                mc_entity_drop_retire(&call, NULL) == MC_ENTITY_BUSY && call == same &&
                mc_entity_drop_native_close(call) == MC_WORLD_STORAGE_OK &&
                mc_entity_drop_execution_inspect(call, &execution) &&
                execution.publication_world.consumed &&
                execution.publication_world.native_close_attempted &&
                execution.publication_world.initial_close_status == before.initial_close_status &&
                !execution.publication_world.failed && !execution.completed;
        } else {
            const mc_world_access_failure *protected = NULL;
            passed &=
                !completed && raw == MC_ENTITY_WORLD_FAILED && error.publication != NULL &&
                error.publication->cause.system_error == 57 && s->cq.receipt != NULL &&
                s->cq.receipt->holds == 1 && call->receipt.receipt == NULL &&
                mc_world_access_failure_lease_inspect(
                    &s->cq.execution.leases[execution.publication_world.world.slot], &protected) &&
                protected->receipt == s->cq.receipt &&
                mc_world_access_receipt_retire(&s->cq.receipt, NULL) == MC_WORLD_STORAGE_BUSY;
            if (drop_is(s, "callback-failure"))
                passed &= error.source_exception && !error.native_policy &&
                          !strcmp(error.exception_class, "java/lang/IllegalStateException") &&
                          execution.result->ownership_status == MC_WORLD_STORAGE_OK;
            else
                passed &= execution.result->ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE;
            protected = NULL;
        }
    }
    if (!passed)
        fprintf(stderr, "Owned Entity drop source/ownership contract failed: %s\n", argv[1]);
    VERIFY(mc_entity_drop_native_close(call) == MC_WORLD_STORAGE_OK &&
           mc_entity_drop_execution_inspect(call, &execution) &&
           execution.result != &s->cq.result &&
           (drop_is(s, "capture-capacity") ||
            (execution.constructor_world.created && execution.constructor_world.consumed &&
             execution.publication_world.created && execution.publication_world.consumed)));
    if (drop_is(s, "success") || drop_is(s, "native-close")) {
        mc_entity_tick_execution_world_token probe, current;
        VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, NULL, cq_site, &probe) ==
                   MC_WORLD_STORAGE_OK &&
               mc_entity_drop_native_close(call) == MC_WORLD_STORAGE_OK &&
               mc_entity_tick_execution_current_world(&s->cq.execution, &current) &&
               cq_token_same(probe, current) &&
               mc_entity_tick_execution_pop_world(&s->cq.execution, &probe, false) ==
                   MC_WORLD_STORAGE_OK);
    }
    error = (mc_entity_drop_error){0};
    state = (mc_entity_drop_state){0};
    execution = (mc_entity_drop_execution_view){0};
    s->cq.result = (mc_world_runtime_execution_result){0};
    VERIFY(mc_entity_drop_retire(&call, NULL) == MC_ENTITY_OK && call == NULL);
    VERIFY(drop_stop(s, completed) == 0);
    free(s);
    VERIFY(passed);
    printf("PASS: %u actual execution-owned Entity drop checks;mode=%s\n", checks, argv[1]);
    return 0;
}
