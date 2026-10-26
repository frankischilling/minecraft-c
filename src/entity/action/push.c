#include "action_internal.h"

bool mc_action_candidate(mc_entity *candidate, mc_entity *player, mc_player_action_error *e,
                         unsigned int pc) {
    if (candidate == NULL)
        return true; /* checkcast preserves null until invoke. */
    if (candidate->classes != player->classes || candidate->holds == 0 ||
        candidate->phase != MC_ENTITY_READY)
        return mc_action_fail(e, MC_ENTITY_STATE_ERROR,
                              "Action nearby Entity actual held class namespace", pc);
    return true;
}

bool mc_action_collidable(mc_entity *candidate, bool *out, mc_player_action_error *e) {
    e->push_bytecode = 0;
    if (candidate == NULL)
        return mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Nearby i_ null receiver", 527);
    if (mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        *out = !candidate->base.be;
        return true;
    }
    if (mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_MINECART)) {
        *out = true;
        return true;
    }
    if (mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_ITEM) ||
        mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_FALLING_SAND)) {
        *out = false;
        return true;
    }
    return mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Nearby i_ unsupported actual descendant", 527);
}

static void velocity(mc_entity *entity, double x, double y, double z) {
    /* Actual inherited sn.d(DDD), including the vertical +0 store. */
    entity->base.velocity_x = ac_add(entity->base.velocity_x, x);
    entity->base.velocity_y = ac_add(entity->base.velocity_y, y);
    entity->base.velocity_z = ac_add(entity->base.velocity_z, z);
}

static double float_root(double value) {
    return ac_widen(ac_narrow(_mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(value)))));
}

static bool base_push(mc_entity *receiver, mc_entity *player, mc_player_action_error *e) {
    e->source_class = "sn";
    e->source_method = "h(Lsn;)V";
    e->push_bytecode = 1;
    if (player->base.aG == receiver || player->base.aH == receiver)
        return true;
    e->push_bytecode = 25;
    if (!mc_action_fp(e, 25))
        return false;
    double x = ac_sub(player->base.x, receiver->base.x),
           z = ac_sub(player->base.z, receiver->base.z);
    double ax = x, az = z;
    if (ax < 0.0)
        ax = ac_neg(ax);
    if (az < 0.0)
        az = ac_neg(az);
    double magnitude = ax > az ? ax : az; /* Actual in.a(DD), not Euclidean length. */
    if (magnitude >= ac_widen(.01f)) {
        magnitude = float_root(magnitude);
        x = ac_div(x, magnitude);
        z = ac_div(z, magnitude);
        double inverse = ac_div(1.0, magnitude);
        if (inverse > 1.0)
            inverse = 1.0;
        x = ac_mul(x, inverse);
        z = ac_mul(z, inverse);
        x = ac_mul(x, ac_widen(.05f));
        z = ac_mul(z, ac_widen(.05f));
        x = ac_mul(x, ac_widen(ac_fsub(1.0f, receiver->base.br)));
        z = ac_mul(z, ac_widen(ac_fsub(1.0f, receiver->base.br)));
        e->push_bytecode = 146;
        velocity(receiver, ac_neg(x), 0.0, ac_neg(z));
        e->push_bytecode = 154;
        velocity(player, x, 0.0, z);
    }
    return true;
}

static bool cart_push(mc_entity *cart, mc_entity *player, mc_player_action_error *e) {
    e->source_class = "yl";
    e->source_method = "h(Lsn;)V";
    e->push_bytecode = 4;
    if (cart->base.world == NULL)
        return mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Minecart push null World B receiver", 4);
    if (cart->base.world->fields.B)
        return true;
    if (player == cart->base.aG)
        return true;
    /* Actual argument is canonical dc, hence instanceof ls && !gs is false.
     * The auto-mount block and its fields are genuinely not reached. */
    e->push_bytecode = 94;
    if (!mc_action_fp(e, 94))
        return false;
    double x = ac_sub(player->base.x, cart->base.x), z = ac_sub(player->base.z, cart->base.z);
    double magnitude = ac_add(ac_mul(x, x), ac_mul(z, z));
    if (magnitude >= ac_widen(1.0e-4f)) {
        magnitude = float_root(magnitude);
        x = ac_div(x, magnitude);
        z = ac_div(z, magnitude);
        double inverse = ac_div(1.0, magnitude);
        if (inverse > 1.0)
            inverse = 1.0;
        x = ac_mul(x, inverse);
        z = ac_mul(z, inverse);
        x = ac_mul(x, ac_widen(.1f));
        z = ac_mul(z, ac_widen(.1f));
        x = ac_mul(x, ac_widen(ac_fsub(1.0f, cart->base.br)));
        z = ac_mul(z, ac_widen(ac_fsub(1.0f, cart->base.br)));
        x = ac_mul(x, .5);
        z = ac_mul(z, .5);
        /* Argument dc is not yl: execute the actual non-Minecart tail. */
        e->push_bytecode = 588;
        velocity(cart, ac_neg(x), 0.0, ac_neg(z));
        e->push_bytecode = 604;
        velocity(player, ac_div(x, 4.0), 0.0, ac_div(z, 4.0));
    }
    return true;
}

bool mc_action_push(mc_entity *candidate, mc_entity *player, mc_player_action_error *e) {
    if (candidate == NULL)
        return mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Nearby push null receiver", 536);
    if (mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_MINECART))
        return cart_push(candidate, player, e);
    if (mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_ITEM) ||
        mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_FALLING_SAND))
        return base_push(candidate, player, e);
    return mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Nearby push unsupported actual descendant",
                          536);
}

bool mc_action_pickup(mc_entity *candidate, mc_entity *player, mc_registry_classes *classes,
                      mc_player_action_error *e) {
    if (candidate == NULL)
        return mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Nearby pickup null receiver", 1);
    if (mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_ITEM)) {
        mc_item_pickup_error_init(&e->pickup);
        e->child_kind = MC_PLAYER_ACTION_CHILD_PICKUP;
        const mc_entity_status status =
            mc_item_entity_pickup(candidate, player, classes, NULL, &e->pickup);
        if (status == MC_ENTITY_OK) {
            mc_player_travel_error_init(&e->travel);
            e->child_kind = MC_PLAYER_ACTION_CHILD_NONE;
            return true;
        }
        e->source_class = e->pickup.source_class;
        e->source_method = e->pickup.source_method;
        e->pickup_bytecode = e->pickup.pickup_bytecode;
        e->stack = e->pickup.stack;
        return mc_action_fail(e, status, "Action actual Item pickup descendant",
                              e->pickup.bytecode);
    }
    if (mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_FALLING_SAND) ||
        mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_MINECART))
        return true; /* Real inherited sn.b(gs) return0. */
    return mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Nearby pickup unsupported actual descendant",
                          1);
}
