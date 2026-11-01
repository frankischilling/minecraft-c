#include "mc/entity/player/mining.h"
#include "../../block/registry/registry_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/inventory/mining.h"
#include "player_internal.h"

#include <string.h>
#include <xmmintrin.h>

static mc_entity_status mining_fail(mc_player_mining_error *e, mc_entity_status status,
                                    const char *operation, const char *owner, const char *method,
                                    const char *descriptor, unsigned int pc, bool native_policy) {
    e->status = status;
    e->operation = operation;
    e->site = (mc_player_site){owner, method, descriptor, pc};
    e->native_policy = native_policy;
    return status;
}

static bool player_hold(mc_entity *player, mc_player_mining_error *e) {
    if (player == NULL || !mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        player->local_player == NULL || player->phase != MC_ENTITY_READY) {
        (void)mining_fail(e, MC_ENTITY_INVALID_ARGUMENT,
                          "Player mining canonical READY LocalPlayer", NULL, NULL, NULL, 0, true);
        return false;
    }
    const mc_entity_status status = mc_entity_retain(player);
    if (status != MC_ENTITY_OK) {
        (void)mining_fail(e, status, "Player mining native occurrence", NULL, NULL, NULL, 0, true);
        return false;
    }
    return true;
}

static bool floating(mc_player_mining_error *e, const char *owner, const char *method,
                     const char *descriptor, unsigned int pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    (void)mining_fail(e, MC_ENTITY_UNSUPPORTED_FP, "Player mining floating environment", owner,
                      method, descriptor, pc, true);
    return false;
}

static float divide(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

mc_entity_status mc_local_player_swing(mc_entity *player, mc_player_mining_error *error) {
    mc_player_mining_error e = {0};
    if (player_hold(player, &e)) {
        player->local_player->fields.player.k = -1;
        player->local_player->fields.player.j = true;
        e.site = (mc_player_site){"gs", "J", "()V", 10};
        e.returned = true;
        mc_entity_release(player);
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

static bool harvest_body(mc_entity *player, const mc_block *block, bool *out,
                         mc_player_mining_error *e) {
    e->site = (mc_player_site){"gs", "b", "(Luu;)Z", 5};
    mc_inventory_player *inventory = player->local_player->fields.player.c;
    if (inventory == NULL) {
        (void)mining_fail(e, MC_ENTITY_STATE_ERROR, "actual gs.b null Inventory receiver", "gs",
                          "b", "(Luu;)Z", 5, false);
        return false;
    }
    const mc_inventory_status status =
        mc_inventory_player_can_harvest(inventory, block, out, &e->inventory);
    if (status != MC_INVENTORY_OK) {
        (void)mining_fail(e, MC_ENTITY_STATE_ERROR, e->inventory.operation, "gs", "b", "(Luu;)Z", 5,
                          status != MC_INVENTORY_NULL_REFERENCE &&
                              status != MC_INVENTORY_INDEX_OUT_OF_BOUNDS);
        return false;
    }
    e->harvest = *out;
    e->site.pc = 8;
    return true;
}

static bool speed_body(mc_entity *player, mc_registry_classes *registry, const mc_block *block,
                       float *out, mc_player_mining_error *e) {
    e->site = (mc_player_site){"gs", "a", "(Luu;)F", 5};
    mc_inventory_player *inventory = player->local_player->fields.player.c;
    if (inventory == NULL) {
        (void)mining_fail(e, MC_ENTITY_STATE_ERROR, "actual gs.a null Inventory receiver", "gs",
                          "a", "(Luu;)F", 5, false);
        return false;
    }
    float speed;
    const mc_inventory_status status =
        mc_inventory_player_mining_speed(inventory, block, &speed, &e->inventory);
    if (status != MC_INVENTORY_OK) {
        (void)mining_fail(e, MC_ENTITY_STATE_ERROR, e->inventory.operation, "gs", "a", "(Luu;)F", 5,
                          status != MC_INVENTORY_NULL_REFERENCE &&
                              status != MC_INVENTORY_INDEX_OUT_OF_BOUNDS);
        return false;
    }
    mc_material *water;
    if (mc_registry_material_get(registry, MC_MATERIAL_g, &water, &e->registry) != MC_REGISTRY_OK) {
        (void)mining_fail(e, MC_ENTITY_STATE_ERROR, "Player mining actual Water active use", "gs",
                          "a", "(Luu;)F", 10, true);
        return false;
    }
    if (mc_entity_inside_material(player, water, &e->water, &e->material) != MC_ENTITY_OK) {
        (void)mining_fail(e, e->material.status, e->material.operation, "gs", "a", "(Luu;)F", 13,
                          e->material.native_policy);
        return false;
    }
    if (e->water) {
        if (!floating(e, "gs", "a", "(Luu;)F", 22))
            return false;
        speed = divide(speed, 5.0f);
    }
    e->airborne = !player->base.aX;
    if (e->airborne) {
        if (!floating(e, "gs", "a", "(Luu;)F", 34))
            return false;
        speed = divide(speed, 5.0f);
    }
    e->speed = speed;
    *out = speed;
    e->site.pc = 37;
    return true;
}

mc_entity_status mc_local_player_mining_speed(mc_entity *player, mc_registry_classes *registry,
                                              const mc_block *block, float *out,
                                              mc_player_mining_error *error) {
    mc_player_mining_error e = {0};
    if (out == NULL)
        (void)mining_fail(&e, MC_ENTITY_INVALID_ARGUMENT, "Player mining speed output", NULL, NULL,
                          NULL, 0, true);
    else if (player_hold(player, &e)) {
        e.returned = speed_body(player, registry, block, out, &e);
        mc_entity_release(player);
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_entity_status mc_local_player_can_harvest(mc_entity *player, const mc_block *block, bool *out,
                                             mc_player_mining_error *error) {
    mc_player_mining_error e = {0};
    if (out == NULL)
        (void)mining_fail(&e, MC_ENTITY_INVALID_ARGUMENT, "Player harvest output", NULL, NULL, NULL,
                          0, true);
    else if (player_hold(player, &e)) {
        e.returned = harvest_body(player, block, out, &e);
        mc_entity_release(player);
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_entity_status mc_block_player_strength(const mc_block *block, mc_entity *player, float *out,
                                          mc_player_mining_error *error) {
    mc_player_mining_error e = {0};
    if (out == NULL)
        (void)mining_fail(&e, MC_ENTITY_INVALID_ARGUMENT, "Block strength output", NULL, NULL, NULL,
                          0, true);
    else if (block == NULL)
        (void)mining_fail(&e, MC_ENTITY_STATE_ERROR, "actual Block strength null receiver", "uu",
                          "a", "(Lgs;)F", 1, false);
    else if (floating(&e, "uu", "a", "(Lgs;)F", 5)) {
        const mc_block_fields *fields = mc_block_fields_get(block);
        /* fcmpg5/ifge6 sends NaN to the Player branch, while only negative
         * ordered hardness reaches the literal-zero return at10. */
        if (fields->bo < 0.0f) {
            *out = 0.0f;
            e.site = (mc_player_site){"uu", "a", "(Lgs;)F", 10};
            e.returned = true;
        } else if (player == NULL) {
            (void)mining_fail(&e, MC_ENTITY_STATE_ERROR,
                              "actual Block strength null Player harvest receiver", "uu", "a",
                              "(Lgs;)F", 13, false);
        } else if (player_hold(player, &e)) {
            bool harvest;
            float strength = 0.0f;
            if (harvest_body(player, block, &harvest, &e)) {
                if (!harvest) {
                    if (floating(&e, "uu", "a", "(Lgs;)F", 24)) {
                        strength = divide(divide(1.0f, fields->bo), 100.0f);
                        e.site = (mc_player_site){"uu", "a", "(Lgs;)F", 28};
                        e.returned = true;
                    }
                } else {
                    float speed;
                    if (speed_body(player, block->classes, block, &speed, &e)) {
                        if (floating(&e, "uu", "a", "(Lgs;)F", 38)) {
                            strength = divide(divide(speed, fields->bo), 30.0f);
                            e.site = (mc_player_site){"uu", "a", "(Lgs;)F", 42};
                            e.returned = true;
                        }
                    }
                }
                if (e.returned) {
                    e.strength = strength;
                    *out = strength;
                }
            }
            mc_entity_release(player);
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
