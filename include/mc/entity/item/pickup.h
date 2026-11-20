#ifndef MC_ENTITY_ITEM_PICKUP_H
#define MC_ENTITY_ITEM_PICKUP_H

#include "mc/entity/player/pickup.h"
#include "mc/entity/player/statistics.h"
#include "mc/inventory/runtime/insertion.h"
#include "mc/world/runtime/movement.h"

typedef struct mc_item_pickup_error {
    mc_entity_status status;
    const char *operation, *source_class, *source_method;
    unsigned int bytecode, pickup_bytecode, notification_bytecode, statistic_bytecode;
    int32_t captured_count;
    bool insertion_returned, inserted;
    mc_item_stack_status stack;
    mc_registry_error registry;
    mc_inventory_insert_error insertion;
    mc_player_statistic_error statistic;
    mc_world_movement_error sound;
    mc_player_pickup_error notification;
} mc_item_pickup_error;

void mc_item_pickup_error_init(mc_item_pickup_error *);
/* Genuine client hl.b(gs) on an actual held READY Item; canonical LocalPlayer
 * is admitted only when source first reaches its Inventory field. Remote/delay
 * returns do not require a Player or initialized Registry. The Registry is
 * the actual borrowed code namespace, never a substituted World. Preserve
 * source mutable Stack/Inventory/static reads, insertion remainder, achievement
 * dispatch, both Entity Random draws and actual entity-sound overload.
 * Reached dc.b(Entity,int) constructs the actual em and enqueues it in the
 * captured real App.j manager. Its World comes from actual App.f, independently
 * of the Player's World. Item.K remains after the completed notification.
 * Native failures preserve every completed effect; output/errors/limits are
 * disjoint, and all owners/dependencies remain exclusive and live. */
mc_entity_status mc_item_entity_pickup(mc_entity *item, mc_entity *player,
                                       mc_registry_classes *code_registry,
                                       const mc_inventory_insert_limits *, mc_item_pickup_error *);

#endif
