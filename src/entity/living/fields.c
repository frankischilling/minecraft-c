#include "fields_internal.h"

#include "../mob/internal.h"
#include "../player/player_internal.h"
#include "mc/core/java/java.h"

mc_living_entity_fields *mc_entity_living_fields(mc_entity *entity) {
    if (entity == NULL)
        return NULL;
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
        entity->local_player != NULL)
        return &entity->local_player->fields.living;
    if (mc_entity_has_mob(entity) && mc_entity_is_kind(entity, entity->class_literal->kind))
        return &entity->mob->living;
    return NULL;
}

bool mc_entity_living_heal_source(mc_entity *entity, int32_t amount) {
    mc_living_entity_fields *living = mc_entity_living_fields(entity);
    if (living == NULL)
        return false;
    if (living->Y <= 0)
        return true;
    living->Y = mc_java_i32_add(living->Y, amount);
    if (living->Y > 20)
        living->Y = 20;
    entity->base.by = living->E / 2;
    return true;
}
