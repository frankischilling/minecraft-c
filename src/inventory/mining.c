#include "mc/inventory/mining.h"
#include "internal.h"

static mc_inventory_status mining_error(mc_inventory_error *e, mc_inventory_status status,
                                        const char *operation, bool harvest, int32_t pc) {
    e->status = status;
    e->operation = operation;
    inv_frame(e, MC_REGISTRY_CLIENT, "ix", "fx", harvest ? "b" : "a",
              harvest ? "(Luu;)Z" : "(Luu;)F", pc);
    return status;
}

static bool mining_admit(mc_inventory_player *inventory, const void *out, bool harvest,
                         mc_inventory_error *e) {
    if (inventory == NULL || out == NULL) {
        (void)mining_error(e, MC_INVENTORY_INVALID_ARGUMENT, "Inventory mining native owner/output",
                           harvest, 0);
        return false;
    }
    if (inventory->active) {
        (void)mining_error(e, MC_INVENTORY_BUSY, "Inventory mining entered owner", harvest, 0);
        return false;
    }
    if (inventory->side != MC_REGISTRY_CLIENT) {
        (void)mining_error(e, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                           "Inventory mining client method scope", harvest, 0);
        return false;
    }
    inventory->active = true;
    return true;
}

mc_inventory_status mc_inventory_player_mining_speed(mc_inventory_player *inventory,
                                                     const mc_block *block, float *out,
                                                     mc_inventory_error *error) {
    mc_inventory_error e = {0};
    if (!mining_admit(inventory, out, false, &e))
        return inv_finish(error, &e);
    (void)block; /* The empty selected-cell arm never dereferences its argument. */
    if (inventory->main_slots == NULL)
        (void)mining_error(&e, MC_INVENTORY_NULL_REFERENCE, "actual ix.a null main array", false,
                           10);
    else if (inventory->selected < 0 || inventory->selected >= 36)
        (void)mining_error(&e, MC_INVENTORY_INDEX_OUT_OF_BOUNDS, "actual ix.a main array index",
                           false, 10);
    else if (inventory->main_slots[(size_t)inventory->selected] != NULL)
        (void)mining_error(&e, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                           "actual iz.a(Block) mining speed virtual", false, 25);
    else
        *out = 1.0f;
    inventory->active = false;
    return inv_finish(error, &e);
}

mc_inventory_status mc_inventory_player_can_harvest(mc_inventory_player *inventory,
                                                    const mc_block *block, bool *out,
                                                    mc_inventory_error *error) {
    mc_inventory_error e = {0};
    if (!mining_admit(inventory, out, true, &e))
        return inv_finish(error, &e);
    const mc_block_fields *fields = mc_block_fields_get(block);
    if (fields == NULL)
        (void)mining_error(&e, MC_INVENTORY_NULL_REFERENCE, "actual ix.b null Block receiver", true,
                           1);
    else if (fields->bA == NULL)
        (void)mining_error(&e, MC_INVENTORY_NULL_REFERENCE, "actual ix.b null Material receiver",
                           true, 4);
    else if (mc_material_i(fields->bA))
        *out = true;
    else {
        /* Actual f_(selected), including its main-to-armor subtraction. */
        mc_item_stack *const *array = inventory->main_slots;
        int32_t index = inventory->selected;
        if (array == NULL)
            (void)mining_error(&e, MC_INVENTORY_NULL_REFERENCE,
                               "actual ix.f_ null main array length", true, 17);
        else {
            if (index >= 36) {
                index = mc_java_i32_sub(index, 36);
                array = inventory->armor_slots;
            }
            if (array == NULL)
                (void)mining_error(&e, MC_INVENTORY_NULL_REFERENCE,
                                   "actual ix.f_ null selected array", true, 17);
            else if (index < 0 || index >= (inventory->selected >= 36 ? 4 : 36))
                (void)mining_error(&e, MC_INVENTORY_INDEX_OUT_OF_BOUNDS,
                                   "actual ix.f_ selected array index", true, 17);
            else if (array[(size_t)index] != NULL)
                (void)mining_error(&e, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                   "actual iz.b(Block) harvest virtual", true, 27);
            else
                *out = false;
        }
    }
    inventory->active = false;
    return inv_finish(error, &e);
}
