#include "internal.h"

struct mc_inventory_furnace {
    inv_budget budget;
    mc_registry_side side;
    size_t holds;
    bool active, terminal_sealed;
    mc_tile_entity *tile;
    void *context;
    const inv_furnace_leaf *leaf;
};

mc_inventory_status inv_furnace_leaf_create(mc_registry_side side, void *context,
                                            mc_tile_entity *tile, const inv_furnace_leaf *leaf,
                                            const mc_inventory_limits *limits,
                                            mc_inventory_furnace **out,
                                            mc_inventory_error *result) {
    mc_inventory_error e = {0};
    inv_budget budget;
    if (side != MC_REGISTRY_CLIENT || context == NULL || tile == NULL || leaf == NULL ||
        out == NULL || *out != NULL || !inv_limits(limits, &budget)) {
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
        e.operation = "Furnace fixed SK adapter native admission";
    } else {
        mc_inventory_furnace *o = inv_alloc(&budget, sizeof(*o), &e);
        if (o != NULL) {
            o->budget = budget;
            o->side = side;
            o->holds = 1;
            o->tile = tile;
            o->context = context;
            o->leaf = leaf;
            *out = o;
        }
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_furnace_retain(mc_inventory_furnace *o,
                                                mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (o != NULL) {
        if (o->terminal_sealed) {
            e.status = MC_INVENTORY_BUSY;
            e.operation = "Furnace terminal sealed native occurrence";
        } else if (o->holds == SIZE_MAX)
            e.status = MC_INVENTORY_LIMIT;
        else
            ++o->holds;
    }
    return inv_finish(result, &e);
}

void mc_inventory_furnace_release(mc_inventory_furnace *o) {
    if (o == NULL || --o->holds != 0)
        return;
    o->leaf->release(o->context);
    inv_free(&o->budget, o);
}

mc_inventory_status mc_inventory_furnace_retire(mc_inventory_furnace **slot,
                                                mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (slot == NULL)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (*slot != NULL && (*slot)->active)
        e.status = MC_INVENTORY_BUSY;
    else {
        mc_inventory_furnace_release(*slot);
        *slot = NULL;
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_furnace_end_calls(mc_inventory_furnace *o,
                                                   mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (o != NULL && o->active)
        e.status = MC_INVENTORY_BUSY;
    else if (o != NULL)
        o->leaf->end_calls(o->context);
    return inv_finish(result, &e);
}

bool mc_inventory_furnace_read_state(const mc_inventory_furnace *o,
                                     mc_inventory_furnace_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = (mc_inventory_furnace_state){o->side,        o->tile,   o->holds,
                                        o->budget.used, o->active, o->terminal_sealed};
    return true;
}

mc_inventory_status inv_furnace_node_get(const mc_inventory_furnace *o, inv_furnace_node_view *out,
                                         mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (o == NULL || out == NULL)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else
        *out = (inv_furnace_node_view){o,         o->tile,           o->holds, o->budget.used,
                                       o->active, o->terminal_sealed};
    return inv_finish(result, &e);
}

mc_inventory_status inv_furnace_terminal_admit(const mc_inventory_furnace *o, bool sealed,
                                               mc_inventory_error *result) {
    mc_inventory_error e = {0};
    size_t bytes = 0;
    if (o == NULL || o->side != MC_REGISTRY_CLIENT || o->holds == 0 || o->tile == NULL ||
        o->context == NULL || o->leaf == NULL || !inv_allocation_bytes(o, sizeof(*o), &bytes) ||
        bytes != o->budget.used)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (o->active || o->terminal_sealed != sealed)
        e.status = MC_INVENTORY_BUSY;
    return inv_finish(result, &e);
}

void inv_furnace_terminal_seal(mc_inventory_furnace *o) {
    if (o != NULL)
        o->terminal_sealed = true;
}

static bool enter(mc_inventory_furnace *o, bool output, mc_inventory_error *e, const char *method,
                  const char *desc, int32_t pc) {
    if (o == NULL) {
        e->status = MC_INVENTORY_NULL_REFERENCE;
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        inv_frame(e, MC_REGISTRY_CLIENT, "sk", "sk", method, desc, pc);
    } else if (!output)
        e->status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (o->active || o->terminal_sealed)
        e->status = MC_INVENTORY_BUSY;
    else {
        o->active = true;
        return true;
    }
    return false;
}

#define FURNACE_CALL(method, desc, pc, valid, expression)                                          \
    mc_inventory_error e = {0};                                                                    \
    if (enter(o, (valid), &e, (method), (desc), (pc))) {                                           \
        (void)(expression);                                                                        \
        o->active = false;                                                                         \
    }                                                                                              \
    return inv_finish(result, &e)

mc_inventory_status mc_inventory_furnace_tile_borrow(mc_inventory_furnace *o, mc_tile_entity **out,
                                                     mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (o == NULL || out == NULL)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else
        *out = o->tile; /* Passive native identity view, no SK source method. */
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_furnace_size(mc_inventory_furnace *o, int32_t *out,
                                              mc_inventory_error *result) {
    FURNACE_CALL("a", "()I", 4, out != NULL, o->leaf->size(o->context, out, &e));
}

mc_inventory_status mc_inventory_furnace_name(mc_inventory_furnace *o, const char **out,
                                              mc_inventory_error *result) {
    FURNACE_CALL("c", "()Ljava/lang/String;", 0, out != NULL, (*out = "Furnace"));
}

mc_inventory_status mc_inventory_furnace_limit(mc_inventory_furnace *o, int32_t *out,
                                               mc_inventory_error *result) {
    FURNACE_CALL("d", "()I", 0, out != NULL, (*out = 64));
}

mc_inventory_status mc_inventory_furnace_borrow(mc_inventory_furnace *o, int32_t index,
                                                const mc_item_stack **out,
                                                mc_inventory_error *result) {
    FURNACE_CALL("f_", "(I)Liz;", 5, out != NULL, o->leaf->borrow(o->context, index, out, &e));
}

mc_inventory_status mc_inventory_furnace_get(mc_inventory_furnace *o, int32_t index,
                                             mc_item_stack **out, mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (enter(o, out != NULL, &e, "f_", "(I)Liz;", 5)) {
        const mc_item_stack *borrow = NULL;
        if (o->leaf->borrow(o->context, index, &borrow, &e) == MC_INVENTORY_OK)
            (void)inv_publish_hold(borrow, out, &e);
        o->active = false;
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_furnace_set(mc_inventory_furnace *o, int32_t index,
                                             mc_item_stack *value, mc_inventory_error *result) {
    FURNACE_CALL("a", "(ILiz;)V", 6, true, o->leaf->set(o->context, index, value, &e));
}

mc_inventory_status mc_inventory_furnace_split(mc_inventory_furnace *o, int32_t index,
                                               int32_t amount, mc_item_stack **out,
                                               mc_inventory_error *result) {
    FURNACE_CALL("a", "(II)Liz;", 5, out != NULL,
                 o->leaf->split(o->context, index, amount, out, &e));
}

mc_inventory_status mc_inventory_furnace_changed(mc_inventory_furnace *o,
                                                 mc_inventory_error *result) {
    FURNACE_CALL("y_", "()V", 24, true, o->leaf->changed(o->context, &e));
}

mc_inventory_status mc_inventory_furnace_usable(mc_inventory_furnace *o, mc_entity *player,
                                                bool *out, mc_inventory_error *result) {
    FURNACE_CALL("a_", "(Lgs;)Z", 16, out != NULL, o->leaf->usable(o->context, player, out, &e));
}

mc_inventory_status mc_inventory_furnace_burning(mc_inventory_furnace *o, bool *out,
                                                 mc_inventory_error *result) {
    FURNACE_CALL("b", "()Z", 1, out != NULL, o->leaf->burning(o->context, out, &e));
}

mc_inventory_status mc_inventory_furnace_cook_scale(mc_inventory_furnace *o, int32_t scale,
                                                    int32_t *out, mc_inventory_error *result) {
    FURNACE_CALL("b", "(I)I", 1, out != NULL, o->leaf->cook_scale(o->context, scale, out, &e));
}

mc_inventory_status mc_inventory_furnace_burn_scale(mc_inventory_furnace *o, int32_t scale,
                                                    int32_t *out, mc_inventory_error *result) {
    FURNACE_CALL("c", "(I)I", 1, out != NULL, o->leaf->burn_scale(o->context, scale, out, &e));
}

mc_inventory_status inv_furnace_counters(mc_inventory_furnace *o, int32_t *cook, int32_t *burn,
                                         int32_t *fuel, mc_inventory_error *result) {
    FURNACE_CALL("a", "()V", 131, cook != NULL && burn != NULL && fuel != NULL,
                 o->leaf->counters(o->context, cook, burn, fuel, &e));
}

mc_inventory_status inv_furnace_progress(mc_inventory_furnace *o, int32_t index, int32_t value,
                                         mc_inventory_error *result) {
    FURNACE_CALL("a", "(II)V", 9, true, o->leaf->progress(o->context, index, value, &e));
}

#undef FURNACE_CALL
