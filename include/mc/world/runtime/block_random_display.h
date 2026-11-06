#ifndef MC_WORLD_RUNTIME_BLOCK_RANDOM_DISPLAY_H
#define MC_WORLD_RUNTIME_BLOCK_RANDOM_DISPLAY_H

#include "mc/world/runtime/step.h"

typedef struct mc_world_random_display_block_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_world_random_display_block_site;

typedef enum mc_world_random_display_block_child {
    MC_WORLD_RANDOM_DISPLAY_BLOCK_NONE,
    MC_WORLD_RANDOM_DISPLAY_BLOCK_QUERY,
    MC_WORLD_RANDOM_DISPLAY_BLOCK_MATERIAL,
    MC_WORLD_RANDOM_DISPLAY_BLOCK_SUPPORT,
    MC_WORLD_RANDOM_DISPLAY_BLOCK_PRESENTATION,
    MC_WORLD_RANDOM_DISPLAY_BLOCK_STEP
} mc_world_random_display_block_child;

typedef struct mc_world_random_display_block_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    bool native_policy;
    mc_world_random_display_block_site site, child_site;
    const mc_block *block;
    size_t wrapped_calls;
    mc_registry_error registry;
    mc_world_storage_error cause;
    mc_world_random_display_block_child child;

    /* One reached child owns the diagnostic storage. Callback failures borrow
     * their actual producer receipt; an enclosing retained operation must
     * capture its own occurrence before that producer can be retired. */
    union {
        mc_world_collision_error query;

        struct {
            mc_world_material_error error;
            mc_world_collision_error access;
        } material;

        struct {
            mc_world_solid_support_error error;
            mc_world_nearby_error access;
        } support;

        mc_world_mutation_error presentation;
        mc_world_step_error step;
    } detail;
} mc_world_random_display_block_error;

void mc_world_random_display_block_error_init(mc_world_random_display_block_error *);

/* Actual client Block.b(World,III,Random)V, including every current concrete
 * kind's source inheritance. This operation never invokes simulation a(...),
 * consults Block.n, or substitutes World.r for the supplied Random. BS.i alone
 * captures the actual World Random as its source requires.
 *
 * Keep Block/Registry, nullable World and Random live and exclusive through
 * synchronous acquisition and callbacks. World/Random NULL is tested only at
 * the selected body's reached dereference. Exact source prefixes and callback
 * failures survive; no renderer particle-constructor barrier is suppressed.
 * SS uses actual wrapped receivers and a native ceiling of 1048576 delegates,
 * reporting the pending invocation when that ceiling is reached. Diagnostics
 * are caller-owned/disjoint; use heap storage for this large tagged error.
 * error may be NULL. A returned error never owns a callback receipt. */
mc_world_storage_status
mc_world_runtime_block_random_display(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, mc_java_random *random,
                                      mc_world_random_display_block_error *error);

#endif
