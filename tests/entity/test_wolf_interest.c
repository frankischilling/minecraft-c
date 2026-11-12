/* Real held GI/DC/IX/IZ owners; closed GI.o4..186 component only. No Source
 * superclass, complete GI.o method or complete Mob tick is credited here. */
int mc_wolf_world_fixture_main(int argc, char **argv);
#define main mc_wolf_world_fixture_main
#include "../world/runtime/test_world_random_tick.c"
#undef main
#include "entity/mob/references_internal.h"
#include "entity/mob/tick/internal.h"
#include "entity/player/player_internal.h"
#include "inventory/internal.h"
#include "mc/inventory/player_current.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Wolf interest check%u line%d: %s\n", checks, __LINE__, #c);           \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)

typedef struct wolf_case {
    const char *name;
    int32_t id;
    bool tame, null_inventory, null_array, expected;
    unsigned int tame_calls, id_reads, array_reads;
} wolf_case;

static const wolf_case wolf_cases[] = {
    {"empty_stack", -1, false, false, false, false, 0, 0, 0},
    {"untamed_bone", 352, false, false, false, true, 1, 1, 0},
    {"tame_bone", 352, true, false, false, false, 2, 1, 1},
    {"tame_pork", 319, true, false, false, true, 2, 2, 2},
    {"tame_apple", 260, true, false, false, false, 2, 2, 2},
    {"tame_book", 340, true, false, false, false, 2, 1, 1},
    {"untamed_pork", 319, false, false, false, false, 2, 1, 0},
    {"null_inventory", -1, false, true, false, false, 0, 0, 0},
    {"null_main_array", -1, false, false, true, false, 0, 0, 0},
};

static mc_entity_watch_record *wolf_byte(mc_entity *p) {
    if (!p || !p->watcher || !p->watcher->map)
        return NULL;
    for (mc_entity_watch_node *n = p->watcher->map->head; n; n = n->next)
        if (n->key == 16)
            return n->record;
    return NULL;
}

static void wolf_row(FILE *out, unsigned int index, const wolf_case *c, const mc_mob_tick_error *e,
                     uint64_t own, uint64_t player_seed, uint64_t world_seed, uint64_t math_seed,
                     uint64_t u) {
    const mc_mob_tick_wolf_interest_state *r = &e->state.wolf_interest;
    fprintf(
        out,
        "{\"record\":\"profile\",\"case\":%u,\"scene\":\"%s\","
        "\"scope\":\"GI.o4..186-only\",\"status\":%d,\"pc\":%" PRId32
        ",\"range_returned\":%s,\"interest_returned\":%s,\"interested\":%s,"
        "\"native_policy\":%s,\"source_exception\":%s,\"exception\":\"%s\","
        "\"child_class\":\"%s\",\"child_pc\":%" PRId32 ",\"player\":\"%016" PRIxPTR
        "\",\"inventory\":\"%016" PRIxPTR "\",\"stack\":\"%016" PRIxPTR "\",\"stack_nonnull\":%s,"
        "\"selected_read\":%s,\"selected\":%" PRId32 ",\"tame_calls\":%u,"
        "\"id_reads\":%u,\"ids\":[%" PRId32 ",%" PRId32 ",%" PRId32
        "],\"array_reads\":%u,\"arrays\":[\"%016" PRIxPTR "\",\"%016" PRIxPTR
        "\"],\"items\":[\"%016" PRIxPTR "\",\"%016" PRIxPTR "\"],"
        "\"bone\":\"%016" PRIxPTR "\",\"bone_id\":%" PRId32
        ",\"food_type\":%s,\"food_returned\":%s,\"own_seed\":\"%012" PRIx64
        "\",\"player_seed\":\"%012" PRIx64 "\",\"world_seed\":\"%012" PRIx64
        "\",\"math_seed\":\"%012" PRIx64 "\",\"U\":\"%016" PRIx64
        "\",\"all_streams_unchanged\":true,\"entity_holds_balanced\":true,"
        "\"registry_math_holds_balanced\":true,\"failed_fields_preserved\":true,"
        "\"cleanup_returned\":true,\"parent_return_credit\":false}\n",
        index, c->name, (int)e->status, e->site.pc, e->state.returned ? "true" : "false",
        r->returned ? "true" : "false", r->interested ? "true" : "false",
        e->native_policy ? "true" : "false", e->source_exception ? "true" : "false",
        e->exception_class ? e->exception_class : "none",
        e->child_site.class_name ? e->child_site.class_name : "none", e->child_site.pc,
        r->player_identity, r->inventory_identity, r->stack_identity,
        r->stack_nonnull ? "true" : "false", r->selected_read ? "true" : "false", r->selected,
        (unsigned int)r->tame_calls, (unsigned int)r->id_reads, r->stack_ids[0], r->stack_ids[1],
        r->stack_ids[2], (unsigned int)r->array_reads, r->arrays[0], r->arrays[1], r->items[0],
        r->items[1], r->bone_identity, r->bone_id, r->food_type ? "true" : "false",
        r->food_returned ? "true" : "false", own, player_seed, world_seed, math_seed, u);
}

static bool wolf_one(random_fixture *f, unsigned int index, FILE *out) {
    bool passed = false;
    const wolf_case *c = &wolf_cases[index];
    mc_entity *actor = NULL;
    mc_item_stack *stack = NULL;
    mc_inventory_player *inventory = NULL;
    mc_item_stack **main_array = NULL;
    mc_mob_tick_error *e = calloc(1, sizeof(*e)), *cleanup = calloc(1, sizeof(*cleanup));
    CHECK(e && cleanup);
    const mc_entity_class_literal *wolf = NULL;
    mc_mob_error construction;
    CHECK(mc_entity_class_literal_get(f->entities, MC_ENTITY_CLASS_KIND_WOLF, &wolf) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->entities, f->registry, wolf, &actor, &construction) == MC_MOB_OK &&
          mc_mob_construct(actor, f->world, &construction) == MC_MOB_OK);
    /* Existing helper constructs a genuine dc with real GR/Inventory/Container.
     * Its native owned Player-list occurrence is setup, not Source work here. */
    CHECK(player(f) && f->player->local_player && actor->mob && actor->holds == 1);
    CHECK(mc_entity_set_position(actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(f->player, 10.5, 64.5, 8.5) == MC_ENTITY_OK);
    actor->mob->living.V = true; /* Declared closed-caller input, like Original. */
    CHECK(mc_mob_look_target_store(actor, f->player, &e->reference) == MC_ENTITY_OK);
    CHECK(mt_wolf_tame_component(actor, f->registry, f->math, NULL, c->tame, e) == MC_ENTITY_OK &&
          e->state.returned && e->state.wolf_tame.entered && e->state.wolf_tame.returned &&
          e->state.wolf_tame.value == c->tame && e->site.pc == 50 &&
          !strcmp(e->site.class_name, "gi") && !strcmp(e->site.method, "d") &&
          !strcmp(e->site.descriptor, "(Z)V") && !e->state.wolf_interest.entered &&
          !e->state.environment_returned && !e->state.action_returned &&
          !e->state.behavior_returned);
    mc_entity_watch_record *flags = wolf_byte(actor);
    CHECK(flags && flags->type == 0 && ((flags->byte_value & 4) != 0) == c->tame);
    inventory = f->player->local_player->fields.player.c;
    CHECK(inventory && inventory->selected == 0 && inventory->main_slots &&
          inventory->armor_slots && !inventory->active);
    main_array = inventory->main_slots;
    if (c->id >= 0) {
        CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(stack, c->id, 1, 0) == MC_ITEM_STACK_OK &&
              mc_inventory_player_set(inventory, 0, stack, NULL) == MC_INVENTORY_OK);
    }
    if (c->null_inventory)
        f->player->local_player->fields.player.c = NULL;
    if (c->null_array)
        inventory->main_slots = NULL;
    mc_entity_base_fields actor_base, player_base;
    memcpy(&actor_base, &actor->base, sizeof(actor_base));
    memcpy(&player_base, &f->player->base, sizeof(player_base));
    mc_mob_entity_state expected;
    memcpy(&expected, actor->mob, sizeof(expected));
    expected.wolf.a = c->expected;
    mc_local_player_snapshot player_fields;
    mc_inventory_player inventory_fields;
    memcpy(&player_fields, &f->player->local_player->fields, sizeof(player_fields));
    memcpy(&inventory_fields, inventory, sizeof(inventory_fields));
    mc_item_stack *slots[36], *armor[4];
    memcpy(slots, main_array, sizeof(slots));
    memcpy(armor, inventory->armor_slots, sizeof(armor));
    mc_item_stack_fields stack_fields = {0}, after_stack = {0};
    mc_item_stack_phase stack_phase = MC_ITEM_STACK_RAW, after_phase = MC_ITEM_STACK_RAW;
    if (stack)
        CHECK(mc_item_stack_fields_get(stack, &stack_fields) == MC_ITEM_STACK_OK &&
              mc_item_stack_phase_get(stack, &stack_phase) == MC_ITEM_STACK_OK);
    mc_entity_watch_record flag_fields;
    memcpy(&flag_fields, flags, sizeof(flag_fields));
    const bool watcher_dirty = actor->watcher->dirty;
    CHECK(f->profile->math && !actor->mob->tick_active && !inventory->active);
    const uint64_t own = actor->random->state, player_seed = f->player->random->state;
    const uint64_t world_seed = f->world->fields.r->state;
    const uint64_t math_seed = atomic_load(&f->profile->math->state);
    const uint64_t u = atomic_load(&f->profile->uniquifier);
    const size_t actor_holds = actor->holds, player_holds = f->player->holds;
    const size_t registry_holds = f->registry->runtime_borrows;
    mc_entity_class_snapshot before_classes, after_classes;
    mc_math_class_state before_math, after_math;
    CHECK(mc_entity_classes_get_snapshot(f->entities, &before_classes) == MC_ENTITY_OK &&
          mc_math_classes_state(f->math, &before_math, NULL) == MC_MATH_CLASS_OK);
    const mc_entity_status status = mt_wolf_action_component(actor, f->registry, f->math, NULL, e);
    const bool returned = !c->null_inventory && !c->null_array;
    const mc_mob_tick_wolf_interest_state *r = &e->state.wolf_interest;
    CHECK((status == MC_ENTITY_OK) == returned && e->state.returned == returned &&
          e->site.pc == (returned ? 186 : 51) && r->entered &&
          r->player_identity == (uintptr_t)f->player &&
          r->inventory_identity == (c->null_inventory ? 0 : (uintptr_t)inventory) &&
          r->inventory_returned == returned && r->returned == returned &&
          r->selected_read == !c->null_inventory && r->stack_nonnull == (stack != NULL) &&
          r->stack_identity == (uintptr_t)stack && r->interested == c->expected &&
          r->tame_calls == c->tame_calls && r->id_reads == c->id_reads &&
          r->array_reads == c->array_reads);
    CHECK(!e->state.environment_returned && !e->state.action_returned &&
          !e->state.behavior_returned && !e->state.wolf_tame.entered && !e->state.cleanup_pending &&
          e->state.concrete_random_calls == 0 && e->state.concrete_world_random_calls == 0 &&
          e->state.sounds == 0 && e->state.particles == 0 && e->state.path_calls == 0 &&
          e->state.ray_calls == 0);
    if (!returned)
        CHECK(e->source_exception && !e->native_policy && e->exception_class &&
              !strcmp(e->exception_class, "java/lang/NullPointerException") &&
              e->child_site.pc == (c->null_inventory ? 51 : 24) &&
              !strcmp(e->child_site.class_name, c->null_inventory ? "gi" : "ix"));
    for (size_t i = 0; i < r->id_reads; ++i)
        CHECK(r->stack_ids[i] == c->id);
    if (r->array_reads == 2)
        CHECK(r->arrays[0] != 0 && r->arrays[0] == r->arrays[1] && r->items[0] != 0 &&
              r->items[0] == r->items[1] && r->food_type && r->food_returned);
    if (c->id >= 0 && !c->tame)
        CHECK(r->bone_identity != 0 && r->bone_id_read && r->bone_id == 352);
    CHECK(actor->holds == actor_holds && f->player->holds == player_holds &&
          f->registry->runtime_borrows == registry_holds && !actor->mob->tick_active &&
          !inventory->active && actor->mob->living.private_b == f->player &&
          actor->mob->look_target_hold == f->player &&
          f->player->local_player->fields.player.c == (c->null_inventory ? NULL : inventory) &&
          inventory->main_slots == (c->null_array ? NULL : main_array));
    CHECK(mc_mob_tick_cleanup(actor, cleanup) == MC_ENTITY_OK && !cleanup->state.cleanup_pending);
    CHECK(f->player->local_player->fields.player.c == (c->null_inventory ? NULL : inventory) &&
          inventory->main_slots == (c->null_array ? NULL : main_array) &&
          actor->holds == actor_holds && f->player->holds == player_holds);
    CHECK(!memcmp(&actor_base, &actor->base, sizeof(actor_base)) &&
          !memcmp(&player_base, &f->player->base, sizeof(player_base)) &&
          !memcmp(&expected, actor->mob, sizeof(expected)) &&
          !memcmp(&player_fields, &f->player->local_player->fields, sizeof(player_fields)) &&
          !memcmp(&inventory_fields, inventory, sizeof(inventory_fields)) &&
          !memcmp(slots, main_array, sizeof(slots)) &&
          !memcmp(armor, inventory->armor_slots, sizeof(armor)) &&
          !memcmp(&flag_fields, flags, sizeof(flag_fields)) &&
          actor->watcher->dirty == watcher_dirty);
    if (stack)
        CHECK(mc_item_stack_fields_get(stack, &after_stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_phase_get(stack, &after_phase) == MC_ITEM_STACK_OK &&
              !memcmp(&stack_fields, &after_stack, sizeof(stack_fields)) &&
              stack_phase == after_phase);
    CHECK(actor->random->state == own && f->player->random->state == player_seed &&
          f->world->fields.r->state == world_seed &&
          atomic_load(&f->profile->math->state) == math_seed &&
          atomic_load(&f->profile->uniquifier) == u);
    CHECK(mc_entity_classes_get_snapshot(f->entities, &after_classes) == MC_ENTITY_OK &&
          mc_math_classes_state(f->math, &after_math, NULL) == MC_MATH_CLASS_OK &&
          before_classes.source == after_classes.source &&
          before_classes.entity_class == after_classes.entity_class &&
          before_classes.bounds_class == after_classes.bounds_class &&
          before_classes.watcher_class == after_classes.watcher_class &&
          before_classes.next_id == after_classes.next_id &&
          before_classes.bounds_pool_cursor == after_classes.bounds_pool_cursor &&
          before_classes.bounds_pool_identity == after_classes.bounds_pool_identity &&
          before_classes.watcher_types_identity == after_classes.watcher_types_identity &&
          before_classes.watcher_type_count == after_classes.watcher_type_count &&
          before_classes.live_entities == after_classes.live_entities &&
          before_classes.allocation_bytes == after_classes.allocation_bytes &&
          before_classes.live_path_arenas == after_classes.live_path_arenas &&
          before_math.holds == after_math.holds);
    wolf_row(out, index, c, e, own, player_seed, world_seed, math_seed, u);
    if (index == 0) {
        mc_mob_tick_limits no_work = {0};
        CHECK(mt_wolf_action_component(actor, NULL, NULL, &no_work, e) == MC_ENTITY_OK &&
              e->state.returned && e->state.wolf_interest.returned &&
              !e->state.wolf_interest.stack_nonnull && e->state.wolf_interest.tame_calls == 0 &&
              e->state.wolf_interest.array_reads == 0 && e->site.pc == 186);
        fputs("{\"record\":\"native_control\",\"name\":\"unused_services_and_zero_work\","
              "\"passed\":true}\n",
              out);
        CHECK(mt_wolf_action_component(f->player, f->registry, f->math, NULL, e) ==
                  MC_ENTITY_STATE_ERROR &&
              e->native_policy && !e->source_exception && !e->state.returned &&
              !e->state.wolf_interest.entered && e->site.pc == -1);
        fputs("{\"record\":\"native_control\",\"name\":\"actual_player_receiver_rejected\","
              "\"passed\":true}\n",
              out);
    }
    if (index == 1) {
        CHECK(actor->mob->wolf.a &&
              mt_wolf_action_component(actor, NULL, f->math, NULL, e) == MC_ENTITY_STATE_ERROR &&
              !actor->mob->wolf.a && e->native_policy && !e->source_exception && e->site.pc == 70 &&
              !e->state.returned && e->state.wolf_interest.id_reads == 1 &&
              e->state.wolf_interest.tame_calls == 1 && actor->holds == actor_holds &&
              f->player->holds == player_holds && !actor->mob->tick_active);
        CHECK(mc_mob_tick_cleanup(actor, cleanup) == MC_ENTITY_OK && !actor->mob->wolf.a);
        fputs("{\"record\":\"native_control\",\"name\":\"reached_registry_failure_prefix\","
              "\"passed\":true}\n",
              out);
    }
    CHECK(actor->random->state == own && f->player->random->state == player_seed &&
          f->world->fields.r->state == world_seed &&
          atomic_load(&f->profile->math->state) == math_seed &&
          atomic_load(&f->profile->uniquifier) == u && actor->holds == actor_holds &&
          f->player->holds == player_holds && f->registry->runtime_borrows == registry_holds);
    passed = true;
done:
    /* Only final allocator disposal restores these saved allocation pointers.
     * All assertions and checked call cleanup above saw the actual NULL fields. */
    if (inventory) {
        if (c->null_inventory && f->player)
            f->player->local_player->fields.player.c = inventory;
        if (c->null_array)
            inventory->main_slots = main_array;
        if (mc_inventory_player_set(inventory, 0, NULL, NULL) != MC_INVENTORY_OK)
            passed = false;
    }
    mc_item_stack_release(stack);
    mc_mob_error retired;
    if (actor && mc_mob_retire(&actor, &retired) != MC_MOB_OK)
        passed = false;
    free(cleanup);
    free(e);
    return passed;
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    char path[8192];
    int n = snprintf(path, sizeof(path), "%s/native.ndjson", argv[3]);
    FILE *out = n > 0 && (size_t)n < sizeof(path) ? open_file(path, "wb") : NULL;
    if (!out)
        return 2;
    for (unsigned int i = 0; i < COUNT(wolf_cases); ++i) {
        random_fixture *f = calloc(1, sizeof(*f));
        if (!f)
            return 2;
        n = snprintf(path, sizeof(path), "%s/case%u", argv[2], i);
        const bool passed =
            n > 0 && (size_t)n < sizeof(path) && setup(f, argv[1], path) && wolf_one(f, i, out);
        const bool cleaned = teardown(f);
        free(f);
        if (!passed || !cleaned)
            return 1;
    }
    if (fclose(out))
        return 1;
    printf("Wolf interest: %u checks;9 real tail profiles;3 native controls\n", checks);
    return 0;
}
