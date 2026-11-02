#include "active_internal.h"
#include "container_interaction.h"
#include "nbt_internal.h"
#include "statistics_status_internal.h"

#include <string.h>

static void at(mc_player_tick_error *e, unsigned int pc) {
    e->source_class = "gs";
    e->source_method = "w_()V";
    e->player_bytecode = pc;
}

static bool bed(mc_active_scope *s, bool *out, mc_player_tick_error *e) {
    mc_entity *p = s->player;
    mc_world_runtime *world = p->base.world;
    e->bed_bytecode = 8;
    const mc_player_coordinates *v = p->local_player->fields.player.v;
    if (v == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                              "Sleeping Player null bed coordinate receiver");
    const int32_t x = v->x, y = v->y, z = v->z;
    e->bed_bytecode = 25;
    if (world == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Sleeping Player null World bed receiver");
    int32_t id;
    if (!mc_active_world_result(e, mc_world_runtime_block_get(world, x, y, z, &id, &e->world),
                                "Sleeping Player actual bed block query"))
        return false;
    e->bed_bytecode = 28;
    if (!mc_active_registry(s, e))
        return false;
    const mc_block *block;
    if (!mc_active_registry_result(
            e, mc_registry_block_static_get(s->registry, "T", &block, &e->registry)))
        return false;
    e->bed_bytecode = 31;
    const mc_block_fields *fields = mc_block_fields_get(block);
    if (fields == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Sleeping Player live Block.T receiver");
    *out = id == fields->bn;
    return true;
}

static bool sleep_prefix(mc_active_scope *s, mc_player_tick_error *e) {
    mc_entity *p = s->player;
    mc_player_fields *v = &p->local_player->fields.player;
    at(e, 1); /* Actual gs.N reads only u. */
    if (v->u) {
        v->private_a = mc_java_i32_add(v->private_a, 1);
        if (v->private_a > 100)
            v->private_a = 100;
        at(e, 36);
        if (p->base.world == NULL)
            return mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Sleeping tick null World B receiver");
        if (!p->base.world->fields.B) {
            at(e, 43);
            bool in_bed;
            if (!bed(s, &in_bed, e))
                return false;
            if (!in_bed) {
                at(e, 53);
                return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                                      "Sleeping tick genuine gs.a(ZZZ) wake descendant dependency");
            }
            at(e, 63);
            if (p->base.world == NULL)
                return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                                      "Sleeping tick null World daytime receiver");
            if (p->base.world->fields.f < 4) {
                at(e, 73);
                return mc_active_fail(
                    e, MC_ENTITY_STATE_ERROR,
                    "Daytime sleeping tick genuine gs.a(ZZZ) wake descendant dependency");
            }
        }
    } else if (v->private_a > 0) {
        v->private_a = mc_java_i32_add(v->private_a, 1);
        if (v->private_a >= 110)
            v->private_a = 0;
    }
    return true;
}

static bool close(mc_active_scope *s, mc_player_tick_error *e) {
    mc_entity *p = s->player;
    const mc_player_site parent = {"gs", "w_", "()V", 143};
    e->close_error = (mc_player_close_error){.parent_site = parent};
    if (s->context != NULL && s->context->close.close != NULL) {
        /* This port is the WHOLE actual DC.r child. Its GS.r prefix belongs
         * inside the host, so callbacks observe the original active e. */
        const mc_entity_status status =
            s->context->close.close(s->context->close.context, p, &e->close_error);
        e->close_error.status = status;
        /* The enclosing GS143 already has its independent caller cell. Keep
         * an actual supplied child parent; only an absent parent gets GS143. */
        if (e->close_error.parent_site.class_name == NULL)
            e->close_error.parent_site = parent;
        if (status == MC_ENTITY_OK)
            return true;
        const mc_player_site *site = &e->close_error.site;
        if (site->class_name != NULL && site->method != NULL && site->descriptor != NULL &&
            strcmp(site->class_name, "dc") == 0 && strcmp(site->method, "r") == 0 &&
            strcmp(site->descriptor, "()V") == 0) {
            e->source_class = "dc";
            e->source_method = "r()V";
            e->close_bytecode = site->pc;
        }
        return mc_active_fail(e, status,
                              e->close_error.operation == NULL
                                  ? "Player close actual whole DC.r child"
                                  : e->close_error.operation);
    }
    /* Without a host, execute the real GS.r prefix and retain the exact
     * reached DC.r descendant frontier. No display completion is supplied. */
    e->source_class = "dc";
    e->source_method = "r()V";
    e->close_bytecode = 1;
    mc_player_container_error prefix = {0};
    const mc_entity_status restored = mc_player_container_restore_default(p, &prefix);
    if (restored != MC_ENTITY_OK) {
        e->close_error = (mc_player_close_error){.status = restored,
                                                 .operation = prefix.operation,
                                                 .exception_class = prefix.exception_class,
                                                 .site = prefix.site,
                                                 .parent_site = {"dc", "r", "()V", 1},
                                                 .native_policy = prefix.native_policy,
                                                 .source_exception = prefix.source_exception,
                                                 .source_error = prefix.source_error};
        return mc_active_fail(e, restored, prefix.operation);
    }
    e->close_bytecode = 5;
    client_application *actual = p->local_player->fields.minecraft;
    e->close_bytecode = 9;
    e->close_error.site = (mc_player_site){"dc", "r", "()V", 9};
    e->close_error.status = MC_ENTITY_STATE_ERROR;
    e->close_error.native_policy = actual != NULL;
    e->close_error.source_exception = actual == NULL;
    e->close_error.exception_class = actual == NULL ? "java/lang/NullPointerException" : NULL;
    e->close_error.operation =
        actual == NULL ? "Player close null Minecraft invocation receiver"
                       : "Player close genuine Minecraft.a(Screen) descendant dependency";
    return mc_active_fail(e, e->close_error.status, e->close_error.operation);
}

static bool smoothing(mc_active_scope *s, mc_player_tick_error *e) {
    mc_entity *p = s->player;
    mc_player_fields *v = &p->local_player->fields.player;
    v->o = v->r;
    v->p = v->s;
    v->q = v->t;
    at(e, 186);
    if (!mc_active_fp(e))
        return false;
    const double x = at_sub(p->base.x, v->r), y = at_sub(p->base.y, v->s),
                 z = at_sub(p->base.z, v->t);
    if (x > 10.0) {
        v->r = p->base.x;
        v->o = v->r;
    }
    if (z > 10.0) {
        v->t = p->base.z;
        v->q = v->t;
    }
    if (y > 10.0) {
        v->s = p->base.y;
        v->p = v->s;
    }
    if (x < -10.0) {
        v->r = p->base.x;
        v->o = v->r;
    }
    if (z < -10.0) {
        v->t = p->base.z;
        v->q = v->t;
    }
    if (y < -10.0) {
        v->s = p->base.y;
        v->p = v->s;
    }
    v->r = at_add(v->r, at_mul(x, .25));
    v->t = at_add(v->t, at_mul(z, .25));
    v->s = at_add(v->s, at_mul(y, .25));
    at(e, 383);
    if (!mc_active_registry(s, e))
        return false;
    if (!mc_active_registry_result(
            e, mc_registry_classes_initialize_statistics(s->registry, &e->registry)))
        return false;
    const mc_statistic *statistic = s->registry->statistics.named[5];
    at(e, 387);
    if (mc_local_player_add_statistic(p, statistic, 1, &e->statistic) != MC_PLAYER_STATISTIC_OK)
        return mc_active_fail(e, mc_player_statistic_entity_status(&e->statistic),
                              "Player tick actual play-time statistic descendant");
    if (p->base.aH == NULL) {
        v->private_bN = NULL; /* Actual gs.w_ store399. */
        mc_player_nbt_coordinates_collect_held(p);
    }
    return true;
}

bool mc_active_player(mc_active_scope *s, mc_player_tick_error *e) {
    mc_entity *p = s->player;
    if (!sleep_prefix(s, e))
        return false;
    at(e, 111);
    if (!mc_active_living(s, e))
        return false;
    at(e, 118);
    if (p->base.world == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Player post-tick null World B receiver");
    if (!p->base.world->fields.B && p->local_player->fields.player.e != NULL) {
        at(e, 136);
        bool eligible;
        if (!mc_active_container(s, p->local_player->fields.player.e, &eligible, e))
            return false;
        if (!eligible) {
            at(e, 143);
            if (!close(s, e))
                return false;
            at(e, 148);
            mc_container *fresh_default = p->local_player->fields.player.d;
            at(e, 151);
            p->local_player->fields.player.e = fresh_default;
        }
    }
    return smoothing(s, e);
}
