#include "player_ally_retire_adapter.h"

#include "../../src/entity/entity_internal.h"
#include "../../src/entity/player/ally_alert_occurrences_internal.h"
#include "../../src/world/runtime/runtime_internal.h"

/* Exact former allocation-free validator. Only this native validation leaf
 * differs; begin/retire, watchers and every linked core object are shared. */
bool mc_player_ally_alert_occurrences_valid(const mc_player_ally_alert_state *s, bool player_held,
                                            bool target_held) {
    if ((player_held && s->player->holds < 1u + (size_t)(s->player == s->target)) ||
        (target_held && s->target->holds < 1u + (size_t)(s->target == s->player)))
        return false;
    if (s->wolves == NULL)
        return true;
    size_t count;
    if (mc_world_class_entity_list_count(s->wolves, &count) != MC_WORLD_STORAGE_OK ||
        s->world->local_leases == 0)
        return false;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *entity = NULL;
        if (mc_world_class_entity_list_get(s->wolves, i, &entity) != MC_WORLD_STORAGE_OK)
            return false;
        size_t holds = (size_t)(player_held && s->player == entity) +
                       (size_t)(target_held && s->target == entity);
        for (size_t j = 0; j < count; ++j) {
            mc_entity *other = NULL;
            if (mc_world_class_entity_list_get(s->wolves, j, &other) != MC_WORLD_STORAGE_OK)
                return false;
            if (other == entity)
                ++holds;
        }
        if (entity->holds < holds)
            return false;
    }
    return true;
}

const char *mc_benchmark_ally_retire_adapter(void) {
    return "quadratic";
}

size_t mc_benchmark_ally_retire_failed_allocations(void) {
    return 0;
}
