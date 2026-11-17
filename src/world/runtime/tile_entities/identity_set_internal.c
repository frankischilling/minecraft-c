#include "identity_set_internal.h"

#include "../../../core/java/hash_table_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define TILE_SET_INITIAL_CAPACITY UINT32_C(16)
#define TILE_SET_TREE_CAPACITY UINT32_C(64)
#define TILE_SET_MAX_CAPACITY UINT32_C(0x40000000)

typedef struct tile_identity_node {
    mc_java_hash_node link;
    mc_tile_entity *key;
    const char *source_name;
    uint32_t identity_hash;
} tile_identity_node;

struct mc_tile_identity_set_snapshot {
    mc_tile_identity_set *owner;
    mc_tile_identity_set_snapshot *previous, *next;
    size_t count, initialized, bytes;
    bool published;
    mc_tile_entity *items[];
};

struct mc_tile_identity_set {
    mc_tile_identity_set_limits limits;
    mc_tile_identity_set_allocator allocator;
    mc_java_hash_node **buckets;
    mc_tile_identity_set_snapshot *snapshots;
    size_t size, retained_bytes, peak_bytes;
    size_t snapshot_count, live_snapshots, failed_snapshots, snapshot_bytes;
    uint32_t capacity, threshold, mod_count;
};

static mc_tile_identity_set_status set_result(mc_tile_identity_set_error *error,
                                              mc_tile_identity_set_status status,
                                              const char *operation,
                                              mc_tile_entity_status tile_status) {
    if (error != NULL)
        *error = (mc_tile_identity_set_error){.status = status,
                                              .operation = operation,
                                              .tile_status = tile_status,
                                              .index = SIZE_MAX};
    return status;
}

static mc_tile_identity_set_status set_ok(mc_tile_identity_set_error *error) {
    return set_result(error, MC_TILE_IDENTITY_SET_OK, NULL, MC_TILE_ENTITY_OK);
}

static mc_tile_identity_set_status set_invalid(mc_tile_identity_set_error *error,
                                               const char *operation) {
    return set_result(error, MC_TILE_IDENTITY_SET_INVALID_ARGUMENT, operation, MC_TILE_ENTITY_OK);
}

static mc_tile_identity_set_status set_limit(mc_tile_identity_set_error *error,
                                             const char *operation) {
    return set_result(error, MC_TILE_IDENTITY_SET_LIMIT, operation, MC_TILE_ENTITY_OK);
}

static void *set_default_allocate(void *context, size_t bytes) {
    (void)context;
    return malloc(bytes);
}

static void set_default_deallocate(void *context, void *storage) {
    (void)context;
    free(storage);
}

static void set_release(const mc_tile_identity_set_allocator *allocator, void *storage) {
    if (storage != NULL && allocator->deallocate != NULL)
        allocator->deallocate(allocator->context, storage);
}

static bool set_sum(size_t left, size_t right, size_t *out) {
    if (right > SIZE_MAX - left)
        return false;
    *out = left + right;
    return true;
}

static bool set_product(size_t count, size_t width, size_t *out) {
    if (count > SIZE_MAX / width)
        return false;
    *out = count * width;
    return true;
}

static void set_peak(mc_tile_identity_set *set, size_t bytes) {
    if (bytes > set->peak_bytes)
        set->peak_bytes = bytes;
}

static tile_identity_node *set_entry(mc_java_hash_node *node) {
    return (tile_identity_node *)node;
}

static const tile_identity_node *set_const_entry(const mc_java_hash_node *node) {
    return (const tile_identity_node *)node;
}

static bool set_before(uint32_t left, uint32_t right) {
    return (left ^ UINT32_C(0x80000000)) < (right ^ UINT32_C(0x80000000));
}

static uint32_t set_threshold(uint32_t capacity) {
    return capacity == TILE_SET_MAX_CAPACITY ? UINT32_C(0x7fffffff) : capacity - capacity / 4u;
}

/* Actual Tile classes are not Comparable and inherit identity equals. Equal
 * complete hashes therefore search both subtrees, right before left as in
 * TreeNode.find. Insertion's class-name tie order is not a lookup shortcut. */
static mc_java_hash_node *set_find_tree(mc_java_hash_node *node, uint32_t hash,
                                        const mc_tile_entity *key) {
    while (node != NULL) {
        if (node->hash != hash) {
            node = node->children[set_before(hash, node->hash) ? 0u : 1u];
        } else if (set_entry(node)->key == key) {
            return node;
        } else if (node->children[0] == NULL) {
            node = node->children[1];
        } else if (node->children[1] == NULL) {
            node = node->children[0];
        } else {
            mc_java_hash_node *found = set_find_tree(node->children[1], hash, key);
            if (found != NULL)
                return found;
            node = node->children[0];
        }
    }
    return NULL;
}

static mc_java_hash_node *set_find(const mc_tile_identity_set *set, uint32_t hash,
                                   const mc_tile_entity *key) {
    if (set->capacity == 0)
        return NULL;
    mc_java_hash_node *node = set->buckets[hash & (set->capacity - 1u)];
    if (node != NULL && node->tree)
        return set_find_tree(mc_java_hash_root(node), hash, key);
    for (; node != NULL; node = node->next)
        if (node->hash == hash && set_entry(node)->key == key)
            return node;
    return NULL;
}

/* Names are admitted immutable Source Class.getName ASCII projections. Cached
 * raw hashes came from these actual keys before admission/allocation. Thus the
 * callback cannot fail or draw, including equal-class/equal-hash <= left arm.
 * No native address, Class address or construction ordinal orders the keys. */
static int set_tie_break(void *context, const mc_java_hash_node *left,
                         const mc_java_hash_node *right) {
    (void)context;
    const tile_identity_node *a = set_const_entry(left), *b = set_const_entry(right);
    if (a->key != NULL && b->key != NULL) {
        const int names = strcmp(a->source_name, b->source_name);
        if (names != 0)
            return names < 0 ? -1 : 1;
    }
    return a->identity_hash <= b->identity_hash ? -1 : 1;
}

static mc_tile_identity_set_status set_hash(mc_tile_entity *key, mc_java_identity_thread *thread,
                                            uint32_t *hash, uint32_t *identity,
                                            mc_tile_identity_set_error *error,
                                            const char *operation) {
    uint32_t raw;
    const mc_tile_entity_status status = mc_tile_entity_identity_hash(key, thread, &raw);
    if (status != MC_TILE_ENTITY_OK)
        return set_result(error, MC_TILE_IDENTITY_SET_TILE_FAILED, operation, status);
    *identity = raw;
    *hash = raw ^ (raw >> 16u);
    return MC_TILE_IDENTITY_SET_OK;
}

static void set_resize(mc_tile_identity_set *set, mc_java_hash_node **table, uint32_t capacity) {
    mc_java_hash_resize(set->buckets, set->capacity, table, capacity, set_tie_break, NULL);
    set_release(&set->allocator, set->buckets);
    set->buckets = table;
    set->capacity = capacity;
    set->threshold = set_threshold(capacity);
}

mc_tile_identity_set_limits mc_tile_identity_set_default_limits(void) {
    const mc_tile_identity_set_limits limits = {65536u, 8u * 1024u * 1024u, 16u * 1024u * 1024u};
    return limits;
}

const char *mc_tile_identity_set_status_string(mc_tile_identity_set_status status) {
    switch (status) {
        case MC_TILE_IDENTITY_SET_OK:
            return "ok";
        case MC_TILE_IDENTITY_SET_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_TILE_IDENTITY_SET_NULL_COLLECTION:
            return "null Collection";
        case MC_TILE_IDENTITY_SET_LIMIT:
            return "resource limit";
        case MC_TILE_IDENTITY_SET_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_TILE_IDENTITY_SET_TILE_FAILED:
            return "Tile identity/ownership failure";
        case MC_TILE_IDENTITY_SET_BUSY:
            return "live snapshot";
    }
    return "unknown Tile identity Set status";
}

mc_tile_identity_set_status mc_tile_identity_set_create(const mc_tile_identity_set_limits *limits,
                                                        mc_tile_identity_set **out,
                                                        mc_tile_identity_set_error *error) {
    const mc_tile_identity_set_allocator allocator = {
        .context = NULL, .allocate = set_default_allocate, .deallocate = set_default_deallocate};
    return mc_tile_identity_set_create_with_allocator(limits, &allocator, out, error);
}

mc_tile_identity_set_status mc_tile_identity_set_create_with_allocator(
    const mc_tile_identity_set_limits *requested, const mc_tile_identity_set_allocator *allocator,
    mc_tile_identity_set **out, mc_tile_identity_set_error *error) {
    if (out == NULL || allocator == NULL || allocator->allocate == NULL)
        return set_invalid(error, "Set constructor arguments");
    const mc_tile_identity_set_allocator selected = *allocator;
    const mc_tile_identity_set_limits limits =
        requested == NULL ? mc_tile_identity_set_default_limits() : *requested;
    if (limits.max_entries > (size_t)INT32_MAX)
        return set_invalid(error, "Set entry limit exceeds Java size");
    if (limits.max_retained_bytes < sizeof(mc_tile_identity_set) ||
        limits.max_allocation_bytes < sizeof(mc_tile_identity_set))
        return set_limit(error, "Set owner bytes");
    mc_tile_identity_set *set = selected.allocate(selected.context, sizeof(*set));
    if (set == NULL)
        return set_result(error, MC_TILE_IDENTITY_SET_ALLOCATION_FAILED, "Set owner allocation",
                          MC_TILE_ENTITY_OK);
    *set = (mc_tile_identity_set){.limits = limits,
                                  .allocator = selected,
                                  .retained_bytes = sizeof(*set),
                                  .peak_bytes = sizeof(*set)};
    *out = set;
    return set_ok(error);
}

mc_tile_identity_set_status mc_tile_identity_set_contains(const mc_tile_identity_set *set,
                                                          mc_tile_entity *key,
                                                          mc_java_identity_thread *current_thread,
                                                          bool *out,
                                                          mc_tile_identity_set_error *error) {
    if (set == NULL || out == NULL)
        return set_invalid(error, "Set.contains arguments");
    if (set->capacity == 0) {
        *out = false;
        return set_ok(error);
    }
    uint32_t hash, identity;
    const mc_tile_identity_set_status status =
        set_hash(key, current_thread, &hash, &identity, error, "Set.contains hash");
    if (status != MC_TILE_IDENTITY_SET_OK)
        return status;
    *out = set_find(set, hash, key) != NULL;
    return set_ok(error);
}

mc_tile_identity_set_status mc_tile_identity_set_add(mc_tile_identity_set *set, mc_tile_entity *key,
                                                     mc_java_identity_thread *current_thread,
                                                     bool *changed,
                                                     mc_tile_identity_set_error *error) {
    if (set == NULL || changed == NULL)
        return set_invalid(error, "Set.add arguments");
    uint32_t hash, identity;
    mc_tile_identity_set_status status =
        set_hash(key, current_thread, &hash, &identity, error, "Set.add hash");
    if (status != MC_TILE_IDENTITY_SET_OK)
        return status;
    if (set_find(set, hash, key) != NULL) {
        *changed = false;
        return set_ok(error);
    }
    const char *source_name = NULL;
    if (key != NULL) {
        const mc_tile_entity_class_literal *literal = NULL;
        mc_tile_entity_status cause = mc_tile_entity_get_class(key, &literal);
        if (cause == MC_TILE_ENTITY_OK)
            cause = mc_tile_entity_class_literal_source_name(literal, &source_name);
        if (cause != MC_TILE_ENTITY_OK)
            return set_result(error, MC_TILE_IDENTITY_SET_TILE_FAILED,
                              "Set.add class-name admission", cause);
    }
    if (set->size == set->limits.max_entries)
        return set_limit(error, "Set.add entry count");
    mc_java_hash_node *head = set->capacity == 0 ? NULL : set->buckets[hash & (set->capacity - 1u)];
    size_t chain = 0;
    if (head != NULL && !head->tree)
        for (mc_java_hash_node *node = head; node != NULL; node = node->next)
            ++chain;
    const bool initial = set->capacity == 0;
    uint32_t capacity = initial ? TILE_SET_INITIAL_CAPACITY : set->capacity;
    const bool collision = chain >= 8u && capacity < TILE_SET_TREE_CAPACITY;
    uint32_t capacities[2];
    size_t table_count = 0;
    if (initial)
        capacities[table_count++] = capacity;
    if (collision) {
        capacity *= 2u;
        capacities[table_count++] = capacity;
    }
    const bool growth =
        set->size + 1u > set_threshold(capacity) && capacity < TILE_SET_MAX_CAPACITY;
    if (growth) {
        capacity *= 2u;
        capacities[table_count++] = capacity;
    }
    size_t old_table_bytes, final_table_bytes, retained, peak, table_bytes[2];
    if (!set_product(set->capacity, sizeof(mc_java_hash_node *), &old_table_bytes) ||
        !set_product(capacity, sizeof(mc_java_hash_node *), &final_table_bytes) ||
        !set_sum(set->retained_bytes - old_table_bytes, final_table_bytes, &retained) ||
        !set_sum(retained, sizeof(tile_identity_node), &retained) ||
        !set_sum(set->retained_bytes, sizeof(tile_identity_node), &peak))
        return set_limit(error, "Set.add byte arithmetic");
    for (size_t i = 0; i < table_count; ++i)
        if (!set_product(capacities[i], sizeof(mc_java_hash_node *), &table_bytes[i]) ||
            !set_sum(peak, table_bytes[i], &peak))
            return set_limit(error, "Set.add reservation arithmetic");
    if (retained > set->limits.max_retained_bytes || peak > set->limits.max_allocation_bytes)
        return set_limit(error, "Set.add requested bytes");
    tile_identity_node *entry = set->allocator.allocate(set->allocator.context, sizeof(*entry));
    if (entry == NULL)
        return set_result(error, MC_TILE_IDENTITY_SET_ALLOCATION_FAILED, "Set.add node allocation",
                          MC_TILE_ENTITY_OK);
    *entry = (tile_identity_node){
        .link = {.hash = hash}, .key = key, .source_name = source_name, .identity_hash = identity};
    size_t reservation = set->retained_bytes + sizeof(*entry);
    set_peak(set, reservation);
    mc_java_hash_node **tables[2] = {NULL, NULL};
    for (size_t i = 0; i < table_count; ++i) {
        tables[i] = set->allocator.allocate(set->allocator.context, table_bytes[i]);
        if (tables[i] == NULL) {
            for (size_t j = 0; j < i; ++j)
                set_release(&set->allocator, tables[j]);
            set_release(&set->allocator, entry);
            return set_result(error, MC_TILE_IDENTITY_SET_ALLOCATION_FAILED,
                              "Set.add table allocation", MC_TILE_ENTITY_OK);
        }
        reservation += table_bytes[i];
        set_peak(set, reservation);
        for (uint32_t j = 0; j < capacities[i]; ++j)
            tables[i][j] = NULL;
    }
    if (key != NULL) {
        const mc_tile_entity_status cause = mc_tile_entity_retain(key);
        if (cause != MC_TILE_ENTITY_OK) {
            for (size_t i = 0; i < table_count; ++i)
                set_release(&set->allocator, tables[i]);
            set_release(&set->allocator, entry);
            return set_result(error, MC_TILE_IDENTITY_SET_TILE_FAILED, "Set.add Tile retain",
                              cause);
        }
    }
    size_t table_index = 0;
    if (initial) {
        set_resize(set, tables[table_index], capacities[table_index]);
        ++table_index;
    }
    mc_java_hash_node *node = &entry->link;
    const uint32_t index = hash & (set->capacity - 1u);
    head = set->buckets[index];
    if (head == NULL) {
        set->buckets[index] = node;
    } else if (head->tree) {
        mc_java_hash_tree_add(set->buckets, set->capacity, head, node, set_tie_break, NULL);
    } else {
        mc_java_hash_node *tail = head;
        while (tail->next != NULL)
            tail = tail->next;
        tail->next = node;
        node->previous = tail;
        if (chain >= 8u) {
            if (collision) {
                set_resize(set, tables[table_index], capacities[table_index]);
                ++table_index;
            } else {
                mc_java_hash_treeify(set->buckets, set->capacity, index, set_tie_break, NULL);
            }
        }
    }
    ++set->size;
    ++set->mod_count;
    if (growth)
        set_resize(set, tables[table_index], capacities[table_index]);
    set->retained_bytes = retained;
    *changed = true;
    return set_ok(error);
}

static void set_remove_node(mc_tile_identity_set *set, mc_java_hash_node *node, bool movable) {
    if (node->tree) {
        mc_java_hash_tree_remove(set->buckets, set->capacity, node, movable);
    } else {
        if (node->previous == NULL)
            set->buckets[node->hash & (set->capacity - 1u)] = node->next;
        else
            node->previous->next = node->next;
        if (node->next != NULL)
            node->next->previous = node->previous;
    }
    --set->size;
    ++set->mod_count;
    set->retained_bytes -= sizeof(tile_identity_node);
    mc_tile_entity *key = set_entry(node)->key;
    if (key != NULL)
        mc_tile_entity_release(key);
    set_release(&set->allocator, node);
}

mc_tile_identity_set_status mc_tile_identity_set_remove(mc_tile_identity_set *set,
                                                        mc_tile_entity *key,
                                                        mc_java_identity_thread *current_thread,
                                                        bool *changed,
                                                        mc_tile_identity_set_error *error) {
    if (set == NULL || changed == NULL)
        return set_invalid(error, "Set.remove arguments");
    uint32_t hash, identity;
    const mc_tile_identity_set_status status =
        set_hash(key, current_thread, &hash, &identity, error, "Set.remove hash");
    if (status != MC_TILE_IDENTITY_SET_OK)
        return status;
    mc_java_hash_node *node = set_find(set, hash, key);
    if (node == NULL) {
        *changed = false;
        return set_ok(error);
    }
    set_remove_node(set, node, true);
    *changed = true;
    return set_ok(error);
}

static bool set_span_valid(const mc_tile_identity_set_span *span) {
    return span->count <= (size_t)INT32_MAX && (span->items != NULL || span->count == 0);
}

static mc_tile_identity_set_status set_bulk_failure(mc_tile_identity_set_error *error,
                                                    const mc_tile_identity_set_error *cause,
                                                    size_t index, bool changed) {
    if (error != NULL) {
        *error = *cause;
        error->index = error->completed = index;
        error->changed_prefix = changed;
    }
    return cause->status;
}

mc_tile_identity_set_status
mc_tile_identity_set_add_all_span(mc_tile_identity_set *set, const mc_tile_identity_set_span *span,
                                  mc_java_identity_thread *current_thread, bool *changed,
                                  mc_tile_identity_set_error *error) {
    if (set == NULL || changed == NULL)
        return set_invalid(error, "Set.addAll arguments");
    if (span == NULL)
        return set_result(error, MC_TILE_IDENTITY_SET_NULL_COLLECTION, "Set.addAll null Collection",
                          MC_TILE_ENTITY_OK);
    if (!set_span_valid(span))
        return set_invalid(error, "Set.addAll ArrayList span");
    bool modified = false;
    for (size_t i = 0; i < span->count; ++i) {
        bool added;
        mc_tile_identity_set_error attempt;
        const mc_tile_identity_set_status status =
            mc_tile_identity_set_add(set, span->items[i], current_thread, &added, &attempt);
        if (status != MC_TILE_IDENTITY_SET_OK)
            return set_bulk_failure(error, &attempt, i, modified);
        modified = modified || added;
    }
    *changed = modified;
    return set_ok(error);
}

mc_tile_identity_set_status
mc_tile_identity_set_remove_all_set(mc_tile_identity_set *set, const mc_tile_identity_set *other,
                                    mc_java_identity_thread *current_thread, bool *changed,
                                    mc_tile_identity_set_error *error) {
    if (set == NULL || changed == NULL)
        return set_invalid(error, "Set.removeAll arguments");
    if (other == NULL)
        return set_result(error, MC_TILE_IDENTITY_SET_NULL_COLLECTION,
                          "Set.removeAll null Collection", MC_TILE_ENTITY_OK);
    const bool ordinary_remove = set->size > other->size;
    const mc_tile_identity_set *iterated = ordinary_remove ? other : set;
    bool modified = false;
    size_t index = 0;
    for (mc_java_hash_node *node = mc_java_hash_first(iterated->buckets, iterated->capacity);
         node != NULL; ++index) {
        mc_java_hash_node *next = mc_java_hash_next(iterated->buckets, iterated->capacity, node);
        mc_tile_identity_set_error attempt;
        bool selected;
        const mc_tile_identity_set_status status =
            ordinary_remove ? mc_tile_identity_set_remove(set, set_entry(node)->key, current_thread,
                                                          &selected, &attempt)
                            : mc_tile_identity_set_contains(other, set_entry(node)->key,
                                                            current_thread, &selected, &attempt);
        if (status != MC_TILE_IDENTITY_SET_OK)
            return set_bulk_failure(error, &attempt, index, modified);
        if (selected) {
            if (!ordinary_remove)
                set_remove_node(set, node, false);
            modified = true;
        }
        node = next;
    }
    *changed = modified;
    return set_ok(error);
}

mc_tile_identity_set_status mc_tile_identity_set_remove_all_span(
    mc_tile_identity_set *set, const mc_tile_identity_set_span *span,
    mc_java_identity_thread *current_thread, bool *changed, mc_tile_identity_set_error *error) {
    if (set == NULL || changed == NULL)
        return set_invalid(error, "Set.removeAll arguments");
    if (span == NULL)
        return set_result(error, MC_TILE_IDENTITY_SET_NULL_COLLECTION,
                          "Set.removeAll null Collection", MC_TILE_ENTITY_OK);
    if (!set_span_valid(span))
        return set_invalid(error, "Set.removeAll ArrayList span");
    bool modified = false;
    if (set->size > span->count) {
        for (size_t i = 0; i < span->count; ++i) {
            bool removed;
            mc_tile_identity_set_error attempt;
            const mc_tile_identity_set_status status = mc_tile_identity_set_remove(
                set, span->items[i], current_thread, &removed, &attempt);
            if (status != MC_TILE_IDENTITY_SET_OK)
                return set_bulk_failure(error, &attempt, i, modified);
            modified = modified || removed;
        }
    } else {
        for (mc_java_hash_node *node = mc_java_hash_first(set->buckets, set->capacity);
             node != NULL;) {
            mc_java_hash_node *next = mc_java_hash_next(set->buckets, set->capacity, node);
            const mc_tile_entity *key = set_entry(node)->key;
            for (size_t i = 0; i < span->count; ++i)
                if (span->items[i] == key) {
                    set_remove_node(set, node, false);
                    modified = true;
                    break;
                }
            node = next;
        }
    }
    *changed = modified;
    return set_ok(error);
}

static void set_snapshot_unlink(mc_tile_identity_set_snapshot *snapshot) {
    mc_tile_identity_set *set = snapshot->owner;
    if (snapshot->previous == NULL)
        set->snapshots = snapshot->next;
    else
        snapshot->previous->next = snapshot->next;
    if (snapshot->next != NULL)
        snapshot->next->previous = snapshot->previous;
    --set->snapshot_count;
    if (snapshot->published)
        --set->live_snapshots;
    else
        --set->failed_snapshots;
    set->snapshot_bytes -= snapshot->bytes;
    set->retained_bytes -= snapshot->bytes;
    for (size_t i = 0; i < snapshot->initialized; ++i)
        if (snapshot->items[i] != NULL)
            mc_tile_entity_release(snapshot->items[i]);
    set_release(&set->allocator, snapshot);
}

mc_tile_identity_set_status mc_tile_identity_set_to_array(mc_tile_identity_set *set,
                                                          mc_tile_identity_set_snapshot **out,
                                                          mc_tile_identity_set_error *error) {
    if (set == NULL || out == NULL)
        return set_invalid(error, "Set.toArray arguments");
    size_t items_bytes, bytes, retained;
    if (!set_product(set->size, sizeof(mc_tile_entity *), &items_bytes) ||
        !set_sum(sizeof(mc_tile_identity_set_snapshot), items_bytes, &bytes) ||
        !set_sum(set->retained_bytes, bytes, &retained))
        return set_limit(error, "Set.toArray byte arithmetic");
    if (retained > set->limits.max_retained_bytes || retained > set->limits.max_allocation_bytes)
        return set_limit(error, "Set.toArray requested bytes");
    mc_tile_identity_set_snapshot *snapshot =
        set->allocator.allocate(set->allocator.context, bytes);
    if (snapshot == NULL)
        return set_result(error, MC_TILE_IDENTITY_SET_ALLOCATION_FAILED, "Set.toArray allocation",
                          MC_TILE_ENTITY_OK);
    *snapshot = (mc_tile_identity_set_snapshot){
        .owner = set, .next = set->snapshots, .count = set->size, .bytes = bytes};
    if (set->snapshots != NULL)
        set->snapshots->previous = snapshot;
    set->snapshots = snapshot;
    ++set->snapshot_count;
    ++set->failed_snapshots;
    set->snapshot_bytes += bytes;
    set->retained_bytes = retained;
    set_peak(set, retained);
    for (mc_java_hash_node *node = mc_java_hash_first(set->buckets, set->capacity); node != NULL;
         node = mc_java_hash_next(set->buckets, set->capacity, node)) {
        mc_tile_entity *key = set_entry(node)->key;
        if (key != NULL) {
            const mc_tile_entity_status cause = mc_tile_entity_retain(key);
            if (cause != MC_TILE_ENTITY_OK) {
                const mc_tile_identity_set_status status = set_result(
                    error, MC_TILE_IDENTITY_SET_TILE_FAILED, "Set.toArray Tile retain", cause);
                if (error != NULL)
                    error->index = error->completed = snapshot->initialized;
                return status;
            }
        }
        snapshot->items[snapshot->initialized++] = key;
    }
    snapshot->published = true;
    --set->failed_snapshots;
    ++set->live_snapshots;
    *out = snapshot;
    return set_ok(error);
}

mc_tile_identity_set_status
mc_tile_identity_set_snapshot_inspect(const mc_tile_identity_set_snapshot *snapshot,
                                      mc_tile_identity_set_snapshot_view *out,
                                      mc_tile_identity_set_error *error) {
    if (snapshot == NULL || out == NULL || !snapshot->published)
        return set_invalid(error, "Set snapshot view arguments");
    *out = (mc_tile_identity_set_snapshot_view){.items = snapshot->items, .count = snapshot->count};
    return set_ok(error);
}

mc_tile_identity_set_status
mc_tile_identity_set_snapshot_retire(mc_tile_identity_set_snapshot **slot,
                                     mc_tile_identity_set_error *error) {
    if (slot == NULL)
        return set_invalid(error, "Set snapshot retirement slot");
    if (*slot == NULL)
        return set_ok(error);
    if (!(*slot)->published)
        return set_invalid(error, "Set snapshot retirement owner");
    set_snapshot_unlink(*slot);
    *slot = NULL;
    return set_ok(error);
}

mc_tile_identity_set_status mc_tile_identity_set_inspect(const mc_tile_identity_set *set,
                                                         mc_tile_identity_set_view *out,
                                                         mc_tile_identity_set_error *error) {
    if (set == NULL || out == NULL)
        return set_invalid(error, "Set view arguments");
    *out = (mc_tile_identity_set_view){.size = set->size,
                                       .capacity = set->capacity,
                                       .threshold = set->threshold,
                                       .mod_count = set->mod_count,
                                       .retained_bytes = set->retained_bytes,
                                       .peak_bytes = set->peak_bytes,
                                       .snapshot_count = set->snapshot_count,
                                       .live_snapshots = set->live_snapshots,
                                       .failed_snapshots = set->failed_snapshots,
                                       .snapshot_bytes = set->snapshot_bytes};
    return set_ok(error);
}

mc_tile_identity_set_status mc_tile_identity_set_retire(mc_tile_identity_set **slot,
                                                        mc_tile_identity_set_error *error) {
    if (slot == NULL)
        return set_invalid(error, "Set retirement slot");
    mc_tile_identity_set *set = *slot;
    if (set == NULL)
        return set_ok(error);
    if (set->live_snapshots != 0)
        return set_result(error, MC_TILE_IDENTITY_SET_BUSY, "Set owns live snapshots",
                          MC_TILE_ENTITY_OK);
    while (set->snapshots != NULL)
        set_snapshot_unlink(set->snapshots);
    const mc_tile_identity_set_allocator allocator = set->allocator;
    for (uint32_t i = 0; i < set->capacity; ++i) {
        mc_java_hash_node *node = set->buckets[i];
        while (node != NULL) {
            mc_java_hash_node *next = node->next;
            mc_tile_entity *key = set_entry(node)->key;
            if (key != NULL)
                mc_tile_entity_release(key);
            set_release(&allocator, node);
            node = next;
        }
    }
    set_release(&allocator, set->buckets);
    set_release(&allocator, set);
    *slot = NULL;
    return set_ok(error);
}
