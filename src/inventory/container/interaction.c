#include "interaction_internal.h"

#include <stdlib.h>

static mc_inventory_status begin(mc_container *container, mc_entity *player, int32_t slot,
                                 int32_t button, bool shift, bool close,
                                 const mc_container_interaction_host *host,
                                 mc_container_interaction_call **out,
                                 mc_container_interaction_error *error) {
    if (out == NULL || *out != NULL) {
        if (error != NULL) {
            mc_container_interaction_error_init(error);
            (void)inv_interaction_fail(error, MC_INVENTORY_INVALID_ARGUMENT,
                                       "Container invocation fresh native output");
        }
        return MC_INVENTORY_INVALID_ARGUMENT;
    }
    mc_container_interaction_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        if (error != NULL) {
            mc_container_interaction_error_init(error);
            (void)inv_interaction_fail(error, MC_INVENTORY_ALLOCATION_FAILED,
                                       "Container invocation native allocation");
        }
        return MC_INVENTORY_ALLOCATION_FAILED;
    }
    mc_container_interaction_error_init(&c->error);
    c->state = (mc_container_interaction_state){.container = container,
                                                .player = player,
                                                .error = &c->error,
                                                .slot = slot,
                                                .button = button,
                                                .shift = shift,
                                                .close = close};
    if (host != NULL)
        c->host = *host;
    *out = c;
    if (container == NULL)
        (void)inv_interaction_null(&c->error, "Container null actual invocation receiver");
    else if (container->side != MC_REGISTRY_CLIENT)
        (void)inv_interaction_fail(&c->error, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                   "Container interaction client source scope");
    else if (mc_container_retain(container, &c->error.inventory) != MC_INVENTORY_OK)
        (void)inv_interaction_inventory(&c->error, c->error.inventory.status);
    else {
        c->container_held = true;
        if (container->interaction_active)
            (void)inv_interaction_fail(&c->error, MC_INVENTORY_BUSY,
                                       "Container actual interaction already active");
        else {
            c->state.active = container->interaction_active = true;
            const bool returned =
                close ? inv_interaction_close_body(c)
                      : inv_interaction_click_body(c, slot, button, shift, 0, &c->result);
            c->state.active = container->interaction_active = false;
            c->state.returned = returned && c->error.status == MC_INVENTORY_OK;
            c->state.result = c->result;
            c->state.source_pc = c->error.site.pc;
        }
    }
    if (error != NULL)
        *error = c->error;
    return c->error.status;
}

mc_inventory_status mc_container_click_begin(mc_container *container, mc_entity *player,
                                             int32_t slot, int32_t button, bool shift,
                                             const mc_container_interaction_host *host,
                                             mc_container_interaction_call **out,
                                             mc_container_interaction_error *error) {
    return begin(container, player, slot, button, shift, false, host, out, error);
}

mc_inventory_status mc_container_close_begin(mc_container *container, mc_entity *player,
                                             const mc_container_interaction_host *host,
                                             mc_container_interaction_call **out,
                                             mc_container_interaction_error *error) {
    return begin(container, player, 0, 0, false, true, host, out, error);
}

bool mc_container_interaction_inspect(const mc_container_interaction_call *c,
                                      mc_container_interaction_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

mc_inventory_status mc_container_interaction_take_result(mc_container_interaction_call *c,
                                                         mc_item_stack **out) {
    if (c == NULL || out == NULL || *out != NULL || c->result_taken)
        return MC_INVENTORY_INVALID_ARGUMENT;
    if (c->state.active)
        return MC_INVENTORY_BUSY;
    if (!c->state.returned)
        return MC_INVENTORY_INVALID_ARGUMENT;
    if (c->result != NULL) {
        size_t i = 0;
        while (i < c->hold_count && c->holds[i] != c->result)
            ++i;
        if (i == c->hold_count)
            return MC_INVENTORY_INVALID_ARGUMENT;
        c->holds[i] = NULL;
    }
    *out = c->result;
    c->result = NULL;
    c->state.result = NULL;
    c->result_taken = true;
    return MC_INVENTORY_OK;
}

mc_inventory_status mc_container_interaction_retire(mc_container_interaction_call **out,
                                                    mc_container_interaction_error *error) {
    if (out == NULL)
        return MC_INVENTORY_INVALID_ARGUMENT;
    mc_container_interaction_call *c = *out;
    if (c == NULL) {
        mc_container_interaction_error_init(error);
        return MC_INVENTORY_OK;
    }
    if (c->state.active) {
        if (error != NULL) {
            mc_container_interaction_error_init(error);
            (void)inv_interaction_fail(error, MC_INVENTORY_BUSY,
                                       "Container invocation active retirement");
        }
        return MC_INVENTORY_BUSY;
    }
    if (c->container_held &&
        mc_container_release(c->state.container, &c->error.inventory) != MC_INVENTORY_OK) {
        (void)inv_interaction_inventory(&c->error, c->error.inventory.status);
        if (error != NULL)
            *error = c->error;
        return c->error.inventory.status;
    }
    for (size_t i = 0; i < c->hold_count; ++i)
        mc_item_stack_release(c->holds[i]);
    free(c->holds);
    free(c);
    *out = NULL;
    mc_container_interaction_error_init(error);
    return MC_INVENTORY_OK;
}
