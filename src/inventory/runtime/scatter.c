#include "scatter_internal.h"

#include "../internal.h"

static mc_entity_status failed(mc_player_death_state *s, mc_entity_status status,
                               const char *operation, bool source_null) {
    s->status = status;
    s->operation = operation;
    s->native_policy = !source_null;
    s->source_exception = source_null;
    s->exception_class = source_null ? "java/lang/NullPointerException" : NULL;
    return status;
}

static mc_entity_status drop_status(mc_player_drop_status status) {
    return status == MC_PLAYER_DROP_LIMIT        ? MC_ENTITY_LIMIT
           : status == MC_PLAYER_DROP_ALLOCATION ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_PLAYER_DROP_BUSY || status == MC_PLAYER_DROP_CLEANUP ? MC_ENTITY_BUSY
           : status == MC_PLAYER_DROP_FLOATING ? MC_ENTITY_UNSUPPORTED_FP
           : status == MC_PLAYER_DROP_WORLD    ? MC_ENTITY_WORLD_FAILED
                                               : MC_ENTITY_STATE_ERROR;
}

mc_entity_status
mc_inventory_player_scatter_entered(mc_inventory_player *inventory, mc_registry_classes *registry,
                                    mc_math_classes *math, mc_player_death_state *s,
                                    mc_player_drop_call **drop, mc_player_drop_error *error,
                                    mc_player_drop_error *cleanup) {
    if (inventory == NULL)
        return failed(s, MC_ENTITY_STATE_ERROR, "Player death null Inventory invocation receiver",
                      true);
    if (inventory->side != MC_REGISTRY_CLIENT || inventory->terminal_sealed || inventory->active)
        return failed(s,
                      inventory->active || inventory->terminal_sealed ? MC_ENTITY_BUSY
                                                                      : MC_ENTITY_STATE_ERROR,
                      "Player scatter actual exclusive client Inventory", false);
    inventory->active = true;
    s->inventory_entered = true;
    mc_entity_status result = MC_ENTITY_OK;
    for (int armor = 0; armor < 2 && result == MC_ENTITY_OK; ++armor) {
        s->inventory_armor = armor != 0;
        const int32_t length = armor ? 4 : 36;
        for (int32_t slot = 0;; ++slot) {
            s->inventory_slot = slot;
            s->inventory_pc = armor ? 54 : 7;
            mc_item_stack **array = armor ? inventory->armor_slots : inventory->main_slots;
            if (array == NULL) {
                result = failed(s, MC_ENTITY_STATE_ERROR,
                                "Player scatter null Inventory array receiver", true);
                break;
            }
            if (slot >= length)
                break;
            s->inventory_pc = armor ? 64 : 17;
            if (array[slot] == NULL)
                continue;
            /* Player argument20/68 precedes the fresh array24/72 and cell29/76. */
            mc_entity *player = inventory->player;
            array = armor ? inventory->armor_slots : inventory->main_slots;
            s->inventory_pc = armor ? 76 : 29;
            if (array == NULL) {
                result = failed(s, MC_ENTITY_STATE_ERROR,
                                "Player scatter null argument array receiver", true);
                break;
            }
            mc_item_stack *stack = array[slot];
            s->inventory_pc = armor ? 78 : 31;
            const mc_player_drop_status child =
                mc_local_player_drop_stack_begin(registry, math, player, stack, true, drop, error);
            s->drop_error = error;
            if (child != MC_PLAYER_DROP_OK) {
                result = drop_status(child);
                s->status = result;
                s->operation = error->operation;
                s->native_policy = error->native_policy;
                s->source_exception = error->source_exception;
                s->source_error = error->source_error;
                s->exception_class = error->exception_class;
                break;
            }
            ++s->drops_returned;
            array = armor ? inventory->armor_slots : inventory->main_slots;
            s->inventory_pc = armor ? 87 : 40;
            if (array == NULL) {
                result = failed(s, MC_ENTITY_STATE_ERROR,
                                "Player scatter null clearing array receiver", true);
                break;
            }
            mc_item_stack *old = array[slot];
            array[slot] = NULL;
            ++s->slots_cleared;
            mc_item_stack_release(old);
            if (mc_local_player_drop_retire(drop, cleanup) != MC_PLAYER_DROP_OK) {
                result = failed(s, MC_ENTITY_BUSY, "Player scatter returned child cleanup", false);
                s->cleanup_status = result;
                s->cleanup_operation = cleanup->operation;
                break;
            }
            ++s->drops_retired;
            s->drop_error = NULL;
        }
    }
    inventory->active = false;
    if (result == MC_ENTITY_OK) {
        s->inventory_pc = 94;
        s->inventory_returned = true;
    }
    return result;
}
