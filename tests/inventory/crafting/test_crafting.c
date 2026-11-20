#include "mc/inventory/container.h"
#include "mc/item/recipes/matching.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool mc_test_registered_matching(mc_registry_side side, const mc_crafting_manager *manager);
static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static mc_item_stack *stack_new(int32_t id, int32_t count, int32_t damage) {
    mc_item_stack *stack = NULL;
    if (mc_item_stack_allocate(&stack) != MC_ITEM_STACK_OK)
        return NULL;
    if (mc_item_stack_construct(stack, id, count, damage) != MC_ITEM_STACK_OK) {
        mc_item_stack_release(stack);
        return NULL;
    }
    return stack;
}

static bool fields(const mc_item_stack *stack, int32_t id, int32_t count, int32_t damage) {
    mc_item_stack_fields f;
    return mc_item_stack_fields_get(stack, &f) == MC_ITEM_STACK_OK && f.id == id &&
           f.count == count && f.damage == damage && f.animation == 0;
}

static bool standalone(mc_registry_side side) {
    bool ok = false;
    mc_inventory_crafting *grid = NULL, *odd = NULL;
    mc_inventory_result *result = NULL;
    mc_inventory_player *player = NULL;
    mc_item_stack *input = stack_new(17, 10, 2), *output = NULL;
    const mc_item_stack *borrowed = NULL;
    mc_inventory_error error;
    CHECK(input != NULL);
    CHECK(mc_inventory_crafting_create(side, NULL, 2, 2, NULL, &grid, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_crafting_set(grid, 0, input, &error) == MC_INVENTORY_NULL_REFERENCE &&
          error.mutation_reached && error.notification_reached && error.frame_count == 1 &&
          error.frames[0].pc == 12);
    CHECK(mc_inventory_crafting_borrow_slot(grid, 0, &borrowed, NULL) == MC_INVENTORY_OK &&
          borrowed == input);
    CHECK(mc_inventory_crafting_borrow_slot(grid, 4, &borrowed, NULL) == MC_INVENTORY_OK &&
          borrowed == NULL);
    CHECK(mc_inventory_crafting_borrow_xy(grid, 2, 0, &borrowed, NULL) == MC_INVENTORY_OK &&
          borrowed == NULL);
    CHECK(mc_inventory_crafting_borrow_xy(grid, 0, 2, &borrowed, NULL) == MC_INVENTORY_OK &&
          borrowed == NULL);
    CHECK(mc_inventory_crafting_borrow_xy(grid, 0, -1, &borrowed, &error) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          error.frame_count == 2 && error.frames[1].pc == 15);
    CHECK(mc_inventory_crafting_borrow_xy(grid, 0, INT32_MIN, &borrowed, NULL) == MC_INVENTORY_OK &&
          borrowed == input);
    CHECK(mc_inventory_crafting_split(grid, 0, -3, &output, &error) ==
              MC_INVENTORY_NULL_REFERENCE &&
          output == NULL && fields(input, 17, 13, 2) && error.frames[0].pc == 81);
    CHECK(mc_inventory_crafting_split(grid, 1, 1, &output, NULL) == MC_INVENTORY_OK &&
          output == NULL);
    CHECK(mc_inventory_crafting_create(side, NULL, -2, -2, NULL, &odd, NULL) == MC_INVENTORY_OK);
    int32_t size;
    CHECK(mc_inventory_crafting_size(odd, &size, NULL) == MC_INVENTORY_OK && size == 4);
    CHECK(mc_inventory_crafting_borrow_xy(odd, 0, 0, &borrowed, NULL) == MC_INVENTORY_OK &&
          borrowed == NULL);
    CHECK(mc_inventory_crafting_retire(&odd, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_crafting_create(side, NULL, INT32_MAX, 2, NULL, &odd, &error) ==
              MC_INVENTORY_NEGATIVE_ARRAY_SIZE &&
          odd == NULL && error.frames[0].pc == 12);
    CHECK(mc_inventory_result_create(side, NULL, &result, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_result_set(result, 0, input, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_result_split(result, 0, 0, &output, NULL) == MC_INVENTORY_OK &&
          output == input);
    mc_item_stack_release(output);
    output = NULL;
    CHECK(mc_inventory_result_borrow(result, 1, &borrowed, &error) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          error.frames[0].pc == 6);
    CHECK(mc_inventory_player_create(side, NULL, NULL, &player, NULL) == MC_INVENTORY_OK);
    mc_inventory_player_state state;
    CHECK(mc_inventory_player_read_state(player, &state) && state.player == NULL &&
          state.cursor == NULL && state.selected == 0 && !state.changed);
    CHECK(mc_inventory_player_set(player, 39, input, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_player_split(player, 39, 4, &output, NULL) == MC_INVENTORY_OK &&
          fields(input, 17, 9, 2) && fields(output, 17, 4, 2));
    mc_item_stack_release(output);
    output = NULL;
    CHECK(mc_inventory_player_changed(player, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_player_read_state(player, &state) && state.changed);
    CHECK(mc_inventory_player_borrow(player, -1, &borrowed, &error) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          error.frames[0].pc == 23);
    CHECK(mc_inventory_player_split(player, 40, 1, &output, &error) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          error.frames[0].pc == 29 && output == NULL);
    ok = true;
cleanup:
    mc_item_stack_release(input);
    mc_item_stack_release(output);
    mc_inventory_crafting_retire(&grid, NULL);
    mc_inventory_crafting_retire(&odd, NULL);
    mc_inventory_result_retire(&result, NULL);
    mc_inventory_player_retire(&player, NULL);
    return ok;
}

static bool containers(mc_registry_side side, mc_registry_classes *classes) {
    bool ok = false;
    mc_inventory_player *inventory = NULL;
    mc_container *player = NULL, *workbench = NULL;
    mc_item_stack *input = stack_new(17, 2, 0), *previous = NULL, *output = NULL;
    mc_inventory_error error;
    CHECK(input != NULL &&
          mc_inventory_player_create(side, NULL, NULL, &inventory, NULL) == MC_INVENTORY_OK);
    CHECK(mc_container_player_create(side, classes, inventory, true, NULL, &player, NULL) ==
          MC_INVENTORY_OK);
    CHECK(mc_container_workbench_create(side, classes, inventory, NULL, 5, -3, 9, NULL, &workbench,
                                        NULL) == MC_INVENTORY_OK);
    mc_container_state p, w;
    CHECK(mc_container_read_state(player, &p) && mc_container_read_state(workbench, &w));
    CHECK(p.constructed && !p.active && p.local && p.slots == 45 && p.snapshots == 45 &&
          p.listeners == 0 && p.blocked_players == 0 && p.window_id == 0 && p.transaction == 0);
    CHECK(p.slot_list != p.snapshot_list && p.snapshot_list != p.listener_list &&
          p.listener_list != p.blocked_player_set);
    CHECK(w.constructed && w.slots == 46 && w.snapshots == 46 && w.world == NULL && w.x == 5 &&
          w.y == -3 && w.z == 9);
    for (int32_t i = 0; i < 45; ++i) {
        mc_container_slot_state slot;
        CHECK(mc_container_slot_read(player, i, &slot, NULL) == MC_INVENTORY_OK &&
              slot.container_index == i);
        if (i == 0)
            CHECK(slot.kind == MC_SLOT_CRAFTING && slot.inventory == p.result &&
                  slot.crafting_input == p.crafting && slot.player == NULL && slot.x == 144 &&
                  slot.y == 36);
        else if (i < 5)
            CHECK(slot.kind == MC_SLOT_NORMAL && slot.inventory == p.crafting &&
                  slot.inventory_index == i - 1);
        else if (i < 9)
            CHECK(slot.kind == MC_SLOT_ARMOR && slot.inventory == inventory &&
                  slot.armor_container == player && slot.armor_type == i - 5 &&
                  slot.inventory_index == 44 - i);
        else if (i < 36)
            CHECK(slot.inventory == inventory && slot.inventory_index == i);
        else
            CHECK(slot.inventory == inventory && slot.inventory_index == i - 36);
    }
    for (int32_t i = 0; i < 46; ++i) {
        mc_container_slot_state slot;
        CHECK(mc_container_slot_read(workbench, i, &slot, NULL) == MC_INVENTORY_OK &&
              slot.container_index == i);
        CHECK(slot.kind != MC_SLOT_ARMOR);
        if (i > 0 && i < 10)
            CHECK(slot.inventory == w.crafting && slot.inventory_index == i - 1 &&
                  slot.x == 30 + ((i - 1) % 3) * 18 && slot.y == 17 + ((i - 1) / 3) * 18);
    }
    CHECK(mc_inventory_player_retire(&inventory, NULL) == MC_INVENTORY_BUSY && inventory != NULL);
    mc_inventory_crafting *child = p.crafting;
    CHECK(mc_inventory_crafting_retire(&child, NULL) == MC_INVENTORY_BUSY && child == p.crafting);
    CHECK(mc_inventory_crafting_set(p.crafting, 0, input, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_result_get(p.result, 0, &previous, NULL) == MC_INVENTORY_OK &&
          fields(previous, 5, 4, 0));
    CHECK(mc_inventory_crafting_set(p.crafting, 0, input, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_result_get(p.result, 0, &output, NULL) == MC_INVENTORY_OK &&
          output != previous && fields(output, 5, 4, 0) && fields(previous, 5, 4, 0));
    mc_item_stack_release(output);
    output = NULL;
    CHECK(mc_inventory_crafting_split(p.crafting, 0, 1, &output, NULL) == MC_INVENTORY_OK &&
          fields(input, 17, 1, 0) && fields(output, 17, 1, 0));
    mc_item_stack_release(output);
    output = NULL;
    CHECK(mc_inventory_crafting_split(p.crafting, 0, 99, &output, NULL) == MC_INVENTORY_OK &&
          output == input);
    mc_item_stack_release(output);
    output = NULL;
    CHECK(mc_inventory_result_get(p.result, 0, &output, NULL) == MC_INVENTORY_OK && output == NULL);
    mc_inventory_limits tiny = {1024u, 65536u};
    mc_container *failed = NULL;
    CHECK(mc_container_player_create(side, classes, inventory, false, &tiny, &failed, &error) ==
              MC_INVENTORY_LIMIT &&
          failed == NULL);
    CHECK(mc_container_player_create(side, classes, NULL, false, NULL, &failed, &error) ==
              MC_INVENTORY_NULL_REFERENCE &&
          failed == NULL && error.frames[0].pc == 45);
    const size_t budgets[] = {1024u, 1600u, 2400u, 3600u, 5200u, 7200u, 12000u};
    for (size_t i = 0; i < sizeof(budgets) / sizeof(*budgets); ++i) {
        const mc_inventory_limits limits = {budgets[i], 65536u};
        mc_container *attempt = workbench;
        const mc_inventory_status status =
            mc_container_player_create_default(side, classes, inventory, &limits, &attempt, &error);
        if (status == MC_INVENTORY_OK) {
            mc_container_state created;
            CHECK(attempt != workbench && mc_container_read_state(attempt, &created) &&
                  created.local);
            CHECK(mc_container_retire(&attempt, NULL) == MC_INVENTORY_OK);
        } else
            CHECK(status == MC_INVENTORY_LIMIT && attempt == workbench);
        mc_inventory_player_state owner;
        CHECK(mc_inventory_player_read_state(inventory, &owner) && owner.bound_containers == 2);
    }
    ok = true;
cleanup:
    mc_item_stack_release(input);
    mc_item_stack_release(previous);
    mc_item_stack_release(output);
    mc_container_retire(&player, NULL);
    mc_container_retire(&workbench, NULL);
    mc_inventory_player_retire(&inventory, NULL);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *classes = NULL;
    if (argc != 3 || (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0))
        goto cleanup;
    const mc_registry_side side =
        strcmp(argv[1], "client") == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER;
    if (mc_resource_files_create(argv[2], &files) != MC_REGISTRY_OK ||
        !mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {side, profile, materials, 0,
                                         mc_resource_files_source(files)};
    mc_registry_error registry_error;
    if (mc_registry_classes_allocate(&options, &classes, &registry_error) != MC_REGISTRY_OK ||
        mc_registry_classes_initialize_statistics(classes, &registry_error) != MC_REGISTRY_OK) {
        fprintf(stderr, "Actual registry bootstrap failed: %d %s\n", (int)registry_error.status,
                registry_error.message == NULL ? "" : registry_error.message);
        goto cleanup;
    }
    mc_registry_crafting_view view;
    if (mc_registry_classes_inspect_crafting(classes, &view, NULL) != MC_REGISTRY_OK ||
        mc_crafting_manager_size(view.a) != 151 || !standalone(side) ||
        !containers(side, classes) || !mc_test_registered_matching(side, view.a))
        goto cleanup;
    printf("Concrete crafting checks passed: %u\n", checks);
    result = 0;
cleanup:
    if (mc_registry_classes_free(classes, NULL) != MC_REGISTRY_OK)
        return 1;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
