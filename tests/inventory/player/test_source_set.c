#include "inventory/internal.h"
#include "inventory/player/source_internal.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static size_t checks;

#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Inventory source setter check%zu at%d: %s\n", checks, __LINE__,       \
                    #value);                                                                       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool source_failure(const mc_inventory_error *error, mc_inventory_status status,
                           int32_t pc) {
    return error->status == status && !error->mutation_reached && !error->notification_reached &&
           error->frame_count == 1 && strcmp(error->frames[0].class_name, "ix") == 0 &&
           strcmp(error->frames[0].method, "a") == 0 &&
           strcmp(error->frames[0].descriptor, "(ILiz;)V") == 0 && error->frames[0].pc == pc;
}

int main(void) {
    int result = 1;
    mc_inventory_player *inventory = NULL;
    mc_item_stack **main_slots = NULL, **armor_slots = NULL;
    mc_item_stack *value = NULL, *raw = NULL;
    mc_inventory_error error;
    CHECK(mc_inventory_player_source_set(NULL, -1, NULL, &error) == MC_INVENTORY_INVALID_ARGUMENT &&
          error.frame_count == 0 && !error.mutation_reached);
    CHECK(mc_inventory_player_source_set(NULL, INT32_MAX, NULL, NULL) ==
          MC_INVENTORY_INVALID_ARGUMENT);
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &inventory, NULL) ==
              MC_INVENTORY_OK &&
          inventory != NULL);
    main_slots = inventory->main_slots;
    armor_slots = inventory->armor_slots;
    CHECK(main_slots != NULL && armor_slots != NULL && !inventory->changed && !inventory->active);

    /* IX reads its captured main array's length before inspecting any slot.
     * With a real main descriptor, armor null precedes its adjusted bounds. */
    static const int32_t null_main_slots[] = {-1, 0, 35, 36, 40, INT32_MIN, INT32_MAX};
    inventory->main_slots = NULL;
    inventory->armor_slots = NULL;
    const mc_inventory_player null_before = *inventory;
    for (size_t i = 0; i < sizeof(null_main_slots) / sizeof(null_main_slots[0]); ++i) {
        CHECK(mc_inventory_player_source_set(inventory, null_main_slots[i], NULL, &error) ==
                  MC_INVENTORY_NULL_REFERENCE &&
              source_failure(&error, MC_INVENTORY_NULL_REFERENCE, 7) &&
              memcmp(&null_before, inventory, sizeof(null_before)) == 0);
        CHECK(mc_inventory_player_source_set(inventory, null_main_slots[i], NULL, NULL) ==
                  MC_INVENTORY_NULL_REFERENCE &&
              memcmp(&null_before, inventory, sizeof(null_before)) == 0);
    }
    inventory->active = true;
    CHECK(mc_inventory_player_source_set(inventory, 0, NULL, &error) == MC_INVENTORY_BUSY &&
          error.frame_count == 0 && !error.mutation_reached && inventory->active);
    inventory->active = false;
    inventory->side = MC_REGISTRY_SERVER;
    CHECK(mc_inventory_player_source_set(inventory, 0, NULL, &error) ==
              MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY &&
          error.frame_count == 0 && !error.mutation_reached && !inventory->active);
    inventory->side = MC_REGISTRY_CLIENT;
    inventory->main_slots = main_slots;
    static const int32_t null_armor_slots[] = {36, 39, 40, INT32_MAX};
    const mc_inventory_player armor_before = *inventory;
    for (size_t i = 0; i < sizeof(null_armor_slots) / sizeof(null_armor_slots[0]); ++i) {
        CHECK(mc_inventory_player_source_set(inventory, null_armor_slots[i], NULL, &error) ==
                  MC_INVENTORY_NULL_REFERENCE &&
              source_failure(&error, MC_INVENTORY_NULL_REFERENCE, 24) &&
              memcmp(&armor_before, inventory, sizeof(armor_before)) == 0);
    }
    CHECK(mc_inventory_player_source_set(inventory, -1, NULL, &error) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          source_failure(&error, MC_INVENTORY_INDEX_OUT_OF_BOUNDS, 24));
    CHECK(mc_inventory_player_source_set(inventory, 35, NULL, &error) == MC_INVENTORY_OK &&
          error.mutation_reached && !error.notification_reached && error.frame_count == 0 &&
          !inventory->changed && !inventory->active);
    inventory->armor_slots = armor_slots;
    static const int32_t bad_slots[] = {-1, INT32_MIN, 40, INT32_MAX};
    const mc_inventory_player before = *inventory;
    for (size_t i = 0; i < sizeof(bad_slots) / sizeof(bad_slots[0]); ++i)
        CHECK(mc_inventory_player_source_set(inventory, bad_slots[i], NULL, &error) ==
                  MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
              source_failure(&error, MC_INVENTORY_INDEX_OUT_OF_BOUNDS, 24) &&
              memcmp(&before, inventory, sizeof(before)) == 0);

    /* Source assignment does not read selected, clamp Stack count or force a
     * constructor phase. Exercise first/last cells of both actual arrays. */
    CHECK(mc_item_stack_allocate(&value) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(value, 35, 130, -17) == MC_ITEM_STACK_OK);
    inventory->selected = INT32_MAX;
    static const int32_t valid_slots[] = {0, 35, 36, 39};
    for (size_t i = 0; i < sizeof(valid_slots) / sizeof(valid_slots[0]); ++i)
        CHECK(mc_inventory_player_source_set(inventory, valid_slots[i], value, &error) ==
                  MC_INVENTORY_OK &&
              error.mutation_reached && !error.notification_reached && error.frame_count == 0 &&
              inventory->selected == INT32_MAX && !inventory->changed && !inventory->active);
    CHECK(main_slots[0] == value && main_slots[35] == value && armor_slots[0] == value &&
          armor_slots[3] == value);
    mc_item_stack_release(value);
    value = NULL; /* Only the actual four slot occurrences now own the Stack. */
    CHECK(mc_inventory_player_source_set(inventory, 35, NULL, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_source_set(inventory, 36, NULL, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_source_set(inventory, 39, NULL, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_player_source_set(inventory, 0, main_slots[0], &error) == MC_INVENTORY_OK &&
          error.mutation_reached);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(main_slots[0], &fields) == MC_ITEM_STACK_OK && fields.id == 35 &&
          fields.count == 130 && fields.damage == -17 && fields.animation == 0);
    CHECK(mc_item_stack_allocate(&raw) == MC_ITEM_STACK_OK &&
          mc_inventory_player_source_set(inventory, 0, raw, NULL) == MC_INVENTORY_OK);
    mc_item_stack_release(raw);
    raw = NULL;
    mc_item_stack_phase phase;
    CHECK(mc_inventory_player_source_set(inventory, 0, main_slots[0], NULL) == MC_INVENTORY_OK &&
          mc_item_stack_phase_get(main_slots[0], &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_RAW);
    inventory->changed = true;
    CHECK(mc_inventory_player_source_set(inventory, 0, NULL, &error) == MC_INVENTORY_OK &&
          error.mutation_reached && !error.notification_reached && main_slots[0] == NULL &&
          inventory->changed && inventory->selected == INT32_MAX && !inventory->active);
    for (size_t i = 0; i < 36; ++i)
        CHECK(main_slots[i] == NULL);
    for (size_t i = 0; i < 4; ++i)
        CHECK(armor_slots[i] == NULL);
    result = 0;
cleanup:
    if (inventory != NULL) {
        inventory->main_slots = main_slots;
        inventory->armor_slots = armor_slots;
        inventory->active = false;
        inventory->side = MC_REGISTRY_CLIENT;
    }
    mc_item_stack_release(value);
    mc_item_stack_release(raw);
    if (mc_inventory_player_retire(&inventory, NULL) != MC_INVENTORY_OK)
        return 1;
    if (result == 0)
        printf("PASS: %zu Inventory source setter ordering/ownership checks\n", checks);
    return result;
}
