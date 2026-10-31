#include "mc/inventory/player_current.h"
#include "internal.h"

#include <string.h>

static mc_inventory_status current_result(mc_inventory_current_error *error,
                                          mc_inventory_status status, const char *operation,
                                          bool native_policy) {
    if (error != NULL) {
        error->status = status;
        error->operation = operation;
        error->native_policy = native_policy;
    }
    return status;
}

mc_inventory_status mc_inventory_player_current(mc_inventory_player *inventory,
                                                const mc_item_stack **out,
                                                mc_inventory_current_error *error) {
    if (error != NULL)
        memset(error, 0, sizeof(*error));
    if (inventory == NULL || out == NULL || !inv_side_valid(inventory->side))
        return current_result(error, MC_INVENTORY_INVALID_ARGUMENT,
                              "Player current Stack native owner/output", true);
    if (inventory->active)
        return current_result(error, MC_INVENTORY_BUSY, "Player current Stack active inventory",
                              true);
    if (inventory->side != MC_REGISTRY_CLIENT)
        return current_result(error, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                              "Player current Stack client source scope", true);
    inventory->active = true;
    const int32_t selected = inventory->selected;
    if (error != NULL) {
        error->source_entered = true;
        error->selected = selected;
        error->site = (mc_inventory_site){"ix", "b", "()Liz;", 1};
    }
    mc_inventory_status status = MC_INVENTORY_OK;
    if (selected >= 9 || selected < 0) {
        *out = NULL;
        if (error != NULL)
            error->site.pc = 27;
    } else {
        if (error != NULL) {
            error->array_read = true;
            error->site.pc = 24;
        }
        if (inventory->main_slots == NULL)
            status = current_result(error, MC_INVENTORY_NULL_REFERENCE,
                                    "actual ix.b null main array", false);
        else {
            *out = inventory->main_slots[(size_t)selected];
            if (error != NULL)
                error->site.pc = 25;
        }
    }
    if (status == MC_INVENTORY_OK) {
        current_result(error, MC_INVENTORY_OK, NULL, false);
        if (error != NULL)
            error->source_returned = true;
    }
    inventory->active = false;
    return status;
}
