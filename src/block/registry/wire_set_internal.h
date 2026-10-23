#ifndef MC_BLOCK_REGISTRY_WIRE_SET_INTERNAL_H
#define MC_BLOCK_REGISTRY_WIRE_SET_INTERNAL_H

#include "mc/block/registry/registry.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct mc_registry_wire_set mc_registry_wire_set;
typedef struct mc_registry_wire_snapshot mc_registry_wire_snapshot;

typedef struct mc_registry_wire_position {
    int32_t x, y, z;
    /* Native object correspondence only. Never an ordering or entropy input. */
    uint64_t identity;
    bool identity_hash_ready;
    int32_t identity_hash;
} mc_registry_wire_position;

/* Optional measured input for the selected library's non-Comparable,
 * equal-full-hash tree tie. Called lazily, once per successfully supplied key
 * hash; coordinate hashes, duplicates and ordinary buckets do not call it.
 * This is conditional collection replay, not a default JVM identity model.
 * The provider must not mutate this Set or release its defining Registry.
 * Failure leaves the logical Set and ordinary outputs unchanged; already
 * supplied identity hashes stay cached. NULL fails only if a tie is reached. */
typedef mc_registry_status (*mc_registry_wire_identity_hash)(void *context,
                                                             const mc_registry_wire_position *key,
                                                             int32_t *out);

typedef struct mc_registry_wire_set_view {
    const void *map, *table;
    size_t size;
    uint32_t capacity;
    int32_t threshold, mod_count;
    float load_factor;
    uint64_t constructed_keys;
} mc_registry_wire_set_view;

typedef struct mc_registry_wire_node_view {
    uint64_t identity, next, previous, parent, left, right;
    mc_registry_wire_position key;
    uint32_t hash, bucket;
    bool tree, red;
} mc_registry_wire_node_view;

/* Live, exclusively used Registry/Set required. Construction preserves the
 * two separate permanent Set/Map allocations and publishes only on success.
 * Gameplay storage is reclaimable and shares the Registry byte bound. These
 * operations perform no class use, World calls or Random draws. Construction
 * preserves arena failure diagnostics; gameplay operations leave last_error
 * unchanged. Outputs are unchanged on failure. Each add/remove first constructs
 * its fresh immutable key, including duplicate/absent coordinates. Each
 * reached operation commits its complete logical transition or preserves the
 * previous Set; this native resource policy is not Java OOME equivalence. */
mc_registry_status mc_registry_wire_set_create(mc_registry_classes *classes,
                                               mc_registry_wire_set **out);
/* Bind before any fresh key or table construction. Later binding is BUSY;
 * clear does not reset the profile or already assigned object identities. */
mc_registry_status mc_registry_wire_set_identity_profile(mc_registry_wire_set *set,
                                                         mc_registry_wire_identity_hash input,
                                                         void *context);
mc_registry_status mc_registry_wire_set_add_fresh(mc_registry_wire_set *set, int32_t x, int32_t y,
                                                  int32_t z, bool *changed);
mc_registry_status mc_registry_wire_set_remove_fresh(mc_registry_wire_set *set, int32_t x,
                                                     int32_t y, int32_t z, bool *changed);
/* Internal comparison seam: re-add a currently live key, without constructing
 * another key. No external key ownership is exposed. */
mc_registry_status mc_registry_wire_set_readd_existing(mc_registry_wire_set *set, uint64_t identity,
                                                       bool *changed);
mc_registry_status mc_registry_wire_set_clear(mc_registry_wire_set *set);

/* Copy and clear deliberately remain separate. A shallow, immutable snapshot
 * owns independent key holds through clear/refill and nested snapshots. */
mc_registry_status mc_registry_wire_snapshot_copy(mc_registry_wire_set *set,
                                                  mc_registry_wire_snapshot **out);
size_t mc_registry_wire_snapshot_size(const mc_registry_wire_snapshot *snapshot);
mc_registry_status mc_registry_wire_snapshot_at(const mc_registry_wire_snapshot *snapshot,
                                                size_t index, mc_registry_wire_position *out);
void mc_registry_wire_snapshot_release(mc_registry_wire_snapshot *snapshot);

/* Inert borrowed projections, valid until the next mutation of this owner.
 * node_at follows actual bucket/next traversal. No writable links escape. */
mc_registry_status mc_registry_wire_set_inspect(const mc_registry_wire_set *set,
                                                mc_registry_wire_set_view *out);
mc_registry_status mc_registry_wire_set_node_at(const mc_registry_wire_set *set, size_t index,
                                                mc_registry_wire_node_view *out);
/* Final quiescent retirement before the permanent arena. No source callback. */
void mc_registry_wire_sets_retire(mc_registry_classes *classes);

#endif
