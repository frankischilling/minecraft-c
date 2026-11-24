#include "hash_table_internal.h"

#include <stddef.h>

static unsigned hash_side(const mc_java_hash_node *left, const mc_java_hash_node *right,
                          mc_java_hash_compare tie_break, void *context) {
    uint32_t left_hash = left->hash, right_hash = right->hash;
    if (left_hash != right_hash)
        return (left_hash ^ UINT32_C(0x80000000)) < (right_hash ^ UINT32_C(0x80000000)) ? 0u : 1u;
    return tie_break(context, left, right) < 0 ? 0u : 1u;
}

/* Promote one child while preserving the separate traversal chain. */
static mc_java_hash_node *hash_rotate(mc_java_hash_node *root, mc_java_hash_node *node,
                                      unsigned side) {
    mc_java_hash_node *pivot = node->children[side];
    mc_java_hash_node *parent = node->parent;
    node->children[side] = pivot->children[1u - side];
    if (node->children[side] != NULL)
        node->children[side]->parent = node;
    pivot->parent = parent;
    if (parent == NULL) {
        root = pivot;
        root->red = false;
    } else {
        parent->children[parent->children[1] == node ? 1 : 0] = pivot;
    }
    pivot->children[1u - side] = node;
    node->parent = pivot;
    return root;
}

static mc_java_hash_node *hash_balance_add(mc_java_hash_node *root, mc_java_hash_node *node) {
    node->red = true;
    for (;;) {
        mc_java_hash_node *parent = node->parent;
        if (parent == NULL) {
            node->red = false;
            return node;
        }
        mc_java_hash_node *grand = parent->parent;
        if (!parent->red || grand == NULL)
            return root;
        unsigned side = grand->children[1] == parent ? 1u : 0u;
        mc_java_hash_node *uncle = grand->children[1u - side];
        if (uncle != NULL && uncle->red) {
            parent->red = false;
            uncle->red = false;
            grand->red = true;
            node = grand;
            continue;
        }
        if (node == parent->children[1u - side]) {
            root = hash_rotate(root, parent, 1u - side);
            node = parent;
            parent = node->parent;
            grand = parent->parent;
        }
        parent->red = false;
        grand->red = true;
        return hash_rotate(root, grand, side);
    }
}

static bool hash_red(const mc_java_hash_node *node) {
    return node != NULL && node->red;
}

static mc_java_hash_node *hash_balance_remove(mc_java_hash_node *root, mc_java_hash_node *node) {
    while (node != NULL && node != root) {
        mc_java_hash_node *parent = node->parent;
        if (parent == NULL) {
            node->red = false;
            return node;
        }
        if (node->red) {
            node->red = false;
            return root;
        }
        unsigned side = parent->children[1] == node ? 1u : 0u;
        mc_java_hash_node *sibling = parent->children[1u - side];
        if (hash_red(sibling)) {
            sibling->red = false;
            parent->red = true;
            root = hash_rotate(root, parent, 1u - side);
            parent = node->parent;
            sibling = parent == NULL ? NULL : parent->children[1u - side];
        }
        if (sibling == NULL) {
            node = parent;
            continue;
        }
        mc_java_hash_node *near = sibling->children[side];
        mc_java_hash_node *far = sibling->children[1u - side];
        if (!hash_red(near) && !hash_red(far)) {
            sibling->red = true;
            node = parent;
            continue;
        }
        if (!hash_red(far)) {
            if (near != NULL)
                near->red = false;
            sibling->red = true;
            root = hash_rotate(root, sibling, side);
            parent = node->parent;
            sibling = parent == NULL ? NULL : parent->children[1u - side];
        }
        if (sibling != NULL) {
            sibling->red = parent != NULL && parent->red;
            far = sibling->children[1u - side];
            if (far != NULL)
                far->red = false;
        }
        if (parent != NULL) {
            parent->red = false;
            root = hash_rotate(root, parent, 1u - side);
        }
        node = root;
    }
    return root;
}

static void hash_front(mc_java_hash_node **buckets, uint32_t capacity, mc_java_hash_node *root) {
    uint32_t index = root->hash & (capacity - 1u);
    mc_java_hash_node *head = buckets[index];
    if (head == root)
        return;
    if (root->next != NULL)
        root->next->previous = root->previous;
    if (root->previous != NULL)
        root->previous->next = root->next;
    root->previous = NULL;
    root->next = head;
    if (head != NULL)
        head->previous = root;
    buckets[index] = root;
}

void mc_java_hash_treeify(mc_java_hash_node **buckets, uint32_t capacity, uint32_t index,
                          mc_java_hash_compare tie_break, void *context) {
    mc_java_hash_node *root = NULL, *previous = NULL;
    for (mc_java_hash_node *node = buckets[index]; node != NULL; node = node->next) {
        node->previous = previous;
        previous = node;
        node->parent = node->children[0] = node->children[1] = NULL;
        node->red = false;
        node->tree = true;
        if (root == NULL) {
            root = node;
            continue;
        }
        mc_java_hash_node *parent = root;
        for (;;) {
            unsigned side = hash_side(node, parent, tie_break, context);
            if (parent->children[side] == NULL) {
                parent->children[side] = node;
                node->parent = parent;
                root = hash_balance_add(root, node);
                break;
            }
            parent = parent->children[side];
        }
    }
    hash_front(buckets, capacity, root);
}

static void hash_plain(mc_java_hash_node *head) {
    for (mc_java_hash_node *node = head; node != NULL; node = node->next) {
        node->tree = node->red = false;
        node->parent = node->children[0] = node->children[1] = NULL;
    }
}

void mc_java_hash_tree_add(mc_java_hash_node **buckets, uint32_t capacity, mc_java_hash_node *head,
                           mc_java_hash_node *node, mc_java_hash_compare tie_break, void *context) {
    mc_java_hash_node *root = mc_java_hash_root(head), *parent = root;
    for (;;) {
        unsigned side = hash_side(node, parent, tie_break, context);
        if (parent->children[side] == NULL) {
            parent->children[side] = node;
            node->tree = true;
            node->parent = node->previous = parent;
            node->next = parent->next;
            if (node->next != NULL)
                node->next->previous = node;
            parent->next = node;
            hash_front(buckets, capacity, hash_balance_add(root, node));
            return;
        }
        parent = parent->children[side];
    }
}

void mc_java_hash_resize(mc_java_hash_node *const *old_buckets, uint32_t old_capacity,
                         mc_java_hash_node **buckets, uint32_t capacity,
                         mc_java_hash_compare tie_break, void *context) {
    for (uint32_t index = 0; index < old_capacity; ++index) {
        mc_java_hash_node *head = old_buckets[index];
        if (head == NULL)
            continue;
        if (head->next == NULL) {
            buckets[head->hash & (capacity - 1u)] = head;
            continue;
        }
        bool tree = head->tree;
        mc_java_hash_node *heads[2] = {NULL, NULL}, *tails[2] = {NULL, NULL};
        size_t counts[2] = {0, 0};
        while (head != NULL) {
            mc_java_hash_node *next = head->next;
            unsigned side = (head->hash & old_capacity) == 0 ? 0u : 1u;
            head->next = NULL;
            head->previous = tails[side];
            if (tails[side] == NULL)
                heads[side] = head;
            else
                tails[side]->next = head;
            tails[side] = head;
            ++counts[side];
            head = next;
        }
        for (unsigned side = 0; side < 2u; ++side) {
            if (heads[side] == NULL)
                continue;
            uint32_t target = index + (side == 0 ? 0 : old_capacity);
            buckets[target] = heads[side];
            if (tree) {
                if (counts[side] <= 6u)
                    hash_plain(heads[side]);
                else if (heads[1u - side] != NULL)
                    mc_java_hash_treeify(buckets, capacity, target, tie_break, context);
            }
        }
    }
}

void mc_java_hash_tree_remove(mc_java_hash_node **buckets, uint32_t capacity,
                              mc_java_hash_node *node, bool movable) {
    uint32_t index = node->hash & (capacity - 1u);
    mc_java_hash_node *root = mc_java_hash_root(buckets[index]);
    if (node->previous == NULL)
        buckets[index] = node->next;
    else
        node->previous->next = node->next;
    if (node->next != NULL)
        node->next->previous = node->previous;
    if (buckets[index] == NULL)
        return;
    if (movable && (root->children[1] == NULL || root->children[0] == NULL ||
                    root->children[0]->children[0] == NULL)) {
        hash_plain(buckets[index]);
        return;
    }
    mc_java_hash_node *left = node->children[0], *right = node->children[1], *replacement;
    if (left != NULL && right != NULL) {
        mc_java_hash_node *successor = right;
        while (successor->children[0] != NULL)
            successor = successor->children[0];
        bool red = successor->red;
        successor->red = node->red;
        node->red = red;
        mc_java_hash_node *tail = successor->children[1], *parent = node->parent;
        if (successor == right) {
            node->parent = successor;
            successor->children[1] = node;
        } else {
            mc_java_hash_node *previous = successor->parent;
            node->parent = previous;
            previous->children[previous->children[1] == successor ? 1 : 0] = node;
            successor->children[1] = right;
            right->parent = successor;
        }
        node->children[0] = NULL;
        node->children[1] = tail;
        if (tail != NULL)
            tail->parent = node;
        successor->children[0] = left;
        left->parent = successor;
        successor->parent = parent;
        if (parent == NULL)
            root = successor;
        else
            parent->children[parent->children[1] == node ? 1 : 0] = successor;
        replacement = tail == NULL ? node : tail;
    } else {
        replacement = left != NULL ? left : right != NULL ? right : node;
    }
    if (replacement != node) {
        mc_java_hash_node *parent = node->parent;
        replacement->parent = parent;
        if (parent == NULL) {
            root = replacement;
            root->red = false;
        } else {
            parent->children[parent->children[1] == node ? 1 : 0] = replacement;
        }
        node->parent = node->children[0] = node->children[1] = NULL;
    }
    if (!node->red)
        root = hash_balance_remove(root, replacement);
    if (replacement == node) {
        mc_java_hash_node *parent = node->parent;
        if (parent != NULL) {
            if (parent->children[0] == node)
                parent->children[0] = NULL;
            else if (parent->children[1] == node)
                parent->children[1] = NULL;
        }
        node->parent = NULL;
    }
    if (movable)
        hash_front(buckets, capacity, root);
}
