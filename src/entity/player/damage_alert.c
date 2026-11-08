#include "damage_alert_internal.h"

#include "mc/entity/player/ally_alert.h"
#include "player_internal.h"

#include <string.h>

static mc_entity_player_alert_state view(const mc_player_ally_alert_state *state) {
    const mc_player_ally_alert_error *error = state->error;
    const mc_player_ally_alert_cleanup *cleanup = state->cleanup;
    return (mc_entity_player_alert_state){.entered = true,
                                          .returned = state->returned,
                                          .pending = true,
                                          .active = state->active,
                                          .status = error->status,
                                          .operation = error->operation,
                                          .site = error->site,
                                          .child = error->child,
                                          .native_policy = error->native_policy,
                                          .source_exception = error->source_exception,
                                          .source_error = error->source_error,
                                          .exception_class = error->exception_class,
                                          .wolves_examined = state->wolves_examined,
                                          .wolves_alerted = state->wolves_alerted,
                                          .receiver = state->player,
                                          .error = error,
                                          .cleanup = cleanup,
                                          .cleanup_status = cleanup->status,
                                          .cleanup_operation = cleanup->operation};
}

bool mc_player_damage_alert_inspect(const mc_entity *p, mc_entity_player_alert_state *out) {
    if (p == NULL || p->local_player == NULL || out == NULL)
        return false;
    mc_player_ally_alert_state state;
    if (!mc_player_ally_alert_inspect(p->local_player->pending_damage_alert, &state))
        return false;
    *out = view(&state);
    return true;
}

static void caller_site(mc_entity_effect_error *e) {
    e->source_class = "gs";
    e->source_bytecode = e->player_bytecode = 144;
}

mc_entity_status mc_player_damage_alert_cleanup(mc_entity *p, mc_entity_effect_error *e) {
    if (p->local_player->pending_damage_alert == NULL)
        return MC_ENTITY_OK;
    mc_entity_player_alert_state state = {0};
    (void)mc_player_damage_alert_inspect(p, &state);
    mc_player_ally_alert_cleanup cleanup;
    const mc_entity_status status =
        mc_player_ally_alert_retire(&p->local_player->pending_damage_alert, &cleanup);
    caller_site(e);
    if (status != MC_ENTITY_OK) {
        /* Refresh only the still-owned invocation. Its original failure is
         * independent of this cleanup attempt's native refusal. */
        (void)mc_player_damage_alert_inspect(p, &state);
        state.cleanup_status = status;
        state.cleanup_operation = cleanup.operation;
        e->player_alert = state;
        e->status = status;
        e->operation = cleanup.operation;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        return status;
    }
    state.pending = state.active = false;
    state.receiver = NULL;
    state.error = NULL;
    state.cleanup = NULL;
    state.cleanup_status = MC_ENTITY_OK;
    state.cleanup_operation = NULL;
    e->player_alert = state;
    return MC_ENTITY_OK;
}

bool mc_player_damage_alert_entered(mc_entity *p, mc_entity *source, mc_entity_effect_error *e) {
    caller_site(e);
    const mc_entity_status status =
        mc_player_ally_alert_begin(p, source, false, &p->local_player->pending_damage_alert);
    if (!mc_player_damage_alert_inspect(p, &e->player_alert)) {
        e->status = status;
        e->operation = "Player damage ally alert retained invocation admission";
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        return false;
    }
    if (status != MC_ENTITY_OK) {
        e->status = status;
        e->operation = e->player_alert.operation;
        e->cause =
            e->player_alert.source_exception && e->player_alert.exception_class != NULL &&
                    strcmp(e->player_alert.exception_class, "java/lang/NullPointerException") == 0
                ? MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE
                : MC_ENTITY_EFFECT_CAUSE_WORLD;
        return false;
    }
    if (!e->player_alert.returned) {
        e->status = MC_ENTITY_STATE_ERROR;
        e->operation = "Player damage ally alert missing source return";
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        return false;
    }
    return mc_player_damage_alert_cleanup(p, e) == MC_ENTITY_OK;
}
