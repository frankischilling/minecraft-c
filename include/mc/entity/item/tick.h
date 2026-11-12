#ifndef MC_ENTITY_ITEM_TICK_H
#define MC_ENTITY_ITEM_TICK_H

#include "mc/entity/movement/movement.h"
#include "mc/entity/tick/environment.h"
#include "mc/world/runtime/blocks.h"

typedef struct mc_item_entity_tick_limits {
    mc_entity_tick_limits environment;
    mc_entity_move_limits movement;
} mc_item_entity_tick_limits;

typedef enum mc_item_entity_tick_child {
    MC_ITEM_TICK_CHILD_NONE,
    MC_ITEM_TICK_CHILD_BASE,
    MC_ITEM_TICK_CHILD_MATERIAL,
    MC_ITEM_TICK_CHILD_SOLID,
    MC_ITEM_TICK_CHILD_MOVEMENT,
    MC_ITEM_TICK_CHILD_REGISTRY,
    MC_ITEM_TICK_CHILD_SOUND,
    MC_ITEM_TICK_CHILD_BLOCK,
    MC_ITEM_TICK_CHILD_EFFECT
} mc_item_entity_tick_child;

typedef struct mc_item_entity_tick_error {
    mc_entity_status status;
    const char *operation;
    unsigned int bytecode, super_bytecode, pushout_bytecode;
    mc_item_entity_tick_child child;

    union {
        mc_entity_tick_error base;
        mc_world_material_error material;
        mc_world_solid_support_error solid;
        mc_entity_move_error movement;
        mc_registry_error registry;
        mc_world_movement_error sound;
        mc_world_storage_error block;
        mc_entity_effect_error effect;
    } child_error;
} mc_item_entity_tick_error;

void mc_item_entity_tick_error_init(mc_item_entity_tick_error *);

/* Actual client hl.w_ on a genuinely held READY canonical Item. Executes
 * invokespecial sn.w_ -> inherited virtual sn.U first, including actual Item
 * k_ and damage/K dispatch, then the entire Item gravity/material/fizz,
 * six-neighbor inherited sn.c, mover, friction/bounce and aging/death body.
 * It does not read or validate the nullable Stack, insert/pick up the Item,
 * publish an Entity or supply a World tick, renderer or WorldAccess callback.
 * Genuine missing descendants fail at their reached call, keeping all prior
 * field, pool, Random, watcher and World effects. be does not skip the body.
 *
 * Code Registry is the caller's actual loaded namespace, independent of
 * nullable Item.world. Keep all owners/context/limits live and exclusive
 * through synchronous callbacks; native holds preserve storage, not values.
 * No constructor diagnostics, implicit pool reset, rollback or retry is added.
 * NULL limits choose existing independent environment/mover ceilings. Fresh
 * success is empty; read only the discriminator's active full failed child.
 * Error storage/limits are disjoint from every owner/input. FP admission is
 * delayed to reached strict x86-64 arithmetic. No server gameplay is claimed. */
mc_entity_status mc_item_entity_tick_in_context(mc_entity *, mc_registry_classes *,
                                                const mc_item_entity_tick_limits *,
                                                mc_item_entity_tick_error *);

#endif
