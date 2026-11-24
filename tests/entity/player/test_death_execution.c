/* Reuse actual Player owners and native helpers; imported main is never run. */
int pdeath_unused_drop_main(int, char **);
#define MC_PLAYER_DROP_EXECUTION_MAIN pdeath_unused_drop_main
#include "test_drop_execution.c"
#undef MC_PLAYER_DROP_EXECUTION_MAIN

#include "entity/living/fields_internal.h"
#include "entity/player/death_execution_internal.h"
#include "inventory/player/source_internal.h"
#include "mc/entity/movement/effects.h"

typedef struct pdeath_fixture {
    pd_fixture pd;
    mc_entity_effect_error *error, *probe;
    mc_item_stack *slots[40];
    mc_entity *items[41], *source;
    size_t callbacks, fail_at, total_slots;
    bool valid, notch, native_stop, capture, capacity;
} pdeath_fixture;

static bool pdeath_slots(pdeath_fixture *s, size_t cleared) {
    mc_inventory_player *inventory = s->pd.game.player->local_player->fields.player.c;
    for (size_t i = 0; i < 40; ++i) {
        mc_item_stack *actual = i < 36 ? inventory->main_slots[i] : inventory->armor_slots[i - 36];
        CHECK(actual == (i < cleared ? NULL : s->slots[i]));
    }
    CHECK(!inventory->active && !inventory->changed);
    return true;
}

static bool pdeath_items(pdeath_fixture *s) {
    mc_world_runtime *world = s->pd.game.world;
    CHECK(world->entity_membership.count == s->callbacks && world->fields.b->count == s->callbacks);
    for (size_t i = 0; i < s->callbacks; ++i) {
        mc_entity *item = world->entity_membership.entries[i];
        mc_item_stack_fields fields;
        CHECK(item == s->items[i] && world->fields.b->items[i] == item && item->item.c == 40 &&
              item->base.bF && mc_item_stack_fields_get(item->stack, &fields) == MC_ITEM_STACK_OK);
        if (s->notch && i == 0)
            CHECK(fields.id == 260 && fields.count == 1 && fields.damage == 0);
        else {
            const size_t slot = i - (s->notch ? 1u : 0u);
            CHECK(slot < 40 && item->stack == s->slots[slot] && fields.id == 3 &&
                  fields.count == (int32_t)slot + 1 && fields.damage == (int32_t)slot);
        }
    }
    return true;
}

static mc_world_storage_status pdeath_added(void *context, mc_world_runtime *world, mc_entity *item,
                                            mc_world_storage_error *cause,
                                            mc_world_access_failure *failure) {
    pdeath_fixture *s = context;
    fixture *f = &s->pd.game;
    const size_t index = s->callbacks++;
    const bool apple = s->notch && index == 0;
    const size_t slot = index - (s->notch ? 1u : 0u);
    mc_item_stack_fields fields = {0};
    mc_player_death_state state = {0};
    mc_player_death_execution_view view = {0};
    s->valid &=
        index < s->total_slots + (s->notch ? 1u : 0u) && world == f->world && item != NULL &&
        item->item.c == 40 && mc_item_stack_fields_get(item->stack, &fields) == MC_ITEM_STACK_OK &&
        mc_player_death_inspect(f->player, &state) && state.pending && state.active &&
        mc_player_death_execution_inspect(f->player, &view) && view.has_child &&
        view.child.execution == &s->pd.execution && view.child.constructor_world.consumed &&
        view.child.publication_world.open && world->entity_membership.count == index + 1 &&
        world->entity_membership.entries[index] == item && world->fields.b->items[index] == item &&
        s->pd.execution.leases[view.child.publication_world.world.slot].phase ==
            MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
    if (s->valid) {
        if (apple)
            s->valid &= fields.id == 260 && fields.count == 1 && fields.damage == 0 &&
                        state.apple_allocated && !state.apple_returned && !state.inventory_entered;
        else {
            mc_inventory_player *inventory = f->player->local_player->fields.player.c;
            mc_item_stack *current =
                slot < 36 ? inventory->main_slots[slot] : inventory->armor_slots[slot - 36];
            s->valid &= slot < 40 && fields.id == 3 && fields.count == (int32_t)slot + 1 &&
                        fields.damage == (int32_t)slot && current == s->slots[slot] &&
                        state.inventory_entered && state.slots_cleared == slot &&
                        state.inventory_armor == (slot >= 36) &&
                        state.inventory_slot == (int32_t)(slot < 36 ? slot : slot - 36) &&
                        state.inventory_pc == (slot < 36 ? 31u : 78u);
        }
    }
    if (!s->valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Bounded Player death observer input", 0};
        return cause->status;
    }
    s->items[index] = item; /* Borrowed from the actual World membership until terminal cleanup. */
    if (s->fail_at != 0 && index + 1 == s->fail_at) {
        bool removed = false;
        const mc_world_storage_status removed_status =
            mc_world_runtime_remove_world_access(world, s->pd.receiver, &removed, cause);
        if (removed_status != MC_WORLD_STORAGE_OK || !removed)
            return removed_status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                                         : removed_status;
        s->pd.removed = true;
        const mc_world_storage_status created =
            mc_world_access_receipt_create(s->pd.receiver, NULL, &s->pd.receipt, failure, cause);
        if (created != MC_WORLD_STORAGE_OK)
            return created;
        s->pd.receipt->exception_class = "java/lang/IllegalStateException";
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "Player death actual Source callback failure", 57};
        if (s->capture) {
            s->pd.saved_bindings = world->persistent_bindings;
            world->persistent_bindings =
                0; /* Native integrity input after real receipt creation. */
        }
        return cause->status;
    }
    if (s->native_stop) {
        s->pd.saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Native integrity input; no Source field changes. */
    }
    return MC_WORLD_STORAGE_OK;
}

static bool pdeath_control(pdeath_fixture *s) {
    pd_fixture *p = &s->pd;
    fixture *f = &p->game;
    const char *mode = p->mode;
    const bool missing = strcmp(mode, "math-missing") == 0;
    const bool limited = strcmp(mode, "math-limit") == 0;
    const bool null_world = strcmp(mode, "nullable-world") == 0;
    const bool preflight = strcmp(mode, "preflight") == 0;
    const bool empty = strcmp(mode, "empty") == 0 || missing || limited || null_world || preflight;
    s->notch = strcmp(mode, "notch41") == 0 || strcmp(mode, "fail-apple") == 0;
    s->native_stop = strcmp(mode, "native-stop") == 0;
    s->capture = strcmp(mode, "callback-capture") == 0;
    s->capacity = strcmp(mode, "scope-capacity") == 0;
    if (strncmp(mode, "fail-", 5) == 0 && strcmp(mode, "fail-apple") != 0)
        s->fail_at = (size_t)strtoul(mode + 5, NULL, 10);
    if (strcmp(mode, "lease-source") == 0 || s->capture || strcmp(mode, "fail-apple") == 0)
        s->fail_at = 1;
    s->total_slots = empty ? 0 : 40;
    CHECK(pd_chunk(p));
    for (size_t i = 0; i < s->total_slots; ++i)
        CHECK(pd_stack(3, (int32_t)i + 1, (int32_t)i, &s->slots[i]) &&
              mc_inventory_player_source_set(f->player->local_player->fields.player.c, (int32_t)i,
                                             s->slots[i], NULL) == MC_INVENTORY_OK);
    if (s->notch) {
        static const uint16_t text[] = {'N', 'o', 't', 'c', 'h'};
        mc_player_string *name = NULL;
        CHECK(mc_player_string_create(text, 5, &name) == MC_PLAYER_OK);
        mc_player_string_release((mc_player_string *)f->player->local_player->fields.player.l);
        f->player->local_player->fields.player.l = name;
    }
    if (!missing)
        CHECK(mc_math_classes_create(limited ? 1024 : SIZE_MAX, &p->math_owner, NULL) ==
              MC_MATH_CLASS_OK);
    mc_math_execution_init(&p->math, p->math_owner);
    mc_entity_tick_execution_init(&p->execution, &p->math);
    CHECK(mc_entity_tick_execution_push_world(&p->execution, f->world, pd_site, &p->outer) ==
          MC_WORLD_STORAGE_OK);
    if (s->capacity)
        while (p->extra_count + 1 < MC_ENTITY_TICK_EXECUTION_WORLDS) {
            CHECK(mc_entity_tick_execution_push_world(&p->execution, f->world, pd_site,
                                                      &p->extra[p->extra_count]) ==
                  MC_WORLD_STORAGE_OK);
            ++p->extra_count;
        }
    if (missing || limited)
        CHECK(mc_item_entity_allocate(f->app.entities, &s->source, NULL) == MC_ENTITY_OK &&
              mc_item_entity_construct_world(s->source, f->world, NULL) == MC_ENTITY_OK);
    if (!empty) {
        const mc_world_access_source_callbacks callbacks = {.entity_added = pdeath_added};
        CHECK(mc_world_access_source_create(&callbacks, s, &p->receiver, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(f->world, p->receiver, NULL) ==
                  MC_WORLD_STORAGE_OK);
    }
    mc_entity_living_fields(f->player)->Y = 0; /* Actual dying Player input. */
    mc_java_random expected = *f->player->random;
    const size_t player_holds = f->player->holds;
    mc_entity_effect_error_init(s->error);
    mc_entity_effect_error_init(s->probe);
    mc_entity_status raw = MC_ENTITY_WORLD_FAILED;
    s->valid = true;
    if (preflight) {
        for (unsigned int invalid = 0; invalid < 2; ++invalid) {
            s->error->status = MC_ENTITY_LIMIT;
            s->error->operation = "Preserved Player death diagnostic";
            const char *operation = s->error->operation;
            s->error->player_death.site.pc = 209;
            raw = MC_ENTITY_WORLD_FAILED;
            CHECK(!mc_player_death_entered_in_execution(invalid == 0 ? NULL : &p->execution,
                                                        invalid == 1 ? NULL : f->app.world_classes,
                                                        pd_code, f->player, f->app.registry, NULL,
                                                        &raw, s->error, &p->owned) &&
                  raw == MC_ENTITY_WORLD_FAILED && s->error->status == MC_ENTITY_LIMIT &&
                  s->error->operation == operation && s->error->player_death.site.pc == 209 &&
                  !mc_entity_effect_has_pending(f->player) && f->player->holds == player_holds &&
                  p->math.owned == NULL && !p->math.failed &&
                  !mc_entity_living_fields(f->player)->ah);
            p->owned = (mc_world_runtime_execution_result){0};
        }
    } else {
        if (null_world)
            f->player->base.world = NULL;
        const bool completed = mc_player_death_entered_in_execution(
            &p->execution, f->app.world_classes, pd_code, f->player, f->app.registry, s->source,
            &raw, s->error, &p->owned);
        if (p->saved_bindings != 0)
            f->world->persistent_bindings = p->saved_bindings;
        if (null_world)
            f->player->base.world = f->world; /* Teardown operand only; Source is never resumed. */
        const bool failed =
            missing || limited || null_world || s->fail_at != 0 || s->native_stop || s->capacity;
        CHECK(completed == !failed && s->valid && s->error->player_death.entered &&
              mc_entity_living_fields(f->player)->ah && pdeath_items(s));
        size_t cleared = s->total_slots;
        if (missing || limited || null_world || s->capacity || s->native_stop)
            cleared = 0;
        else if (s->fail_at != 0)
            cleared = s->notch ? 0 : s->fail_at - 1;
        CHECK(pdeath_slots(s, cleared));
        if (!failed) {
            const size_t count = s->total_slots + (s->notch ? 1u : 0u);
            CHECK(raw == MC_ENTITY_OK && s->error->status == MC_ENTITY_OK &&
                  s->error->player_death.returned && !s->error->player_death.pending &&
                  s->error->player_death.slots_cleared == s->total_slots &&
                  s->error->player_death.drops_returned == count &&
                  s->error->player_death.drops_retired == count && s->callbacks == count &&
                  f->world->fields.b->count == count &&
                  f->world->entity_membership.count == count && f->player->base.width == .2f &&
                  f->player->base.height == .2f && f->player->base.offset == .1f &&
                  pd_statistic(f, 16, (int32_t)count) && pd_statistic(f, 19, 1));
        } else {
            mc_player_death_execution_view view;
            CHECK(mc_player_death_execution_inspect(f->player, &view) &&
                  view.execution == &p->execution && !view.completed &&
                  s->error->player_death.pending && !s->error->player_death.returned &&
                  !s->error->player_death.statistic_returned && pd_statistic(f, 19, 0));
            if (missing || limited)
                CHECK(raw == MC_ENTITY_OK && s->error->status == MC_ENTITY_OK && p->math.failed &&
                      p->math.first.caller.pc == 91 &&
                      !strcmp(p->math.first.caller.class_name, "gs") && !view.has_child &&
                      view.drop == NULL && s->error->player_death.site.pc == 91 &&
                      s->error->player_death.inventory_returned && s->callbacks == 0);
            else if (null_world)
                CHECK(raw == MC_ENTITY_STATE_ERROR && s->error->player_death.source_exception &&
                      !s->error->player_death.size_stored && !view.has_child && s->callbacks == 0);
            else {
                CHECK(view.has_child && view.drop != NULL && view.child.result != view.result &&
                      view.result != &p->owned && !view.drop_returned &&
                      s->error->player_death.slots_cleared == cleared);
                if (s->capacity)
                    CHECK(raw == MC_ENTITY_OK && view.drop_status == MC_PLAYER_DROP_OK &&
                          view.child.result->ownership_status == MC_WORLD_STORAGE_LIMIT &&
                          !view.child.constructor_world.created && s->callbacks == 0 &&
                          s->error->player_death.inventory_pc == 31);
                else if (s->native_stop) {
                    CHECK(raw == MC_ENTITY_OK && s->error->status == MC_ENTITY_OK &&
                          view.drop_status == MC_PLAYER_DROP_OK && s->callbacks == 1 &&
                          view.child.publication_status == MC_WORLD_STORAGE_OK &&
                          view.child.publication_returned && view.child.publication_world.open &&
                          view.child.result->ownership_status ==
                              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                          s->error->player_death.inventory_pc == 31 && pd_statistic(f, 16, 0));
                    const mc_player_drop_call *actual = view.drop;
                    const size_t bindings = f->world->persistent_bindings;
                    f->world->persistent_bindings = 0;
                    const mc_entity_status refused = mc_entity_effect_cleanup(f->player, s->probe);
                    f->world->persistent_bindings = bindings;
                    CHECK(refused != MC_ENTITY_OK &&
                          mc_player_death_execution_inspect(f->player, &view) &&
                          view.drop == actual && s->callbacks == 1 &&
                          s->error->player_death.inventory_pc == 31);
                } else {
                    const mc_world_access_failure *protected = NULL;
                    CHECK(raw == MC_ENTITY_WORLD_FAILED &&
                          view.drop_status == MC_PLAYER_DROP_WORLD && s->callbacks == s->fail_at &&
                          s->error->player_death.source_exception &&
                          s->error->player_death.drops_returned == cleared &&
                          view.child.result->ownership_status ==
                              (s->capture ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                          : MC_WORLD_STORAGE_OK) &&
                          mc_world_access_failure_lease_inspect(
                              &p->execution.leases[view.child.publication_world.world.slot],
                              &protected) &&
                          protected->receipt == p->receipt && p->receiver->active_holds == 1 &&
                          mc_world_access_receipt_retire(&p->receipt, NULL) ==
                              MC_WORLD_STORAGE_BUSY &&
                          pd_statistic(f, 16, (int32_t)cleared));
                    if (s->capture)
                        CHECK(p->execution.receipt_count == 0 &&
                              mc_entity_tick_execution_capture_receipt(&p->execution, protected) ==
                                  MC_WORLD_STORAGE_OK &&
                              p->execution.receipt_count == 1);
                    protected = NULL;
                }
            }
            view = (mc_player_death_execution_view){0}; /* Expire all pending child borrows. */
            const size_t callbacks = s->callbacks;
            CHECK(mc_entity_effect_cleanup(f->player, s->error) == MC_ENTITY_OK &&
                  !mc_entity_effect_has_pending(f->player) && s->callbacks == callbacks &&
                  pdeath_slots(s, cleared) && pdeath_items(s));
        }
        const size_t draws =
            (missing || limited || null_world || s->capacity) ? 0 : s->callbacks * 2;
        for (size_t i = 0; i < draws; ++i)
            (void)mc_java_random_next_float(&expected);
        CHECK(f->player->random->state == expected.state && f->player->holds == player_holds);
    }
    mc_entity_effect_error_init(s->error);
    mc_entity_effect_error_init(s->probe);
    p->owned = (mc_world_runtime_execution_result){0};
    while (p->extra_count != 0) {
        --p->extra_count;
        CHECK(mc_entity_tick_execution_pop_world(&p->execution, &p->extra[p->extra_count], true) ==
              MC_WORLD_STORAGE_OK);
    }
    CHECK(mc_entity_tick_execution_pop_world(&p->execution, &p->outer, true) ==
              MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(&p->execution) == MC_WORLD_STORAGE_OK &&
          mc_math_execution_retire(&p->math) == MC_MATH_CLASS_OK &&
          mc_math_classes_release(&p->math_owner, NULL) == MC_MATH_CLASS_OK &&
          mc_world_access_receipt_retire(&p->receipt, NULL) == MC_WORLD_STORAGE_OK);
    if (p->receiver != NULL) {
        if (!p->removed) {
            bool removed = false;
            CHECK(mc_world_runtime_remove_world_access(f->world, p->receiver, &removed, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  removed);
        }
        CHECK(mc_world_access_free(p->receiver, NULL) == MC_WORLD_STORAGE_OK);
        p->receiver = NULL;
    }
    mc_entity_release(s->source);
    s->source = NULL;
    for (size_t i = 0; i < 40; ++i) {
        mc_item_stack_release(s->slots[i]);
        s->slots[i] = NULL;
    }
    memset(s->items, 0,
           sizeof(s->items)); /* Expire actual membership borrows before World teardown. */
    CHECK(pd_chunks_retire(p));
    return true;
}

static bool pdeath_mode(const char *mode) {
    static const char *const modes[] = {"empty",          "full40",           "notch41",
                                        "fail-apple",     "math-missing",     "math-limit",
                                        "nullable-world", "preflight",        "lease-source",
                                        "native-stop",    "callback-capture", "scope-capacity"};
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
        if (strcmp(mode, modes[i]) == 0)
            return true;
    if (strncmp(mode, "fail-", 5) == 0) {
        char *end = NULL;
        const unsigned long value = strtoul(mode + 5, &end, 10);
        return end != mode + 5 && *end == '\0' && value >= 1 && value <= 40;
    }
    return false;
}

int main(int argc, char **argv) {
    if (argc != 4 || !pdeath_mode(argv[1]) || CD(argv[3]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    printf("PLAYER_DEATH_EXECUTION_BEGIN mode=%s\n", argv[1]);
    fflush(stdout);
    pdeath_fixture *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return 1;
    s->pd.mode = argv[1];
    s->error = malloc(sizeof(*s->error));
    s->probe = malloc(sizeof(*s->probe));
    if (s->error == NULL || s->probe == NULL || !setup(&s->pd.game, argv[2], "normal", true) ||
        !pdeath_control(s) || !teardown(&s->pd.game))
        return 1;
    free(s->error);
    free(s->probe);
    free(s);
    printf("PASS: %zu execution-owned Player death checks;mode=%s\n", checks, argv[1]);
    return 0;
}
