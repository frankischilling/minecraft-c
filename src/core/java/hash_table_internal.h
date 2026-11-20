#ifndef MC_CORE_JAVA_HASH_TABLE_INTERNAL_H
#define MC_CORE_JAVA_HASH_TABLE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Private intrusive storage shared by typed collection adapters. The adapter
 * owns each node and its key; this header contains only structural state.
 * Keeping flags before the links leaves this header 48 bytes on 64-bit targets
 * and 28 bytes on 32-bit targets. */
typedef struct mc_java_hash_node mc_java_hash_node;

struct mc_java_hash_node {
    uint32_t hash;
    bool red, tree;
    mc_java_hash_node *next, *previous, *parent, *children[2];
};

/* The kernel orders distinct complete hashes as signed JDK ints. This callback
 * is used ONLY for equal complete hashes between distinct keys: negative
 * selects left, positive selects right. NULL is permitted only when distinct
 * inserted keys cannot share a complete hash, as for Integer's bijective
 * spread. Equality/duplicate lookup occurs before insertion. Tie comparison
 * must not allocate, fail, reenter or mutate nodes. */
typedef int (*mc_java_hash_compare)(void *context, const mc_java_hash_node *left,
                                    const mc_java_hash_node *right);

/* These structural operations require exclusive access, valid links, a
 * nonzero power-of-two capacity, non-NULL nodes where used, and the tie policy
 * above. They perform no allocations, releases, key hashing or size/modCount
 * updates. The typed adapter owns admission, failure staging and publication. */
static inline mc_java_hash_node *mc_java_hash_root(mc_java_hash_node *node) {
    while (node->parent != NULL)
        node = node->parent;
    return node;
}

void mc_java_hash_treeify(mc_java_hash_node **buckets, uint32_t capacity, uint32_t index,
                          mc_java_hash_compare tie_break, void *context);
void mc_java_hash_tree_add(mc_java_hash_node **buckets, uint32_t capacity, mc_java_hash_node *head,
                           mc_java_hash_node *node, mc_java_hash_compare tie_break, void *context);

/* Transfer to a disjoint, zeroed table. old_capacity is zero for first table
 * construction, otherwise capacity must be twice old_capacity. The old table
 * storage remains owned by the adapter; its node links are changed in place.
 * Tree splits preserve the next-chain order and use the same hash/tie policy
 * when rebuilding. */
void mc_java_hash_resize(mc_java_hash_node *const *old_buckets, uint32_t old_capacity,
                         mc_java_hash_node **buckets, uint32_t capacity,
                         mc_java_hash_compare tie_break, void *context);

/* node is a member of a tree bin. Unlink it without freeing it. Ordinary
 * removal passes true; iterator removal passes false to suppress the small
 * tree shape conversion and root-to-front movement. */
void mc_java_hash_tree_remove(mc_java_hash_node **buckets, uint32_t capacity,
                              mc_java_hash_node *node, bool movable);

/* Borrowed table/next order, without persistent iterator or fail-fast state.
 * first permits capacity zero with NULL buckets. next requires a current node
 * still linked in this table and a positive capacity; it returns the next
 * chain member or scans subsequent buckets. Neither function changes links. */
static inline mc_java_hash_node *mc_java_hash_first_from(mc_java_hash_node *const *buckets,
                                                         uint32_t capacity, uint32_t index) {
    for (; index < capacity; ++index)
        if (buckets[index] != NULL)
            return buckets[index];
    return NULL;
}

static inline mc_java_hash_node *mc_java_hash_first(mc_java_hash_node *const *buckets,
                                                    uint32_t capacity) {
    return mc_java_hash_first_from(buckets, capacity, 0);
}

static inline mc_java_hash_node *mc_java_hash_next(mc_java_hash_node *const *buckets,
                                                   uint32_t capacity,
                                                   const mc_java_hash_node *node) {
    if (node->next != NULL)
        return node->next;
    return mc_java_hash_first_from(buckets, capacity, (node->hash & (capacity - 1u)) + 1u);
}

#endif
