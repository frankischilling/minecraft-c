#include "mc/world/schedule/membership.h"

#include <stdlib.h>

/* Native representation of reached F membership semantics.
 * Bucket/collision traversal order is not a Java HashSet identity-order claim. */
typedef struct scheduled_membership_node {
    mc_scheduled_tick *tick;
    uint32_t hash;
    struct scheduled_membership_node *next;
} scheduled_membership_node;

struct mc_scheduled_membership {
    scheduled_membership_node **buckets;
    size_t bucket_count;
    size_t count;
    size_t max_entries;
    size_t max_allocation_bytes;
    size_t allocation_bytes;
};

const char *mc_scheduled_membership_status_string(mc_scheduled_membership_status status)
{
    switch (status) {
        case MC_SCHEDULED_MEMBERSHIP_OK:
            return "ok";
        case MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_SCHEDULED_MEMBERSHIP_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_SCHEDULED_MEMBERSHIP_LIMIT:
            return "collection resource limit";
        case MC_SCHEDULED_MEMBERSHIP_HOLD_LIMIT:
            return "scheduled tick hold limit";
    }
    return "unknown scheduled membership status";
}

static mc_scheduled_membership_status membership_record_status(mc_scheduled_tick_status status)
{
    switch (status) {
        case MC_SCHEDULED_TICK_OK:
            return MC_SCHEDULED_MEMBERSHIP_OK;
        case MC_SCHEDULED_TICK_INVALID_ARGUMENT:
            return MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT;
        case MC_SCHEDULED_TICK_ALLOCATION_FAILED:
            return MC_SCHEDULED_MEMBERSHIP_ALLOCATION_FAILED;
        case MC_SCHEDULED_TICK_LIMIT:
            return MC_SCHEDULED_MEMBERSHIP_HOLD_LIMIT;
    }
    return MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT;
}

static mc_scheduled_membership_status membership_hash(const mc_scheduled_tick *tick, uint32_t *out)
{
    int32_t hash;
    const mc_scheduled_tick_status status = mc_scheduled_tick_hash(tick, &hash);
    if (status != MC_SCHEDULED_TICK_OK) {
        return membership_record_status(status);
    }
    *out = (uint32_t)hash;
    return MC_SCHEDULED_MEMBERSHIP_OK;
}

static size_t membership_bucket(uint32_t hash, size_t bucket_count)
{
    /* Native spread/index only. Equality is never replaced with this hash.
     * Nonzero bucket counts are checked powers of two, initially 16. */
    const uint32_t spread = hash ^ (hash >> 16);
    return (size_t)spread & (bucket_count - 1);
}

static mc_scheduled_membership_status membership_contains(const mc_scheduled_membership *membership,
                                                          const mc_scheduled_tick *query,
                                                          uint32_t hash, bool *out)
{
    if (membership->bucket_count != 0) {
        const size_t bucket = membership_bucket(hash, membership->bucket_count);
        const scheduled_membership_node *node = membership->buckets[bucket];
        for (; node != NULL; node = node->next) {
            if (node->hash == hash) {
                bool equal;
                const mc_scheduled_tick_status status =
                    mc_scheduled_tick_equals(query, node->tick, &equal);
                if (status != MC_SCHEDULED_TICK_OK) {
                    return membership_record_status(status);
                }
                if (equal) {
                    *out = true;
                    return MC_SCHEDULED_MEMBERSHIP_OK;
                }
            }
        }
    }
    *out = false;
    return MC_SCHEDULED_MEMBERSHIP_OK;
}

mc_scheduled_membership_status mc_scheduled_membership_create(size_t max_entries,
                                                              size_t max_allocation_bytes,
                                                              mc_scheduled_membership **out)
{
    if (out == NULL || max_entries > (size_t)INT32_MAX) {
        return MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT;
    }
    if (sizeof(mc_scheduled_membership) > max_allocation_bytes) {
        return MC_SCHEDULED_MEMBERSHIP_LIMIT;
    }
    mc_scheduled_membership *membership = malloc(sizeof(*membership));
    if (membership == NULL) {
        return MC_SCHEDULED_MEMBERSHIP_ALLOCATION_FAILED;
    }
    *membership = (mc_scheduled_membership){
        .buckets = NULL,
        .bucket_count = 0,
        .count = 0,
        .max_entries = max_entries,
        .max_allocation_bytes = max_allocation_bytes,
        .allocation_bytes = sizeof(*membership),
    };
    *out = membership;
    return MC_SCHEDULED_MEMBERSHIP_OK;
}

void mc_scheduled_membership_destroy(mc_scheduled_membership *membership)
{
    if (membership == NULL) {
        return;
    }
    for (size_t i = 0; i < membership->bucket_count; ++i) {
        scheduled_membership_node *node = membership->buckets[i];
        while (node != NULL) {
            scheduled_membership_node *next = node->next;
            mc_scheduled_tick_release(node->tick);
            free(node);
            node = next;
        }
    }
    free(membership->buckets);
    free(membership);
}

mc_scheduled_membership_status
mc_scheduled_membership_contains(const mc_scheduled_membership *membership,
                                 const mc_scheduled_tick *query, bool *out)
{
    if (membership == NULL || query == NULL || out == NULL) {
        return MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT;
    }
    uint32_t hash;
    const mc_scheduled_membership_status status = membership_hash(query, &hash);
    if (status != MC_SCHEDULED_MEMBERSHIP_OK) {
        return status;
    }
    return membership_contains(membership, query, hash, out);
}

mc_scheduled_membership_status
mc_scheduled_membership_size(const mc_scheduled_membership *membership, int32_t *out)
{
    if (membership == NULL || out == NULL) {
        return MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT;
    }
    /* count <= max_entries <= INT32_MAX is maintained at insertion. */
    *out = (int32_t)membership->count;
    return MC_SCHEDULED_MEMBERSHIP_OK;
}

mc_scheduled_membership_status
mc_scheduled_membership_native_bytes(const mc_scheduled_membership *membership, size_t *out)
{
    if (membership == NULL || out == NULL) {
        return MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT;
    }
    *out = membership->allocation_bytes;
    return MC_SCHEDULED_MEMBERSHIP_OK;
}

mc_scheduled_membership_status mc_scheduled_membership_add(mc_scheduled_membership *membership,
                                                           mc_scheduled_tick *tick, bool *added)
{
    if (membership == NULL || tick == NULL || added == NULL) {
        return MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT;
    }
    uint32_t hash;
    mc_scheduled_membership_status status = membership_hash(tick, &hash);
    if (status != MC_SCHEDULED_MEMBERSHIP_OK) {
        return status;
    }
    bool present;
    status = membership_contains(membership, tick, hash, &present);
    if (status != MC_SCHEDULED_MEMBERSHIP_OK) {
        return status;
    }
    if (present) {
        /* Duplicate is successful even at capacity: preserve original owner,
         * due time and sequence, with no allocation or candidate hold change. */
        *added = false;
        return MC_SCHEDULED_MEMBERSHIP_OK;
    }
    if (membership->count >= membership->max_entries) {
        return MC_SCHEDULED_MEMBERSHIP_LIMIT;
    }

    size_t next_bucket_count = membership->bucket_count;
    if (next_bucket_count == 0) {
        next_bucket_count = 16;
    } else if (membership->count + 1 > next_bucket_count - next_bucket_count / 4) {
        if (next_bucket_count > SIZE_MAX / 2) {
            return MC_SCHEDULED_MEMBERSHIP_LIMIT;
        }
        next_bucket_count *= 2;
    }
    const bool grow = next_bucket_count != membership->bucket_count;
    size_t new_bucket_bytes = 0;
    if (grow) {
        if (next_bucket_count > SIZE_MAX / sizeof(scheduled_membership_node *)) {
            return MC_SCHEDULED_MEMBERSHIP_LIMIT;
        }
        new_bucket_bytes = next_bucket_count * sizeof(scheduled_membership_node *);
    }
    /* Existing allocation_bytes includes OLD buckets and all retained nodes.
     * Check NEW buckets + staged node while every old allocation stays live. */
    size_t remaining = membership->max_allocation_bytes - membership->allocation_bytes;
    if (new_bucket_bytes > remaining) {
        return MC_SCHEDULED_MEMBERSHIP_LIMIT;
    }
    remaining -= new_bucket_bytes;
    if (sizeof(scheduled_membership_node) > remaining) {
        return MC_SCHEDULED_MEMBERSHIP_LIMIT;
    }
    scheduled_membership_node **new_buckets = NULL;
    if (grow) {
        new_buckets = malloc(new_bucket_bytes);
        if (new_buckets == NULL) {
            return MC_SCHEDULED_MEMBERSHIP_ALLOCATION_FAILED;
        }
        for (size_t i = 0; i < next_bucket_count; ++i) {
            new_buckets[i] = NULL;
        }
    }
    scheduled_membership_node *node = malloc(sizeof(*node));
    if (node == NULL) {
        free(new_buckets);
        return MC_SCHEDULED_MEMBERSHIP_ALLOCATION_FAILED;
    }
    const mc_scheduled_tick_status retained = mc_scheduled_tick_retain(tick);
    if (retained != MC_SCHEDULED_TICK_OK) {
        free(node);
        free(new_buckets);
        return membership_record_status(retained);
    }

    /* All fallible work has completed. Rehash existing nodes without changing
     * a record field or hold, then install this same retained candidate. */
    const size_t old_bucket_bytes = membership->bucket_count * sizeof(scheduled_membership_node *);
    const size_t final_bytes = membership->allocation_bytes + new_bucket_bytes + sizeof(*node) -
                               (grow ? old_bucket_bytes : 0);
    if (grow) {
        for (size_t i = 0; i < membership->bucket_count; ++i) {
            scheduled_membership_node *current = membership->buckets[i];
            while (current != NULL) {
                scheduled_membership_node *next = current->next;
                const size_t bucket = membership_bucket(current->hash, next_bucket_count);
                current->next = new_buckets[bucket];
                new_buckets[bucket] = current;
                current = next;
            }
        }
        scheduled_membership_node **old_buckets = membership->buckets;
        membership->buckets = new_buckets;
        membership->bucket_count = next_bucket_count;
        free(old_buckets);
    }
    const size_t bucket = membership_bucket(hash, membership->bucket_count);
    node->tick = tick;
    node->hash = hash;
    node->next = membership->buckets[bucket];
    membership->buckets[bucket] = node;
    membership->count += 1;
    membership->allocation_bytes = final_bytes;
    *added = true;
    return MC_SCHEDULED_MEMBERSHIP_OK;
}

mc_scheduled_membership_status mc_scheduled_membership_remove(mc_scheduled_membership *membership,
                                                              const mc_scheduled_tick *query,
                                                              bool *removed)
{
    if (membership == NULL || query == NULL || removed == NULL) {
        return MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT;
    }
    uint32_t hash;
    const mc_scheduled_membership_status status = membership_hash(query, &hash);
    if (status != MC_SCHEDULED_MEMBERSHIP_OK) {
        return status;
    }
    if (membership->bucket_count != 0) {
        const size_t bucket = membership_bucket(hash, membership->bucket_count);
        scheduled_membership_node **link = &membership->buckets[bucket];
        while (*link != NULL) {
            scheduled_membership_node *node = *link;
            if (node->hash == hash) {
                bool equal;
                const mc_scheduled_tick_status compared =
                    mc_scheduled_tick_equals(query, node->tick, &equal);
                if (compared != MC_SCHEDULED_TICK_OK) {
                    return membership_record_status(compared);
                }
                if (equal) {
                    *link = node->next;
                    membership->count -= 1;
                    membership->allocation_bytes -= sizeof(*node);
                    mc_scheduled_tick_release(node->tick);
                    free(node);
                    *removed = true;
                    return MC_SCHEDULED_MEMBERSHIP_OK;
                }
            }
            link = &node->next;
        }
    }
    *removed = false;
    return MC_SCHEDULED_MEMBERSHIP_OK;
}

mc_scheduled_membership_status
mc_scheduled_membership_shift_time(mc_scheduled_membership *membership, int64_t delta)
{
    if (membership == NULL) {
        return MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < membership->bucket_count; ++i) {
        scheduled_membership_node *node = membership->buckets[i];
        for (; node != NULL; node = node->next) {
            /* Stored nonnull owners have a collection hold. The concrete
             * record method is total and allocation/callback-free in this
             * valid domain; do not invent per-record failure boundaries. */
            (void)mc_scheduled_tick_time_shift(node->tick, delta);
        }
    }
    return MC_SCHEDULED_MEMBERSHIP_OK;
}
