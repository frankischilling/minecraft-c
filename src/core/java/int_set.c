#include "mc/core/java/int_set.h"

#include "hash_table_internal.h"

#include <limits.h>
#include <stdlib.h>

#define INT_SET_INITIAL_CAPACITY UINT32_C(16)
#define INT_SET_TREE_CAPACITY UINT32_C(64)
#define INT_SET_MAX_CAPACITY UINT32_C(0x40000000)

typedef struct int_set_node {
    mc_java_hash_node node;
    int32_t key;
} int_set_node;

/* Preserve the previous allocation/accounting sizes after moving the key
 * behind the shared intrusive header. */
#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(mc_java_hash_node) == 48u && sizeof(int_set_node) == 56u,
               "64-bit Integer set node budget");
#elif UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(mc_java_hash_node) == 28u && sizeof(int_set_node) == 32u,
               "32-bit Integer set node budget");
#endif

struct mc_java_int_set {
    mc_java_int_set_limits limits;
    mc_java_int_set_allocator allocator;
    mc_java_hash_node **buckets;
    size_t size;
    uint32_t capacity, threshold;
};

static void *int_set_default_allocate(void *context, size_t bytes) {
    (void)context;
    return malloc(bytes);
}

static void int_set_default_deallocate(void *context, void *storage) {
    (void)context;
    free(storage);
}

static void int_set_release(const mc_java_int_set_allocator *allocator, void *storage) {
    if (storage != NULL && allocator->deallocate != NULL)
        allocator->deallocate(allocator->context, storage);
}

static bool int_set_sum(size_t left, size_t right, size_t *out) {
    if (right > SIZE_MAX - left)
        return false;
    *out = left + right;
    return true;
}

static bool int_set_product(size_t count, size_t width, size_t *out) {
    if (count > SIZE_MAX / width)
        return false;
    *out = count * width;
    return true;
}

static bool int_set_bytes(uint32_t capacity, size_t count, size_t *out) {
    size_t table, nodes, total;
    return int_set_product((size_t)capacity, sizeof(mc_java_hash_node *), &table) &&
           int_set_product(count, sizeof(int_set_node), &nodes) &&
           int_set_sum(sizeof(mc_java_int_set), table, &total) && int_set_sum(total, nodes, out);
}

static uint32_t int_set_hash(int32_t key) {
    uint32_t bits = (uint32_t)key;
    return bits ^ (bits >> 16u);
}

static bool int_set_before(uint32_t left, uint32_t right) {
    return (left ^ UINT32_C(0x80000000)) < (right ^ UINT32_C(0x80000000));
}

static int32_t int_set_key(const mc_java_hash_node *node) {
    return ((const int_set_node *)node)->key;
}

static uint32_t int_set_threshold(uint32_t capacity) {
    return capacity == INT_SET_MAX_CAPACITY ? UINT32_C(0x7fffffff) : capacity - capacity / 4u;
}

/* Integer's h ^ (h >>> 16) is bijective over all 32 bits, so complete spread
 * hash equality is exactly value equality. This is not a general Object-key
 * lookup: distinct Object keys may have equal complete hashes. */
static mc_java_hash_node *int_set_find(const mc_java_int_set *set, uint32_t hash) {
    if (set->capacity == 0)
        return NULL;
    mc_java_hash_node *node = set->buckets[hash & (set->capacity - 1u)];
    if (node != NULL && node->tree) {
        node = mc_java_hash_root(node);
        while (node != NULL && node->hash != hash)
            node = node->children[int_set_before(hash, node->hash) ? 0 : 1];
        return node;
    }
    while (node != NULL && node->hash != hash)
        node = node->next;
    return node;
}

static void int_set_resize(mc_java_int_set *set, mc_java_hash_node **table, uint32_t capacity) {
    mc_java_hash_resize(set->buckets, set->capacity, table, capacity, NULL, NULL);
    int_set_release(&set->allocator, set->buckets);
    set->buckets = table;
    set->capacity = capacity;
    set->threshold = int_set_threshold(capacity);
}

mc_java_int_set_limits mc_java_int_set_default_limits(void) {
    const mc_java_int_set_limits limits = {65536u, 8u * 1024u * 1024u, 16u * 1024u * 1024u};
    return limits;
}

const char *mc_java_int_set_status_string(mc_java_int_set_status status) {
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
                                              mc_java_int_set **out) {
    const mc_java_int_set_allocator allocator = {.context = NULL,
                                                 .allocate = int_set_default_allocate,
                                                 .deallocate = int_set_default_deallocate};
    return mc_java_int_set_create_with_allocator(requested, &allocator, out);
}

mc_java_int_set_status
mc_java_int_set_create_with_allocator(const mc_java_int_set_limits *requested,
                                      const mc_java_int_set_allocator *allocator,
                                      mc_java_int_set **out) {
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

size_t mc_java_int_set_size(const mc_java_int_set *set) {
    return set == NULL ? 0 : set->size;
}

bool mc_java_int_set_contains(const mc_java_int_set *set, int32_t key) {
    return set != NULL && int_set_find(set, int_set_hash(key)) != NULL;
}

mc_java_int_set_status mc_java_int_set_add(mc_java_int_set *set, int32_t key, bool *changed) {
    if (set == NULL || changed == NULL)
        return MC_JAVA_INT_SET_INVALID_ARGUMENT;
    uint32_t hash = int_set_hash(key);
    if (int_set_find(set, hash) != NULL) {
        *changed = false;
        return MC_JAVA_INT_SET_OK;
    }
    if (set->size == set->limits.max_entries)
        return MC_JAVA_INT_SET_LIMIT;
    mc_java_hash_node *head = set->capacity == 0 ? NULL : set->buckets[hash & (set->capacity - 1u)];
    size_t chain = 0;
    if (head != NULL && !head->tree) {
        for (mc_java_hash_node *node = head; node != NULL; node = node->next)
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
        if (!int_set_product((size_t)capacities[i], sizeof(mc_java_hash_node *), &table_bytes[i]) ||
            !int_set_sum(peak, table_bytes[i], &peak))
            return MC_JAVA_INT_SET_LIMIT;
    }
    if (retained > set->limits.max_retained_bytes || peak > set->limits.max_allocation_bytes)
        return MC_JAVA_INT_SET_LIMIT;
    int_set_node *entry = set->allocator.allocate(set->allocator.context, sizeof(*entry));
    if (entry == NULL)
        return MC_JAVA_INT_SET_ALLOCATION_FAILED;
    *entry = (int_set_node){.node = {.hash = hash}, .key = key};
    mc_java_hash_node *node = &entry->node;
    mc_java_hash_node **tables[2] = {NULL, NULL};
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
        mc_java_hash_tree_add(set->buckets, set->capacity, head, node, NULL, NULL);
    } else {
        mc_java_hash_node *tail = head;
        while (tail->next != NULL)
            tail = tail->next;
        tail->next = node;
        node->previous = tail;
        if (chain >= 8u) {
            if (collision) {
                int_set_resize(set, tables[table_index], capacities[table_index]);
                ++table_index;
            } else {
                mc_java_hash_treeify(set->buckets, set->capacity, index, NULL, NULL);
            }
        }
    }
    ++set->size;
    if (growth)
        int_set_resize(set, tables[table_index], capacities[table_index]);
    *changed = true;
    return MC_JAVA_INT_SET_OK;
}

mc_java_int_set_status mc_java_int_set_remove(mc_java_int_set *set, int32_t key, bool *changed) {
    if (set == NULL || changed == NULL)
        return MC_JAVA_INT_SET_INVALID_ARGUMENT;
    uint32_t hash = int_set_hash(key);
    mc_java_hash_node *node = int_set_find(set, hash);
    if (node == NULL) {
        *changed = false;
        return MC_JAVA_INT_SET_OK;
    }
    if (node->tree) {
        mc_java_hash_tree_remove(set->buckets, set->capacity, node, true);
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

mc_java_int_set_status mc_java_int_set_first(const mc_java_int_set *set, int32_t *key) {
    if (set == NULL || key == NULL)
        return MC_JAVA_INT_SET_INVALID_ARGUMENT;
    const mc_java_hash_node *node = mc_java_hash_first(set->buckets, set->capacity);
    if (node == NULL)
        return MC_JAVA_INT_SET_EMPTY;
    *key = int_set_key(node);
    return MC_JAVA_INT_SET_OK;
}

mc_java_int_set_status mc_java_int_set_copy_order(const mc_java_int_set *set, int32_t *output,
                                                  size_t capacity, size_t *count) {
    if (set == NULL || count == NULL || (output == NULL && capacity != 0))
        return MC_JAVA_INT_SET_INVALID_ARGUMENT;
    if (capacity < set->size)
        return MC_JAVA_INT_SET_LIMIT;
    size_t written = 0;
    for (const mc_java_hash_node *node = mc_java_hash_first(set->buckets, set->capacity);
         node != NULL; node = mc_java_hash_next(set->buckets, set->capacity, node))
        output[written++] = int_set_key(node);
    *count = written;
    return MC_JAVA_INT_SET_OK;
}

void mc_java_int_set_clear(mc_java_int_set *set) {
    if (set == NULL)
        return;
    for (uint32_t i = 0; i < set->capacity; ++i) {
        mc_java_hash_node *node = set->buckets[i];
        while (node != NULL) {
            mc_java_hash_node *next = node->next;
            int_set_release(&set->allocator, node);
            node = next;
        }
        set->buckets[i] = NULL;
    }
    set->size = 0;
}

void mc_java_int_set_free(mc_java_int_set *set) {
    if (set == NULL)
        return;
    const mc_java_int_set_allocator allocator = set->allocator;
    mc_java_int_set_clear(set);
    int_set_release(&allocator, set->buckets);
    int_set_release(&allocator, set);
}
