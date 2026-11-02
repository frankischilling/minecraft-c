#include "terminal_inventory.h"

#include "../../block/registry/registry_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "player_internal.h"

static mc_entity_status fail(mc_player_terminal_inventory_error *error, mc_entity_status status,
                             const char *operation) {
    error->status = status;
    error->operation = operation;
    return status;
}

static mc_entity_status publish(mc_player_terminal_inventory_error *out,
                                const mc_player_terminal_inventory_error *error) {
    if (out != NULL)
        *out = *error;
    return error->status;
}

static bool represented(const mc_entity *player) {
    return mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
           player->local_player != NULL;
}

static bool empty(const mc_inventory_player_terminal *owner) {
    return !owner->sealed && owner->inventory == NULL && owner->default_container == NULL &&
           owner->active_container == NULL && owner->player == NULL;
}

static bool context(const mc_entity *player, mc_registry_classes *registry, mc_world_runtime *world,
                    mc_player_terminal_inventory_error *error) {
    if (!represented(player) || registry == NULL || world == NULL) {
        fail(error, MC_ENTITY_INVALID_ARGUMENT, "Player terminal actual owner and dependencies");
        return false;
    }
    if (player->phase != MC_ENTITY_READY) {
        fail(error, MC_ENTITY_INVALID_PHASE, "Player terminal READY actual Player");
        return false;
    }
    if (player->holds == 0 || player->classes->source != MC_ENTITY_CLIENT ||
        player->classes->profile == NULL || registry->options.side != MC_REGISTRY_CLIENT ||
        registry->options.random_profile != player->classes->profile ||
        registry->entities != player->classes || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT ||
        world->classes->entities != player->classes ||
        world->classes->profile != player->classes->profile ||
        world->classes->registry != registry || player->base.world != world) {
        fail(error, MC_ENTITY_INVALID_ARGUMENT,
             "Player terminal exact CLIENT Player Registry World namespace");
        return false;
    }
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase != MC_WORLD_RUNTIME_READY || world->entity_containers_retired ||
        world->tile_entity_containers_retired) {
        fail(error, MC_ENTITY_INVALID_PHASE, "Player terminal live GAME World");
        return false;
    }
    if (world->active_calls != 0 || world->local_leases != 0) {
        fail(error, MC_ENTITY_BUSY, "Player terminal World still entered or leased");
        return false;
    }
    return true;
}

static mc_entity_status inventory(mc_player_terminal_inventory_error *error,
                                  mc_inventory_status status) {
    if (status == MC_INVENTORY_OK)
        return MC_ENTITY_OK;
    return fail(error,
                status == MC_INVENTORY_BUSY                ? MC_ENTITY_BUSY
                : status == MC_INVENTORY_LIMIT             ? MC_ENTITY_LIMIT
                : status == MC_INVENTORY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                                                           : MC_ENTITY_STATE_ERROR,
                error->inventory.operation);
}

mc_entity_status mc_player_terminal_inventory_adopt(mc_entity *player,
                                                    mc_registry_classes *registry,
                                                    mc_world_runtime *world,
                                                    mc_container **owned_it,
                                                    mc_player_terminal_inventory_error *out) {
    mc_player_terminal_inventory_error error = {0};
    if (!context(player, registry, world, &error))
        return publish(out, &error);
    mc_local_player_state *state = player->local_player;
    if (!empty(&state->terminal_inventory)) {
        fail(&error, MC_ENTITY_BUSY, "Player terminal native Inventory owner already occupied");
        return publish(out, &error);
    }
    if (owned_it == NULL || owned_it == &state->fields.player.d ||
        owned_it == &state->fields.player.e || *owned_it == NULL ||
        *owned_it != state->fields.player.e || *owned_it == state->fields.player.d) {
        fail(&error, MC_ENTITY_INVALID_ARGUMENT, "Player terminal actual distinct IT owning slot");
        return publish(out, &error);
    }
    mc_inventory_player_terminal prepared = {0};
    if (inventory(&error, mc_inventory_player_terminal_prepare(
                              state->fields.player.c, player, registry, world,
                              state->fields.player.d, *owned_it, &prepared, &error.inventory)) !=
        MC_ENTITY_OK)
        return publish(out, &error);
    /* No fallible operation follows the Inventory's atomic seal. Move the
     * actual native owner only; every Source field remains the same value. */
    state->terminal_inventory = prepared;
    *owned_it = NULL;
    return publish(out, &error);
}

mc_entity_status mc_player_terminal_inventory_admit(const mc_entity *player,
                                                    mc_registry_classes *registry,
                                                    mc_world_runtime *world,
                                                    mc_player_terminal_inventory_error *out) {
    mc_player_terminal_inventory_error error = {0};
    if (!represented(player)) {
        fail(&error, MC_ENTITY_INVALID_ARGUMENT, "Player terminal actual represented Player");
        return publish(out, &error);
    }
    const mc_inventory_player_terminal *owner = &player->local_player->terminal_inventory;
    if (empty(owner))
        return publish(out, &error); /* Existing unsealed owner protocol is unchanged. */
    if (!context(player, registry, world, &error))
        return publish(out, &error);
    if (!owner->sealed || owner->player != player ||
        owner->inventory != player->local_player->fields.player.c ||
        owner->default_container != player->local_player->fields.player.d ||
        owner->active_container != player->local_player->fields.player.e) {
        fail(&error, MC_ENTITY_STATE_ERROR,
             "Player terminal actual Source and native owner aliases");
        return publish(out, &error);
    }
    (void)inventory(&error,
                    mc_inventory_player_terminal_admit(owner, registry, world, &error.inventory));
    return publish(out, &error);
}

bool mc_player_terminal_inventory_inspect(const mc_entity *player,
                                          mc_inventory_player_terminal *out) {
    if (!represented(player) || out == NULL)
        return false;
    *out = player->local_player->terminal_inventory;
    return true;
}

bool mc_local_player_terminal_sealed(const mc_entity *player) {
    return represented(player) && player->phase == MC_ENTITY_READY &&
           player->local_player->terminal_inventory.sealed;
}

void mc_player_terminal_inventory_dispose(mc_entity *player) {
    if (!represented(player))
        return;
    mc_inventory_player_terminal_dispose(&player->local_player->terminal_inventory);
}
