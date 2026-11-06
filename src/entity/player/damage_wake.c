#include "damage_wake_internal.h"

#include "mc/entity/player/sleep.h"
#include "player_internal.h"

#include <stdlib.h>

struct mc_player_damage_wake_work {
    mc_entity_player_wake_state state;
    mc_player_sleep_call *call;
    mc_player_sleep_error error;
    mc_player_sleep_cleanup cleanup;
};

static mc_entity_status status_of(mc_player_sleep_status status) {
    switch (status) {
        case MC_PLAYER_SLEEP_OK:
            return MC_ENTITY_OK;
        case MC_PLAYER_SLEEP_BUSY:
            return MC_ENTITY_BUSY;
        case MC_PLAYER_SLEEP_LIMIT:
            return MC_ENTITY_LIMIT;
        case MC_PLAYER_SLEEP_ALLOCATION_FAILED:
            return MC_ENTITY_ALLOCATION_FAILED;
        default:
            return MC_ENTITY_STATE_ERROR;
    }
}

static mc_entity_player_wake_state view(const struct mc_player_damage_wake_work *w) {
    mc_entity_player_wake_state state = w->state;
    state.error = state.pending ? &w->error : NULL;
    state.cleanup = state.pending ? &w->cleanup : NULL;
    return state;
}

bool mc_player_damage_wake_inspect(const mc_entity *p, mc_entity_player_wake_state *out) {
    if (p == NULL || p->local_player == NULL || out == NULL ||
        p->local_player->pending_damage_wake == NULL)
        return false;
    *out = view(p->local_player->pending_damage_wake);
    return true;
}

static mc_entity_status cleanup_failed(struct mc_player_damage_wake_work *w,
                                       mc_entity_effect_error *e, mc_entity_status status,
                                       const char *operation) {
    w->state.cleanup_status = status;
    w->state.cleanup_operation = operation;
    e->status = status;
    e->operation = operation;
    e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
    e->source_class = "gs";
    e->source_bytecode = e->player_bytecode = 35;
    e->player_wake = view(w);
    return status;
}

mc_entity_status mc_player_damage_wake_cleanup(mc_entity *p, mc_entity_effect_error *e) {
    struct mc_player_damage_wake_work *w = p->local_player->pending_damage_wake;
    if (w == NULL)
        return MC_ENTITY_OK;
    if (w->state.active)
        return cleanup_failed(w, e, MC_ENTITY_BUSY, "Player active damage wake cleanup");
    const mc_player_sleep_status status = mc_player_sleep_retire(&w->call, &w->cleanup);
    if (status != MC_PLAYER_SLEEP_OK) {
        /* Retirement may have consumed a nested child before another owner
         * refuses. Refresh the surviving typed borrows before exposing them. */
        mc_player_sleep_state sleep;
        if (mc_player_sleep_inspect(w->call, &sleep) && sleep.failure != NULL)
            w->error = *sleep.failure;
        return cleanup_failed(w, e, status_of(status), w->cleanup.operation);
    }
    w->state.pending = false;
    w->state.receiver = NULL;
    w->state.cleanup_status = MC_ENTITY_OK;
    w->state.cleanup_operation = NULL;
    e->player_wake = view(w);
    p->local_player->pending_damage_wake = NULL;
    free(w);
    mc_entity_release(p);
    return MC_ENTITY_OK;
}

bool mc_player_damage_wake_entered(mc_entity *p, mc_registry_classes *registry,
                                   mc_entity_effect_error *e) {
    struct mc_player_damage_wake_work *w = calloc(1, sizeof(*w));
    if (w == NULL) {
        e->status = MC_ENTITY_ALLOCATION_FAILED;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Player damage wake retained invocation allocation";
        return false;
    }
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK) {
        free(w);
        e->status = held;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Player damage wake self occurrence";
        return false;
    }
    w->state = (mc_entity_player_wake_state){
        .entered = true, .pending = true, .active = true, .receiver = p};
    p->local_player->pending_damage_wake = w;
    const mc_player_sleep_context context = {.code_registry = registry};
    const mc_player_sleep_status status =
        mc_player_wake_begin(p, true, true, false, &context, &w->call, &w->error);
    w->state.active = false;
    w->state.returned = status == MC_PLAYER_SLEEP_OK;
    w->state.status = status_of(status);
    w->state.operation = w->error.operation;
    w->state.site = w->error.site;
    w->state.child = w->error.child;
    w->state.native_policy = w->error.native_policy;
    w->state.source_exception = w->error.source_exception;
    w->state.source_error = w->error.source_error;
    w->state.exception_class = w->error.exception_class;
    e->player_wake = view(w);
    if (status != MC_PLAYER_SLEEP_OK) {
        e->status = status_of(status);
        e->cause = status == MC_PLAYER_SLEEP_SOURCE_NULL ? MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE
                                                         : MC_ENTITY_EFFECT_CAUSE_WORLD;
        e->operation = w->error.operation;
        return false;
    }
    return mc_player_damage_wake_cleanup(p, e) == MC_ENTITY_OK;
}
