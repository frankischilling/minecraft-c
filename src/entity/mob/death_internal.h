#ifndef MC_ENTITY_MOB_DEATH_INTERNAL_H
#define MC_ENTITY_MOB_DEATH_INTERNAL_H

#include "mc/entity/mob/death.h"
#include "mc/entity/movement/effects.h"

/* Actual LS.b(SN)V entered at the enclosing LS.a(SN,int) invoke316.
 * Canonical client animals, Monsters, Slime, Squid and Ghast; the facade already holds the
 * receiver. Allocate actor-resident pending work only at this reached entry.
 * Work owns an independent receiver occurrence and admits/retains nonnull
 * source only at its first reached callback17/26. Registry/World/Random are
 * inspected only at their actual uses. Error is mandatory scratch/value
 * storage, not the work owner; NULL public effect output remains safe.
 * Success retires native children and preserves passive error.death receipts.
 * Failure leaves all remaining work on actor->mob->pending_death. Source work
 * is never replayed by cleanup, and published Items are never removed. */
bool mc_mob_death_entered(mc_entity *, mc_registry_classes *, mc_entity *source,
                          mc_entity_effect_error *);

/* Native checked cleanup, including source failure prefixes. Active work or
 * child retirement refusal preserves the same pending slot and source cause.
 * Empty work succeeds. Caller keeps the whole graph exclusive until cleanup
 * returns; the work's final receiver self-hold may be its last occurrence.
 * Error may be NULL; a provided value receives cleanup state, never ownership.
 * Borrowed failed-drop views expire when their call is consumed. */
mc_entity_status mc_mob_death_cleanup(mc_entity *, mc_entity_effect_error *);

/* Passive current pending-work view, including active synchronous callbacks.
 * No pending work or invalid arguments returns false and preserves output. */
bool mc_mob_death_inspect(const mc_entity *, mc_mob_death_state *);

#endif
