#ifndef MC_INVENTORY_RUNTIME_TICK_INTERNAL_H
#define MC_INVENTORY_RUNTIME_TICK_INTERNAL_H

#include "../../block/registry/registry_internal.h"
#include "../../entity/entity_internal.h"
#include "../../item/registry/registry_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "mc/inventory/runtime/tick.h"

typedef struct mc_inventory_tick_scope {
    mc_registry_classes *classes;
    mc_inventory_tick_limits limits;
    bool registry_held;
} mc_inventory_tick_scope;

bool mc_inventory_tick_fail(mc_inventory_tick_error *, mc_inventory_tick_status, const char *);
bool mc_inventory_tick_world_result(mc_inventory_tick_error *, mc_world_storage_status,
                                    const char *);
bool mc_inventory_tick_stack_result(mc_inventory_tick_error *, mc_item_stack_status, const char *);
bool mc_inventory_tick_map_lookup(mc_inventory_tick_scope *, mc_item_stack *, mc_world_runtime *,
                                  mc_saved_data_value **, mc_inventory_tick_error *);
bool mc_inventory_tick_map_update(mc_inventory_tick_scope *, mc_item_stack *, mc_world_runtime *,
                                  mc_entity *, bool, mc_inventory_tick_error *);

bool mc_inventory_tick_is_player(mc_entity *, bool *, mc_inventory_tick_error *);
#endif
