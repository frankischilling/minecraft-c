#ifndef MC_ENTITY_MOUNT_MOUNT_H
#define MC_ENTITY_MOUNT_MOUNT_H

#include "mc/entity/entity.h"

typedef struct mc_entity_mount_error {
    mc_entity_status status;
    const char *operation;
    unsigned int bytecode, position_bytecode;
    bool source_null, native_policy;
} mc_entity_mount_error;

/* Actual client SN.i(Entity), inherited by Skeleton and Spider. All arguments
 * and supplied scope actors are live/exclusive genuine owners through return.
 * Existing native mount/passenger occurrences prove their own lifetime; an
 * authored borrowed link must occur in this explicit independently live scope.
 * Reached source stores survive failure. No World, Random, tick or callback is
 * introduced. Source aH/aG stores retain separate native occurrences, including
 * cycles; ordinary dismount/replacement or checked World terminal retirement
 * consumes those holds. Caller must dismount isolated actors before releasing
 * their last external roots. Null target executes the original dismount arm. */
mc_entity_status mc_entity_mount(mc_entity *rider, mc_entity *target, mc_entity *const *scope,
                                 size_t scope_count, mc_entity_mount_error *error);

#endif
