#include "mc/entity/player/pickup.h"
#include "../../client/application_access.h"
#include "player_internal.h"

static mc_entity_status fail(mc_player_pickup_error *e, mc_entity_status status,
                             const char *operation, unsigned int pc) {
    e->status = status;
    e->operation = operation;
    e->notification_pc = pc;
    return status;
}

static mc_entity_status manager_status(mc_effect_renderer_status status) {
    switch (status) {
        case MC_EFFECT_RENDERER_OK:
            return MC_ENTITY_OK;
        case MC_EFFECT_RENDERER_LIMIT:
            return MC_ENTITY_LIMIT;
        case MC_EFFECT_RENDERER_ALLOCATION_FAILED:
            return MC_ENTITY_ALLOCATION_FAILED;
        case MC_EFFECT_RENDERER_BUSY:
            return MC_ENTITY_BUSY;
        case MC_EFFECT_RENDERER_WORLD_FAILED:
            return MC_ENTITY_WORLD_FAILED;
        case MC_EFFECT_RENDERER_RANDOM_FAILED:
            return MC_ENTITY_PROFILE_FAILED;
        default:
            return MC_ENTITY_STATE_ERROR;
    }
}

mc_entity_status mc_local_player_pickup_notification(mc_entity *player, mc_entity *picked,
                                                     int32_t amount, mc_player_pickup_error *out) {
    mc_player_pickup_error e = {0};
    mc_client_effect_renderer *manager = NULL;
    mc_entity *particle = NULL;
    bool player_held = false, manager_held = false;
    (void)amount;
    if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        fail(&e, MC_ENTITY_INVALID_ARGUMENT, "LocalPlayer pickup notification concrete receiver",
             0);
    else if (player->phase != MC_ENTITY_READY)
        fail(&e, MC_ENTITY_INVALID_PHASE, "LocalPlayer pickup notification READY receiver", 0);
    else if ((e.entity.status = mc_entity_retain(player)) != MC_ENTITY_OK)
        fail(&e, e.entity.status, "LocalPlayer pickup notification native Player hold", 0);
    else {
        player_held = true;
        client_application *app = player->local_player->fields.minecraft;
        if (app == NULL) {
            fail(&e, MC_ENTITY_STATE_ERROR, "LocalPlayer pickup notification null Minecraft j", 4);
            goto finished;
        }
        manager = mc_client_application_live_effect_renderer(app);
        if (manager != NULL) {
            const mc_effect_renderer_status held = mc_client_effect_renderer_retain(manager);
            if (held != MC_EFFECT_RENDERER_OK) {
                fail(&e, manager_status(held), "LocalPlayer pickup captured manager hold", 4);
                goto finished;
            }
            manager_held = true;
        }
        e.entity.status = mc_pickup_particle_allocate(player->classes, &particle, &e.entity);
        if (e.entity.status != MC_ENTITY_OK) {
            fail(&e, e.entity.status, "LocalPlayer pickup actual new em", 7);
            goto finished;
        }
        app = player->local_player->fields.minecraft; /* Actual b12 reload. */
        if (app == NULL) {
            fail(&e, MC_ENTITY_STATE_ERROR, "LocalPlayer pickup notification null Minecraft f", 15);
            goto finished;
        }
        mc_world_runtime *world = mc_client_application_live_world(app);
        const mc_entity_status constructed =
            mc_pickup_particle_construct(particle, world, picked, player, -.5f, &e.particle);
        if (constructed != MC_ENTITY_OK) {
            fail(&e, constructed, "LocalPlayer pickup actual em constructor", 22);
            goto finished;
        }
        if (manager == NULL) {
            fail(&e, MC_ENTITY_STATE_ERROR, "LocalPlayer pickup notification null captured j", 25);
            goto finished;
        }
        const mc_effect_renderer_status added =
            mc_client_effect_renderer_add(manager, particle, &e.renderer);
        if (added != MC_EFFECT_RENDERER_OK)
            fail(&e, manager_status(added), "LocalPlayer pickup actual dn add", 25);
    }
finished:
    mc_entity_release(particle);
    if (manager_held) {
        const mc_effect_renderer_status released = mc_client_effect_renderer_release(manager);
        if (released != MC_EFFECT_RENDERER_OK)
            fail(&e, manager_status(released), "LocalPlayer pickup captured manager retirement", 0);
    }
    if (player_held)
        mc_entity_release(player);
    if (e.status == MC_ENTITY_OK)
        e = (mc_player_pickup_error){0};
    if (out != NULL)
        *out = e;
    return e.status;
}
