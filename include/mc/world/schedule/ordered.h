#ifndef MC_WORLD_SCHEDULE_ORDERED_H
#define MC_WORLD_SCHEDULE_ORDERED_H

#include "mc/world/schedule/entry.h"

#include <stddef.h>

/* Ordered scheduled-update owner, separate from full-key membership F and
 * world scheduling. Preserves captured Temurin17 TreeSet/TreeMap structural
 * history while comparing CURRENT signed (due_time,sequence) record fields. */
typedef struct mc_scheduled_ordered mc_scheduled_ordered;

typedef enum mc_scheduled_ordered_status {
    MC_SCHEDULED_ORDERED_OK = 0,
    MC_SCHEDULED_ORDERED_INVALID_ARGUMENT,
    MC_SCHEDULED_ORDERED_ALLOCATION_FAILED,
    MC_SCHEDULED_ORDERED_LIMIT,
    MC_SCHEDULED_ORDERED_HOLD_LIMIT,
    MC_SCHEDULED_ORDERED_EMPTY
} mc_scheduled_ordered_status;

const char *mc_scheduled_ordered_status_string(mc_scheduled_ordered_status status);

/* Fresh, empty unique native owner. max_entries is a NODE limit in
 * 0..INT32_MAX; a larger configured limit is INVALID_ARGUMENT, never clamped.
 * max_allocation_bytes bounds requested owner+live nodes+one staged node.
 * Tick allocations, F/world/other owners, allocator overhead and C stack are
 * excluded. This native resource boundary does not reproduce JVM allocation,
 * superclass/bootstrap or exception-object failure interleavings.
 *
 * Required out is unchanged on failure and must not already own an unreleased
 * collection. No copy/reset operation is provided. destroy(NULL) is safe;
 * destroy drops one E hold PER NODE, including multiple nodes holding the SAME
 * tick pointer. F/caller holds and original record fields are never consumed. */
mc_scheduled_ordered_status mc_scheduled_ordered_create(size_t max_entries,
                                                        size_t max_allocation_bytes,
                                                        mc_scheduled_ordered **out);
void mc_scheduled_ordered_destroy(mc_scheduled_ordered *ordered);

/* Exclusive synchronous access and valid live owners are required. Candidates
 * and queries must have independent caller holds through their calls. Outputs
 * must be disjoint from all owner/record/input storage and each other. Null
 * owner/record/required output is INVALID_ARGUMENT, with outputs unchanged.
 * No callbacks, user comparators, concurrent/fail-fast iterator, arbitrary Java
 * Object key or stale-pointer detector is supplied. Size/native_bytes allocate
 * nothing; size is the exact admitted signed int node count. */
mc_scheduled_ordered_status mc_scheduled_ordered_size(const mc_scheduled_ordered *ordered,
                                                      int32_t *out);
mc_scheduled_ordered_status mc_scheduled_ordered_native_bytes(const mc_scheduled_ordered *ordered,
                                                              size_t *out);

/* Search CURRENT comparison from the historical root. Zero on that path means
 * OK,false, retaining the original key/hold/shape/count without allocation or
 * candidate retain, even at capacity. Full x/y/z/block and pointer identity are
 * NOT extra ordering/dedup criteria. Mutated trees are not repaired or scanned
 * globally: a path miss can admit another node holding the SAME tick pointer.
 *
 * A miss admits/allocates a staged node and retains the candidate before any
 * link/fixup. Failure frees staged state, preserving E, holds and added output.
 * Success links the SAME owner and applies the captured red-black case order.
 * LIMIT is collection admission; HOLD_LIMIT is record-retain overflow. Prior
 * F/sequence/world effects remain separate and are never rolled back here. */
mc_scheduled_ordered_status mc_scheduled_ordered_add(mc_scheduled_ordered *ordered,
                                                     mc_scheduled_tick *tick, bool *added);

/* Structural root-leftmost lookup WITHOUT comparison, removal or reordering.
 * Success publishes an independent caller hold on that SAME tick; out must not
 * already own an unreleased record. EMPTY is the original no-element exceptional
 * boundary, not successful null/false. HOLD_LIMIT reports native retain failure.
 * Both leave out/E unchanged. Keep the returned hold across E/F removals and
 * later world callback/reentry. Current keys may disagree with structural order. */
mc_scheduled_ordered_status mc_scheduled_ordered_first(const mc_scheduled_ordered *ordered,
                                                       mc_scheduled_tick **out);

/* Always search query's CURRENT comparison from root, even for the exact tick
 * returned by first. OK,false means path miss, even if that identity exists in
 * another branch. A comparison-zero match may be a DISTINCT stored identity.
 * No known-node/min deletion, cached key, identity shortcut, resort or repair.
 *
 * Successful deletion preserves captured successor-copy/color/fixup history.
 * A two-child delete transfers successor's existing node hold without retain;
 * exactly the found stored owner's one E hold is retired. Query/F/caller holds
 * remain independent. Remove allocates/retains nothing and does not mutate
 * record fields. Bool result is separate from native success. No combined F/E
 * transaction, metadata update, time shift, tick callback or iterator is added. */
mc_scheduled_ordered_status mc_scheduled_ordered_remove(mc_scheduled_ordered *ordered,
                                                        const mc_scheduled_tick *query,
                                                        bool *removed);

#endif
