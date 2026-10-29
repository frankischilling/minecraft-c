#include "mc/entity/appearance.h"
#include "player_internal.h"

static mc_player_status appearance_hold_error(mc_player_error *error, mc_entity_status status) {
    if (error != NULL)
        error->entity =
            (mc_entity_error){.status = status, .operation = "appearance native Entity occurrence"};
    return mc_player_fail(error, MC_PLAYER_ENTITY_FAILED, "appearance native Entity occurrence",
                          NULL, NULL, NULL, 0);
}

mc_player_status mc_entity_update_cloak(mc_entity *entity, mc_player_error *error) {
    mc_player_error_clear(error);
    if (entity == NULL)
        return mc_player_fail(error, MC_PLAYER_NULL_REFERENCE, "appearance null Entity receiver",
                              NULL, NULL, NULL, 0);
    if (entity->classes->source != MC_ENTITY_CLIENT)
        return mc_player_fail(error, MC_PLAYER_INVALID_ARGUMENT, "appearance client namespace",
                              NULL, NULL, NULL, 0);
    const bool player = mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER);
    if (!player && !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_MINECART) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE))
        return mc_player_fail(error, MC_PLAYER_INVALID_ARGUMENT,
                              "appearance unsupported concrete Entity descendant", NULL, NULL, NULL,
                              0);
    const mc_entity_status held = mc_entity_retain(entity);
    if (held != MC_ENTITY_OK)
        return appearance_hold_error(error, held);
    mc_player_status status = MC_PLAYER_OK;
    if (player) {
        mc_player_fields *fields = &entity->local_player->fields.player;
        mc_player_string *replacement = NULL;
        status = mc_player_cloak_string(fields->l, &replacement);
        if (status != MC_PLAYER_OK) {
            mc_player_fail(error, status, "Player cloak String construction", "gs", "u_", "()V",
                           25);
        } else {
            const mc_player_string *old_name = fields->n;
            fields->n = replacement; /* Actual gs.u_ putfield28. */
            mc_player_string_release((mc_player_string *)old_name);
            status = mc_player_string_retain(replacement);
            if (status != MC_PLAYER_OK) {
                mc_player_fail(error, status, "Player cloak bB native String occurrence", "gs",
                               "u_", "()V", 36);
            } else {
                const mc_player_string *old_cloak = entity->base.client_only.bB;
                entity->base.client_only.bB = fields->n; /* Actual putfield36. */
                mc_player_string_release((mc_player_string *)old_cloak);
            }
        }
    } /* The other admitted targets inherit the actual sn.u_ return0. */
    mc_entity_release(entity);
    return status;
}

const mc_player_string *mc_entity_skin_url(const mc_entity *entity) {
    return entity == NULL ? NULL : entity->base.client_only.bA;
}

const mc_player_string *mc_entity_cloak_url(const mc_entity *entity) {
    return entity == NULL ? NULL : entity->base.client_only.bB;
}
