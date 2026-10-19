#ifndef MC_WORLD_SCHEDULE_MEMBERSHIP_H
#define MC_WORLD_SCHEDULE_MEMBERSHIP_H

#include "mc/world/schedule/entry.h"

#include <stddef.h>

/* Owns scheduled-update membership independently of ordered scheduling.
 * World scheduling, metadata, callbacks and entropy have separate owners. */
typedef struct mc_scheduled_membership mc_scheduled_membership;

typedef enum mc_scheduled_membership_status {
    MC_SCHEDULED_MEMBERSHIP_OK = 0,
    MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT,
    MC_SCHEDULED_MEMBERSHIP_ALLOCATION_FAILED,
    MC_SCHEDULED_MEMBERSHIP_LIMIT,
    MC_SCHEDULED_MEMBERSHIP_HOLD_LIMIT
} mc_scheduled_membership_status;

const char *mc_scheduled_membership_status_string(mc_scheduled_membership_status status);

/* One native owning collection, initially empty. Explicit max_entries is in
 * 0..INT32_MAX so original size() remains an exact signed int. Larger limits
 * are INVALID_ARGUMENT, not clamped. Insufficient own-byte budget is LIMIT.
 * max_allocation_bytes bounds simultaneous requested owner+node+bucket bytes,
 * including OLD and NEW bucket arrays plus a staged node during growth.
 * Existing tick allocations, E/world/other owners, allocator overhead and C
 * stack are excluded; their external lifetimes/budgets remain caller-owned.
 * Native allocation/hold limits are not Java HashSet OOME interleaving models.
 *
 * out is required and unchanged on failure. Success publishes one unique owner;
 * the output slot must not already own another live collection. There is no
 * owning copy/reset operation. destroy(NULL) is safe. Destroy drops ONLY this
 * collection's one tick hold per retained entry, never E/caller holds or fields. */
mc_scheduled_membership_status mc_scheduled_membership_create(size_t max_entries,
                                                              size_t max_allocation_bytes,
                                                              mc_scheduled_membership **out);
void mc_scheduled_membership_destroy(mc_scheduled_membership *membership);

/* Exclusive synchronous access is required; no concurrent/fail-fast iterator,
 * arbitrary Object/null key, stale-pointer check or reentrant callback exists.
 * Owners must be valid/live. Query/candidate must have an independent caller
 * hold through the call, even when it is the SAME retained tick. Outputs must
 * be disjoint from each other and all collection/record/input storage.
 * Null membership/query/required output is INVALID_ARGUMENT, out unchanged.
 *
 * Equality is exact full x/y/z/block. Time/sequence/hash alone are not keys.
 * Immutable record keys make cached original hashes stable; changing due time
 * never moves membership. Full equality is checked within hash collisions.
 * Contains/size/native_bytes allocate nothing and do not mutate state. */
mc_scheduled_membership_status
mc_scheduled_membership_contains(const mc_scheduled_membership *membership,
                                 const mc_scheduled_tick *query, bool *out);
mc_scheduled_membership_status
mc_scheduled_membership_size(const mc_scheduled_membership *membership, int32_t *out);
mc_scheduled_membership_status
mc_scheduled_membership_native_bytes(const mc_scheduled_membership *membership, size_t *out);

/* Native success is separate from the original bool result. Duplicate add is
 * OK,false, allocates/retains NOTHING, and keeps the originally stored SAME
 * owner/time/sequence. Unique add takes one hold on the supplied owner; all
 * allocations and retain complete before infallible rehash/link publication.
 * Any failure frees staged state and preserves membership/capacity/holds/out.
 * HOLD_LIMIT identifies a failed tick retain; LIMIT is collection capacity or
 * own-allocation admission. No sequence construction/rollback occurs here.
 *
 * Remove accepts an equal DISTINCT query and releases the stored owner's F
 * hold, not the query's caller hold. OK,false means absent. Removal allocates
 * nothing, preserves bucket capacity and changes no tick fields. Remaining E
 * and independent caller holds continue to protect that same stored identity.
 * Caller executes contains -> F.add -> E.add and E.remove -> F.remove at the
 * original separate points. This API never reserves/repairs/rolls back E. */
mc_scheduled_membership_status mc_scheduled_membership_add(mc_scheduled_membership *membership,
                                                           mc_scheduled_tick *tick, bool *added);
mc_scheduled_membership_status mc_scheduled_membership_remove(mc_scheduled_membership *membership,
                                                              const mc_scheduled_tick *query,
                                                              bool *removed);

/* All retained SAME owners receive the same Java-wrapped i64 delta exactly
 * once. After the initial null check there are no allocations, quota/hold
 * checks, callbacks or failure boundaries inside the loop. Private traversal
 * order is unobservable in this valid exclusive domain. No E reindex/resort,
 * metadata write, record sequence change or membership operation occurs.
 * Server caller computes wrapped newTime-oldTime BEFORE this operation and
 * sets world metadata to newTime only AFTER all record updates complete. */
mc_scheduled_membership_status
mc_scheduled_membership_shift_time(mc_scheduled_membership *membership, int64_t delta);

#endif
