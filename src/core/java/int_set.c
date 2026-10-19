#include "mc/core/java/int_set.h"

#include <limits.h>
#include <stdlib.h>

#define INT_SET_INITIAL_CAPACITY UINT32_C(16)
#define INT_SET_TREE_CAPACITY UINT32_C(64)
#define INT_SET_MAX_CAPACITY UINT32_C(0x40000000)

typedef struct int_set_node int_set_node;

struct int_set_node {
    int32_t key;
    uint32_t hash;
    int_set_node *next, *previous, *parent, *children[2];
    bool red, tree;
};

struct mc_java_int_set {
    mc_java_int_set_limits limits;
    mc_java_int_set_allocator allocator;
    int_set_node **buckets;
    size_t size;
    uint32_t capacity, threshold;
};

static void *int_set_default_allocate(void *context, size_t bytes)
{
    (void)context;
    return malloc(bytes);
}

static void int_set_default_deallocate(void *context, void *storage)
{
    (void)context;
    free(storage);
}

static void int_set_release(const mc_java_int_set_allocator *allocator, void *storage)
{
    if (storage != NULL && allocator->deallocate != NULL)
        allocator->deallocate(allocator->context, storage);
}

static bool int_set_sum(size_t left, size_t right, size_t *out)
{
    if (right > SIZE_MAX - left)
        return false;
    *out = left + right;
    return true;
}

static bool int_set_product(size_t count, size_t width, size_t *out)
{
    if (count > SIZE_MAX / width)
        return false;
    *out = count * width;
    return true;
}

static bool int_set_bytes(uint32_t capacity, size_t count, size_t *out)
{
    size_t table, nodes, total;
    return int_set_product((size_t)capacity, sizeof(int_set_node *), &table) &&
           int_set_product(count, sizeof(int_set_node), &nodes) &&
           int_set_sum(sizeof(mc_java_int_set), table, &total) && int_set_sum(total, nodes, out);
}

static uint32_t int_set_hash(int32_t key)
{
    uint32_t bits = (uint32_t)key;
    return bits ^ (bits >> 16u);
}

static bool int_set_before(uint32_t left, uint32_t right)
{
    return (left ^ UINT32_C(0x80000000)) < (right ^ UINT32_C(0x80000000));
}

static uint32_t int_set_threshold(uint32_t capacity)
{
    return capacity == INT_SET_MAX_CAPACITY ? UINT32_C(0x7fffffff) : capacity - capacity / 4u;
}

static int_set_node *int_set_root(int_set_node *node)
{
    while (node->parent != NULL)
        node = node->parent;
    return node;
}

static int_set_node *int_set_find(const mc_java_int_set *set, int32_t key, uint32_t hash)
{
    if (set->capacity == 0)
        return NULL;
    int_set_node *node = set->buckets[hash & (set->capacity - 1u)];
    if (node != NULL && node->tree) {
        node = int_set_root(node);
        while (node != NULL && node->hash != hash)
            node = node->children[int_set_before(hash, node->hash) ? 0 : 1];
        return node;
    }
    while (node != NULL && node->key != key)
        node = node->next;
    return node;
}

/* Promote one child while preserving the separate traversal chain. */
static int_set_node *int_set_rotate(int_set_node *root, int_set_node *node, unsigned side)
{
    int_set_node *pivot = node->children[side];
    int_set_node *parent = node->parent;
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

static int_set_node *int_set_balance_add(int_set_node *root, int_set_node *node)
{
    node->red = true;
    for (;;) {
        int_set_node *parent = node->parent;
        if (parent == NULL) {
            node->red = false;
            return node;
        }
        int_set_node *grand = parent->parent;
        if (!parent->red || grand == NULL)
            return root;
        unsigned side = grand->children[1] == parent ? 1u : 0u;
        int_set_node *uncle = grand->children[1u - side];
        if (uncle != NULL && uncle->red) {
            parent->red = false;
            uncle->red = false;
            grand->red = true;
            node = grand;
            continue;
        }
        if (node == parent->children[1u - side]) {
            root = int_set_rotate(root, parent, 1u - side);
            node = parent;
            parent = node->parent;
            grand = parent->parent;
        }
        parent->red = false;
        grand->red = true;
        return int_set_rotate(root, grand, side);
    }
}

static bool int_set_red(const int_set_node *node)
{
    return node != NULL && node->red;
}

static int_set_node *int_set_balance_remove(int_set_node *root, int_set_node *node)
{
    while (node != NULL && node != root) {
        int_set_node *parent = node->parent;
        if (parent == NULL) {
            node->red = false;
            return node;
        }
        if (node->red) {
            node->red = false;
            return root;
        }
        unsigned side = parent->children[1] == node ? 1u : 0u;
        int_set_node *sibling = parent->children[1u - side];
        if (int_set_red(sibling)) {
            sibling->red = false;
            parent->red = true;
            root = int_set_rotate(root, parent, 1u - side);
            parent = node->parent;
            sibling = parent == NULL ? NULL : parent->children[1u - side];
        }
        if (sibling == NULL) {
            node = parent;
            continue;
        }
        int_set_node *near = sibling->children[side];
        int_set_node *far = sibling->children[1u - side];
        if (!int_set_red(near) && !int_set_red(far)) {
            sibling->red = true;
            node = parent;
            continue;
        }
        if (!int_set_red(far)) {
            if (near != NULL)
                near->red = false;
            sibling->red = true;
            root = int_set_rotate(root, sibling, side);
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
            root = int_set_rotate(root, parent, 1u - side);
        }
        node = root;
    }
    return root;
}

static void int_set_front(int_set_node **buckets, uint32_t capacity, int_set_node *root)
{
    uint32_t index = root->hash & (capacity - 1u);
    int_set_node *head = buckets[index];
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

static void int_set_treeify(int_set_node **buckets, uint32_t capacity, uint32_t index)
{
    int_set_node *root = NULL, *previous = NULL;
    for (int_set_node *node = buckets[index]; node != NULL; node = node->next) {
        node->previous = previous;
        previous = node;
        node->parent = node->children[0] = node->children[1] = NULL;
        node->red = false;
        node->tree = true;
        if (root == NULL) {
            root = node;
            continue;
        }
        int_set_node *parent = root;
        for (;;) {
            unsigned side = int_set_before(node->hash, parent->hash) ? 0u : 1u;
            if (parent->children[side] == NULL) {
                parent->children[side] = node;
                node->parent = parent;
                root = int_set_balance_add(root, node);
                break;
            }
            parent = parent->children[side];
        }
    }
    int_set_front(buckets, capacity, root);
}

static void int_set_plain(int_set_node *head)
{
    for (int_set_node *node = head; node != NULL; node = node->next) {
        node->tree = node->red = false;
        node->parent = node->children[0] = node->children[1] = NULL;
    }
}

static void int_set_tree_add(mc_java_int_set *set, int_set_node *head, int_set_node *node)
{
    int_set_node *root = int_set_root(head), *parent = root;
    for (;;) {
        unsigned side = int_set_before(node->hash, parent->hash) ? 0u : 1u;
        if (parent->children[side] == NULL) {
            parent->children[side] = node;
            node->tree = true;
            node->parent = node->previous = parent;
            node->next = parent->next;
            if (node->next != NULL)
                node->next->previous = node;
            parent->next = node;
            int_set_front(set->buckets, set->capacity, int_set_balance_add(root, node));
            return;
        }
        parent = parent->children[side];
    }
}

static void int_set_resize(mc_java_int_set *set, int_set_node **table, uint32_t capacity)
{
    uint32_t old_capacity = set->capacity;
    for (uint32_t index = 0; index < old_capacity; ++index) {
        int_set_node *head = set->buckets[index];
        if (head == NULL)
            continue;
        if (head->next == NULL) {
            table[head->hash & (capacity - 1u)] = head;
            continue;
        }
        bool tree = head->tree;
        int_set_node *heads[2] = {NULL, NULL}, *tails[2] = {NULL, NULL};
        size_t counts[2] = {0, 0};
        while (head != NULL) {
            int_set_node *next = head->next;
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
            table[target] = heads[side];
            if (tree) {
                if (counts[side] <= 6u)
                    int_set_plain(heads[side]);
                else if (heads[1u - side] != NULL)
                    int_set_treeify(table, capacity, target);
            }
        }
    }
    int_set_release(&set->allocator, set->buckets);
    set->buckets = table;
    set->capacity = capacity;
    set->threshold = int_set_threshold(capacity);
}

static void int_set_tree_remove(mc_java_int_set *set, int_set_node *node)
{
    uint32_t index = node->hash & (set->capacity - 1u);
    int_set_node *root = int_set_root(set->buckets[index]);
    if (node->previous == NULL)
        set->buckets[index] = node->next;
    else
        node->previous->next = node->next;
    if (node->next != NULL)
        node->next->previous = node->previous;
    if (set->buckets[index] == NULL)
        return;
    if (root->children[1] == NULL || root->children[0] == NULL ||
        root->children[0]->children[0] == NULL) {
        int_set_plain(set->buckets[index]);
        return;
    }
    int_set_node *left = node->children[0], *right = node->children[1], *replacement;
    if (left != NULL && right != NULL) {
        int_set_node *successor = right;
        while (successor->children[0] != NULL)
            successor = successor->children[0];
        bool red = successor->red;
        successor->red = node->red;
        node->red = red;
        int_set_node *tail = successor->children[1], *parent = node->parent;
        if (successor == right) {
            node->parent = successor;
            successor->children[1] = node;
        } else {
            int_set_node *previous = successor->parent;
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
        int_set_node *parent = node->parent;
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
        root = int_set_balance_remove(root, replacement);
    if (replacement == node) {
        int_set_node *parent = node->parent;
        if (parent != NULL) {
            if (parent->children[0] == node)
                parent->children[0] = NULL;
            else if (parent->children[1] == node)
                parent->children[1] = NULL;
        }
        node->parent = NULL;
    }
    int_set_front(set->buckets, set->capacity, root);
}

mc_java_int_set_limits mc_java_int_set_default_limits(void)
{
    const mc_java_int_set_limits limits = {65536u, 8u * 1024u * 1024u, 16u * 1024u * 1024u};
    return limits;
}

const char *mc_java_int_set_status_string(mc_java_int_set_status status)
{
    switch (status) {
        case MC_JAVA_INT_SET_OK:
            return "ok";
        case MC_JAVA_INT_SET_EMPTY:
            return "empty set";
        case MC_JAVA_INT_SET_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_JAVA_INT_SET_LIMIT:
            return "resource limit";
        case MC_JAVA_INT_SET_ALLOCATION_FAILED:
            return "allocation failed";
    }
    return "unknown integer set status";
}

mc_java_int_set_status mc_java_int_set_create(const mc_java_int_set_limits *requested,
                                              mc_java_int_set **out)
{
    const mc_java_int_set_allocator allocator = {.context = NULL,
                                                 .allocate = int_set_default_allocate,
                                                 .deallocate = int_set_default_deallocate};
    return mc_java_int_set_create_with_allocator(requested, &allocator, out);
}

mc_java_int_set_status
mc_java_int_set_create_with_allocator(const mc_java_int_set_limits *requested,
                                      const mc_java_int_set_allocator *allocator,
                                      mc_java_int_set **out)
{
    if (out == NULL || allocator == NULL || allocator->allocate == NULL)
        return MC_JAVA_INT_SET_INVALID_ARGUMENT;
    const mc_java_int_set_allocator selected = *allocator;
    mc_java_int_set_limits limits =
        requested == NULL ? mc_java_int_set_default_limits() : *requested;
    if (limits.max_entries > (size_t)INT32_MAX)
        return MC_JAVA_INT_SET_INVALID_ARGUMENT;
    if (limits.max_retained_bytes < sizeof(mc_java_int_set) ||
        limits.max_allocation_bytes < sizeof(mc_java_int_set))
        return MC_JAVA_INT_SET_LIMIT;
    mc_java_int_set *set = selected.allocate(selected.context, sizeof(*set));
    if (set == NULL)
        return MC_JAVA_INT_SET_ALLOCATION_FAILED;
    *set = (mc_java_int_set){.limits = limits, .allocator = selected};
    *out = set;
    return MC_JAVA_INT_SET_OK;
}

size_t mc_java_int_set_size(const mc_java_int_set *set)
{
    return set == NULL ? 0 : set->size;
}

bool mc_java_int_set_contains(const mc_java_int_set *set, int32_t key)
{
    return set != NULL && int_set_find(set, key, int_set_hash(key)) != NULL;
}

mc_java_int_set_status mc_java_int_set_add(mc_java_int_set *set, int32_t key, bool *changed)
{
    if (set == NULL || changed == NULL)
        return MC_JAVA_INT_SET_INVALID_ARGUMENT;
    uint32_t hash = int_set_hash(key);
    if (int_set_find(set, key, hash) != NULL) {
        *changed = false;
        return MC_JAVA_INT_SET_OK;
    }
    if (set->size == set->limits.max_entries)
        return MC_JAVA_INT_SET_LIMIT;
    int_set_node *head = set->capacity == 0 ? NULL : set->buckets[hash & (set->capacity - 1u)];
    size_t chain = 0;
    if (head != NULL && !head->tree) {
        for (int_set_node *node = head; node != NULL; node = node->next)
            ++chain;
    }
    bool initial = set->capacity == 0;
    uint32_t capacity = initial ? INT_SET_INITIAL_CAPACITY : set->capacity;
    bool collision = chain >= 8u && capacity < INT_SET_TREE_CAPACITY;
    uint32_t capacities[2];
    size_t table_count = 0;
    if (initial)
        capacities[table_count++] = capacity;
    if (collision) {
        capacity *= 2u;
        capacities[table_count++] = capacity;
    }
    bool growth = set->size + 1u > int_set_threshold(capacity) && capacity < INT_SET_MAX_CAPACITY;
    if (growth) {
        capacity *= 2u;
        capacities[table_count++] = capacity;
    }
    size_t old_bytes, retained, peak, table_bytes[2];
    if (!int_set_bytes(set->capacity, set->size, &old_bytes) ||
        !int_set_bytes(capacity, set->size + 1u, &retained) ||
        !int_set_sum(old_bytes, sizeof(int_set_node), &peak))
        return MC_JAVA_INT_SET_LIMIT;
    for (size_t i = 0; i < table_count; ++i) {
        if (!int_set_product((size_t)capacities[i], sizeof(int_set_node *), &table_bytes[i]) ||
            !int_set_sum(peak, table_bytes[i], &peak))
            return MC_JAVA_INT_SET_LIMIT;
    }
    if (retained > set->limits.max_retained_bytes || peak > set->limits.max_allocation_bytes)
        return MC_JAVA_INT_SET_LIMIT;
    int_set_node *node = set->allocator.allocate(set->allocator.context, sizeof(*node));
    if (node == NULL)
        return MC_JAVA_INT_SET_ALLOCATION_FAILED;
    *node = (int_set_node){.key = key, .hash = hash};
    int_set_node **tables[2] = {NULL, NULL};
    for (size_t i = 0; i < table_count; ++i) {
        tables[i] = set->allocator.allocate(set->allocator.context, table_bytes[i]);
        if (tables[i] == NULL) {
            for (size_t j = 0; j < i; ++j)
                int_set_release(&set->allocator, tables[j]);
            int_set_release(&set->allocator, node);
            return MC_JAVA_INT_SET_ALLOCATION_FAILED;
        }
        for (uint32_t j = 0; j < capacities[i]; ++j)
            tables[i][j] = NULL;
    }
    size_t table_index = 0;
    if (initial) {
        int_set_resize(set, tables[table_index], capacities[table_index]);
        ++table_index;
    }
    uint32_t index = hash & (set->capacity - 1u);
    head = set->buckets[index];
    if (head == NULL) {
        set->buckets[index] = node;
    } else if (head->tree) {
        int_set_tree_add(set, head, node);
    } else {
        int_set_node *tail = head;
        while (tail->next != NULL)
            tail = tail->next;
        tail->next = node;
        node->previous = tail;
        if (chain >= 8u) {
            if (collision) {
                int_set_resize(set, tables[table_index], capacities[table_index]);
                ++table_index;
            } else {
                int_set_treeify(set->buckets, set->capacity, index);
            }
        }
    }
    ++set->size;
    if (growth)
        int_set_resize(set, tables[table_index], capacities[table_index]);
    *changed = true;
    return MC_JAVA_INT_SET_OK;
}

mc_java_int_set_status mc_java_int_set_remove(mc_java_int_set *set, int32_t key, bool *changed)
{
    if (set == NULL || changed == NULL)
        return MC_JAVA_INT_SET_INVALID_ARGUMENT;
    uint32_t hash = int_set_hash(key);
    int_set_node *node = int_set_find(set, key, hash);
    if (node == NULL) {
        *changed = false;
        return MC_JAVA_INT_SET_OK;
    }
    if (node->tree) {
        int_set_tree_remove(set, node);
    } else {
        if (node->previous == NULL)
            set->buckets[hash & (set->capacity - 1u)] = node->next;
        else
            node->previous->next = node->next;
        if (node->next != NULL)
            node->next->previous = node->previous;
    }
    int_set_release(&set->allocator, node);
    --set->size;
    *changed = true;
    return MC_JAVA_INT_SET_OK;
}

mc_java_int_set_status mc_java_int_set_first(const mc_java_int_set *set, int32_t *key)
{
    if (set == NULL || key == NULL)
        return MC_JAVA_INT_SET_INVALID_ARGUMENT;
    for (uint32_t i = 0; i < set->capacity; ++i) {
        if (set->buckets[i] != NULL) {
            *key = set->buckets[i]->key;
            return MC_JAVA_INT_SET_OK;
        }
    }
    return MC_JAVA_INT_SET_EMPTY;
}

mc_java_int_set_status mc_java_int_set_copy_order(const mc_java_int_set *set, int32_t *output,
                                                  size_t capacity, size_t *count)
{
    if (set == NULL || count == NULL || (output == NULL && capacity != 0))
        return MC_JAVA_INT_SET_INVALID_ARGUMENT;
    if (capacity < set->size)
        return MC_JAVA_INT_SET_LIMIT;
    size_t written = 0;
    for (uint32_t i = 0; i < set->capacity; ++i) {
        for (int_set_node *node = set->buckets[i]; node != NULL; node = node->next)
            output[written++] = node->key;
    }
    *count = written;
    return MC_JAVA_INT_SET_OK;
}

void mc_java_int_set_clear(mc_java_int_set *set)
{
    if (set == NULL)
        return;
    for (uint32_t i = 0; i < set->capacity; ++i) {
        int_set_node *node = set->buckets[i];
        while (node != NULL) {
            int_set_node *next = node->next;
            int_set_release(&set->allocator, node);
            node = next;
        }
        set->buckets[i] = NULL;
    }
    set->size = 0;
}

void mc_java_int_set_free(mc_java_int_set *set)
{
    if (set == NULL)
        return;
    const mc_java_int_set_allocator allocator = set->allocator;
    mc_java_int_set_clear(set);
    int_set_release(&allocator, set->buckets);
    int_set_release(&allocator, set);
}
