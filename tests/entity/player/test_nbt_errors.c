/* Native passive diagnostic-storage contracts, not Player gameplay answers. */
#include "mc/entity/nbt/errors.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            result = 1;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(void) {
    int result = 0;
    mc_entity_nbt_error *error = malloc(sizeof(*error));
    CHECK(error != NULL);
    mc_entity_nbt_error_init(error);
    CHECK(sizeof(*error) == 544 && error->late_hook == MC_ENTITY_NBT_LATE_NONE &&
          error->index == SIZE_MAX && !error->minecart_read.selected &&
          !error->minecart_write.selected && !error->inventory.slot_selected &&
          error->inventory.status == MC_MINECART_INVENTORY_OK);
    error->status = MC_ENTITY_NBT_WRONG_TAG;
    error->operation = "authored common diagnostic";
    error->index = 7;
    error->base_bytecode = 311;
    error->local_player_bytecode = 2;
    error->player_bytecode = 17;
    error->stack_bytecode = 14;
    error->nbt.offset = 9;
    error->registry.initializer_bytecode = 13;
    error->stack.status = MC_ITEM_STACK_INVALID_FIELD;
    mc_entity_nbt_select_player_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_PLAYER && error->living_bytecode == 0 &&
          error->player_inventory.status == MC_INVENTORY_NBT_OK &&
          error->player_inventory.index == SIZE_MAX && error->player_inventory.slot == -1 &&
          error->player_inventory.field == NULL && error->player_inventory.operation == NULL &&
          error->player_inventory.registry.initializer_pc == -1 &&
          error->player_inventory.registry.constructor_pc == -1 &&
          error->player_inventory.registry.block_id == -1 &&
          error->player_inventory.registry.item_argument == -1 &&
          error->player_inventory.registry.active_initializer_pc == -1);
    CHECK(error->status == MC_ENTITY_NBT_WRONG_TAG && error->operation != NULL &&
          error->index == 7 && error->base_bytecode == 311 && error->local_player_bytecode == 2 &&
          error->player_bytecode == 17 && error->stack_bytecode == 14 && error->nbt.offset == 9 &&
          error->registry.initializer_bytecode == 13 &&
          error->stack.status == MC_ITEM_STACK_INVALID_FIELD);
    error->player_inventory.registry.status = MC_REGISTRY_LIMIT;
    error->player_inventory.registry.initializer_pc = 123;
    error->player_inventory.index = 3;
    error->living_bytecode = 55;
    mc_entity_nbt_select_player_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_PLAYER && error->living_bytecode == 55 &&
          error->player_inventory.index == 3 &&
          error->player_inventory.registry.status == MC_REGISTRY_LIMIT &&
          error->player_inventory.registry.initializer_pc == 123);
    mc_entity_nbt_select_cart_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_CART && !error->minecart_read.selected &&
          !error->minecart_read.entered && error->minecart_read.declaring_class == NULL &&
          !error->minecart_write.selected && error->minecart_write.method == NULL &&
          error->inventory.status == MC_MINECART_INVENTORY_OK && !error->inventory.slot_selected &&
          error->inventory.operation == NULL &&
          error->inventory.stack_cause == MC_MINECART_INVENTORY_STACK_NONE);
    error->minecart_read.selected = true;
    error->minecart_read.bytecode = 137;
    error->inventory.slot_selected = true;
    error->inventory.slot = 5;
    mc_entity_nbt_select_cart_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_CART && error->minecart_read.selected &&
          error->minecart_read.bytecode == 137 && error->inventory.slot_selected &&
          error->inventory.slot == 5 && error->status == MC_ENTITY_NBT_WRONG_TAG &&
          error->index == 7 && error->base_bytecode == 311 && error->player_bytecode == 17 &&
          error->nbt.offset == 9 && error->registry.initializer_bytecode == 13);
    mc_entity_nbt_select_player_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_PLAYER && error->living_bytecode == 0 &&
          error->player_inventory.index == SIZE_MAX &&
          error->player_inventory.registry.status == MC_REGISTRY_OK &&
          error->player_inventory.registry.initializer_pc == -1);
    mc_entity_nbt_error_init(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_NONE && error->status == MC_ENTITY_NBT_OK &&
          error->index == SIZE_MAX && error->base_bytecode == 0 && error->player_bytecode == 0 &&
          !error->minecart_read.selected && !error->minecart_write.selected &&
          !error->inventory.slot_selected && error->inventory.status == MC_MINECART_INVENTORY_OK);
    mc_entity_nbt_select_cart_detail(NULL);
    mc_entity_nbt_select_player_detail(NULL);
cleanup:
    free(error);
    if (result == 0 && printf("Player NBT error contracts: %u checks\n", checks) < 0)
        return 1;
    return result;
}
