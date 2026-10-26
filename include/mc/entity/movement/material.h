#ifndef MC_ENTITY_MOVEMENT_MATERIAL_H
#define MC_ENTITY_MOVEMENT_MATERIAL_H
#include "mc/world/runtime/material_movement.h"

typedef struct mc_entity_material_error {
    mc_entity_status status;
    const char *operation;
    unsigned int bytecode;
    mc_world_storage_error world;
    mc_registry_error registry;
} mc_entity_material_error;

/* Exact inherited sn/lq.a(Material) eye immersion with actual virtual w/t.
 * Admits held READY canonical Item, Falling, Minecart and client LocalPlayer;
 * the first three inherit zero eye offset, LocalPlayer inherits gs.w=0.12f.
 * Unsupported concrete owners fail explicitly. This is not Player tick or
 * Living movement. Uses the same Entity's position and nullable World, actual
 * live Block/Material identities, floor->float->floor Y, any-material fluid
 * height and strict boundary/NaN comparison. Preserve out on native failure;
 * no velocity/position mutation. Keep owners exclusive and dependencies live;
 * disjoint output/errors. Each actual World receiver is held across its
 * descendant callbacks. Metadata reloads Entity.world after block49 at aI73;
 * a changed nonnull binding must use the same Entity/Registry namespace and
 * proper native ownership. This adds no public Entity transfer operation.
 * Strict x86-64 FP is checked at eye arithmetic9 and again before rp.d85 after
 * effectful metadata82. Unsupported FP preserves output and all prior World
 * effects; no FP environment reset or successful truncated scan occurs. */
mc_entity_status mc_entity_inside_material(mc_entity *, mc_material *, bool *,
                                           mc_entity_material_error *);
#endif
