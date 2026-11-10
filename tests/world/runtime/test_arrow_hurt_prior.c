/* A real prior caller's failed Mob death remains owned by that caller. */
#define MC_ARROW_NATURAL_FLIGHT_MAIN arrow_prior_unused_natural_main
#include "test_arrow_natural_flight.c"
#undef MC_ARROW_NATURAL_FLIGHT_MAIN

#include "entity/arrow/tick_internal.h"
#include "entity/mob/death_internal.h"
#include "entity/mob/internal.h"

typedef struct prior_fixture {
    nf_fixture flight;
    mc_entity_effect_error error;
    const void *pending;
} prior_fixture;

static bool prior_run(prior_fixture *s) {
    bool ok = false, inserted = false;
    nf_fixture *n = &s->flight;
    const mc_entity_class_literal *kind = NULL;
    mc_mob_error mob = {0};
    CHECK(mc_entity_class_literal_get(n->world->entity_classes[0], MC_ENTITY_CLASS_KIND_PIG,
                                      &kind) == MC_ENTITY_OK &&
          mc_mob_allocate(n->world->entity_classes[0], n->world->registry[0], kind, &n->victim,
                          &mob) == MC_MOB_OK &&
          mc_mob_construct(n->victim, n->world->worlds[0], &mob) == MC_MOB_OK &&
          mc_entity_set_position(n->victim, 9.6, 65.75, 8.5) == MC_ENTITY_OK &&
          mc_world_runtime_insert_entity(n->world->worlds[0], n->victim, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    /* Actual nullable Source assignment on the published victim. Its prior
     * World occurrence is balanced; no Source repair or retry follows. The
     * World/Chunk membership independently keeps the actual actor alive. */
    mc_world_runtime *old_world = n->victim->base.world;
    n->victim->base.world = NULL;
    mc_world_runtime_binding_release(old_world);
    mc_entity_effect_error_init(&s->error);
    CHECK(!mc_mob_death_entered(n->victim, n->world->registry[0], NULL, &s->error) &&
          s->error.status == MC_ENTITY_STATE_ERROR && s->error.death.pending &&
          s->error.death.source_exception && s->error.death.ah_stored &&
          n->victim->mob->pending_death != NULL && n->victim->base.world == NULL);
    s->pending = n->victim->mob->pending_death;
    const mc_java_random victim_random = *n->victim->random;
    const int32_t health = n->victim->mob->living.Y;
    const size_t holds = n->victim->holds;
    mc_world_runtime *const world = n->world->worlds[0];
    const mc_world_reference_list *const source_r = world->fields.R;
    size_t occurrences_before = 0;
    CHECK(mc_world_nearby_owned_valid(world));
    for (size_t i = 0; i < world->nearby_entities.count; ++i)
        occurrences_before += world->nearby_entities.entries[i] == n->victim;
    const mc_world_storage_status status = mc_world_runtime_tick_entity(
        n->world->worlds[0], n->arrow, &tick_context, NULL, tick_error);
    CHECK(status != MC_WORLD_STORAGE_OK && tick_error->child == MC_WORLD_ENTITY_TICK_CHILD_ARROW &&
          tick_error->child_error.arrow.call != NULL);
    mc_arrow_tick_call *c = tick_error->child_error.arrow.call;
    mc_arrow_tick_state state = {0};
    CHECK(mc_arrow_tick_inspect(c, &state) && state.source_pc == 741 && state.hurt_invoked &&
          state.hurt_returned && state.hurt_prior_pending && !state.hurt_cleanup_acquired &&
          !state.hurt_continued && !state.hurt_result && c->hurt != NULL &&
          c->hurt->status == MC_ENTITY_BUSY && c->hurt->execution.acquired_work == NULL &&
          c->hurt->execution.death_work == NULL &&
          state.failure->classification == MC_ARROW_TICK_FAILURE_NATIVE &&
          n->victim->mob->pending_death == s->pending && n->victim->mob->living.Y == health &&
          n->victim->mob->living.ah && n->victim->random->state == victim_random.state &&
          !n->arrow->base.be && n->arrow->base.x == 8.5);
    bool quiet = false;
    size_t steps = 0;
    while (!quiet && steps++ < 48) {
        CHECK(mc_arrow_tick_native_unwind_step(c, &quiet) == MC_ARROW_TICK_OK &&
              n->victim->mob->pending_death == s->pending && s->error.death.pending);
    }
    const bool prefix = quiet && steps < 48 && c->hurt->execution.cleaned &&
                        mc_arrow_tick_inspect(c, &state) && state.failure->effect == NULL &&
                        state.hurt_bytes == sizeof(mc_arrow_tick_hurt_work);
    const bool retired = mc_world_entity_tick_error_retire(tick_error);
    size_t occurrences_after = 0;
    for (size_t i = 0; i < world->nearby_entities.count; ++i)
        occurrences_after += world->nearby_entities.entries[i] == n->victim;
    /* Source R owns its actual selected occurrence after the query view and
     * Arrow parent retire. Measure only those ledger holds, as natural-pig
     * does; never clear/requery R or consume the old caller's pending work. */
    CHECK(prefix && retired && n->arrow->arrow->tick_call == NULL &&
          n->victim->mob->pending_death == s->pending && mc_world_nearby_owned_valid(world) &&
          world->fields.R == source_r && world->nearby_entities.list == source_r &&
          world->nearby_entities.count == 1 && source_r->count == 1 && occurrences_after == 1 &&
          world->nearby_entities.entries[0] == n->victim && source_r->items[0] == n->victim &&
          holds >= occurrences_before && n->victim->holds >= holds - occurrences_before &&
          n->victim->holds - (holds - occurrences_before) == occurrences_after &&
          n->victim->mob->living.Y == health && n->victim->random->state == victim_random.state &&
          n->victim->base.world == NULL);
    /* Only the original caller now ends that actual old pending death. */
    CHECK(mc_entity_effect_error_retire(&s->error) && n->victim->mob->pending_death == NULL &&
          !s->error.death.pending && n->victim->mob->living.Y == health &&
          n->victim->mob->living.ah && n->victim->base.world == NULL);
    s->pending = NULL;
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    prior_fixture s = {0};
    char root[8192];
    int result = 1;
    if (argc != 3 || strcmp(argv[1], "prior-pending") != 0 ||
        !decode_path(argv[2], root, sizeof(root)))
        return 2;
    puts("ARROW_HURT_PRIOR_BEGIN mode=prior-pending");
    fflush(stdout);
    if (nf_setup(&s.flight, root, NULL) && nf_construct(&s.flight) && prior_run(&s))
        result = 0;
    if (tick_error != NULL && !mc_world_entity_tick_error_retire(tick_error))
        return 1;
    if (s.pending != NULL && !mc_entity_effect_error_retire(&s.error))
        return 1;
    if (!nf_cleanup(&s.flight))
        return 1;
    if (result == 0)
        printf("PASS: %u actual Arrow prior caller checks;mode=prior-pending\n", checks);
    return result;
}
