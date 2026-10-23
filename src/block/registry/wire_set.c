#include "managed_internal.h"
#include "registry_internal.h"
#include "wire_set_internal.h"

#include "mc/core/java/java.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>

#define WIRE_INITIAL_CAPACITY UINT32_C(16)
#define WIRE_TREE_CAPACITY UINT32_C(64)
#define WIRE_MAX_CAPACITY UINT32_C(0x40000000)

typedef struct wire_key wire_key;
typedef struct wire_node wire_node;

struct wire_key {
    mc_registry_wire_position position;
    uint32_t hash;
    size_t holds;
};

struct wire_node {
    wire_key *key;
    uint64_t identity;
    wire_node *next, *previous, *parent, *children[2];
    bool tree, red;
};

typedef struct wire_map {
    wire_node **table;
    size_t size;
    uint32_t capacity;
    int32_t threshold, mod_count;
    float load_factor;
} wire_map;

struct mc_registry_wire_set {
    mc_registry_classes *classes;
    wire_map *map;
    mc_registry_wire_set *next_owned;
    mc_registry_wire_identity_hash identity_input;
    void *identity_context;
    uint64_t constructed_keys, constructed_nodes;
};

/* Older MSVC diagnoses this standard C17 flexible array as extension C4200. */
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
typedef struct wire_array {
    size_t size;
    wire_key *keys[];
} wire_array;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

struct mc_registry_wire_snapshot {
    mc_registry_classes *classes;
    wire_array *array;
    size_t size;
};

/* A resize changes existing ordinary/tree links only after every fallible
 * reservation succeeds. New plain nodes are needed solely by untreeification;
 * keeping them in a pool avoids cloning unaffected bins or the whole Set. */
typedef struct wire_resize {
    wire_node **table;
    wire_node *plain_nodes;
    uint32_t capacity;
} wire_resize;

static bool valid(const mc_registry_wire_set *set) {
    return set != NULL && set->classes != NULL && set->map != NULL;
}

static uint32_t coordinate_hash(int32_t x, int32_t y, int32_t z) {
    const uint32_t hash =
        (uint32_t)x * UINT32_C(8976890) + (uint32_t)y * UINT32_C(981131) + (uint32_t)z;
    return hash ^ (hash >> 16u);
}

static bool hash_before(uint32_t left, uint32_t right) {
    return (left ^ UINT32_C(0x80000000)) < (right ^ UINT32_C(0x80000000));
}

static bool product(size_t count, size_t width, size_t *out) {
    if (count > SIZE_MAX / width)
        return false;
    *out = count * width;
    return true;
}

static int32_t threshold(uint32_t capacity) {
    return capacity == WIRE_MAX_CAPACITY ? INT32_MAX : (int32_t)(capacity - capacity / 4u);
}

static bool equal(const wire_key *left, const wire_key *right) {
    return left == right ||
           (left->position.x == right->position.x && left->position.y == right->position.y &&
            left->position.z == right->position.z);
}

static mc_registry_status key_create(mc_registry_wire_set *set, int32_t x, int32_t y, int32_t z,
                                     wire_key **out) {
    if (set->constructed_keys == UINT64_MAX)
        return MC_REGISTRY_LIMIT;
    void *storage = NULL;
    const mc_registry_status status =
        mc_registry_managed_allocate(set->classes, sizeof(wire_key), &storage);
    if (status != MC_REGISTRY_OK)
        return status;
    wire_key *key = storage;
    key->position =
        (mc_registry_wire_position){.x = x, .y = y, .z = z, .identity = ++set->constructed_keys};
    key->hash = coordinate_hash(x, y, z);
    key->holds = 1;
    *out = key;
    return MC_REGISTRY_OK;
}

static void key_release(mc_registry_classes *classes, wire_key *key) {
    if (key == NULL)
        return;
    assert(key->holds != 0);
    if (--key->holds == 0)
        mc_registry_managed_release(classes, key);
}

static mc_registry_status node_create(mc_registry_wire_set *set, wire_key *key, bool tree,
                                      wire_node **out) {
    if (set->constructed_nodes == UINT64_MAX || (key != NULL && key->holds == SIZE_MAX))
        return MC_REGISTRY_LIMIT;
    void *storage = NULL;
    const mc_registry_status status =
        mc_registry_managed_allocate(set->classes, sizeof(wire_node), &storage);
    if (status != MC_REGISTRY_OK)
        return status;
    wire_node *node = storage;
    node->identity = ++set->constructed_nodes;
    node->key = key;
    node->tree = tree;
    if (key != NULL)
        ++key->holds;
    *out = node;
    return MC_REGISTRY_OK;
}

static void node_release(mc_registry_classes *classes, wire_node *node) {
    if (node == NULL)
        return;
    key_release(classes, node->key);
    mc_registry_managed_release(classes, node);
}

static void list_release(mc_registry_classes *classes, wire_node *head) {
    while (head != NULL) {
        wire_node *next = head->next;
        node_release(classes, head);
        head = next;
    }
}

static void append(wire_node **head, wire_node **tail, wire_node *node) {
    node->previous = *tail;
    if (*tail == NULL)
        *head = node;
    else
        (*tail)->next = node;
    *tail = node;
}

static wire_node *root(wire_node *node) {
    while (node->parent != NULL)
        node = node->parent;
    return node;
}

/* Non-Comparable equal-hash lookup must search both subtrees. Neither a hash
 * match nor a previously chosen identity direction establishes key equality. */
static wire_node *tree_find(wire_node *node, const wire_key *key) {
    while (node != NULL) {
        if (hash_before(key->hash, node->key->hash)) {
            node = node->children[0];
        } else if (hash_before(node->key->hash, key->hash)) {
            node = node->children[1];
        } else if (equal(key, node->key)) {
            return node;
        } else if (node->children[0] == NULL) {
            node = node->children[1];
        } else if (node->children[1] == NULL) {
            node = node->children[0];
        } else {
            wire_node *found = tree_find(node->children[1], key);
            if (found != NULL)
                return found;
            node = node->children[0];
        }
    }
    return NULL;
}

static wire_node *find(const mc_registry_wire_set *set, const wire_key *key) {
    if (set->map->capacity == 0)
        return NULL;
    wire_node *head = set->map->table[key->hash & (set->map->capacity - 1u)];
    if (head != NULL && head->tree)
        return tree_find(root(head), key);
    for (wire_node *node = head; node != NULL; node = node->next)
        if (node->key->hash == key->hash && equal(key, node->key))
            return node;
    return NULL;
}

static mc_registry_status identity_hash(mc_registry_wire_set *set, wire_key *key) {
    if (key->position.identity_hash_ready)
        return MC_REGISTRY_OK;
    if (set->identity_input == NULL)
        return MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
    int32_t value;
    const mc_registry_status status =
        set->identity_input(set->identity_context, &key->position, &value);
    if (status != MC_REGISTRY_OK)
        return status;
    key->position.identity_hash = value;
    key->position.identity_hash_ready = true;
    return MC_REGISTRY_OK;
}

static mc_registry_status direction(mc_registry_wire_set *set, wire_key *key, wire_key *parent,
                                    bool cached_only, unsigned int *side) {
    if (key->hash != parent->hash) {
        *side = hash_before(key->hash, parent->hash) ? 0u : 1u;
        return MC_REGISTRY_OK;
    }
    /* All keys have the same actual position-class family. The selected
     * library reaches identityHashCode(key) before identityHashCode(parent). */
    if (cached_only) {
        assert(key->position.identity_hash_ready && parent->position.identity_hash_ready);
    } else {
        mc_registry_status status = identity_hash(set, key);
        if (status != MC_REGISTRY_OK)
            return status;
        status = identity_hash(set, parent);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    *side = key->position.identity_hash <= parent->position.identity_hash ? 0u : 1u;
    return MC_REGISTRY_OK;
}

static wire_node *rotate(wire_node *tree_root, wire_node *node, unsigned int side) {
    wire_node *pivot = node->children[side];
    wire_node *parent = node->parent;
    node->children[side] = pivot->children[1u - side];
    if (node->children[side] != NULL)
        node->children[side]->parent = node;
    pivot->parent = parent;
    if (parent == NULL) {
        tree_root = pivot;
        pivot->red = false;
    } else {
        parent->children[parent->children[1] == node ? 1u : 0u] = pivot;
    }
    pivot->children[1u - side] = node;
    node->parent = pivot;
    return tree_root;
}

static wire_node *balance_add(wire_node *tree_root, wire_node *node) {
    node->red = true;
    for (;;) {
        wire_node *parent = node->parent;
        if (parent == NULL) {
            node->red = false;
            return node;
        }
        wire_node *grand = parent->parent;
        if (!parent->red || grand == NULL)
            return tree_root;
        const unsigned int side = grand->children[1] == parent ? 1u : 0u;
        wire_node *uncle = grand->children[1u - side];
        if (uncle != NULL && uncle->red) {
            parent->red = false;
            uncle->red = false;
            grand->red = true;
            node = grand;
            continue;
        }
        if (node == parent->children[1u - side]) {
            tree_root = rotate(tree_root, parent, 1u - side);
            node = parent;
            parent = node->parent;
            grand = parent->parent;
        }
        parent->red = false;
        grand->red = true;
        return rotate(tree_root, grand, side);
    }
}

static bool red(const wire_node *node) {
    return node != NULL && node->red;
}

static wire_node *balance_remove(wire_node *tree_root, wire_node *node) {
    while (node != NULL && node != tree_root) {
        wire_node *parent = node->parent;
        if (parent == NULL)
            return node;
        if (node->red) {
            node->red = false;
            return tree_root;
        }
        const unsigned int side = parent->children[1] == node ? 1u : 0u;
        wire_node *sibling = parent->children[1u - side];
        if (red(sibling)) {
            sibling->red = false;
            parent->red = true;
            tree_root = rotate(tree_root, parent, 1u - side);
            parent = node->parent;
            sibling = parent == NULL ? NULL : parent->children[1u - side];
        }
        if (sibling == NULL) {
            node = parent;
            continue;
        }
        wire_node *near = sibling->children[side];
        wire_node *far = sibling->children[1u - side];
        if (!red(near) && !red(far)) {
            sibling->red = true;
            node = parent;
            continue;
        }
        if (!red(far)) {
            if (near != NULL)
                near->red = false;
            sibling->red = true;
            tree_root = rotate(tree_root, sibling, side);
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
            tree_root = rotate(tree_root, parent, 1u - side);
        }
        node = tree_root;
    }
    return tree_root;
}

static void front(wire_node **head, wire_node *tree_root) {
    if (*head == tree_root)
        return;
    if (tree_root->next != NULL)
        tree_root->next->previous = tree_root->previous;
    if (tree_root->previous != NULL)
        tree_root->previous->next = tree_root->next;
    if (*head != NULL)
        (*head)->previous = tree_root;
    tree_root->next = *head;
    tree_root->previous = NULL;
    *head = tree_root;
}

static mc_registry_status treeify(mc_registry_wire_set *set, wire_node **head, bool cached_only) {
    wire_node *tree_root = NULL;
    for (wire_node *node = *head; node != NULL; node = node->next) {
        node->parent = node->children[0] = node->children[1] = NULL;
        node->red = false;
        if (tree_root == NULL) {
            tree_root = node;
            continue;
        }
        wire_node *parent = tree_root;
        for (;;) {
            unsigned int side;
            const mc_registry_status status =
                direction(set, node->key, parent->key, cached_only, &side);
            if (status != MC_REGISTRY_OK)
                return status;
            if (parent->children[side] == NULL) {
                parent->children[side] = node;
                node->parent = parent;
                tree_root = balance_add(tree_root, node);
                break;
            }
            parent = parent->children[side];
        }
    }
    if (tree_root != NULL)
        front(head, tree_root);
    return MC_REGISTRY_OK;
}

static mc_registry_status copy_nodes(mc_registry_wire_set *set, wire_node *source,
                                     const wire_node *omit, wire_node *extra, bool tree,
                                     wire_node **out) {
    wire_node *head = NULL, *tail = NULL;
    for (wire_node *node = source; node != NULL; node = node->next) {
        if (node == omit)
            continue;
        wire_node *copy = NULL;
        const mc_registry_status status = node_create(set, node->key, tree, &copy);
        if (status != MC_REGISTRY_OK) {
            list_release(set->classes, head);
            return status;
        }
        append(&head, &tail, copy);
    }
    if (extra != NULL) {
        wire_node *copy = NULL;
        const mc_registry_status status = node_create(set, extra->key, tree, &copy);
        if (status != MC_REGISTRY_OK) {
            list_release(set->classes, head);
            return status;
        }
        append(&head, &tail, copy);
    }
    *out = head;
    return MC_REGISTRY_OK;
}

static void resize_discard(mc_registry_wire_set *set, wire_resize *plan) {
    list_release(set->classes, plan->plain_nodes);
    mc_registry_managed_release(set->classes, plan->table);
    *plan = (wire_resize){0};
}

/* Count the pending key before changing any live bin. Its eventual tree-chain
 * insertion/root move can change traversal, but cannot change split counts.
 * Actual replacement keys/order are assigned only during infallible commit. */
static mc_registry_status resize_prepare(mc_registry_wire_set *set, uint32_t capacity,
                                         wire_node *pending, uint32_t override_index,
                                         wire_node *override, wire_resize *out) {
    size_t bytes;
    if (!product((size_t)capacity, sizeof(wire_node *), &bytes))
        return MC_REGISTRY_LIMIT;
    wire_resize plan = {.capacity = capacity};
    void *storage = NULL;
    mc_registry_status status = mc_registry_managed_allocate(set->classes, bytes, &storage);
    if (status != MC_REGISTRY_OK)
        return status;
    plan.table = storage;
    const uint32_t old_capacity = set->map->capacity;
    const uint32_t pending_index =
        pending == NULL || old_capacity == 0 ? 0 : pending->key->hash & (old_capacity - 1u);
    wire_node *pool_tail = NULL;
    for (uint32_t index = 0; index < old_capacity; ++index) {
        wire_node *head =
            override != NULL && index == override_index ? override : set->map->table[index];
        if (head == NULL || !head->tree)
            continue;
        size_t counts[2] = {0, 0};
        for (wire_node *node = head; node != NULL; node = node->next)
            ++counts[(node->key->hash & old_capacity) == 0 ? 0u : 1u];
        if (pending != NULL && index == pending_index && override == NULL)
            ++counts[(pending->key->hash & old_capacity) == 0 ? 0u : 1u];
        if (counts[0] + counts[1] <= 1)
            continue;
        for (unsigned int side = 0; side < 2u; ++side) {
            if (counts[side] > 6u)
                continue;
            for (size_t i = 0; i < counts[side]; ++i) {
                wire_node *node = NULL;
                status = node_create(set, NULL, false, &node);
                if (status != MC_REGISTRY_OK) {
                    resize_discard(set, &plan);
                    return status;
                }
                append(&plan.plain_nodes, &pool_tail, node);
            }
        }
    }
    *out = plan;
    return MC_REGISTRY_OK;
}

static wire_node *untreeify_reserved(mc_registry_wire_set *set, wire_node *source,
                                     wire_node **pool) {
    wire_node *head = NULL, *tail = NULL;
    while (source != NULL) {
        wire_node *next = source->next;
        wire_node *replacement = *pool;
        assert(replacement != NULL && source->key->holds != SIZE_MAX);
        *pool = replacement->next;
        replacement->next = NULL;
        replacement->previous = NULL;
        replacement->key = source->key;
        ++replacement->key->holds;
        append(&head, &tail, replacement);
        node_release(set->classes, source);
        source = next;
    }
    return head;
}

static void resize_commit(mc_registry_wire_set *set, wire_resize *plan) {
    wire_map *map = set->map;
    const uint32_t old_capacity = map->capacity;
    wire_node **old_table = map->table;
    for (uint32_t index = 0; index < old_capacity; ++index) {
        wire_node *head = old_table[index];
        if (head == NULL)
            continue;
        if (head->next == NULL) {
            plan->table[head->key->hash & (plan->capacity - 1u)] = head;
            continue;
        }
        const bool tree = head->tree;
        wire_node *heads[2] = {NULL, NULL}, *tails[2] = {NULL, NULL};
        size_t counts[2] = {0, 0};
        while (head != NULL) {
            wire_node *next = head->next;
            const unsigned int side = (head->key->hash & old_capacity) == 0 ? 0u : 1u;
            head->next = NULL;
            append(&heads[side], &tails[side], head);
            ++counts[side];
            head = next;
        }
        const bool both = heads[0] != NULL && heads[1] != NULL;
        for (unsigned int side = 0; side < 2u; ++side) {
            if (heads[side] == NULL)
                continue;
            if (tree) {
                if (counts[side] <= 6u) {
                    heads[side] = untreeify_reserved(set, heads[side], &plan->plain_nodes);
                } else if (both) {
                    /* A split cannot introduce a new full-hash collision.
                     * Such keys already received their hashes when treeified
                     * or inserted. No provider/fallible operation occurs here. */
                    const mc_registry_status status = treeify(set, &heads[side], true);
                    assert(status == MC_REGISTRY_OK);
                    (void)status;
                }
            }
            plan->table[index + (side == 0 ? 0 : old_capacity)] = heads[side];
        }
    }
    assert(plan->plain_nodes == NULL);
    mc_registry_managed_release(set->classes, old_table);
    map->table = plan->table;
    map->capacity = plan->capacity;
    map->threshold = threshold(plan->capacity);
    *plan = (wire_resize){0};
}

static void tree_add_commit(wire_node **head, wire_node *node, wire_node *parent,
                            unsigned int side) {
    wire_node *tree_root = root(*head);
    parent->children[side] = node;
    node->parent = node->previous = parent;
    node->next = parent->next;
    if (node->next != NULL)
        node->next->previous = node;
    parent->next = node;
    front(head, balance_add(tree_root, node));
}

static mc_registry_status add_key(mc_registry_wire_set *set, wire_key *key, bool *changed) {
    wire_map *map = set->map;
    if (find(set, key) != NULL) {
        *changed = false;
        return MC_REGISTRY_OK;
    }
    if (map->size == (size_t)INT32_MAX)
        return MC_REGISTRY_LIMIT;
    const uint32_t index = map->capacity == 0 ? 0 : key->hash & (map->capacity - 1u);
    wire_node *head = map->capacity == 0 ? NULL : map->table[index];
    wire_node *parent = NULL;
    unsigned int side = 0;
    mc_registry_status status;
    if (head != NULL && head->tree) {
        parent = root(head);
        for (;;) {
            status = direction(set, key, parent->key, false, &side);
            if (status != MC_REGISTRY_OK)
                return status;
            if (parent->children[side] == NULL)
                break;
            parent = parent->children[side];
        }
    }
    wire_resize initial = {0}, growth = {0};
    wire_node *node = NULL, *converted = NULL;
    if (map->capacity == 0) {
        status = resize_prepare(set, WIRE_INITIAL_CAPACITY, NULL, 0, NULL, &initial);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    status = node_create(set, key, head != NULL && head->tree, &node);
    if (status != MC_REGISTRY_OK)
        goto failure;
    size_t chain = 0;
    if (head != NULL && !head->tree)
        for (wire_node *entry = head; entry != NULL; entry = entry->next)
            ++chain;
    uint32_t capacity = map->capacity == 0 ? WIRE_INITIAL_CAPACITY : map->capacity;
    if (chain >= 8u) {
        if (capacity < WIRE_TREE_CAPACITY) {
            capacity *= 2u;
            status = resize_prepare(set, capacity, node, 0, NULL, &growth);
            if (status != MC_REGISTRY_OK)
                goto failure;
        } else {
            status = copy_nodes(set, head, NULL, node, true, &converted);
            if (status != MC_REGISTRY_OK)
                goto failure;
            status = treeify(set, &converted, false);
            if (status != MC_REGISTRY_OK)
                goto failure;
        }
    }
    if (map->size + 1u > (size_t)threshold(capacity) && capacity < WIRE_MAX_CAPACITY) {
        assert(growth.table == NULL);
        capacity *= 2u;
        status = resize_prepare(set, capacity, node, index, converted, &growth);
        if (status != MC_REGISTRY_OK)
            goto failure;
    }
    /* Every fallible reservation and first identity input is complete. */
    if (initial.table != NULL)
        resize_commit(set, &initial);
    wire_node **bucket = &map->table[key->hash & (map->capacity - 1u)];
    if (converted != NULL) {
        *bucket = converted;
        converted = NULL;
        list_release(set->classes, head);
        node_release(set->classes, node);
    } else if (head == NULL) {
        *bucket = node;
    } else if (head->tree) {
        tree_add_commit(bucket, node, parent, side);
    } else {
        wire_node *tail = head;
        while (tail->next != NULL)
            tail = tail->next;
        tail->next = node;
        node->previous = tail;
    }
    ++map->size;
    map->mod_count = mc_java_i32_add(map->mod_count, 1);
    if (growth.table != NULL)
        resize_commit(set, &growth);
    *changed = true;
    return MC_REGISTRY_OK;
failure:
    list_release(set->classes, converted);
    node_release(set->classes, node);
    resize_discard(set, &initial);
    resize_discard(set, &growth);
    return status;
}

static void unlink_node(wire_node **head, wire_node *node) {
    if (node->previous == NULL)
        *head = node->next;
    else
        node->previous->next = node->next;
    if (node->next != NULL)
        node->next->previous = node->previous;
}

/* The caller has already reserved a replacement plain chain if the source
 * shape test requires untreeification. This branch changes tree links only. */
static void tree_remove_commit(wire_node **head, wire_node *node) {
    wire_node *tree_root = root(*head);
    unlink_node(head, node);
    if (*head == NULL)
        return;
    wire_node *left = node->children[0], *right = node->children[1], *replacement;
    if (left != NULL && right != NULL) {
        wire_node *successor = right;
        while (successor->children[0] != NULL)
            successor = successor->children[0];
        const bool color = successor->red;
        successor->red = node->red;
        node->red = color;
        wire_node *tail = successor->children[1], *parent = node->parent;
        if (successor == right) {
            node->parent = successor;
            successor->children[1] = node;
        } else {
            wire_node *previous = successor->parent;
            node->parent = previous;
            previous->children[previous->children[1] == successor ? 1u : 0u] = node;
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
            tree_root = successor;
        else
            parent->children[parent->children[1] == node ? 1u : 0u] = successor;
        replacement = tail == NULL ? node : tail;
    } else {
        replacement = left != NULL ? left : right != NULL ? right : node;
    }
    if (replacement != node) {
        wire_node *parent = node->parent;
        replacement->parent = parent;
        if (parent == NULL) {
            tree_root = replacement;
            replacement->red = false;
        } else {
            parent->children[parent->children[1] == node ? 1u : 0u] = replacement;
        }
        node->parent = node->children[0] = node->children[1] = NULL;
    }
    if (!node->red)
        tree_root = balance_remove(tree_root, replacement);
    if (replacement == node) {
        wire_node *parent = node->parent;
        node->parent = NULL;
        if (parent != NULL)
            parent->children[parent->children[1] == node ? 1u : 0u] = NULL;
    }
    front(head, tree_root);
}

static mc_registry_status remove_key(mc_registry_wire_set *set, wire_key *key, bool *changed) {
    wire_node *node = find(set, key);
    if (node == NULL) {
        *changed = false;
        return MC_REGISTRY_OK;
    }
    wire_map *map = set->map;
    wire_node **bucket = &map->table[key->hash & (map->capacity - 1u)];
    if (node->tree) {
        wire_node *tree_root = root(*bucket);
        const bool remains = (*bucket)->next != NULL;
        const bool to_plain =
            remains && (tree_root->children[1] == NULL || tree_root->children[0] == NULL ||
                        tree_root->children[0]->children[0] == NULL);
        if (to_plain) {
            wire_node *plain = NULL;
            const mc_registry_status status = copy_nodes(set, *bucket, node, NULL, false, &plain);
            if (status != MC_REGISTRY_OK)
                return status;
            wire_node *old = *bucket;
            *bucket = plain;
            list_release(set->classes, old);
        } else {
            tree_remove_commit(bucket, node);
            node_release(set->classes, node);
        }
    } else {
        unlink_node(bucket, node);
        node_release(set->classes, node);
    }
    --map->size;
    map->mod_count = mc_java_i32_add(map->mod_count, 1);
    *changed = true;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_wire_set_create(mc_registry_classes *classes,
                                               mc_registry_wire_set **out) {
    if (classes == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_registry_wire_set *set = mc_registry_allocate(classes, sizeof(*set));
    if (set == NULL)
        return classes->last_error.status;
    set->classes = classes;
    wire_map *map = mc_registry_allocate(classes, sizeof(*map));
    if (map == NULL)
        return classes->last_error.status;
    map->load_factor = 0.75f;
    set->map = map;
    set->next_owned = classes->wire_sets;
    classes->wire_sets = set;
    *out = set;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_wire_set_identity_profile(mc_registry_wire_set *set,
                                                         mc_registry_wire_identity_hash input,
                                                         void *context) {
    if (!valid(set))
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (set->constructed_keys != 0 || set->map->capacity != 0)
        return MC_REGISTRY_BUSY;
    set->identity_input = input;
    set->identity_context = context;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_wire_set_add_fresh(mc_registry_wire_set *set, int32_t x, int32_t y,
                                                  int32_t z, bool *changed) {
    if (!valid(set) || changed == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    wire_key *key = NULL;
    mc_registry_status status = key_create(set, x, y, z, &key);
    if (status != MC_REGISTRY_OK)
        return status;
    status = add_key(set, key, changed);
    key_release(set->classes, key);
    return status;
}

mc_registry_status mc_registry_wire_set_remove_fresh(mc_registry_wire_set *set, int32_t x,
                                                     int32_t y, int32_t z, bool *changed) {
    if (!valid(set) || changed == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    wire_key *key = NULL;
    mc_registry_status status = key_create(set, x, y, z, &key);
    if (status != MC_REGISTRY_OK)
        return status;
    status = remove_key(set, key, changed);
    key_release(set->classes, key);
    return status;
}

mc_registry_status mc_registry_wire_set_readd_existing(mc_registry_wire_set *set, uint64_t identity,
                                                       bool *changed) {
    if (!valid(set) || changed == NULL || identity == 0)
        return MC_REGISTRY_INVALID_ARGUMENT;
    for (uint32_t i = 0; i < set->map->capacity; ++i)
        for (wire_node *node = set->map->table[i]; node != NULL; node = node->next)
            if (node->key->position.identity == identity) {
                *changed = false;
                return MC_REGISTRY_OK;
            }
    return MC_REGISTRY_INVALID_ARGUMENT;
}

mc_registry_status mc_registry_wire_set_clear(mc_registry_wire_set *set) {
    if (!valid(set))
        return MC_REGISTRY_INVALID_ARGUMENT;
    for (uint32_t i = 0; i < set->map->capacity; ++i) {
        list_release(set->classes, set->map->table[i]);
        set->map->table[i] = NULL;
    }
    set->map->size = 0;
    set->map->mod_count = mc_java_i32_add(set->map->mod_count, 1);
    return MC_REGISTRY_OK;
}

static mc_registry_status array_create(mc_registry_classes *classes, size_t size,
                                       wire_array **out) {
    if (size > (SIZE_MAX - sizeof(wire_array)) / sizeof(wire_key *))
        return MC_REGISTRY_LIMIT;
    void *storage = NULL;
    const mc_registry_status status = mc_registry_managed_allocate(
        classes, sizeof(wire_array) + size * sizeof(wire_key *), &storage);
    if (status != MC_REGISTRY_OK)
        return status;
    wire_array *array = storage;
    array->size = size;
    *out = array;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_wire_snapshot_copy(mc_registry_wire_set *set,
                                                  mc_registry_wire_snapshot **out) {
    if (!valid(set) || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (set->classes->wire_snapshot_count == SIZE_MAX)
        return MC_REGISTRY_LIMIT;
    void *storage = NULL;
    mc_registry_status status =
        mc_registry_managed_allocate(set->classes, sizeof(mc_registry_wire_snapshot), &storage);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_wire_snapshot *snapshot = storage;
    snapshot->classes = set->classes;
    snapshot->size = set->map->size;
    wire_array *temporary = NULL;
    status = array_create(set->classes, snapshot->size, &temporary);
    if (status != MC_REGISTRY_OK)
        goto failure;
    size_t index = 0;
    for (uint32_t bucket = 0; bucket < set->map->capacity; ++bucket)
        for (wire_node *node = set->map->table[bucket]; node != NULL; node = node->next)
            temporary->keys[index++] = node->key;
    assert(index == snapshot->size);
    if (snapshot->size != 0) {
        /* HashSet.toArray and ArrayList's copied Object[] overlap. Their
         * native charges remain live together until the constructor returns. */
        status = array_create(set->classes, snapshot->size, &snapshot->array);
        if (status != MC_REGISTRY_OK)
            goto failure;
        for (index = 0; index < snapshot->size; ++index) {
            wire_key *key = temporary->keys[index];
            if (key->holds == SIZE_MAX) {
                status = MC_REGISTRY_LIMIT;
                while (index != 0)
                    key_release(set->classes, snapshot->array->keys[--index]);
                goto failure;
            }
            ++key->holds;
            snapshot->array->keys[index] = key;
        }
    }
    mc_registry_managed_release(set->classes, temporary);
    ++set->classes->wire_snapshot_count;
    *out = snapshot;
    return MC_REGISTRY_OK;
failure:
    mc_registry_managed_release(set->classes, snapshot->array);
    mc_registry_managed_release(set->classes, temporary);
    mc_registry_managed_release(set->classes, snapshot);
    return status;
}

size_t mc_registry_wire_snapshot_size(const mc_registry_wire_snapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->size;
}

mc_registry_status mc_registry_wire_snapshot_at(const mc_registry_wire_snapshot *snapshot,
                                                size_t index, mc_registry_wire_position *out) {
    if (snapshot == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (index >= snapshot->size)
        return MC_REGISTRY_INDEX_OUT_OF_RANGE;
    *out = snapshot->array->keys[index]->position;
    return MC_REGISTRY_OK;
}

void mc_registry_wire_snapshot_release(mc_registry_wire_snapshot *snapshot) {
    if (snapshot == NULL)
        return;
    mc_registry_classes *classes = snapshot->classes;
    assert(classes->wire_snapshot_count != 0);
    for (size_t i = 0; i < snapshot->size; ++i)
        key_release(classes, snapshot->array->keys[i]);
    mc_registry_managed_release(classes, snapshot->array);
    --classes->wire_snapshot_count;
    mc_registry_managed_release(classes, snapshot);
}

mc_registry_status mc_registry_wire_set_inspect(const mc_registry_wire_set *set,
                                                mc_registry_wire_set_view *out) {
    if (!valid(set) || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    *out = (mc_registry_wire_set_view){.map = set->map,
                                       .table = set->map->table,
                                       .size = set->map->size,
                                       .capacity = set->map->capacity,
                                       .threshold = set->map->threshold,
                                       .mod_count = set->map->mod_count,
                                       .load_factor = set->map->load_factor,
                                       .constructed_keys = set->constructed_keys};
    return MC_REGISTRY_OK;
}

static uint64_t node_identity(const wire_node *node) {
    return node == NULL ? 0 : node->identity;
}

mc_registry_status mc_registry_wire_set_node_at(const mc_registry_wire_set *set, size_t index,
                                                mc_registry_wire_node_view *out) {
    if (!valid(set) || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (index >= set->map->size)
        return MC_REGISTRY_INDEX_OUT_OF_RANGE;
    for (uint32_t bucket = 0; bucket < set->map->capacity; ++bucket) {
        for (wire_node *node = set->map->table[bucket]; node != NULL; node = node->next) {
            if (index-- != 0)
                continue;
            *out = (mc_registry_wire_node_view){
                .identity = node->identity,
                .next = node_identity(node->next),
                .previous = node->tree ? node_identity(node->previous) : 0,
                .parent = node->tree ? node_identity(node->parent) : 0,
                .left = node->tree ? node_identity(node->children[0]) : 0,
                .right = node->tree ? node_identity(node->children[1]) : 0,
                .key = node->key->position,
                .hash = node->key->hash,
                .bucket = bucket,
                .tree = node->tree,
                .red = node->tree && node->red};
            return MC_REGISTRY_OK;
        }
    }
    return MC_REGISTRY_INVALID_ARGUMENT;
}

void mc_registry_wire_sets_retire(mc_registry_classes *classes) {
    if (classes == NULL)
        return;
    assert(classes->wire_snapshot_count == 0);
    mc_registry_wire_set *set = classes->wire_sets;
    classes->wire_sets = NULL;
    while (set != NULL) {
        mc_registry_wire_set *next = set->next_owned;
        for (uint32_t i = 0; i < set->map->capacity; ++i)
            list_release(classes, set->map->table[i]);
        mc_registry_managed_release(classes, set->map->table);
        set->map->table = NULL;
        set->map->size = 0;
        set->map->capacity = 0;
        set->map->threshold = 0;
        set->next_owned = NULL;
        set = next;
    }
}
