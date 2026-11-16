#include "chest_internal.h"

struct mc_inventory_chest {
    inv_budget budget;
    mc_registry_side side;
    size_t holds, depth, leaf_paths, node_paths;
    bool large, active, constructor_entered, constructor_failed, terminal_sealed;
    const char *name;
    mc_inventory_chest *left, *right;
    void *context;
    const inv_chest_leaf *leaf;
    mc_tile_entity *tile_identity;
};

static mc_inventory_status allocate(mc_registry_side side, const mc_inventory_limits *limits,
                                    mc_inventory_chest **out, mc_inventory_error *e) {
    inv_budget budget;
    if (side != MC_REGISTRY_CLIENT || out == NULL || *out != NULL || !inv_limits(limits, &budget)) {
        e->status = MC_INVENTORY_INVALID_ARGUMENT;
        e->operation = "Chest Inventory native allocation admission";
    } else {
        mc_inventory_chest *o = inv_alloc(&budget, sizeof(*o), e);
        if (o != NULL) {
            o->budget = budget;
            o->side = side;
            o->holds = o->depth = 1;
            o->node_paths = 1;
            *out = o;
        }
    }
    return e->status;
}

mc_inventory_status inv_chest_leaf_create(mc_registry_side side, void *context,
                                          mc_tile_entity *tile, const inv_chest_leaf *leaf,
                                          const mc_inventory_limits *limits,
                                          mc_inventory_chest **out, mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (leaf == NULL || leaf->release == NULL || leaf->size == NULL || leaf->name == NULL ||
        leaf->limit == NULL || leaf->borrow == NULL || leaf->set == NULL || leaf->split == NULL ||
        leaf->changed == NULL || leaf->usable == NULL) {
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
        e.operation = "Chest fixed adapter native leaves";
    } else if (allocate(side, limits, out, &e) == MC_INVENTORY_OK) {
        (*out)->context = context;
        (*out)->tile_identity = tile;
        (*out)->leaf = leaf;
        (*out)->leaf_paths = 1;
        (*out)->constructor_entered = true;
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_chest_retain(mc_inventory_chest *o, mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (o != NULL) {
        if (o->terminal_sealed) {
            e.status = MC_INVENTORY_BUSY;
            e.operation = "Chest terminal sealed native occurrence";
        } else if (o->holds == SIZE_MAX) {
            e.status = MC_INVENTORY_LIMIT;
            e.operation = "Chest Inventory native occurrence overflow";
        } else
            ++o->holds;
    }
    return inv_finish(result, &e);
}

void mc_inventory_chest_release(mc_inventory_chest *o) {
    if (o == NULL || --o->holds != 0)
        return;
    if (o->large) {
        mc_inventory_chest_release(o->left);
        mc_inventory_chest_release(o->right);
    } else if (o->leaf != NULL)
        o->leaf->release(o->context);
    inv_free(&o->budget, o);
}

mc_inventory_status mc_inventory_chest_retire(mc_inventory_chest **slot,
                                              mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (slot == NULL)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (*slot != NULL && (*slot)->active) {
        e.status = MC_INVENTORY_BUSY;
        e.operation = "Chest Inventory active retirement";
    } else {
        mc_inventory_chest_release(*slot);
        *slot = NULL;
    }
    return inv_finish(result, &e);
}

static bool calls_idle(const mc_inventory_chest *o) {
    return o == NULL ||
           (!o->active && (!o->large || (calls_idle(o->left) && calls_idle(o->right))));
}

static void end_calls(mc_inventory_chest *o) {
    if (o == NULL)
        return;
    if (o->large) {
        end_calls(o->left);
        end_calls(o->right);
    } else if (o->leaf->end_calls != NULL)
        o->leaf->end_calls(o->context);
}

mc_inventory_status mc_inventory_chest_end_calls(mc_inventory_chest *o,
                                                 mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (!calls_idle(o)) {
        e.status = MC_INVENTORY_BUSY;
        e.operation = "Chest diagnostic records have active invocations";
    } else
        end_calls(o);
    return inv_finish(result, &e);
}

mc_inventory_status
mc_inventory_chest_large_create(mc_registry_side side, const char *name, mc_inventory_chest *left,
                                mc_inventory_chest *right, const mc_inventory_limits *limits,
                                mc_inventory_chest **out, mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (mc_inventory_chest_large_allocate(side, limits, out, &e) == MC_INVENTORY_OK &&
        mc_inventory_chest_large_construct(*out, name, left, right, &e) != MC_INVENTORY_OK) {
        mc_inventory_chest_release(*out);
        *out = NULL;
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_chest_large_allocate(mc_registry_side side,
                                                      const mc_inventory_limits *limits,
                                                      mc_inventory_chest **out,
                                                      mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (allocate(side, limits, out, &e) == MC_INVENTORY_OK)
        (*out)->large = true;
    return inv_finish(result, &e);
}

static bool contains(const mc_inventory_chest *root, const mc_inventory_chest *candidate) {
    return root != NULL &&
           (root == candidate ||
            (root->large && (contains(root->left, candidate) || contains(root->right, candidate))));
}

mc_inventory_status mc_inventory_chest_large_construct(mc_inventory_chest *o, const char *name,
                                                       mc_inventory_chest *left,
                                                       mc_inventory_chest *right,
                                                       mc_inventory_error *result) {
    mc_inventory_error e = {0};
    const size_t depth = left == NULL ? 0 : left->depth;
    const size_t other = right == NULL ? 0 : right->depth;
    const size_t a_paths = left == NULL ? 0 : left->leaf_paths;
    const size_t b_paths = right == NULL ? 0 : right->leaf_paths;
    const size_t a_nodes = left == NULL ? 0 : left->node_paths;
    const size_t b_nodes = right == NULL ? 0 : right->node_paths;
    if (o == NULL || !o->large || o->constructor_entered || o->active || o->terminal_sealed ||
        (left != NULL && (!left->constructor_entered || left->constructor_failed)) ||
        (right != NULL && (!right->constructor_entered || right->constructor_failed)) ||
        contains(left, o) || contains(right, o) || (left != NULL && left->side != o->side) ||
        (right != NULL && right->side != o->side) || depth >= 256 || other >= 256 ||
        a_paths > o->budget.max_slots || b_paths > o->budget.max_slots - a_paths ||
        a_nodes >= o->budget.max_slots || b_nodes > o->budget.max_slots - 1 - a_nodes) {
        e.status = MC_INVENTORY_LIMIT;
        e.operation = "OG one source constructor native graph admission";
    } else {
        o->constructor_entered = true;
        o->name = name; /* OG.a6, nullable source String. */
        if (mc_inventory_chest_retain(left, &e) == MC_INVENTORY_OK) {
            o->left = left; /* OG.b11. */
            if (mc_inventory_chest_retain(right, &e) == MC_INVENTORY_OK)
                o->right = right; /* OG.c16. */
        }
        o->constructor_failed = e.status != MC_INVENTORY_OK;
        o->depth = 1 + (depth > other ? depth : other);
        o->leaf_paths = (o->left == NULL ? 0 : o->left->leaf_paths) +
                        (o->right == NULL ? 0 : o->right->leaf_paths);
        o->node_paths = 1 + (o->left == NULL ? 0 : o->left->node_paths) +
                        (o->right == NULL ? 0 : o->right->node_paths);
    }
    return inv_finish(result, &e);
}

static bool enter(mc_inventory_chest *o, const void *out, mc_inventory_error *e) {
    if (out == NULL) {
        e->status = MC_INVENTORY_INVALID_ARGUMENT;
        e->operation = "Chest Inventory native output";
    } else if (o == NULL) {
        e->status = MC_INVENTORY_NULL_REFERENCE;
        e->operation = "Chest Inventory actual null receiver";
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
    } else if (o->active || o->terminal_sealed) {
        e->status = MC_INVENTORY_BUSY;
        e->operation = "Chest Inventory already active";
    } else {
        o->active = true;
        return true;
    }
    return false;
}

mc_inventory_status inv_chest_leaf_count(const mc_inventory_chest *o, size_t *out,
                                         mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (out == NULL)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (o == NULL)
        *out = 0;
    else
        *out = o->leaf_paths;
    return inv_finish(result, &e);
}

mc_inventory_status inv_chest_node_count(const mc_inventory_chest *o, size_t *out,
                                         mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (out == NULL)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else
        *out = o == NULL ? 0 : o->node_paths;
    return inv_finish(result, &e);
}

mc_inventory_status inv_chest_node_at(const mc_inventory_chest *o, size_t ordinal,
                                      inv_chest_node_view *out, mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (o == NULL || out == NULL)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (ordinal >= o->node_paths)
        e.status = MC_INVENTORY_INDEX_OUT_OF_BOUNDS;
    else if (ordinal == 0)
        *out = (inv_chest_node_view){o,
                                     o->left,
                                     o->right,
                                     o->tile_identity,
                                     o->holds,
                                     o->large,
                                     o->active,
                                     o->terminal_sealed,
                                     !o->constructor_entered ? INV_CHEST_RAW
                                     : o->constructor_failed ? INV_CHEST_FAILED
                                                             : INV_CHEST_READY};
    else {
        const size_t left = o->left == NULL ? 0 : o->left->node_paths;
        --ordinal;
        (void)inv_chest_node_at(ordinal < left ? o->left : o->right,
                                ordinal < left ? ordinal : ordinal - left, out, &e);
    }
    return inv_finish(result, &e);
}

mc_inventory_status inv_chest_terminal_admit(const mc_inventory_chest *o, bool sealed,
                                             mc_inventory_error *result) {
    mc_inventory_error e = {0};
    size_t bytes;
    if (o == NULL || !inv_allocation_bytes(o, sizeof(*o), &bytes) || o->budget.used != bytes ||
        o->side != MC_REGISTRY_CLIENT || o->holds == 0 || !o->constructor_entered ||
        o->constructor_failed || o->node_paths == 0 || o->node_paths > o->budget.max_slots ||
        (o->large
             ? (o->leaf != NULL || o->context != NULL || o->tile_identity != NULL)
             : (o->leaf == NULL || o->context == NULL || o->left != NULL || o->right != NULL))) {
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
        e.operation = "Chest terminal local native graph";
    } else if (o->active || o->terminal_sealed != sealed) {
        e.status = MC_INVENTORY_BUSY;
        e.operation = "Chest terminal active or changed seal";
    } else if (o->large && inv_chest_terminal_admit(o->left, sealed, &e) == MC_INVENTORY_OK)
        (void)inv_chest_terminal_admit(o->right, sealed, &e);
    return inv_finish(result, &e);
}

void inv_chest_terminal_seal(mc_inventory_chest *o) {
    if (o == NULL)
        return;
    o->terminal_sealed = true;
    if (o->large) {
        inv_chest_terminal_seal(o->left);
        inv_chest_terminal_seal(o->right);
    }
}

mc_inventory_status inv_chest_leaf_at(const mc_inventory_chest *o, size_t ordinal,
                                      inv_chest_leaf_view *out, mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (out == NULL || o == NULL)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (!o->large) {
        if (ordinal != 0)
            e.status = MC_INVENTORY_INDEX_OUT_OF_BOUNDS;
        else
            *out = (inv_chest_leaf_view){o, o->tile_identity, o->holds};
    } else {
        size_t left;
        if (inv_chest_leaf_count(o->left, &left, &e) == MC_INVENTORY_OK)
            (void)inv_chest_leaf_at(ordinal < left ? o->left : o->right,
                                    ordinal < left ? ordinal : ordinal - left, out, &e);
    }
    return inv_finish(result, &e);
}

static bool child(mc_inventory_chest *o, mc_inventory_status status, const char *method,
                  const char *desc, int32_t pc, mc_inventory_error *e) {
    if (status == MC_INVENTORY_OK)
        return true;
    inv_frame(e, o->side, "og", "og", method, desc, pc);
    return false;
}

mc_inventory_status mc_inventory_chest_size(mc_inventory_chest *o, int32_t *out,
                                            mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (enter(o, out, &e)) {
        if (o->large) {
            int32_t a, b;
            if (child(o, mc_inventory_chest_size(o->left, &a, &e), "a", "()I", 4, &e) &&
                child(o, mc_inventory_chest_size(o->right, &b, &e), "a", "()I", 13, &e))
                *out = mc_java_i32_add(a, b);
        } else
            (void)o->leaf->size(o->context, out, &e);
        o->active = false;
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_chest_name(mc_inventory_chest *o, const char **out,
                                            mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (enter(o, out, &e)) {
        if (o->large)
            *out = o->name;
        else
            (void)o->leaf->name(o->context, out, &e);
        o->active = false;
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_chest_limit(mc_inventory_chest *o, int32_t *out,
                                             mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (enter(o, out, &e)) {
        if (o->large)
            (void)child(o, mc_inventory_chest_limit(o->left, out, &e), "d", "()I", 4, &e);
        else
            (void)o->leaf->limit(o->context, out, &e);
        o->active = false;
    }
    return inv_finish(result, &e);
}

static bool select_child(mc_inventory_chest *o, int32_t *slot, const char *method, const char *desc,
                         mc_inventory_chest **selected, bool *right, mc_inventory_error *e) {
    int32_t size;
    if (!child(o, mc_inventory_chest_size(o->left, &size, e), method, desc, 5, e))
        return false;
    *right = *slot >= size;
    *selected = *right ? o->right : o->left;
    if (*right) {
        if (!child(o, mc_inventory_chest_size(o->left, &size, e), method, desc, 22, e))
            return false;
        *slot = mc_java_i32_sub(*slot, size);
    }
    return true;
}

mc_inventory_status mc_inventory_chest_borrow(mc_inventory_chest *o, int32_t index,
                                              const mc_item_stack **out,
                                              mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (enter(o, out, &e)) {
        if (o->large) {
            mc_inventory_chest *selected;
            bool right;
            if (select_child(o, &index, "f_", "(I)Liz;", &selected, &right, &e))
                (void)child(o, mc_inventory_chest_borrow(selected, index, out, &e), "f_", "(I)Liz;",
                            right ? 28 : 39, &e);
        } else
            (void)o->leaf->borrow(o->context, index, out, &e);
        o->active = false;
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_chest_get(mc_inventory_chest *o, int32_t index,
                                           mc_item_stack **out, mc_inventory_error *result) {
    mc_inventory_error e = {0};
    const mc_item_stack *value;
    if (out == NULL)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (mc_inventory_chest_borrow(o, index, &value, &e) == MC_INVENTORY_OK)
        (void)inv_publish_hold(value, out, &e);
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_chest_set(mc_inventory_chest *o, int32_t index,
                                           mc_item_stack *value, mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (enter(o, &e, &e)) {
        if (o->large) {
            mc_inventory_chest *selected;
            bool right;
            if (select_child(o, &index, "a", "(ILiz;)V", &selected, &right, &e))
                (void)child(o, mc_inventory_chest_set(selected, index, value, &e), "a", "(ILiz;)V",
                            right ? 29 : 43, &e);
        } else
            (void)o->leaf->set(o->context, index, value, &e);
        o->active = false;
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_chest_split(mc_inventory_chest *o, int32_t index, int32_t amount,
                                             mc_item_stack **out, mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (enter(o, out, &e)) {
        if (o->large) {
            mc_inventory_chest *selected;
            bool right;
            if (select_child(o, &index, "a", "(II)Liz;", &selected, &right, &e))
                (void)child(o, mc_inventory_chest_split(selected, index, amount, out, &e), "a",
                            "(II)Liz;", right ? 29 : 41, &e);
        } else
            (void)o->leaf->split(o->context, index, amount, out, &e);
        o->active = false;
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_chest_changed(mc_inventory_chest *o, mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (enter(o, &e, &e)) {
        if (o->large) {
            if (child(o, mc_inventory_chest_changed(o->left, &e), "y_", "()V", 4, &e))
                (void)child(o, mc_inventory_chest_changed(o->right, &e), "y_", "()V", 13, &e);
        } else
            (void)o->leaf->changed(o->context, &e);
        o->active = false;
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_inventory_chest_usable(mc_inventory_chest *o, mc_entity *player, bool *out,
                                              mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (enter(o, out, &e)) {
        if (o->large) {
            bool value;
            if (child(o, mc_inventory_chest_usable(o->left, player, &value, &e), "a_", "(Lgs;)Z", 5,
                      &e)) {
                if (!value)
                    *out = false;
                else
                    (void)child(o, mc_inventory_chest_usable(o->right, player, out, &e), "a_",
                                "(Lgs;)Z", 18, &e);
            }
        } else
            (void)o->leaf->usable(o->context, player, out, &e);
        o->active = false;
    }
    return inv_finish(result, &e);
}
