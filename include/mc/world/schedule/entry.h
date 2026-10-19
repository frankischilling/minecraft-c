#ifndef MC_WORLD_SCHEDULE_ENTRY_H
#define MC_WORLD_SCHEDULE_ENTRY_H

#include <stdbool.h>
#include <stdint.h>

/* Scheduled update record. World, collection and entropy ownership are separate.
 * The original record classes are client qy/server ki. */
typedef struct mc_scheduled_tick mc_scheduled_tick;

/* The future game/class-profile owner initializes next to zero exactly once,
 * corresponding to qy/ki.<clinit>. Share that same owner across worlds and
 * synchronous reentry. Never reset or copy it when creating a world/request.
 * Calls using this owner require exclusive access; no atomics are implicit. */
typedef struct mc_scheduled_tick_sequence {
    int64_t next;
} mc_scheduled_tick_sequence;

typedef struct mc_scheduled_tick_fields {
    int32_t x, y, z, block;
    int64_t due_time, sequence;
} mc_scheduled_tick_fields;

typedef enum mc_scheduled_tick_status {
    MC_SCHEDULED_TICK_OK = 0,
    MC_SCHEDULED_TICK_INVALID_ARGUMENT,
    MC_SCHEDULED_TICK_ALLOCATION_FAILED,
    MC_SCHEDULED_TICK_LIMIT
} mc_scheduled_tick_status;

/* Allocate the owner before consuming sequence. On success: due_time=0, one
 * native hold, update sequence->next by wrapped long +1, store its old value
 * into the record, then assign x/y/z/block in original order and publish out.
 * Null sequence/out or allocation failure preserves both sequence and out.
 * Every signed key/time/sequence value is accepted; no coordinate/ID mask,
 * world presence, immediate-mode or duplicate gate belongs in this module. */
mc_scheduled_tick_status mc_scheduled_tick_create(mc_scheduled_tick_sequence *sequence, int32_t x,
                                                  int32_t y, int32_t z, int32_t block,
                                                  mc_scheduled_tick **out);

/* Native holds are separate from original game fields and collection identity.
 * Retain fails unchanged at SIZE_MAX. release(NULL) is safe; every nonnull
 * release consumes one owned hold and may free the owner. Do not subsequently
 * use a released pointer unless another valid hold was retained for that use.
 * No registration, callback, counter or collection mutation occurs here. */
mc_scheduled_tick_status mc_scheduled_tick_retain(mc_scheduled_tick *tick);
void mc_scheduled_tick_release(mc_scheduled_tick *tick);

/* All nonnull owners must be live, valid record owners with a held reference.
 * Access is exclusive and synchronous. Readonly receiver/other arguments may
 * refer to the same live owner. Mutable outputs and the sequence owner must
 * be disjoint from record storage and from each other.
 * Null receiver/required output is INVALID_ARGUMENT with outputs unchanged.
 * Snapshots copy all original fields; they expose no native ownership count.
 * Time changes never repair, remove or reinsert an ordered collection entry. */
mc_scheduled_tick_status mc_scheduled_tick_fields_get(const mc_scheduled_tick *tick,
                                                      mc_scheduled_tick_fields *out);
mc_scheduled_tick_status mc_scheduled_tick_time_set(mc_scheduled_tick *tick, int64_t time);
mc_scheduled_tick_status mc_scheduled_tick_time_shift(mc_scheduled_tick *tick, int64_t delta);

/* Equality uses full x/y/z/block only. other may be NULL (success, false);
 * otherwise it must be a live record; tick==other is valid and equals true.
 * This is not arbitrary Java Object or
 * subclass dispatch. Hash preserves the original Java wrapped expression.
 * Compare requires two nonnull records, returns exactly -1/0/+1 for signed
 * (due_time,sequence), and has no coordinate, pointer or type tie-breaker.
 * Comparing a live record with itself succeeds with zero. */
mc_scheduled_tick_status mc_scheduled_tick_equals(const mc_scheduled_tick *tick,
                                                  const mc_scheduled_tick *other, bool *out);
mc_scheduled_tick_status mc_scheduled_tick_hash(const mc_scheduled_tick *tick, int32_t *out);
mc_scheduled_tick_status mc_scheduled_tick_compare(const mc_scheduled_tick *tick,
                                                   const mc_scheduled_tick *other, int32_t *out);

#endif
