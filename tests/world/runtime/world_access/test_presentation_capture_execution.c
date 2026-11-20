/* Compact component control: the imported old program never executes. */
int pce_unused_query_main(int, char **);
#define MC_WORLD_COLLISION_QUERY_MAIN pce_unused_query_main
#include "test_collision_query_execution.c"
#undef MC_WORLD_COLLISION_QUERY_MAIN

#include "mc/entity/arrow/construction.h"
#include "world/runtime/execution_children_internal.h"

typedef struct pce_fixture {
    cq_fixture cq;
    mc_entity *actor;
    mc_world_movement_error sound_error;
    mc_world_mutation_error particle_error;
    mc_entity_tick_execution_world_token outer;
    mc_java_random actor_random, world_random;
    mc_entity_tick_execution_error first_native;
    mc_world_storage_status returned, first_pop;
    size_t saved_bindings, live_before, bytes_before, callbacks, entries;
    bool sound, null_error, valid, projected, caller_released;
} pce_fixture;

/* Native component caller provenance, not an asserted SL/Base source parent. */
static const mc_entity_tick_execution_site pce_sound_site = {"native.presentation.control", "sound",
                                                             "()V", 11};
static const mc_entity_tick_execution_site pce_particle_site = {"native.presentation.control",
                                                                "particle", "()V", 23};

static bool pce_random_same(const mc_java_random *a, const mc_java_random *b) {
    uint64_t x, y;
    _Static_assert(sizeof(x) == sizeof(a->next_next_gaussian), "binary64 cache");
    memcpy(&x, &a->next_next_gaussian, sizeof(x));
    memcpy(&y, &b->next_next_gaussian, sizeof(y));
    return a->state == b->state && a->have_next_next_gaussian == b->have_next_next_gaussian &&
           x == y;
}

static bool pce_site_same(mc_entity_tick_execution_site a, mc_entity_tick_execution_site b) {
    return a.class_name == b.class_name && a.method == b.method && a.descriptor == b.descriptor &&
           a.pc == b.pc;
}

static bool pce_source_same(mc_world_storage_error a, mc_world_storage_error b) {
    return a.status == b.status && a.operation == b.operation && a.system_error == b.system_error;
}

static mc_world_storage_status pce_callback(pce_fixture *s, mc_world_runtime *world,
                                            mc_world_storage_error *cause,
                                            mc_world_access_failure *failure) {
    cq_fixture *q = &s->cq;
    fixture *f = &q->source.game;
    ++s->callbacks;
    q->node = mc_entity_tick_execution_world_lease(&q->execution);
    s->valid &= world == f->worlds[0] && s->callbacks == 1 && world->active_calls == 1 &&
                s->actor != NULL && s->actor->phase == MC_ENTITY_READY &&
                s->actor->holds == (s->sound ? 2u : 1u) && s->actor->base.world == world &&
                pce_random_same(s->actor->random, &s->actor_random) &&
                pce_random_same(world->fields.r, &s->world_random) && q->node != NULL &&
                q->node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                q->receiver->active_holds == 1;
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, q->receiver, &removed, cause);
    q->removed = status == MC_WORLD_STORAGE_OK && removed;
    if (!q->removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    q->free_during = mc_world_access_free(q->receiver, NULL);
    status = mc_world_access_receipt_create(q->receiver, NULL, &q->receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    q->receipt->exception_class = s->sound ? "java/lang/IllegalStateException" : NULL;
    *cause = (mc_world_storage_error){s->sound ? MC_WORLD_STORAGE_IO_ERROR : MC_WORLD_STORAGE_LIMIT,
                                      "Presentation control authored callback failure", 93};
    q->callback_cause = *cause;
    if (s->sound) {
        /* Release ONLY our actual caller occurrence. The SDK's independent
         * temporary is then the final occurrence; its ordinary release after
         * capture destroys the Arrow and drains that Arrow's World binding. */
        mc_entity_release(s->actor);
        s->actor = NULL; /* Drop our now-expired test borrow immediately. */
        s->caller_released = true;
    }
    s->saved_bindings = world->persistent_bindings;
    s->valid &= s->saved_bindings > 0;
    world->persistent_bindings = 0; /* Downward native fault, no Source write. */
    s->projected = true;
    return cause->status;
}

static mc_world_storage_status pce_sound(void *context, mc_world_runtime *world,
                                         const mc_registry_string *name, double x, double y,
                                         double z, float volume, float pitch,
                                         mc_world_storage_error *cause,
                                         mc_world_access_failure *failure) {
    pce_fixture *s = context;
    /* All these finite binary fractions are exact in the SDK argument types. */
    s->valid &= s->sound && name == NULL && x == 8.5 && z == 9.25 && volume == .75f &&
                pitch == 1.25f && s->actor != NULL &&
                y == s->actor->base.y - (double)s->actor->base.offset;
    return pce_callback(s, world, cause, failure);
}

static mc_world_storage_status pce_particle(void *context, mc_world_runtime *world,
                                            const mc_registry_string *name, double x, double y,
                                            double z, double vx, double vy, double vz,
                                            mc_world_storage_error *cause,
                                            mc_world_access_failure *failure) {
    pce_fixture *s = context;
    s->valid &= !s->sound && name == NULL && x == 8.5 && y == 65.75 && z == 9.25 && vx == .25 &&
                vy == -.5 && vz == .75;
    return pce_callback(s, world, cause, failure);
}

static int pce_run(pce_fixture *s) {
    cq_fixture *q = &s->cq;
    fixture *f = &q->source.game;
    mc_world_runtime *world = f->worlds[0];
    s->valid = true;
    const size_t live_without_actor = f->entity_classes[0]->live_entities;
    VERIFY(mc_arrow_allocate(f->entity_classes[0], &s->actor, NULL) == MC_ENTITY_OK &&
           mc_arrow_construct_position(s->actor, world, 8.5, 65.75, 9.25, NULL) == MC_ENTITY_OK &&
           s->actor->holds == 1 && s->actor->base.world == world);
    s->live_before = f->entity_classes[0]->live_entities;
    s->bytes_before = f->entity_classes[0]->allocated_bytes;
    VERIFY(s->live_before == live_without_actor + 1);
    s->actor_random = *s->actor->random;
    s->world_random = *world->fields.r;
    const mc_world_access_source_callbacks callbacks = {.sound = pce_sound,
                                                        .particle = pce_particle};
    VERIFY(mc_world_access_source_create(&callbacks, s, &q->receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(world, q->receiver, NULL) == MC_WORLD_STORAGE_OK);
    const mc_entity_tick_execution_site site = s->sound ? pce_sound_site : pce_particle_site;
    VERIFY(mc_entity_tick_execution_push_world(&q->execution, world, site, &s->outer) ==
           MC_WORLD_STORAGE_OK);
    ++s->entries;
    if (s->sound)
        s->returned = mc_world_runtime_entity_sound_detailed_in_execution(
            &q->execution, world, s->actor, NULL, .75f, 1.25f, site,
            s->null_error ? NULL : &s->sound_error, &q->result);
    else
        s->returned = mc_world_runtime_particle_detailed_in_execution(
            &q->execution, world, NULL, 8.5, 65.75, 9.25, .25, -.5, .75, site,
            s->null_error ? NULL : &s->particle_error, &q->result);
    /* Native-only refusal while the genuine projection is still in force.
     * Restore before ANY assertion or checked teardown. No Source is replayed. */
    s->first_native = q->result.ownership_error;
    s->first_pop = mc_entity_tick_execution_pop_world(&q->execution, &s->outer, true);
    const size_t live_after = f->entity_classes[0]->live_entities;
    const size_t bytes_after = f->entity_classes[0]->allocated_bytes;
    const bool destroyed = s->caller_released && s->live_before == live_after + 1 &&
                           live_after == live_without_actor && bytes_after < s->bytes_before;
    const size_t actual_drains = destroyed ? 1u : 0u;
    if (s->projected) {
        world->persistent_bindings =
            s->saved_bindings >= actual_drains ? s->saved_bindings - actual_drains : 0;
        s->projected = false;
    }
    VERIFY(s->callbacks == 1 && s->entries == 1 && s->valid && q->removed && q->receipt != NULL &&
           q->free_during == MC_WORLD_STORAGE_BUSY && world->active_calls == 0 &&
           s->saved_bindings > actual_drains && s->sound == destroyed &&
           (s->sound ||
            (live_after == s->live_before && bytes_after == s->bytes_before && s->actor != NULL &&
             s->actor->holds == 1 && pce_random_same(s->actor->random, &s->actor_random))) &&
           pce_random_same(world->fields.r, &s->world_random));
    VERIFY(s->returned == q->callback_cause.status &&
           q->result.ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
           s->first_native.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
           pce_site_same(s->first_native.site, site) &&
           strcmp(s->first_native.operation, s->sound
                                                 ? "World Entity sound owned receipt capture"
                                                 : "World particle owned receipt capture") == 0 &&
           !q->execution.capture_pending && q->execution.first.status == MC_WORLD_STORAGE_OK &&
           q->execution.capture.status == MC_WORLD_STORAGE_OK && q->execution.receipt_count == 0 &&
           q->execution.loader_count == 0 && !q->result.selection_ready &&
           !cq_pending(&q->result) && mg_closes_empty(&q->result));
    VERIFY(s->first_pop == MC_WORLD_STORAGE_UNSUPPORTED_STATE && q->execution.world_open &&
           q->execution.cleanup.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
           strcmp(q->execution.cleanup.operation, "Entity execution World end slot ownership") ==
               0 &&
           cq_token_same(s->outer,
                         (mc_entity_tick_execution_world_token){
                             &q->execution, q->execution.current_lease,
                             q->execution.world_slots[q->execution.current_lease].generation}));
    const mc_world_access_failure *view = NULL;
    VERIFY(q->node != NULL && mc_world_access_failure_lease_inspect(q->node, &view) &&
           view->receiver == q->receiver && view->receipt == q->receipt &&
           q->node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_HELD && q->receiver->active_holds == 1 &&
           q->receiver->memberships == 0 && q->receipt->holds == 1 &&
           (s->sound
                ? q->receipt->exception_class != NULL &&
                      strcmp(q->receipt->exception_class, "java/lang/IllegalStateException") == 0
                : q->receipt->exception_class == NULL));
    VERIFY(q->node->callback_status == q->callback_cause.status &&
           pce_source_same(q->node->callback_error, q->callback_cause));
    if (!s->null_error) {
        const mc_world_access_failure *typed = s->sound ? &s->sound_error.world_access_failure
                                                        : &s->particle_error.world_access_failure;
        const mc_world_storage_error cause =
            s->sound ? s->sound_error.cause : s->particle_error.cause;
        VERIFY(typed->receiver == q->receiver && typed->receipt == q->receipt &&
               pce_source_same(cause, q->callback_cause) &&
               (s->sound
                    ? s->sound_error.bytecode == 53 && s->sound_error.world_access_bytecode == 53
                    : s->particle_error.bytecode == 0 &&
                          s->particle_error.world_access_bytecode == 43));
    }
    VERIFY(mc_world_access_receipt_retire(&q->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
           mc_world_access_free(q->receiver, NULL) == MC_WORLD_STORAGE_BUSY &&
           mc_entity_tick_execution_pop_world(&q->execution, &s->outer, true) ==
               MC_WORLD_STORAGE_OK &&
           cq_owner_error_same(q->result.ownership_error, s->first_native));
    /* Every outer typed borrow expires BEFORE the exact owning lease release. */
    s->sound_error.world_access_failure = s->particle_error.world_access_failure =
        (mc_world_access_failure){0};
    view = NULL;
    VERIFY(mc_entity_tick_execution_retire(&q->execution) == MC_WORLD_STORAGE_OK &&
           cq_owner_error_same(q->result.ownership_error, s->first_native) &&
           mc_world_access_receipt_retire(&q->receipt, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_access_free(q->receiver, NULL) == MC_WORLD_STORAGE_OK);
    if (!s->null_error)
        VERIFY(pce_source_same(s->sound ? s->sound_error.cause : s->particle_error.cause,
                               q->callback_cause));
    q->receiver = NULL;
    q->node = NULL;
    if (s->actor != NULL) {
        mc_entity_release(s->actor);
        s->actor = NULL;
    }
    VERIFY(f->entity_classes[0]->live_entities == live_without_actor &&
           f->providers[0].cache->count == 0 && f->providers[0].cache->insertion_count == 0);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 &&
           (strcmp(argv[1], "sound-error") == 0 || strcmp(argv[1], "sound-null-error") == 0 ||
            strcmp(argv[1], "particle-error") == 0 || strcmp(argv[1], "particle-null-error") == 0));
    printf("PRESENTATION_CAPTURE_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    pce_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->cq.mode = argv[1];
    s->sound = strncmp(argv[1], "sound-", 6) == 0;
    s->null_error = strstr(argv[1], "null-error") != NULL;
    VERIFY(ss_setup(&s->cq.source, argv[2]) == 0 && cq_files(&s->cq, true) == 0 &&
           ss_open(&s->cq.source) == 0);
    mc_entity_tick_execution_init(&s->cq.execution, NULL);
    mc_world_runtime_execution_result_init(&s->cq.result);
    printf("PRESENTATION_CAPTURE_CASE BEGIN\n");
    fflush(stdout);
    VERIFY(pce_run(s) == 0);
    printf("PRESENTATION_CAPTURE_CASE END\n");
    fflush(stdout);
    VERIFY(cq_empty_stop(&s->cq.source) == 0 && cq_files(&s->cq, false) == 0 &&
           ss_finish(&s->cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->cq.inputs[i]);
    const size_t entries = s->entries, callbacks = s->callbacks;
    free(s);
    printf("PRESENTATION_CAPTURE_OPEN Base/Living/full-Arrow/real-renderer-failure\n");
    printf("PASS: %u actual detailed presentation native capture checks;mode=%s;entries=%zu;"
           "callbacks=%zu\n",
           checks, argv[1], entries, callbacks);
    return 0;
}
