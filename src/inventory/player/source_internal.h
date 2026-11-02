#ifndef MC_INVENTORY_PLAYER_SOURCE_INTERNAL_H
#define MC_INVENTORY_PLAYER_SOURCE_INTERNAL_H

#include "mc/inventory/player.h"

/* Actual client IX.a(I,IZ)V over its constructor-owned main36/armor4 arrays.
 * Capture main, read its length7 even for a negative slot, subtract that
 * length14 before capturing armor, then null/bounds-check the selected store24.
 * Native owner/active/side refusals have no source frame; the containing GS
 * invocation owns a null Inventory receiver. Source array-null/index failures
 * have one actual IX.a(ILiz;)V frame. Error output is optional and disjoint.
 *
 * Owners remain live/exclusive. NULL and RAW Stacks are source-valid stores;
 * a checked native hold precedes replacing/releasing the old owning occurrence,
 * including self-alias stores. No allocation, changed flag, count clamp,
 * constructor-phase repair or Player/Container callback is introduced. Earlier
 * effects are preserved on failure. Server source dispatch remains separate. */
mc_inventory_status mc_inventory_player_source_set(mc_inventory_player *, int32_t slot,
                                                   mc_item_stack *value, mc_inventory_error *error);

#endif
