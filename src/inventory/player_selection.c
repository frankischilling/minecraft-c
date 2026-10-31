#include "internal.h"

#include <string.h>

static mc_inventory_status selection_result(mc_inventory_selection_error *error,
                                            mc_inventory_status status, const char *operation) {
    if (error != NULL) {
        error->status = status;
        error->operation = operation;
        error->native_policy = status != MC_INVENTORY_OK;
    }
    return status;
}

static mc_inventory_status selection_admit(mc_inventory_player *inventory,
                                           mc_inventory_selection_error *error) {
    if (error != NULL)
        memset(error, 0, sizeof(*error));
    if (inventory == NULL || !inv_side_valid(inventory->side) || inventory->main_slots == NULL ||
        inventory->armor_slots == NULL)
        return selection_result(error, MC_INVENTORY_INVALID_ARGUMENT,
                                "Player selection actual native owner");
    if (inventory->active)
        return selection_result(error, MC_INVENTORY_BUSY, "Player selection active native owner");
    if (inventory->side != MC_REGISTRY_CLIENT)
        return selection_result(error, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                "Player selection client source scope");
    return MC_INVENTORY_OK;
}

mc_inventory_status mc_inventory_player_scroll(mc_inventory_player *inventory, int32_t wheel,
                                               mc_inventory_selection_error *error) {
    const mc_inventory_status status = selection_admit(inventory, error);
    if (status != MC_INVENTORY_OK)
        return status;
    inventory->active = true;
    if (error != NULL) {
        error->source_entered = true;
        error->site = (mc_inventory_site){"ix", "b", "(I)V", 0};
    }
    if (wheel > 0)
        wheel = 1;
    if (wheel < 0)
        wheel = -1;
    inventory->selected = mc_java_i32_sub(inventory->selected, wheel); /* putfield19 */
    if (error != NULL)
        error->mutation_reached = true;
    while (inventory->selected < 0) {
        inventory->selected = mc_java_i32_add(inventory->selected, 9); /* putfield37 */
        if (error != NULL)
            ++error->loop_iterations;
    }
    while (inventory->selected >= 9) {
        inventory->selected = mc_java_i32_sub(inventory->selected, 9); /* putfield60 */
        if (error != NULL)
            ++error->loop_iterations;
    }
    if (error != NULL)
        error->site.pc = 66;
    inventory->active = false;
    return selection_result(error, MC_INVENTORY_OK, NULL);
}

mc_inventory_status mc_inventory_player_select_hotbar(mc_inventory_player *inventory, int32_t slot,
                                                      mc_inventory_selection_error *error) {
    const mc_inventory_status status = selection_admit(inventory, error);
    if (status != MC_INVENTORY_OK)
        return status;
    if (slot < 0 || slot >= 9)
        return selection_result(error, MC_INVENTORY_INDEX_OUT_OF_BOUNDS,
                                "Player hotbar native caller index policy");
    inventory->active = true;
    inventory->selected = slot; /* Actual ix.c field; caller is Minecraft.k874. */
    if (error != NULL)
        error->mutation_reached = true;
    inventory->active = false;
    return selection_result(error, MC_INVENTORY_OK, NULL);
}
