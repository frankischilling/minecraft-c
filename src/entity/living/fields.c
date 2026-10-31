#include "fields_internal.h"

#include "../mob/internal.h"
#include "../player/player_internal.h"

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
