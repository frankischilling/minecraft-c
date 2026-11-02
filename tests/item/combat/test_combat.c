/* Actual App/World/Player/Registry owners; the included entry point is unused. */
#define main mc_combat_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "entity/mob/internal.h"
#include "item/stack/source_internal.h"
#include "mc/core/java/java.h"
#include "mc/item/combat/combat.h"

#include <limits.h>

static const mc_item *actual_item(fixture *f, int32_t id) {
    const mc_registry_item_array *array = NULL;
    const mc_item *item = NULL;
    if (mc_registry_item_c_get(f->app.registry, &array, NULL) != MC_REGISTRY_OK ||
        mc_registry_item_c_at(f->app.registry, array, id, &item, NULL) != MC_REGISTRY_OK)
        return NULL;
    return item;
}

static bool stack_create(int32_t id, int32_t count, int32_t damage, mc_item_stack **out) {
    CHECK(mc_item_stack_allocate(out) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(*out, id, count, damage) == MC_ITEM_STACK_OK &&
          mc_item_stack_source_animation_set(*out, 5) == MC_ITEM_STACK_OK);
    return true;
}

static bool fields(mc_item_stack *stack, int32_t count, int32_t damage) {
    mc_item_stack_fields value;
    CHECK(mc_item_stack_fields_get(stack, &value) == MC_ITEM_STACK_OK && value.count == count &&
          value.damage == damage && value.animation == 5);
    return true;
}

static bool tool(mc_item_kind kind) {
    return kind == MC_ITEM_SHOVEL || kind == MC_ITEM_PICKAXE || kind == MC_ITEM_AXE;
}

static bool dispatch_controls(fixture *f) {
    const mc_registry_item_array *array = NULL;
    CHECK(mc_registry_item_c_get(f->app.registry, &array, NULL) == MC_REGISTRY_OK && array != NULL);
    bool seen[MC_ITEM_KIND_COUNT] = {false};
    size_t count = 0;
    for (size_t id = 0; id < array->length; ++id) {
        mc_item *item = array->elements[id];
        if (item == NULL)
            continue;
        ++count;
        seen[item->kind] = true;
        const int32_t expected = tool(item->kind)              ? item->dq.bm
                                 : item->kind == MC_ITEM_SWORD ? item->subtype.qd.a
                                                               : 1;
        int32_t damage = -777;
        mc_item_combat_error e;
        const mc_item_constructor_phase phase = item->phase;
        item->phase = MC_ITEM_RAW; /* Literal body reads source fields, not this native phase. */
        CHECK(mc_item_combat_damage(f->app.registry, item, NULL, &damage, &e) == MC_ENTITY_OK &&
              damage == expected && item->phase == MC_ITEM_RAW && !e.damage_stored);
        item->phase = phase;
        bool result = false;
        if (tool(item->kind) || item->kind == MC_ITEM_SWORD) {
            mc_item_stack *stack = NULL;
            const int32_t amount = tool(item->kind) ? 2 : 1;
            CHECK(stack_create(item->fields.bf, 2, 0, &stack) &&
                  mc_item_combat_hit(f->app.registry, item, stack, NULL, NULL, &result, &e) ==
                      MC_ENTITY_OK &&
                  result && e.durability_entered && e.damage_stored && !e.break_selected &&
                  e.amount == amount && e.site.pc == 7 && e.child.pc == 79 &&
                  fields(stack, 2, amount));
            mc_item_stack_release(stack);
        } else {
            CHECK(mc_item_combat_hit(f->app.registry, item, NULL, NULL, NULL, &result, &e) ==
                      MC_ENTITY_OK &&
                  result == (item->kind == MC_ITEM_SADDLE) && !e.damage_stored && !e.count_read &&
                  !e.saddle_stored);
        }
    }
    CHECK(count == 202);
    for (size_t kind = 0; kind < MC_ITEM_KIND_COUNT; ++kind)
        CHECK(seen[kind]);

    int32_t untouched = 123;
    mc_item_combat_error e;
    CHECK(mc_item_combat_damage(NULL, NULL, NULL, &untouched, &e) == MC_ENTITY_STATE_ERROR &&
          untouched == 123 && e.source_exception && !e.native_policy &&
          strcmp(e.exception_class, "java/lang/NullPointerException") == 0);
    const mc_item *sword = actual_item(f, 267);
    CHECK(sword != NULL && sword->kind == MC_ITEM_SWORD);
    bool result = false;
    CHECK(mc_item_combat_hit(f->app.registry, sword, NULL, NULL, NULL, &result, &e) ==
              MC_ENTITY_STATE_ERROR &&
          !result && e.source_exception && e.site.pc == 3 && e.child.pc == 1);
    /* The Item receiver is a Sword, while actual IZ.e rereads the supplied
     * Dirt Stack's id and reaches its own nondamageable Item. */
    mc_item_stack *dirt = NULL;
    CHECK(stack_create(3, 2, 17, &dirt) &&
          mc_item_combat_hit(f->app.registry, sword, dirt, NULL, NULL, &result, &e) ==
              MC_ENTITY_OK &&
          result && e.durability_entered && !e.damage_stored && e.child.pc == 7 &&
          fields(dirt, 2, 17));
    mc_item_stack_release(dirt);
    return true;
}

static bool durability_controls(fixture *f) {
    const mc_item *sword = actual_item(f, 267);
    CHECK(sword != NULL && sword->fields.a == 250);
    mc_item_combat_error e;
    mc_item_stack *stack = NULL;
    CHECK(stack_create(267, 3, 249, &stack) &&
          mc_item_stack_combat_damage(f->app.registry, stack, 1, f->player, &e) == MC_ENTITY_OK &&
          !e.break_selected && !e.statistic_entered && fields(stack, 3, 250));
    int32_t before = -1, after = -1;
    const mc_statistic *broken = f->app.registry->statistics.F[267];
    CHECK(mc_client_stats_writer_value(f->app.stats_writer, broken, &before, NULL) ==
              MC_CLIENT_STATS_OK &&
          mc_item_stack_combat_damage(f->app.registry, stack, 1, f->player, &e) == MC_ENTITY_OK &&
          e.break_selected && e.statistic_returned && e.count_stored && !e.count_clamped &&
          e.damage_reset && fields(stack, 2, 0) &&
          mc_client_stats_writer_value(f->app.stats_writer, broken, &after, NULL) ==
              MC_CLIENT_STATS_OK &&
          after == mc_java_i32_add(before, 1));
    mc_item_stack_release(stack);
    stack = NULL;

    CHECK(stack_create(267, INT_MIN, 250, &stack) &&
          mc_item_stack_combat_damage(f->app.registry, stack, 1, NULL, &e) == MC_ENTITY_OK &&
          e.break_selected && !e.statistic_entered && !e.count_clamped &&
          fields(stack, INT_MAX, 0));
    CHECK(mc_item_stack_source_damage_set(stack, INT_MAX) == MC_ITEM_STACK_OK &&
          mc_item_stack_combat_damage(f->app.registry, stack, 1, NULL, NULL) == MC_ENTITY_OK &&
          fields(stack, INT_MAX, INT_MIN));
    mc_item_stack_release(stack);
    stack = NULL;
    CHECK(stack_create(267, 0, 250, &stack) &&
          mc_item_stack_combat_damage(f->app.registry, stack, 1, NULL, &e) == MC_ENTITY_OK &&
          e.count_clamped && e.count_before == 0 && e.count_after == 0 && fields(stack, 0, 0));
    mc_item_stack_release(stack);
    stack = NULL;

    CHECK(stack_create(267, 3, 250, &stack));
    mc_statistic **saved = f->app.registry->statistics.F;
    f->app.registry->statistics.F = NULL;
    const mc_entity_status missing =
        mc_item_stack_combat_damage(f->app.registry, stack, 1, f->player, &e);
    f->app.registry->statistics.F = saved;
    CHECK(missing == MC_ENTITY_STATE_ERROR && e.source_exception && !e.native_policy &&
          e.site.pc == 47 && e.damage_stored && !e.count_read && !e.damage_reset &&
          fields(stack, 3, 251));
    CHECK(mc_item_stack_source_damage_set(stack, 250) == MC_ITEM_STACK_OK);
    mc_client_stats_writer *writer = f->app.stats_writer;
    f->app.stats_writer = NULL;
    const mc_entity_status writer_failed =
        mc_item_stack_combat_damage(f->app.registry, stack, 1, f->player, &e);
    f->app.stats_writer = writer;
    CHECK(writer_failed == MC_ENTITY_STATE_ERROR && e.source_exception && e.site.pc == 49 &&
          e.statistic_entered && !e.statistic_returned && !e.count_read && !e.damage_reset &&
          e.dependency.pc == 90 && fields(stack, 3, 251));
    mc_item_stack_release(stack);
    stack = NULL;

    /* Cold/RAW Stack is not rejected by a native phase gate: its source id0
     * reaches the real null Item receiver in the actual Item array. */
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_combat_damage(f->app.registry, stack, 1, NULL, &e) ==
              MC_ENTITY_STATE_ERROR &&
          e.source_exception && e.site.pc == 1 && e.dependency.pc == 8);
    mc_item_stack_release(stack);
    stack = NULL;

    /* Source null-array precedence is retained even with an invalid id. */
    CHECK(stack_create(INT_MIN, 3, 17, &stack));
    mc_registry_item_array *items = f->app.registry->items.c;
    f->app.registry->items.c = NULL;
    const mc_entity_status null_array =
        mc_item_stack_combat_damage(f->app.registry, stack, 1, NULL, &e);
    f->app.registry->items.c = items;
    CHECK(null_array == MC_ENTITY_STATE_ERROR && e.source_exception &&
          strcmp(e.exception_class, "java/lang/NullPointerException") == 0 &&
          e.dependency.pc == 7 && !e.damage_stored && fields(stack, 3, 17));
    CHECK(mc_item_stack_combat_damage(f->app.registry, stack, 1, NULL, &e) ==
              MC_ENTITY_STATE_ERROR &&
          e.source_exception &&
          strcmp(e.exception_class, "java/lang/ArrayIndexOutOfBoundsException") == 0 &&
          e.dependency.pc == 7 && !e.damage_stored && fields(stack, 3, 17));
    mc_item_stack_release(stack);
    stack = NULL;

    CHECK(stack_create(267, 3, 0, &stack));
    static const uint16_t unknown_name[] = {'X'};
    const mc_nbt_tag unknown = {
        .type = MC_NBT_INT, .name = {unknown_name, 1}, .value.int_value = 19};
    const mc_nbt_tag *children[] = {&unknown};
    const mc_nbt_tag source = {.type = MC_NBT_COMPOUND, .value.compound = {children, 1}};
    CHECK(mc_item_stack_preserve_source(stack, &source, NULL, NULL) == MC_ITEM_STACK_OK);
    const mc_nbt_tag *provenance = mc_item_stack_source(stack);
    CHECK(provenance != NULL &&
          mc_item_stack_combat_damage(f->app.registry, stack, 1, NULL, &e) == MC_ENTITY_OK &&
          mc_item_stack_source(stack) == provenance && fields(stack, 3, 1));
    mc_item_stack_release(stack);
    return true;
}

static mc_entity_watch_record *pig_record(mc_entity *pig) {
    for (mc_entity_watch_node *node = pig->watcher->map->head; node != NULL; node = node->next)
        if (node->key == 16)
            return node->record;
    return NULL;
}

static bool saddle_controls(fixture *f) {
    const mc_item *item = actual_item(f, 329);
    const mc_entity_class_literal *literal = NULL;
    mc_entity *pig = NULL;
    CHECK(item != NULL && item->kind == MC_ITEM_SADDLE &&
          mc_entity_class_literal_get(f->app.entities, MC_ENTITY_CLASS_KIND_PIG, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->app.entities, f->app.registry, literal, &pig, NULL) == MC_MOB_OK &&
          mc_mob_construct(pig, f->world, NULL) == MC_MOB_OK);
    mc_entity_watch_record *record = pig_record(pig);
    CHECK(record != NULL && record->type == 0);
    mc_item_combat_error e;
    bool result = false;
    record->byte_value = 2;
    record->dirty = pig->watcher->dirty = false;
    mc_item_stack *stack = NULL;
    CHECK(stack_create(329, INT_MIN, 9, &stack) &&
          mc_item_combat_hit(f->app.registry, item, stack, pig, NULL, &result, &e) ==
              MC_ENTITY_OK &&
          result && e.saddle_stored && e.count_stored && record->byte_value == 1 && record->dirty &&
          pig->watcher->dirty && fields(stack, INT_MAX, 9));
    mc_item_stack_release(stack);
    record->byte_value = 3;
    record->dirty = pig->watcher->dirty = false;
    CHECK(mc_item_combat_hit(f->app.registry, item, NULL, pig, NULL, &result, &e) == MC_ENTITY_OK &&
          result && !e.saddle_stored && !e.count_read && record->byte_value == 3 &&
          !record->dirty && !pig->watcher->dirty);
    record->byte_value = 0;
    result = false;
    CHECK(mc_item_combat_hit(f->app.registry, item, NULL, pig, NULL, &result, &e) ==
              MC_ENTITY_STATE_ERROR &&
          !result && e.saddle_stored && !e.count_read && e.source_exception && e.child.pc == 26 &&
          record->byte_value == 1 && record->dirty);
    record->byte_value = 0;
    CHECK(mc_item_combat_hit(f->app.registry, item, NULL, pig, NULL, &result, NULL) ==
              MC_ENTITY_STATE_ERROR &&
          record->byte_value == 1);
    record->byte_value = 0;
    record->type = 1;
    CHECK(mc_item_combat_hit(f->app.registry, item, NULL, pig, NULL, &result, &e) ==
              MC_ENTITY_STATE_ERROR &&
          e.source_exception && strcmp(e.exception_class, "java/lang/ClassCastException") == 0 &&
          e.child.pc == 13 && e.dependency.pc == 6 && e.watcher.pc == 19 && !e.saddle_stored);
    record->type = 0;
    mc_entity_watcher *watcher = pig->watcher;
    pig->watcher = NULL;
    const mc_entity_status missing =
        mc_item_combat_hit(f->app.registry, item, NULL, pig, NULL, &result, &e);
    pig->watcher = watcher;
    CHECK(missing == MC_ENTITY_STATE_ERROR && e.source_exception && e.child.pc == 13 &&
          e.dependency.pc == 6 && !e.saddle_stored);
    mc_entity_release(pig);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "item-combat", true) || !dispatch_controls(f) ||
        !durability_controls(f) || !saddle_controls(f) || !teardown(f))
        return 1;
    free(f);
    printf("PASS: %zu Item combat source-order/ownership checks\n", checks);
    return 0;
}
