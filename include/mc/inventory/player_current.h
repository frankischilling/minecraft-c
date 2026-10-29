#ifndef MC_INVENTORY_PLAYER_CURRENT_H
#define MC_INVENTORY_PLAYER_CURRENT_H

#include "mc/inventory/player.h"

typedef struct mc_inventory_current_error {
    mc_inventory_status status;
    const char *operation;
    mc_inventory_site site;
    bool native_policy, source_entered, array_read, source_returned;
    int32_t selected;
} mc_inventory_current_error;

/* Actual client ix.b()Liz;: read selected, return NULL outside0..8 without
 * reading the main array; otherwise return the exact borrowed main[selected].
 * No retain, clone, count check, changed flag or notification is introduced.
 * Caller keeps inventory and returned Stack live/exclusive through every use.
 * Native owner/output/active/side admission preserves out. Source array-null
 * failure at24 also preserves out and reports its completed selected read.
 * A null native receiver is reported to the containing source caller, whose
 * invocation PC owns the Java null-receiver failure. Error is optional and
 * disjoint from owners/output. Server inventory dispatch remains separate. */
mc_inventory_status mc_inventory_player_current(mc_inventory_player *, const mc_item_stack **out,
                                                mc_inventory_current_error *);

#endif
