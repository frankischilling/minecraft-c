#include "mc/world/schedule/ordered.h"

#include <stdlib.h>

/* Nodes own references independently of membership F and caller locals. A key
 * is the live record owner, never a cached time/sequence or copied payload. */
typedef struct ordered_node {
    mc_scheduled_tick *tick;
    struct ordered_node *parent;
    struct ordered_node *child[2];
    bool black;
} ordered_node;

struct mc_scheduled_ordered {
    ordered_node *root;
    size_t count;
    size_t max_entries;
    size_t max_allocation_bytes;
    size_t allocation_bytes;
};

const char *mc_scheduled_ordered_status_string(mc_scheduled_ordered_status status) {
    switch (status) {
        case MC_SCHEDULED_ORDERED_OK:
            return "ok";
        case MC_SCHEDULED_ORDERED_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_SCHEDULED_ORDERED_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_SCHEDULED_ORDERED_LIMIT:
            return "collection resource limit";
        case MC_SCHEDULED_ORDERED_HOLD_LIMIT:
            return "scheduled tick hold limit";
        case MC_SCHEDULED_ORDERED_EMPTY:
            return "ordered collection is empty";
    }
    return "unknown scheduled ordered status";
}

static mc_scheduled_ordered_status ordered_record_status(mc_scheduled_tick_status status) {
    switch (status) {
        case MC_SCHEDULED_TICK_OK:
            return MC_SCHEDULED_ORDERED_OK;
        case MC_SCHEDULED_TICK_INVALID_ARGUMENT:
            return MC_SCHEDULED_ORDERED_INVALID_ARGUMENT;
        case MC_SCHEDULED_TICK_ALLOCATION_FAILED:
            return MC_SCHEDULED_ORDERED_ALLOCATION_FAILED;
        case MC_SCHEDULED_TICK_LIMIT:
            return MC_SCHEDULED_ORDERED_HOLD_LIMIT;
    }
    return MC_SCHEDULED_ORDERED_INVALID_ARGUMENT;
}

static bool ordered_black(const ordered_node *node) {
    return node == NULL || node->black;
}

static ordered_node *ordered_parent(const ordered_node *node) {
    return node == NULL ? NULL : node->parent;
}

static ordered_node *ordered_child(const ordered_node *node, unsigned side) {
    return node == NULL ? NULL : node->child[side];
}

static void ordered_color(ordered_node *node, bool black) {
    if (node != NULL) {
        node->black = black;
    }
}

/* Promoting side1 is the captured left rotation; side0 is its right mirror.
 * Valid red-black topology supplies the promoted nonnull child. No key is
 * compared/copied/retained here, even when current keys disagree with links. */
static void ordered_rotate(mc_scheduled_ordered *ordered, ordered_node *pivot,
                           unsigned promoted_side) {
    if (pivot == NULL) {
        return;
    }
    const unsigned opposite = promoted_side ^ 1u;
    ordered_node *promoted = pivot->child[promoted_side];
    ordered_node *middle = promoted->child[opposite];
    pivot->child[promoted_side] = middle;
    if (middle != NULL) {
        middle->parent = pivot;
    }
    promoted->parent = pivot->parent;
    if (pivot->parent == NULL) {
        ordered->root = promoted;
    } else {
        const unsigned old_side = pivot->parent->child[0] == pivot ? 0u : 1u;
        pivot->parent->child[old_side] = promoted;
    }
    promoted->child[opposite] = pivot;
    pivot->parent = promoted;
}

static void ordered_insert_fix(mc_scheduled_ordered *ordered, ordered_node *focus) {
    focus->black = false;
    while (focus != NULL && focus != ordered->root && !focus->parent->black) {
        ordered_node *parent = focus->parent;
        ordered_node *grandparent = parent->parent;
        const unsigned parent_side = grandparent->child[0] == parent ? 0u : 1u;
        ordered_node *uncle = grandparent->child[parent_side ^ 1u];
        if (!ordered_black(uncle)) {
            parent->black = true;
            uncle->black = true;
            grandparent->black = false;
            focus = grandparent;
        } else {
            if (focus == parent->child[parent_side ^ 1u]) {
                focus = parent;
                ordered_rotate(ordered, focus, parent_side ^ 1u);
            }
            parent = focus->parent;
            grandparent = parent->parent;
            parent->black = true;
            grandparent->black = false;
            ordered_rotate(ordered, grandparent, parent_side);
        }
    }
    ordered->root->black = true;
}

static void ordered_delete_fix(mc_scheduled_ordered *ordered, ordered_node *focus) {
    while (focus != ordered->root && ordered_black(focus)) {
        ordered_node *parent = ordered_parent(focus);
        const unsigned side = focus == ordered_child(parent, 0) ? 0u : 1u;
        const unsigned other = side ^ 1u;
        ordered_node *sibling = ordered_child(parent, other);
        if (!ordered_black(sibling)) {
            sibling->black = true;
            ordered_color(parent, false);
            ordered_rotate(ordered, parent, other);
            sibling = ordered_child(ordered_parent(focus), other);
        }
        /* Near then far matches the captured left/right mirror branch order. */
        if (ordered_black(ordered_child(sibling, side)) &&
            ordered_black(ordered_child(sibling, other))) {
            ordered_color(sibling, false);
            focus = ordered_parent(focus);
        } else {
            if (ordered_black(ordered_child(sibling, other))) {
                ordered_color(ordered_child(sibling, side), true);
                ordered_color(sibling, false);
                ordered_rotate(ordered, sibling, side);
                sibling = ordered_child(ordered_parent(focus), other);
            }
            ordered_color(sibling, ordered_black(ordered_parent(focus)));
            ordered_color(ordered_parent(focus), true);
            ordered_color(ordered_child(sibling, other), true);
            ordered_rotate(ordered, ordered_parent(focus), other);
            focus = ordered->root;
        }
    }
    ordered_color(focus, true);
}

static ordered_node *ordered_successor(ordered_node *node) {
    if (node == NULL) {
        return NULL;
    }
    if (node->child[1] != NULL) {
        ordered_node *next = node->child[1];
        while (next->child[0] != NULL) {
            next = next->child[0];
        }
        return next;
    }
    ordered_node *child = node;
    ordered_node *parent = node->parent;
    while (parent != NULL && child == parent->child[1]) {
        child = parent;
        parent = parent->parent;
    }
    return parent;
}

/* Return a comparison-path match, or the terminal parent/side for insertion.
 * Never scan for an identity/equal key in other historical branches. */
static mc_scheduled_ordered_status
ordered_search(const mc_scheduled_ordered *ordered, const mc_scheduled_tick *query,
               ordered_node **match, ordered_node **terminal_parent, unsigned *terminal_side) {
    ordered_node *node = ordered->root;
    ordered_node *parent = NULL;
    unsigned side = 0;
    while (node != NULL) {
        int32_t comparison;
        const mc_scheduled_tick_status status =
            mc_scheduled_tick_compare(query, node->tick, &comparison);
        if (status != MC_SCHEDULED_TICK_OK) {
            return ordered_record_status(status);
        }
        if (comparison == 0) {
            *match = node;
            *terminal_parent = parent;
            *terminal_side = side;
            return MC_SCHEDULED_ORDERED_OK;
        }
        parent = node;
        side = comparison < 0 ? 0u : 1u;
        node = node->child[side];
    }
    *match = NULL;
    *terminal_parent = parent;
    *terminal_side = side;
    return MC_SCHEDULED_ORDERED_OK;
}

static void ordered_delete_node(mc_scheduled_ordered *ordered, ordered_node *matched) {
    /* Original size decrement precedes topology changes. No allocation,
     * retain, comparison or callback can fail inside this deletion. */
    ordered->count -= 1;
    mc_scheduled_tick *retired_hold = matched->tick;
    ordered_node *physical = matched;
    if (matched->child[0] != NULL && matched->child[1] != NULL) {
        physical = ordered_successor(matched);
        matched->tick = physical->tick;
        /* Transfer that existing node hold, including when both keys are the
         * SAME pointer. The old matched hold remains in retired_hold locally. */
        physical->tick = NULL;
    }

    ordered_node *replacement =
        physical->child[0] != NULL ? physical->child[0] : physical->child[1];
    if (replacement != NULL) {
        replacement->parent = physical->parent;
        if (physical->parent == NULL) {
            ordered->root = replacement;
        } else {
            const unsigned side = physical == physical->parent->child[0] ? 0u : 1u;
            physical->parent->child[side] = replacement;
        }
        physical->child[0] = NULL;
        physical->child[1] = NULL;
        physical->parent = NULL;
        if (physical->black) {
            ordered_delete_fix(ordered, replacement);
        }
    } else if (physical->parent == NULL) {
        ordered->root = NULL;
    } else {
        /* A black leaf stays linked as its own phantom until fixup finishes. */
        if (physical->black) {
            ordered_delete_fix(ordered, physical);
        }
        if (physical->parent != NULL) {
            if (physical == physical->parent->child[0]) {
                physical->parent->child[0] = NULL;
            } else if (physical == physical->parent->child[1]) {
                physical->parent->child[1] = NULL;
            }
            physical->parent = NULL;
        }
    }
    free(physical);
    ordered->allocation_bytes -= sizeof(ordered_node);
    mc_scheduled_tick_release(retired_hold);
}

mc_scheduled_ordered_status mc_scheduled_ordered_create(size_t max_entries,
                                                        size_t max_allocation_bytes,
                                                        mc_scheduled_ordered **out) {
    if (out == NULL || max_entries > (size_t)INT32_MAX) {
        return MC_SCHEDULED_ORDERED_INVALID_ARGUMENT;
    }
    if (sizeof(mc_scheduled_ordered) > max_allocation_bytes) {
        return MC_SCHEDULED_ORDERED_LIMIT;
    }
    mc_scheduled_ordered *ordered = malloc(sizeof(*ordered));
    if (ordered == NULL) {
        return MC_SCHEDULED_ORDERED_ALLOCATION_FAILED;
    }
    *ordered = (mc_scheduled_ordered){
        .root = NULL,
        .count = 0,
        .max_entries = max_entries,
        .max_allocation_bytes = max_allocation_bytes,
        .allocation_bytes = sizeof(*ordered),
    };
    *out = ordered;
    return MC_SCHEDULED_ORDERED_OK;
}

void mc_scheduled_ordered_destroy(mc_scheduled_ordered *ordered) {
    if (ordered == NULL) {
        return;
    }
    /* Destructive postorder via existing links: no heap scratch, recursion,
     * comparison-path removals or record-field changes are required. */
    ordered_node *node = ordered->root;
    while (node != NULL) {
        if (node->child[0] != NULL) {
            ordered_node *next = node->child[0];
            node->child[0] = NULL;
            node = next;
        } else if (node->child[1] != NULL) {
            ordered_node *next = node->child[1];
            node->child[1] = NULL;
            node = next;
        } else {
            ordered_node *parent = node->parent;
            mc_scheduled_tick_release(node->tick);
            free(node);
            node = parent;
        }
    }
    free(ordered);
}

mc_scheduled_ordered_status mc_scheduled_ordered_size(const mc_scheduled_ordered *ordered,
                                                      int32_t *out) {
    if (ordered == NULL || out == NULL) {
        return MC_SCHEDULED_ORDERED_INVALID_ARGUMENT;
    }
    /* Insertion maintains count <= max_entries <= INT32_MAX. */
    *out = (int32_t)ordered->count;
    return MC_SCHEDULED_ORDERED_OK;
}

mc_scheduled_ordered_status mc_scheduled_ordered_native_bytes(const mc_scheduled_ordered *ordered,
                                                              size_t *out) {
    if (ordered == NULL || out == NULL) {
        return MC_SCHEDULED_ORDERED_INVALID_ARGUMENT;
    }
    *out = ordered->allocation_bytes;
    return MC_SCHEDULED_ORDERED_OK;
}

mc_scheduled_ordered_status mc_scheduled_ordered_snapshot(const mc_scheduled_ordered *ordered,
                                                          const mc_scheduled_tick **entries,
                                                          size_t capacity, size_t *written) {
    if (ordered == NULL || written == NULL || (entries == NULL && capacity != 0)) {
        return MC_SCHEDULED_ORDERED_INVALID_ARGUMENT;
    }
    if (ordered->count > capacity) {
        return MC_SCHEDULED_ORDERED_LIMIT;
    }
    ordered_node *node = ordered->root;
    while (node != NULL && node->child[0] != NULL) {
        node = node->child[0];
    }
    size_t count = 0;
    while (node != NULL) {
        entries[count++] = node->tick;
        node = ordered_successor(node);
    }
    *written = count;
    return MC_SCHEDULED_ORDERED_OK;
}

mc_scheduled_ordered_status mc_scheduled_ordered_add(mc_scheduled_ordered *ordered,
                                                     mc_scheduled_tick *tick, bool *added) {
    if (ordered == NULL || tick == NULL || added == NULL) {
        return MC_SCHEDULED_ORDERED_INVALID_ARGUMENT;
    }
    if (ordered->root == NULL) {
        int32_t self_comparison;
        const mc_scheduled_tick_status status =
            mc_scheduled_tick_compare(tick, tick, &self_comparison);
        if (status != MC_SCHEDULED_TICK_OK) {
            return ordered_record_status(status);
        }
        (void)self_comparison;
    }
    ordered_node *match;
    ordered_node *parent;
    unsigned side;
    const mc_scheduled_ordered_status found = ordered_search(ordered, tick, &match, &parent, &side);
    if (found != MC_SCHEDULED_ORDERED_OK) {
        return found;
    }
    if (match != NULL) {
        *added = false;
        return MC_SCHEDULED_ORDERED_OK;
    }
    if (ordered->count >= ordered->max_entries ||
        sizeof(ordered_node) > ordered->max_allocation_bytes - ordered->allocation_bytes) {
        return MC_SCHEDULED_ORDERED_LIMIT;
    }
    ordered_node *node = malloc(sizeof(*node));
    if (node == NULL) {
        return MC_SCHEDULED_ORDERED_ALLOCATION_FAILED;
    }
    const mc_scheduled_tick_status retained = mc_scheduled_tick_retain(tick);
    if (retained != MC_SCHEDULED_TICK_OK) {
        free(node);
        return ordered_record_status(retained);
    }
    *node = (ordered_node){
        .tick = tick,
        .parent = parent,
        .child = {NULL, NULL},
        .black = true,
    };
    /* Allocation and hold admission are complete; linking/fixup is infallible
     * under exclusive valid topology. No combined F/E operation occurs. */
    if (parent == NULL) {
        ordered->root = node;
    } else {
        parent->child[side] = node;
        ordered_insert_fix(ordered, node);
    }
    ordered->count += 1;
    ordered->allocation_bytes += sizeof(*node);
    *added = true;
    return MC_SCHEDULED_ORDERED_OK;
}

mc_scheduled_ordered_status mc_scheduled_ordered_first(const mc_scheduled_ordered *ordered,
                                                       mc_scheduled_tick **out) {
    if (ordered == NULL || out == NULL) {
        return MC_SCHEDULED_ORDERED_INVALID_ARGUMENT;
    }
    ordered_node *node = ordered->root;
    if (node == NULL) {
        return MC_SCHEDULED_ORDERED_EMPTY;
    }
    while (node->child[0] != NULL) {
        node = node->child[0];
    }
    const mc_scheduled_tick_status retained = mc_scheduled_tick_retain(node->tick);
    if (retained != MC_SCHEDULED_TICK_OK) {
        return ordered_record_status(retained);
    }
    *out = node->tick;
    return MC_SCHEDULED_ORDERED_OK;
}

mc_scheduled_ordered_status mc_scheduled_ordered_remove(mc_scheduled_ordered *ordered,
                                                        const mc_scheduled_tick *query,
                                                        bool *removed) {
    if (ordered == NULL || query == NULL || removed == NULL) {
        return MC_SCHEDULED_ORDERED_INVALID_ARGUMENT;
    }
    ordered_node *match;
    ordered_node *parent;
    unsigned side;
    const mc_scheduled_ordered_status found =
        ordered_search(ordered, query, &match, &parent, &side);
    if (found != MC_SCHEDULED_ORDERED_OK) {
        return found;
    }
    if (match == NULL) {
        *removed = false;
        return MC_SCHEDULED_ORDERED_OK;
    }
    ordered_delete_node(ordered, match);
    *removed = true;
    return MC_SCHEDULED_ORDERED_OK;
}
