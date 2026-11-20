#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_IDENTITY_SET_INTERNAL_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_IDENTITY_SET_INTERNAL_H

#include "identity_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_tile_identity_set mc_tile_identity_set;
typedef struct mc_tile_identity_set_snapshot mc_tile_identity_set_snapshot;

typedef enum mc_tile_identity_set_status {
    MC_TILE_IDENTITY_SET_OK = 0,
    MC_TILE_IDENTITY_SET_INVALID_ARGUMENT,
    MC_TILE_IDENTITY_SET_NULL_COLLECTION,
    MC_TILE_IDENTITY_SET_LIMIT,
    MC_TILE_IDENTITY_SET_ALLOCATION_FAILED,
    MC_TILE_IDENTITY_SET_TILE_FAILED,
    MC_TILE_IDENTITY_SET_BUSY
} mc_tile_identity_set_status;

typedef struct mc_tile_identity_set_error {
    mc_tile_identity_set_status status;
    const char *operation;
    mc_tile_entity_status tile_status;
    size_t index;     /* SIZE_MAX outside a reached collection/snapshot element. */
    size_t completed; /* Completed earlier bulk elements, never rolled back. */
    bool changed_prefix;
} mc_tile_identity_set_error;

typedef struct mc_tile_identity_set_limits {
    size_t max_entries;
    size_t max_retained_bytes;
    size_t max_allocation_bytes;
} mc_tile_identity_set_limits;

typedef struct mc_tile_identity_set_allocator {
    void *context;
    void *(*allocate)(void *context, size_t bytes);
    void (*deallocate)(void *context, void *storage);
} mc_tile_identity_set_allocator;

/* An admitted actual ArrayList's fresh complete element span. The caller keeps
 * its nullable elements independently held and its backing stable throughout
 * the call; duplicates contribute to count. This is not a generic Collection
 * callback seam. A NULL span pointer denotes the actual null Collection;
 * items==NULL is valid only for an actual empty span. */
typedef struct mc_tile_identity_set_span {
    mc_tile_entity *const *items;
    size_t count;
} mc_tile_identity_set_span;

typedef struct mc_tile_identity_set_view {
    size_t size;
    uint32_t capacity, threshold, mod_count; /* mod_count is wrapping Java-int bits. */
    size_t retained_bytes, peak_bytes;       /* Peak includes successful temporary reservations. */
    size_t snapshot_count, live_snapshots, failed_snapshots, snapshot_bytes;
} mc_tile_identity_set_view;

typedef struct mc_tile_identity_set_snapshot_view {
    mc_tile_entity *const *items;
    size_t count;
} mc_tile_identity_set_snapshot_view;

/* Defaults: 65536 entries, 8 MiB currently owned requested bytes, 16 MiB peak
 * reservation. Owner, current table, nodes and all attached snapshots count.
 * Borrowed Tile/Class/World graphs and allocator overhead do not. One add may
 * reserve its node and replacement tables transactionally AFTER hashing; this
 * is a native allocation policy, not a Java OOM-prefix claim. An arena with no
 * deallocate must enforce its cumulative footprint separately, as released
 * logical storage remains with that caller. max_entries is at most INT32_MAX.
 * A copied allocator/context must outlive the Set and its snapshots. allocate
 * may return NULL; nonnull storage is suitably aligned. deallocate cannot fail
 * and never receives NULL. Callbacks cannot reenter or mutate owners/keys/
 * Thread. Admitted internal key comparison cannot fail. Release a previous
 * owner before reusing its output slot. */
mc_tile_identity_set_limits mc_tile_identity_set_default_limits(void);
const char *mc_tile_identity_set_status_string(mc_tile_identity_set_status status);
mc_tile_identity_set_status mc_tile_identity_set_create(const mc_tile_identity_set_limits *limits,
                                                        mc_tile_identity_set **out,
                                                        mc_tile_identity_set_error *error);
mc_tile_identity_set_status mc_tile_identity_set_create_with_allocator(
    const mc_tile_identity_set_limits *limits, const mc_tile_identity_set_allocator *allocator,
    mc_tile_identity_set **out, mc_tile_identity_set_error *error);

/* Actual nullable Tile keys only, pointer== equality, no Comparable keys.
 * Nonnull keys are held actual owners in RAW/READY/failed construction phases.
 * The current logical Thread is explicit, shared above Sets/Worlds; never
 * inferred from mutable Tile.world or created per operation. It must be an
 * admitted nonnull Thread whenever a nonnull-key hash is actually reached.
 * NULL hashes to0 without Thread access. Contains with no allocated table
 * skips hashing entirely; an allocated empty table still hashes. Remove always
 * hashes. Hash effects precede node/table/budget/retain failures and survive.
 * Native invalid arguments preserve out. Successful duplicate add/absent
 * remove return changed=false. No operation initializes a Source class.
 * All owners, inputs, outputs and errors require exclusive/disjoint storage;
 * callbacks cannot mutate keys, class identity or Thread. Dangling/synthetic
 * owners and concurrent access are not admitted. */
mc_tile_identity_set_status mc_tile_identity_set_add(mc_tile_identity_set *set,
                                                     mc_tile_entity *nullable_key,
                                                     mc_java_identity_thread *current_thread,
                                                     bool *changed,
                                                     mc_tile_identity_set_error *error);
mc_tile_identity_set_status mc_tile_identity_set_contains(const mc_tile_identity_set *set,
                                                          mc_tile_entity *nullable_key,
                                                          mc_java_identity_thread *current_thread,
                                                          bool *out,
                                                          mc_tile_identity_set_error *error);
mc_tile_identity_set_status mc_tile_identity_set_remove(mc_tile_identity_set *set,
                                                        mc_tile_entity *nullable_key,
                                                        mc_java_identity_thread *current_thread,
                                                        bool *changed,
                                                        mc_tile_identity_set_error *error);

/* Each call consumes the actual encounter order; earlier completed elements
 * remain on failure and scalar out is preserved. removeAll uses strict
 * this.size>other.size; equal size takes the this-iterator branch. Ordinary
 * removes use movable=true, iterator removal uses the stored hash and
 * movable=false. ArrayList.contains is linear identity comparison, no hash.
 * NULL collection is rejected before the size branch, including empty Sets. */
mc_tile_identity_set_status
mc_tile_identity_set_add_all_span(mc_tile_identity_set *set, const mc_tile_identity_set_span *span,
                                  mc_java_identity_thread *current_thread, bool *changed,
                                  mc_tile_identity_set_error *error);
mc_tile_identity_set_status
mc_tile_identity_set_remove_all_set(mc_tile_identity_set *set, const mc_tile_identity_set *other,
                                    mc_java_identity_thread *current_thread, bool *changed,
                                    mc_tile_identity_set_error *error);
mc_tile_identity_set_status mc_tile_identity_set_remove_all_span(
    mc_tile_identity_set *set, const mc_tile_identity_set_span *span,
    mc_java_identity_thread *current_thread, bool *changed, mc_tile_identity_set_error *error);

/* Complete HashSet.toArray table/next order, including NULL. The snapshot owns
 * one independent hold per nonnull occurrence and blocks Set retirement.
 * Even an empty result owns a snapshot descriptor. No hashing occurs.
 * A native hold failure leaves an unpublished partial child attached to the
 * Set until checked Set retirement; out is preserved and bytes remain counted.
 * View is borrowed until checked snapshot retirement, with no extra hold. */
mc_tile_identity_set_status mc_tile_identity_set_to_array(mc_tile_identity_set *set,
                                                          mc_tile_identity_set_snapshot **out,
                                                          mc_tile_identity_set_error *error);
mc_tile_identity_set_status
mc_tile_identity_set_snapshot_inspect(const mc_tile_identity_set_snapshot *snapshot,
                                      mc_tile_identity_set_snapshot_view *out,
                                      mc_tile_identity_set_error *error);
mc_tile_identity_set_status
mc_tile_identity_set_snapshot_retire(mc_tile_identity_set_snapshot **slot,
                                     mc_tile_identity_set_error *error);
mc_tile_identity_set_status mc_tile_identity_set_inspect(const mc_tile_identity_set *set,
                                                         mc_tile_identity_set_view *out,
                                                         mc_tile_identity_set_error *error);

/* Explicit native retirement only, never Java clear/replay. NULL *slot is
 * harmless; NULL slot is invalid. BUSY preserves every owner while any
 * published snapshot remains. Otherwise release all nodes/failed children,
 * tables and owner, then clear *slot. One node owns exactly one nonnull Tile
 * hold. Native arena release may keep storage with the caller. */
mc_tile_identity_set_status mc_tile_identity_set_retire(mc_tile_identity_set **slot,
                                                        mc_tile_identity_set_error *error);

#endif
