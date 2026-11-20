#ifndef MC_WORLD_RUNTIME_SPAWN_POSTSPAWN_INTERNAL_H
#define MC_WORLD_RUNTIME_SPAWN_POSTSPAWN_INTERNAL_H

#include "../../entity/construction_internal.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/mount/mount.h"
#include "mc/world/runtime/entities.h"

typedef enum mc_world_spawn_postspawn_child {
    MC_WORLD_POSTSPAWN_NONE,
    MC_WORLD_POSTSPAWN_RANDOM,
    MC_WORLD_POSTSPAWN_CLASS,
    MC_WORLD_POSTSPAWN_MOB,
    MC_WORLD_POSTSPAWN_POSITION,
    MC_WORLD_POSTSPAWN_PUBLICATION,
    MC_WORLD_POSTSPAWN_MOUNT
} mc_world_spawn_postspawn_child;

/* Internal caller-owned storage for the actual CQ.a(World,Living,FFF) body.
 * The parent keeps its World, defining Registry and actor live/exclusive until
 * checked retirement. The jockey is one owned occurrence; a parent may move
 * that occurrence out by clearing jockey after successful completion. Source
 * relationships/publications retain their separate actual occurrences.
 * Never copy owning work, overwrite it or begin again before retirement. */
typedef struct mc_world_spawn_postspawn_work {
    bool entered, active, returned;
    unsigned int pc;
    size_t random_calls, publications;
    mc_entity *jockey;
    mc_world_storage_status status;
    const char *operation, *exception_class;
    bool native_policy;
    mc_world_spawn_postspawn_child child;
    mc_mob_error mob;
    mc_entity_position_rotation_error position;
    mc_world_entity_error publication;
    mc_world_access_failure retained_publication;
    bool capture_pending;
    mc_world_storage_error capture_error;
    mc_entity_mount_error mount;
} mc_world_spawn_postspawn_work;

mc_world_storage_status mc_world_spawn_postspawn_begin(mc_world_runtime *, mc_registry_classes *,
                                                       mc_entity *, float x, float y, float z,
                                                       mc_world_spawn_postspawn_work *,
                                                       mc_world_storage_error *admission);
mc_world_storage_status mc_world_spawn_postspawn_retire(mc_world_spawn_postspawn_work *,
                                                        mc_world_storage_error *cleanup);

#endif
