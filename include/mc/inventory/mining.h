#ifndef MC_INVENTORY_MINING_H
#define MC_INVENTORY_MINING_H

#include "mc/inventory/player.h"

/* Actual client ix.a(Block) speed and ix.b(Block) harvest predicates. These
 * read the live selected cell in their distinct source order. Empty cells use
 * the original speed one / harvest false; Material.i returns harvest true
 * before reading selected or either array. Nonempty Stack virtuals remain
 * explicit reached dependencies. Outputs are unchanged on failure. */
mc_inventory_status mc_inventory_player_mining_speed(mc_inventory_player *, const mc_block *,
                                                     float *, mc_inventory_error *);
mc_inventory_status mc_inventory_player_can_harvest(mc_inventory_player *, const mc_block *, bool *,
                                                    mc_inventory_error *);

#endif
