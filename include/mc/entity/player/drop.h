#ifndef MC_ENTITY_PLAYER_DROP_H
#define MC_ENTITY_PLAYER_DROP_H

#include "mc/core/math/classes.h"
#include "mc/entity/player/player.h"
#include "mc/inventory/player.h"

typedef struct mc_player_drop_call mc_player_drop_call;
typedef struct mc_world_entity_error mc_world_entity_error;
typedef struct mc_player_statistic_error mc_player_statistic_error;

typedef enum mc_player_drop_status {
    MC_PLAYER_DROP_OK,
    MC_PLAYER_DROP_ARGUMENT,
    MC_PLAYER_DROP_BUSY,
    MC_PLAYER_DROP_ALLOCATION,
    MC_PLAYER_DROP_LIMIT,
    MC_PLAYER_DROP_NULL_REFERENCE,
    MC_PLAYER_DROP_INVENTORY,
    MC_PLAYER_DROP_ENTITY,
    MC_PLAYER_DROP_MATH,
    MC_PLAYER_DROP_WORLD,
    MC_PLAYER_DROP_REGISTRY,
    MC_PLAYER_DROP_STATISTIC,
    MC_PLAYER_DROP_FLOATING,
    MC_PLAYER_DROP_CLEANUP
} mc_player_drop_status;

typedef struct mc_player_drop_error {
    mc_player_drop_status status;
    const char *operation, *exception_class;
    mc_player_site site, child;
    bool native_policy, source_exception, source_error;
    mc_inventory_error inventory;
    mc_entity_error entity;
    mc_math_class_error math;
    mc_registry_error registry;
    const mc_world_entity_error *publication;
    const mc_player_statistic_error *statistic;
} mc_player_drop_error;

typedef struct mc_player_drop_state {
    mc_entity *player, *item;
    mc_item_stack *stack;
    int32_t selected;
    bool selected_action, scatter, active, returned;
    bool inventory_split_returned, item_allocated, item_constructed, pickup_delay_stored;
    bool publication_entered, publication_returned, inserted, statistic_returned;
    size_t random_draws;
    float draws[4];
    double x, y, z, velocity_x, velocity_y, velocity_z;
    unsigned int source_pc;
    const mc_player_drop_error *error, *cleanup;
} mc_player_drop_state;

/* Actual client GS.D(): capture the selected Inventory slot, split one Item,
 * then invoke GS.a(Stack,false). The second entry executes that same actual
 * Stack overload, including its scatter arm. No World tick, inventory repair,
 * sound, swing or equip reset is introduced. NULL Stack returns before all
 * allocation, Math, World and statistic work.
 *
 * Registry and Math are the actual invoking class namespaces. Keep all input
 * owners and their dependency graphs live/exclusive. Reached calls own their
 * exact Player, split/provided Stack, constructed Item and World occurrences,
 * plus failed publication receipts, until checked retirement. Source failure
 * preserves earlier inventory removal, random draws and publication effects;
 * cleanup never rolls them back or replays the action. Output starts NULL and
 * is disjoint from every owner/error. Allocation/admission failure preserves it.
 * Borrowed state/child views expire on successful retirement. Native binary64
 * sin/cos execute real platform libm; original-JDK bit parity is separate. */
mc_player_drop_status mc_local_player_drop_selected_begin(mc_registry_classes *, mc_math_classes *,
                                                          mc_entity *player,
                                                          mc_player_drop_call **out,
                                                          mc_player_drop_error *);
mc_player_drop_status mc_local_player_drop_stack_begin(mc_registry_classes *, mc_math_classes *,
                                                       mc_entity *player, mc_item_stack *,
                                                       bool scatter, mc_player_drop_call **out,
                                                       mc_player_drop_error *);
bool mc_local_player_drop_inspect(const mc_player_drop_call *, mc_player_drop_state *);
/* Failure preserves the same remaining owner slots for retry. */
mc_player_drop_status mc_local_player_drop_retire(mc_player_drop_call **, mc_player_drop_error *);

#endif
