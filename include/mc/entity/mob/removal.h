#ifndef MC_ENTITY_MOB_REMOVAL_H
#define MC_ENTITY_MOB_REMOVAL_H

#include "mc/entity/mob/construction.h"
#include "mc/world/storage/status.h"

typedef struct mc_world_entity_error mc_world_entity_error;

/* Passive actual UW.K prefix. Receiver/current_child/publication borrow the
 * SAME actor-resident pending work. Checked cleanup expires those views in
 * every earlier copy. Current-child and publication borrows also expire when
 * that iteration completes; an older copy is never current close authority.
 * Integer identities acquire no occurrence. Constructor and pose diagnostics
 * describe actual returned calls, never a replay plan. */
typedef struct mc_slime_removal_state {
    bool entered, pending, active, returned, receiver_held;
    mc_entity_status status, cleanup_status;
    const char *operation, *cleanup_operation;
    bool native_policy, source_exception, source_error, source_proven, source_unresolved;
    const char *exception_class;
    mc_mob_site site, child;
    uint16_t removal_pc, watcher_pc;
    int32_t watcher_index, size, health, index;
    bool size_read, world_b_read, world_remote, health_read, be_stored;
    mc_entity *receiver, *current_child;
    uintptr_t receiver_identity, gate_world_identity;
    uintptr_t children[4], constructor_worlds[4], publication_worlds[4], randoms[4];
    float offset_x[4], offset_z[4], yaw[4];
    double x[4], y[4], z[4];
    bool inserted[4];
    uint8_t children_attempted, constructors_returned, sizes_returned, poses_returned;
    uint8_t publications_invoked, children_completed, children_released, parent_draws;
    bool child_allocated, constructor_invoked, constructor_returned;
    bool size_returned, pose_returned, publication_entered;
    bool publication_returned, publication_continued, publication_result;
    mc_world_storage_status publication_status;
    mc_mob_error mob;
    mc_entity_status pose_status;
    unsigned int pose_pc, position_pc;
    const mc_world_entity_error *publication;
} mc_slime_removal_state;

bool mc_slime_removal_inspect(const mc_entity *, mc_slime_removal_state *);

#endif
