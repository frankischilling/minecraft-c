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

static bool scatter(mc_inventory_player *inventory, mc_registry_classes *registry,
                    mc_math_classes *math, mc_player_death_state *s, mc_player_drop_call **drop,
                    mc_player_drop_error *error, mc_player_drop_error *cleanup,
                    const mc_player_drop_execution_context *execution,
                    mc_world_runtime_execution_result *owned, mc_entity_status *status,
                    mc_player_drop_status *child_status, bool *child_continuation) {
    if (inventory == NULL) {
        *status = failed(s, MC_ENTITY_STATE_ERROR,
                         "Player death null Inventory invocation receiver", true);
        return false;
    }
    if (inventory->side != MC_REGISTRY_CLIENT || inventory->terminal_sealed || inventory->active) {
        *status = failed(s,
                         inventory->active || inventory->terminal_sealed ? MC_ENTITY_BUSY
                                                                         : MC_ENTITY_STATE_ERROR,
                         "Player scatter actual exclusive client Inventory", false);
        return false;
    }
    inventory->active = true;
    s->inventory_entered = true;
    mc_entity_status result = MC_ENTITY_OK;
    bool continued = true;
    for (int armor = 0; armor < 2 && result == MC_ENTITY_OK && continued; ++armor) {
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
            mc_player_drop_status child = MC_PLAYER_DROP_OK;
            bool child_returned;
            if (execution != NULL) {
                *child_status = MC_PLAYER_DROP_OK;
                *child_continuation = false;
            }
            if (execution != NULL)
                child_returned = mc_local_player_drop_stack_begin_in_execution(
                    execution->execution, execution->code_classes, execution->code, registry,
                    player, stack, true, drop, &child, error, owned);
            else {
                child = mc_local_player_drop_stack_begin(registry, math, player, stack, true, drop,
                                                         error);
                child_returned = child == MC_PLAYER_DROP_OK;
            }
            if (execution != NULL) {
                *child_status = child;
                *child_continuation = child_returned;
            }
            s->drop_error = error;
            if (!child_returned) {
                continued = false;
                if (child == MC_PLAYER_DROP_OK)
                    break; /* Actual native/Math false; raw Source status remains OK. */
                result = drop_status(child);
                s->status = result;
                s->operation = error->operation;
                s->native_policy = error->native_policy;
                s->source_exception = error->source_exception;
                s->source_error = error->source_error;
                s->exception_class = error->exception_class;
                break;
            }
            if (execution != NULL) {
                const mc_world_storage_status closed = mc_local_player_drop_native_close(*drop);
                if (closed != MC_WORLD_STORAGE_OK) {
                    mc_world_runtime_execution_result_note(
                        owned, closed, "Player scatter completed owned drop close",
                        (mc_entity_tick_execution_site){"ix", "g", "()V", armor ? 78 : 31});
                    continued = false;
                    break;
                }
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
    if (continued && result == MC_ENTITY_OK) {
        s->inventory_pc = 94;
        s->inventory_returned = true;
    }
    *status = result;
    return continued && result == MC_ENTITY_OK;
}

mc_entity_status
mc_inventory_player_scatter_entered(mc_inventory_player *inventory, mc_registry_classes *registry,
                                    mc_math_classes *math, mc_player_death_state *s,
                                    mc_player_drop_call **drop, mc_player_drop_error *error,
                                    mc_player_drop_error *cleanup) {
    mc_entity_status status = MC_ENTITY_OK;
    (void)scatter(inventory, registry, math, s, drop, error, cleanup, NULL, NULL, &status, NULL,
                  NULL);
    return status;
}

bool mc_inventory_player_scatter_entered_in_execution(
    mc_entity_tick_execution *execution, mc_world_runtime_classes *code_classes,
    mc_world_runtime_execution_code code, mc_inventory_player *inventory,
    mc_registry_classes *registry, mc_player_death_state *s, mc_player_drop_call **drop,
    mc_player_drop_error *error, mc_player_drop_error *cleanup, mc_entity_status *status,
    mc_player_drop_status *child_status, bool *child_continuation,
    mc_world_runtime_execution_result *owned) {
    if (owned != NULL)
        mc_world_runtime_execution_result_init(owned);
    if (execution == NULL || execution->identity != execution || execution->math == NULL ||
        code_classes == NULL || s == NULL || drop == NULL || *drop != NULL || error == NULL ||
        cleanup == NULL || status == NULL || child_status == NULL || child_continuation == NULL ||
        owned == NULL) {
        mc_world_runtime_execution_result_note(
            owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
            "Player scatter execution/code/outputs required",
            (mc_entity_tick_execution_site){"ix", "g", "()V", 0});
        return false;
    }
    const mc_player_drop_execution_context descriptor = {execution, code_classes, code};
    return scatter(inventory, registry, NULL, s, drop, error, cleanup, &descriptor, owned, status,
                   child_status, child_continuation);
}
