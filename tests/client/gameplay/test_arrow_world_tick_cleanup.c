/* Headless actual World output -> SDK cleanup component. No Minecraft.k body
 * or App/startup/window success is supplied; the imported program never runs. */
int nsc_unused_natural_main(int, char **);
#define MC_ARROW_NATURAL_FLIGHT_MAIN nsc_unused_natural_main
#include "../../world/runtime/test_arrow_natural_flight.c"
#undef MC_ARROW_NATURAL_FLIGHT_MAIN

#include "client/runtime/world_tick.h"
#include "entity/arrow/tick_internal.h"
#include "world/runtime/world_access_internal.h"

typedef struct nsc_fixture {
    nf_fixture natural;
    mc_client_world_tick sdk;
    mc_world_access *receiver;
    mc_world_access_receipt *receipt;
    mc_world_access_failure_lease *node;
    mc_world_storage_error callback_cause;
    size_t callbacks, saved_bindings, world_calls;
    bool retry, callback_valid, removed, projected;
} nsc_fixture;

static bool nsc_cause_same(mc_world_storage_error a, mc_world_storage_error b) {
    return a.status == b.status && a.operation == b.operation && a.system_error == b.system_error;
}

static bool nsc_random_same(const mc_java_random *a, const mc_java_random *b) {
    return a->state == b->state && a->have_next_next_gaussian == b->have_next_next_gaussian &&
           ad_bits(a->next_next_gaussian) == ad_bits(b->next_next_gaussian);
}

static mc_world_storage_status nsc_sound(void *context, mc_world_runtime *world,
                                         const mc_registry_string *name, double x, double y,
                                         double z, float volume, float pitch,
                                         mc_world_storage_error *cause,
                                         mc_world_access_failure *failure) {
    nsc_fixture *s = context;
    mc_entity *arrow = s->natural.arrow;
    mc_arrow_tick_call *call = arrow->arrow->tick_call;
    ++s->callbacks;
    s->callback_valid &=
        world == s->natural.world->worlds[0] && s->callbacks == 1 && name != NULL &&
        x == arrow->base.x && y == arrow->base.y - (double)arrow->base.offset &&
        z == arrow->base.z && volume == 1.0f && pitch > 0 && call != NULL && call->state.active &&
        arrow->base.bt == 3 && call->ray == NULL && call->intercept == NULL &&
        call->candidate == NULL && call->selected == NULL && call->hurt_source == NULL &&
        call->hurt_target == NULL && !call->effect_cleanup_owned && world->active_calls == 2;
    s->node = call == NULL ? NULL : mc_entity_tick_execution_world_lease(&call->execution);
    s->callback_valid &= s->node != NULL && s->node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                         s->receiver->active_holds == 1;
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->receiver, &removed, cause);
    s->removed = status == MC_WORLD_STORAGE_OK && removed;
    if (!s->removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    status = mc_world_access_receipt_create(s->receiver, NULL, &s->receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    s->receipt->exception_class = "java/lang/IllegalStateException";
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "SDK cleanup authored block sound failure", 117};
    s->callback_cause = *cause;
    if (s->retry) {
        s->saved_bindings = world->persistent_bindings;
        s->callback_valid &= s->saved_bindings > 0;
        world->persistent_bindings = 0; /* Approved downward native fault only. */
        s->projected = true;
    }
    return cause->status;
}

/* Exact actual ownership occurrences, never a guessed correction constant.
 * The block-sound path has no retained Ray, victim or dying actor occurrence.
 * A checked SDK retirement may still consume some/all explicit World slots. */
static size_t nsc_world_rights(const mc_arrow_tick_call *call, const mc_world_runtime *world) {
    size_t count = 0;
    if (call != NULL)
        for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
            if (call->execution.world_slots[i].phase != MC_ENTITY_TICK_EXECUTION_WORLD_FREE &&
                call->execution.world_slots[i].held &&
                call->execution.world_slots[i].world == world)
                ++count;
    return count;
}

static bool nsc_run(nsc_fixture *s) {
    bool ok = false;
    nf_fixture *n = &s->natural;
    mc_world_runtime *world = n->world->worlds[0];
    const mc_world_access_source_callbacks callbacks = {.sound = nsc_sound};
    s->callback_valid = true;
    CHECK(mc_world_access_source_create(&callbacks, s, &s->receiver, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, s->receiver, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    /* These are three consecutive natural World invocations. Earlier returned
     * frames are consumed by World dispatch, with no state/Random reset. */
    for (size_t i = 0; i < 3; ++i) {
        ++s->world_calls;
        status = mc_world_runtime_tick_entities(world, &tick_context, NULL, &s->sdk.entities);
        if (status != MC_WORLD_STORAGE_OK || s->projected)
            break; /* Restore even an unexpectedly early actual callback before CHECK. */
        if (i < 2) {
            CHECK(status == MC_WORLD_STORAGE_OK && s->callbacks == 0 &&
                  n->arrow->arrow->tick_call == NULL &&
                  s->sdk.entities.child == MC_WORLD_ENTITY_TICK_CHILD_NONE &&
                  mc_client_world_tick_cleanup(&s->sdk));
        }
    }
    /* No assertions while the approved native counter projection is live. */
    mc_arrow_tick_call *const pending = s->sdk.entities.child == MC_WORLD_ENTITY_TICK_CHILD_ARROW
                                            ? s->sdk.entities.child_error.arrow.call
                                            : NULL;
    const mc_world_storage_status first_status = s->sdk.entities.status;
    const char *const first_operation = s->sdk.entities.operation;
    const unsigned int first_pc = s->sdk.entities.bytecode;
    const size_t code_before = n->world->classes[0]->native_borrows;
    const size_t actor_holds = n->arrow->holds;
    const size_t live_entities = n->world->entity_classes[0]->live_entities;
    const mc_java_random actor_random = *n->arrow->random, world_random = *world->fields.r;
    const size_t rights_before = nsc_world_rights(pending, world);
    bool first_cleanup = true;
    if (s->projected)
        first_cleanup = mc_client_world_tick_cleanup(&s->sdk);
    mc_arrow_tick_call *const remaining = s->sdk.entities.child == MC_WORLD_ENTITY_TICK_CHILD_ARROW
                                              ? s->sdk.entities.child_error.arrow.call
                                              : NULL;
    const size_t rights_after = nsc_world_rights(remaining, world);
    const size_t drained = rights_before >= rights_after ? rights_before - rights_after : 0;
    if (s->projected) {
        world->persistent_bindings = s->saved_bindings >= drained ? s->saved_bindings - drained : 0;
        s->projected = false;
    }
    CHECK(status != MC_WORLD_STORAGE_OK && s->world_calls == 3 && s->callbacks == 1 &&
          s->callback_valid && s->removed && pending != NULL && remaining == pending &&
          n->arrow->arrow->tick_call == pending && !s->sdk.active && !s->sdk.failed &&
          !s->sdk.returned && !s->sdk.entities_returned && world->active_calls == 0 &&
          live_entities == n->world->entity_classes[0]->live_entities &&
          actor_holds == n->arrow->holds && code_before == n->world->classes[0]->native_borrows &&
          n->arrow->base.bt == 3 && nsc_random_same(n->arrow->random, &actor_random) &&
          nsc_random_same(world->fields.r, &world_random) && code_before > 0 && rights_before > 0 &&
          rights_after == rights_before && drained == 0);
    mc_arrow_tick_state state;
    CHECK(mc_arrow_tick_inspect(remaining, &state) && !state.active && !state.source_returned &&
          state.receiver == n->arrow && state.source_pc == 1092 && state.ray_call == NULL &&
          state.intercept_call == NULL && state.failure != NULL && state.failure->sound != NULL &&
          state.failure->source_exception && !state.failure->native_policy &&
          state.failure->classification == MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION &&
          nsc_cause_same(state.failure->sound->cause, s->callback_cause));
    const mc_world_access_failure *view = NULL;
    CHECK(s->node != NULL && mc_world_access_failure_lease_inspect(s->node, &view) &&
          s->node->callback_status == s->callback_cause.status &&
          nsc_cause_same(s->node->callback_error, s->callback_cause) &&
          view->receiver == s->receiver && view->receipt == s->receipt &&
          mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_BUSY);
    if (s->retry) {
        CHECK(!first_cleanup &&
              s->sdk.entities.child_error.arrow.cleanup_status != MC_ARROW_TICK_OK &&
              s->sdk.entities.status == first_status &&
              s->sdk.entities.operation == first_operation &&
              s->sdk.entities.bytecode == first_pc && remaining->math.owned != NULL &&
              remaining->code_held && remaining->execution.world_open &&
              !remaining->execution.capture_pending &&
              remaining->query_result.ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              strcmp(remaining->query_result.ownership_error.operation,
                     "World Entity sound owned receipt capture") == 0);
    }
    mc_world_storage_error terminal = {0};
    CHECK(mc_world_runtime_retire_entity_containers(world, &terminal) == MC_WORLD_STORAGE_BUSY &&
          !world->entity_containers_retired && n->arrow->arrow->tick_call == pending &&
          s->sdk.entities.status == first_status && s->callbacks == 1 && s->world_calls == 3);
    if (s->retry)
        CHECK(world->local_leases == 0 && terminal.operation != NULL &&
              strcmp(terminal.operation, "World terminal Arrow active/pending tick call") == 0);
    /* Expire only our passive views. The SDK itself owns/clears actual child
     * diagnostics; no pending call or Source field is manufactured here. */
    view = NULL;
    memset(&state, 0, sizeof(state));
    CHECK(mc_client_world_tick_cleanup(&s->sdk) &&
          s->sdk.entities.child == MC_WORLD_ENTITY_TICK_CHILD_NONE &&
          n->arrow->arrow->tick_call == NULL && s->receiver->active_holds == 0 &&
          n->world->classes[0]->native_borrows + 1 == code_before && s->callbacks == 1 &&
          n->arrow->base.bt == 3 && nsc_random_same(n->arrow->random, &actor_random) &&
          nsc_random_same(world->fields.r, &world_random) && s->world_calls == 3 &&
          mc_client_world_tick_cleanup(&s->sdk) &&
          mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->receiver = NULL;
    s->node = NULL;
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    char root[8192];
    nsc_fixture *s = NULL;
    if (argc != 3 ||
        (strcmp(argv[1], "source-failure") != 0 && strcmp(argv[1], "counter-retry") != 0) ||
        !decode_path(argv[2], root, sizeof(root)))
        return 2;
    printf("ARROW_SDK_CLEANUP_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    s = calloc(1, sizeof(*s));
    CHECK(s != NULL);
    s->retry = strcmp(argv[1], "counter-retry") == 0;
    CHECK(nf_setup(&s->natural, root, &s->sdk.entities) && nf_prepare_block(&s->natural) &&
          nf_construct(&s->natural));
    printf("ARROW_SDK_CLEANUP_CASE BEGIN\n");
    fflush(stdout);
    CHECK(nsc_run(s));
    result = 0;
cleanup:
    if (s == NULL)
        return 1;
    if (s->natural.world == NULL) {
        free(s);
        return 1;
    }
    if (s->projected) {
        /* Only an infrastructure failure can reach this pre-observation path;
         * no SDK cleanup action has run yet, so no owned World slot drained. */
        s->natural.world->worlds[0]->persistent_bindings = s->saved_bindings;
        s->projected = false;
    }
    if (!mc_client_world_tick_cleanup(&s->sdk))
        return 1; /* Keep surviving owners rather than freeing a pending graph. */
    if (s->receipt != NULL &&
        mc_world_access_receipt_retire(&s->receipt, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    if (s->receiver != NULL) {
        bool removed = false;
        if (!s->removed &&
            mc_world_runtime_remove_world_access(s->natural.world->worlds[0], s->receiver, &removed,
                                                 NULL) != MC_WORLD_STORAGE_OK)
            return 1;
        if (mc_world_access_free(s->receiver, NULL) != MC_WORLD_STORAGE_OK)
            return 1;
    }
    if (!nf_cleanup(&s->natural))
        return 1;
    if (result == 0) {
        printf("ARROW_SDK_CLEANUP_CASE END world_calls=%zu callbacks=%zu\n", s->world_calls,
               s->callbacks);
        puts("ARROW_SDK_CLEANUP_OPEN full-Minecraft.k/App-startup/window/victim/render");
        printf("PASS: %u actual Arrow World-to-SDK cleanup checks;mode=%s\n", checks, argv[1]);
    }
    free(s);
    return result;
}
