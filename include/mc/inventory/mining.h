#ifndef MC_INVENTORY_MINING_H
#define MC_INVENTORY_MINING_H

#include "mc/inventory/player.h"

/* Actual client ix.a(Block) speed and ix.b(Block) harvest predicates. These
 * read the live selected cell in their distinct source order. Empty cells use
 * the original speed one / harvest false; Material.i returns harvest true
 * before reading selected or either array. Nonempty cells resolve the actual
 * Item through the supplied client code Registry and invoke its concrete
 * speed/harvest override. An unused Registry/Block may be NULL. Source fields,
 * count, damage and phase remain unchanged; outputs are unchanged on failure. */
mc_inventory_status mc_inventory_player_mining_speed(mc_inventory_player *, mc_registry_classes *,
                                                     const mc_block *, float *,
                                                     mc_inventory_error *);
mc_inventory_status mc_inventory_player_can_harvest(mc_inventory_player *, mc_registry_classes *,
                                                    const mc_block *, bool *, mc_inventory_error *);

#endif
