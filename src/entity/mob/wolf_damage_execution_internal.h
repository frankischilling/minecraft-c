#ifndef MC_ENTITY_MOB_WOLF_DAMAGE_EXECUTION_INTERNAL_H
#define MC_ENTITY_MOB_WOLF_DAMAGE_EXECUTION_INTERNAL_H

#include "../movement/damage_execution_internal.h"

typedef struct mc_wolf_damage_execution_view {
    mc_world_runtime_execution_result *result;
    const mc_world_nearby_error *nearby;
    mc_entity_tick_execution_world_token world;
    mc_world_storage_status query_status;
    bool created, open, query_returned;
} mc_wolf_damage_execution_view;

bool mc_mob_wolf_damage_entered_in_execution(mc_entity *, mc_entity *, int32_t, bool *,
                                             mc_entity_effect_error *,
                                             mc_entity_damage_execution *);
bool mc_mob_wolf_damage_execution_inspect(mc_entity *, mc_wolf_damage_execution_view *);
mc_world_storage_status mc_mob_wolf_damage_native_close(mc_entity *);

#endif
