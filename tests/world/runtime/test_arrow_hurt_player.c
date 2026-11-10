/* Real Player/App/Inventory/StatsWriter owners; imported main never runs. */
int arrow_player_unused_drop_main(int, char **);
#define MC_PLAYER_DROP_EXECUTION_MAIN arrow_player_unused_drop_main
#include "../../entity/player/test_drop_execution.c"
#undef MC_PLAYER_DROP_EXECUTION_MAIN

#include "entity/arrow/tick_internal.h"
#include "entity/living/fields_internal.h"
#include "entity/player/death_execution_internal.h"
#include "inventory/player/source_internal.h"

typedef struct ap_fixture {
    pd_fixture player;
    mc_entity *arrow, *item;
    mc_arrow_tick_call *call;
    mc_world_access *observer;
    mc_world_access_receipt *receipt;
    mc_item_stack *stack;
    const char *mode;
    uint64_t occurrence;
    size_t sounds, impacts, additions;
    bool valid, removed;
} ap_fixture;

static bool ap_is(const ap_fixture *s, const char *mode) {
    return strcmp(s->mode, mode) == 0;
}

static bool ap_name(const mc_registry_string *name, const char *expected) {
    mc_registry_string_view text = {0};
    if (!mc_registry_string_read(name, &text) || text.length != strlen(expected))
        return false;
    for (size_t i = 0; i < text.length; ++i)
        if (text.units[i] != (uint16_t)expected[i])
            return false;
    return true;
}

static mc_world_storage_status ap_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    (void)failure;
    ap_fixture *s = context;
    fixture *f = &s->player.game;
    const bool impact = ap_name(name, "random.drr");
    const mc_entity *actor = impact ? s->arrow : f->player;
    mc_arrow_tick_call *c = s->arrow->arrow->tick_call;
    const bool inputs = world == f->world && c != NULL && c->state.active && !s->arrow->base.be &&
                        x == actor->base.x && y == actor->base.y - (double)actor->base.offset &&
                        z == actor->base.z && pitch > 0.f && volume > 0.f && c->hurt_source == NULL;
    s->valid &= inputs;
    if (impact) {
        ++s->impacts;
        s->valid &= volume == 1.f && c->hurt == NULL && c->hurt_bytes == 0 &&
                    c->state.hurt_returned && c->state.hurt_continued && c->state.hurt_result;
    } else
        s->valid &= c->hurt != NULL && !c->hurt->returned && c->hurt_target == f->player;
    ++s->sounds;
    *cause = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ap_added(void *context, mc_world_runtime *world, mc_entity *item,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    ap_fixture *s = context;
    fixture *f = &s->player.game;
    mc_arrow_tick_call *c = s->arrow->arrow->tick_call;
    mc_player_death_execution_view death = {0};
    ++s->additions;
    s->item = item;
    const bool death_view = mc_player_death_execution_inspect(f->player, &death);
    mc_entity_tick_execution_world_token current = {0};
    const bool current_world =
        c != NULL && mc_entity_tick_execution_current_world(&c->execution, &current);
    s->valid &=
        world == f->world && item != NULL && item->stack == s->stack && item->base.bF &&
        c != NULL && c->hurt != NULL && !c->hurt->returned && death_view && death.has_child &&
        death.execution == &c->execution && death.child.result != &c->query_result &&
        death.child.result != &c->hurt->execution.owned && death.child.publication != NULL &&
        death.child.publication->entered && death.child.publication->body_entered &&
        !death.child.publication->observer_returned &&
        death.child.publication->execution == &c->execution &&
        death.child.publication->owned == death.child.result &&
        death.child.constructor_world.consumed && death.child.publication_world.open &&
        current_world && current.owner == death.child.publication_world.world.owner &&
        current.slot == death.child.publication_world.world.slot &&
        current.generation == death.child.publication_world.world.generation &&
        c->execution.leases[current.slot].phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
        f->player->local_player->fields.player.c->main_slots[0] == s->stack;
    if (ap_is(s, "fatal-source")) {
        bool removed = false;
        mc_world_storage_status status =
            mc_world_runtime_remove_world_access(world, s->observer, &removed, cause);
        if (status != MC_WORLD_STORAGE_OK || !removed)
            return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
        s->removed = true;
        status = mc_world_access_receipt_create(s->observer, NULL, &s->receipt, failure, cause);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        s->receipt->exception_class = "java/lang/IllegalStateException";
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "Arrow actual Player fatal Item Source failure", 57};
        return cause->status;
    }
    *cause = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool ap_construct(ap_fixture *s) {
    fixture *f = &s->player.game;
    bool inserted = false;
    mc_arrow_error error = {0};
    CHECK(pd_chunk(&s->player) &&
          mc_math_classes_create(SIZE_MAX, &s->player.math_owner, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(s->player.math_owner, NULL) == MC_MATH_CLASS_OK &&
          mc_entity_set_position(f->player, 9.6, 65.75, 8.5) == MC_ENTITY_OK &&
          mc_world_runtime_insert_entity(f->world, f->player, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    mc_living_entity_fields *living = mc_entity_living_fields(f->player);
    living->Y = ap_is(s, "live") || ap_is(s, "wake") ? 20 : 4;
    living->E = 20;
    living->au = f->player->base.by = 0;
    living->av = 71;
    f->player->local_player->fields.player.u = ap_is(s, "wake");
    f->world->fields.B = false;
    f->world->fields.q = 2;
    if (!ap_is(s, "live") && !ap_is(s, "wake"))
        CHECK(pd_stack(3, 2, 7, &s->stack) &&
              mc_inventory_player_source_set(f->player->local_player->fields.player.c, 0, s->stack,
                                             NULL) == MC_INVENTORY_OK);
    CHECK(mc_arrow_allocate(f->app.entities, &s->arrow, NULL) == MC_ENTITY_OK &&
          mc_arrow_construct_position(s->arrow, f->world, 8.5, 66, 8.5, &error) == MC_ENTITY_OK &&
          mc_arrow_heading(s->arrow, s->player.math_owner, 1, 0, 0, .6f, 0, &error) ==
              MC_ENTITY_OK);
    inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->world, s->arrow, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    const mc_world_access_source_callbacks callbacks = {.sound = ap_sound,
                                                        .entity_added = ap_added};
    CHECK(mc_world_access_source_create(&callbacks, s, &s->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_access_add_receipted(f->world, s->observer, &s->occurrence, NULL) ==
              MC_WORLD_STORAGE_OK);
    return true;
}

static bool ap_run(ap_fixture *s) {
    fixture *f = &s->player.game;
    mc_arrow_tick_error admission = {0};
    const mc_arrow_tick_status status = mc_arrow_tick_begin(
        s->arrow, f->app.world_classes, s->player.math_owner, NULL, &s->call, &admission);
    mc_arrow_tick_state state = {0};
    CHECK(s->call != NULL && admission.status == MC_ARROW_TICK_OK &&
          mc_arrow_tick_inspect(s->call, &state) && state.hurt_invoked && state.hurt_returned &&
          !state.hurt_prior_pending && s->valid);
    mc_living_entity_fields *living = mc_entity_living_fields(f->player);
    if (ap_is(s, "live") || ap_is(s, "fatal")) {
        const bool fatal = ap_is(s, "fatal");
        CHECK(status == MC_ARROW_TICK_OK && state.source_returned && state.hurt_continued &&
              state.hurt_result && state.hurt_bytes == 0 && s->call->hurt == NULL &&
              state.failure->status == MC_ARROW_TICK_OK && state.failure->effect == NULL &&
              living->Y == (fatal ? 0 : 16) && living->av == 0 && s->sounds == 2 &&
              s->impacts == 1 && s->arrow->base.be && s->arrow->base.bt == 1 &&
              s->arrow->arrow->fields.k == 1 && s->arrow->base.x == 8.5 + (double).6f &&
              s->arrow->base.velocity_x == (double).6f * (double).99f &&
              s->arrow->base.velocity_y == -(double).03f &&
              !mc_entity_effect_has_pending(f->player) && pd_statistic(f, 18, 4));
        if (fatal)
            CHECK(s->additions == 1 && s->item != NULL && s->item->stack == s->stack &&
                  s->item->base.bF && s->item->item.c == 40 &&
                  f->player->local_player->fields.player.c->main_slots[0] == NULL &&
                  pd_statistic(f, 16, 1) && pd_statistic(f, 19, 1));
        else
            CHECK(s->additions == 0 && pd_statistic(f, 16, 0) && pd_statistic(f, 19, 0));
    } else {
        CHECK(status != MC_ARROW_TICK_OK && !state.source_returned && state.source_pc == 741 &&
              !state.hurt_continued && !state.hurt_result && !s->arrow->base.be &&
              s->arrow->base.x == 8.5 && state.hurt_bytes == sizeof(mc_arrow_tick_hurt_work) &&
              state.failure->effect == &s->call->hurt->error && s->impacts == 0);
        if (ap_is(s, "wake"))
            CHECK(living->Y == 20 && living->av == 0 && s->sounds == 0 && s->additions == 0 &&
                  f->player->local_player->fields.player.u &&
                  s->call->hurt->execution.child == MC_ENTITY_DAMAGE_CHILD_WAKE_DEPENDENCY &&
                  s->call->hurt->execution.acquired_work == NULL && !state.hurt_cleanup_acquired &&
                  state.failure->native_policy &&
                  state.failure->classification == MC_ARROW_TICK_FAILURE_NATIVE &&
                  strcmp(state.failure->child_site.class_name, "gs") == 0 &&
                  state.failure->child_site.pc == 35 &&
                  !state.failure->effect->player_wake.entered &&
                  f->player->local_player->pending_damage_wake == NULL && pd_statistic(f, 18, 0));
        else {
            mc_entity_damage_execution_view view = {0};
            mc_player_drop_state drop = {0};
            CHECK(living->Y == 0 && s->sounds == 1 && s->additions == 1 && s->item != NULL &&
                  state.hurt_cleanup_acquired &&
                  state.failure->classification == MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION &&
                  strcmp(state.failure->exception_class, "java/lang/IllegalStateException") == 0 &&
                  mc_entity_damage_execution_inspect(&s->call->hurt->execution, &view) &&
                  view.player_drop != NULL && view.result != &s->call->query_result &&
                  view.result != &s->call->hurt->execution.owned &&
                  mc_local_player_drop_inspect(view.player_drop, &drop) && drop.item == s->item &&
                  !drop.returned && !drop.publication_returned &&
                  f->player->local_player->fields.player.c->main_slots[0] == s->stack &&
                  pd_statistic(f, 18, 4) && pd_statistic(f, 16, 0) && pd_statistic(f, 19, 0) &&
                  mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY);
        }
        const mc_java_random player_random = *f->player->random, arrow_random = *s->arrow->random;
        const size_t sounds = s->sounds, additions = s->additions;
        bool quiet = false;
        size_t steps = 0;
        while (!quiet && steps++ < 48)
            CHECK(mc_arrow_tick_native_unwind_step(s->call, &quiet) == MC_ARROW_TICK_OK);
        CHECK(quiet && steps < 48 && mc_arrow_tick_inspect(s->call, &state) &&
              state.failure->effect == NULL &&
              state.hurt_bytes == sizeof(mc_arrow_tick_hurt_work) &&
              s->call->hurt->execution.cleaned && !mc_entity_effect_has_pending(f->player) &&
              s->sounds == sounds && s->additions == additions &&
              f->player->random->state == player_random.state &&
              s->arrow->random->state == arrow_random.state && s->arrow->base.x == 8.5 &&
              !s->arrow->base.be &&
              (s->stack == NULL ||
               (s->item->stack == s->stack && s->item->base.bF &&
                f->player->local_player->fields.player.c->main_slots[0] == s->stack)));
    }
    CHECK(mc_arrow_tick_retire(&s->call) == MC_ARROW_TICK_OK && s->call == NULL &&
          s->arrow->arrow->tick_call == NULL);
    if (s->receipt != NULL)
        CHECK(mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 ||
        (strcmp(argv[1], "live") != 0 && strcmp(argv[1], "fatal") != 0 &&
         strcmp(argv[1], "fatal-source") != 0 && strcmp(argv[1], "wake") != 0) ||
        CD(argv[3]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    ap_fixture *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return 1;
    s->mode = argv[1];
    s->valid = true;
    printf("ARROW_HURT_PLAYER_BEGIN mode=%s\n", s->mode);
    fflush(stdout);
    const bool passed =
        setup(&s->player.game, argv[2], "normal", true) && ap_construct(s) && ap_run(s);
    if (s->call != NULL && mc_arrow_tick_retire(&s->call) != MC_ARROW_TICK_OK)
        return 1;
    if (s->receipt != NULL &&
        mc_world_access_receipt_retire(&s->receipt, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    if (s->observer != NULL && ((!s->removed && mc_world_access_detach_occurrence(
                                                    s->player.game.world, s->observer,
                                                    s->occurrence, NULL) != MC_WORLD_STORAGE_OK) ||
                                mc_world_access_free(s->observer, NULL) != MC_WORLD_STORAGE_OK))
        return 1;
    mc_entity_release(s->arrow);
    mc_item_stack_release(s->stack);
    if (mc_math_classes_release(&s->player.math_owner, NULL) != MC_MATH_CLASS_OK ||
        !pd_chunks_retire(&s->player) || !teardown(&s->player.game))
        return 1;
    free(s);
    if (!passed)
        return 1;
    printf("PASS: %zu actual Arrow Player hurt checks;mode=%s\n", checks, argv[1]);
    return 0;
}
