#ifndef MC_ENTITY_PLAYER_MINING_H
#define MC_ENTITY_PLAYER_MINING_H

#include "mc/entity/movement/material.h"
#include "mc/entity/player/player.h"

typedef struct mc_player_mining_error {
    mc_entity_status status;
    const char *operation;
    mc_player_site site;
    bool native_policy, returned, water, airborne, harvest;
    float speed, strength;
    mc_inventory_error inventory;
    mc_registry_error registry;
    mc_entity_material_error material;
} mc_player_mining_error;

/* Actual inherited gs.J swing stores and gs.a/b(Block) mining leaves on the
 * canonical LocalPlayer. Native Entity holds span synchronous child callbacks.
 * Negative Block hardness returns zero before dereferencing Player. Remaining
 * strength work uses the live Material/Inventory/eye-immersion/on-ground fields,
 * with binary32 divisions in source order. Keep the Registry graph exclusive;
 * no saved state, selected Stack, World or field is replaced. */
mc_entity_status mc_local_player_swing(mc_entity *, mc_player_mining_error *);
mc_entity_status mc_local_player_mining_speed(mc_entity *, mc_registry_classes *, const mc_block *,
                                              float *, mc_player_mining_error *);
mc_entity_status mc_local_player_can_harvest(mc_entity *, const mc_block *, bool *,
                                             mc_player_mining_error *);
mc_entity_status mc_block_player_strength(const mc_block *, mc_entity *, float *,
                                          mc_player_mining_error *);

#endif
