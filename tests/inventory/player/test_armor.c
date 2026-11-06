/* Genuine App/Writer/World fixture; its original entry point is not executed. */
int mc_armor_unused_statistics_fixture_main(int, char **);
#define main mc_armor_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "inventory/internal.h"
#include "inventory/player/source_internal.h"
#include "item/stack/source_internal.h"
#include "mc/inventory/runtime/armor.h"
#include <limits.h>

static bool armor_stack(int32_t id, int32_t count, int32_t damage, mc_item_stack **out) {
    CHECK(mc_item_stack_allocate(out) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(*out, id, count, damage) == MC_ITEM_STACK_OK);
    return true;
}

static bool armor_fields(mc_item_stack *stack, int32_t count, int32_t damage) {
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.count == count &&
          fields.damage == damage);
    return true;
}

static bool clear_armor(mc_inventory_player *inventory) {
    for (int32_t slot = 36; slot < 40; ++slot)
        CHECK(mc_inventory_player_source_set(inventory, slot, NULL, NULL) == MC_INVENTORY_OK);
    return true;
}

static bool aggregate(fixture *f, mc_inventory_player *inventory) {
    mc_item_stack *pieces[4] = {NULL};
    const int32_t initial[4] = {0, 24, 0, 0};
    for (int32_t i = 0; i < 4; ++i) {
        CHECK(armor_stack(298 + i, 1, initial[i], &pieces[i]));
        CHECK(mc_inventory_player_source_set(inventory, 36 + i, pieces[i], NULL) ==
              MC_INVENTORY_OK);
    }
    /* Leather maxima 33/48/45/39, defense 3/8/6/3. The chestplate has
     * half durability: total remaining141/max165 yields armor17. */
    int32_t value = -1;
    mc_inventory_armor_error e;
    CHECK(mc_inventory_player_armor_value(inventory, f->app.registry, &value, &e) ==
              MC_INVENTORY_ARMOR_OK &&
          value == 17 && e.completed_slots == 4 && e.source_returned && !inventory->active);
    CHECK(mc_inventory_player_armor_wear(inventory, f->app.registry, 1, &e) ==
              MC_INVENTORY_ARMOR_OK &&
          e.wear_calls == 4 && e.cleared_slots == 0 && !inventory->changed);
    for (size_t i = 0; i < 4; ++i)
        CHECK(armor_fields(pieces[i], 1, initial[i] + 1));
    CHECK(clear_armor(inventory));
    for (size_t i = 0; i < 4; ++i)
        mc_item_stack_release(pieces[i]);
    return true;
}

static bool aliases(fixture *f, mc_inventory_player *inventory) {
    mc_item_stack *stack = NULL;
    CHECK(armor_stack(298, 1, 33, &stack));
    CHECK(mc_inventory_player_source_set(inventory, 36, stack, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_source_set(inventory, 37, stack, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_source_set(inventory, 0, stack, NULL) == MC_INVENTORY_OK);
    const mc_statistic *broken = f->app.registry->statistics.F[298];
    int32_t before = -1, after = -1;
    CHECK(mc_client_stats_writer_value(f->app.stats_writer, broken, &before, NULL) ==
          MC_CLIENT_STATS_OK);
    int32_t value = -1;
    mc_inventory_armor_error e;
    CHECK(mc_inventory_player_armor_value(inventory, f->app.registry, &value, &e) ==
              MC_INVENTORY_ARMOR_OK &&
          value == 1);
    CHECK(mc_inventory_player_armor_wear(inventory, f->app.registry, 1, &e) ==
              MC_INVENTORY_ARMOR_OK &&
          e.wear_calls == 2 && e.empty_hooks == 2 && e.cleared_slots == 2 &&
          inventory->armor_slots[0] == NULL && inventory->armor_slots[1] == NULL &&
          inventory->main_slots[0] == stack && armor_fields(stack, 0, 1));
    CHECK(mc_client_stats_writer_value(f->app.stats_writer, broken, &after, NULL) ==
              MC_CLIENT_STATS_OK &&
          after == before + 1);
    /* Both armor occurrences are gone. The main occurrence survives release of
     * the independent allocation hold, and can publish another checked hold. */
    mc_item_stack_release(stack);
    stack = NULL;
    CHECK(mc_inventory_player_get(inventory, 0, &stack, NULL) == MC_INVENTORY_OK &&
          armor_fields(stack, 0, 1));
    CHECK(mc_inventory_player_source_set(inventory, 0, NULL, NULL) == MC_INVENTORY_OK &&
          armor_fields(stack, 0, 1));
    mc_item_stack_release(stack);
    return true;
}

static bool later_failure(fixture *f, mc_inventory_player *inventory) {
    mc_item_stack *first = NULL, *bad = NULL, *last = NULL;
    const mc_statistic *broken = f->app.registry->statistics.F[298];
    int32_t before = -1, after = -1;
    CHECK(mc_client_stats_writer_value(f->app.stats_writer, broken, &before, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(armor_stack(298, 1, 33, &first) && armor_stack(299, 1, 7, &bad) &&
          armor_stack(300, 1, 5, &last));
    CHECK(mc_inventory_player_source_set(inventory, 36, first, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_source_set(inventory, 37, bad, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_source_set(inventory, 38, last, NULL) == MC_INVENTORY_OK &&
          mc_item_stack_source_id_set(bad, -1) == MC_ITEM_STACK_OK);
    mc_inventory_armor_error e;
    CHECK(mc_inventory_player_armor_wear(inventory, f->app.registry, 1, &e) ==
              MC_INVENTORY_ARMOR_REGISTRY_FAILED &&
          e.source_exception && !e.native_policy && e.exception_class != NULL &&
          strcmp(e.exception_class, "java/lang/ArrayIndexOutOfBoundsException") == 0 &&
          e.slot == 1 && e.completed_slots == 1 && e.wear_calls == 1 && e.cleared_slots == 1 &&
          !inventory->active && inventory->armor_slots[0] == NULL &&
          inventory->armor_slots[1] == bad && inventory->armor_slots[2] == last &&
          armor_fields(first, 0, 0) && armor_fields(bad, 1, 7) && armor_fields(last, 1, 5));
    CHECK(mc_client_stats_writer_value(f->app.stats_writer, broken, &after, NULL) ==
              MC_CLIENT_STATS_OK &&
          after == before + 1);
    int32_t untouched = 777;
    CHECK(mc_inventory_player_armor_value(inventory, f->app.registry, &untouched, &e) ==
              MC_INVENTORY_ARMOR_REGISTRY_FAILED &&
          untouched == 777 && !inventory->active);
    CHECK(clear_armor(inventory));
    mc_item_stack_release(first);
    mc_item_stack_release(bad);
    mc_item_stack_release(last);
    return true;
}

static bool boundaries(fixture *f, mc_inventory_player *inventory) {
    mc_inventory_armor_error e;
    int32_t value = 777;
    CHECK(mc_inventory_player_armor_value(inventory, NULL, &value, &e) == MC_INVENTORY_ARMOR_OK &&
          value == 0 && e.completed_slots == 4);
    CHECK(mc_inventory_player_armor_wear(inventory, NULL, INT_MIN, &e) == MC_INVENTORY_ARMOR_OK);
    CHECK(mc_inventory_player_armor_value(NULL, NULL, &value, &e) ==
              MC_INVENTORY_ARMOR_INVALID_ARGUMENT &&
          !e.source_entered);
    inventory->active = true;
    CHECK(mc_inventory_player_armor_wear(inventory, NULL, 1, &e) == MC_INVENTORY_ARMOR_BUSY &&
          !e.source_entered && inventory->active);
    inventory->active = false;
    mc_item_stack **array = inventory->armor_slots;
    inventory->armor_slots = NULL;
    value = 777;
    CHECK(mc_inventory_player_armor_value(inventory, NULL, &value, &e) ==
              MC_INVENTORY_ARMOR_NULL_REFERENCE &&
          value == 777 && e.site.pc == 15 && e.source_exception && !inventory->active);
    CHECK(mc_inventory_player_armor_wear(inventory, NULL, 1, &e) ==
              MC_INVENTORY_ARMOR_NULL_REFERENCE &&
          e.site.pc == 7 && !inventory->active);
    inventory->armor_slots = array;
    mc_item_stack *stack = NULL;
    CHECK(armor_stack(298, 2, INT_MAX, &stack) &&
          mc_inventory_player_source_set(inventory, 36, stack, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_player_armor_wear(inventory, f->app.registry, 1, &e) ==
              MC_INVENTORY_ARMOR_OK &&
          !e.combat.break_selected && armor_fields(stack, 2, INT_MIN));
    /* Negative damage is not clamped: the signed addition wraps back to MAX,
     * breaks one item, and retains the nonempty armor occurrence. */
    CHECK(mc_inventory_player_armor_wear(inventory, f->app.registry, -1, &e) ==
              MC_INVENTORY_ARMOR_OK &&
          e.combat.break_selected && e.cleared_slots == 0 && inventory->armor_slots[0] == stack &&
          armor_fields(stack, 1, 0));
    CHECK(clear_armor(inventory));
    mc_item_stack_release(stack);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "armor", true))
        return 1;
    mc_inventory_player *inventory = f->player->local_player->fields.player.c;
    if (!aggregate(f, inventory) || !aliases(f, inventory) || !later_failure(f, inventory) ||
        !boundaries(f, inventory) || !teardown(f))
        return 1;
    free(f);
    printf("Armor ownership/arithmetic: %zu checks\n", checks);
    return 0;
}
