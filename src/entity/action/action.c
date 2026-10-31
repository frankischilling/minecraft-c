#include "../living/travel_internal.h"
#include "../mob/tick/internal.h"
#include "action_internal.h"

#include <stdlib.h>

static void living_at(mc_player_action_error *e, unsigned int pc) {
    e->source_class = "ls";
    e->source_method = "o()V";
    e->living_bytecode = pc;
}

static void player_at(mc_player_action_error *e, unsigned int pc) {
    e->source_class = "gs";
    e->source_method = "o()V";
    e->player_bytecode = pc;
}

static bool world_status(mc_player_action_error *e, mc_world_storage_status status, const char *op,
                         unsigned int pc) {
    return status == MC_WORLD_STORAGE_OK ||
           mc_action_fail(
               e, status == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_WORLD_FAILED, op,
               pc);
}

static bool entity_status(mc_player_action_error *e, mc_entity_status status, const char *op,
                          unsigned int pc) {
    return status == MC_ENTITY_OK || mc_action_fail(e, status, op, pc);
}

static mc_registry_classes *registry(const mc_player_action_context *c) {
    return c == NULL ? NULL : c->travel.code_registry;
}

static const mc_player_travel_context *travel_context(const mc_player_action_context *c) {
    return c == NULL ? NULL : &c->travel;
}

static bool position(mc_entity *p, double x, double y, double z, mc_player_action_error *e,
                     unsigned int call_pc) {
    p->base.x = x;
    p->base.y = y;
    p->base.z = z;
    e->position_bytecode = 21;
    if (!mc_action_fp(e, call_pc))
        return false;
    e->position_bytecode = 86;
    if (p->bounds == NULL)
        return mc_action_fail(e, MC_ENTITY_STATE_ERROR,
                              "Interpolation null permanent bounds receiver", call_pc);
    mc_entity_source_position(p, x, y, z);
    return true;
}

static bool collision_count(const mc_world_collision_list_view *view, size_t *out,
                            mc_player_action_error *e, unsigned int pc) {
    return world_status(e, mc_world_collision_list_view_count(view, out),
                        "Interpolation live collision List size", pc);
}

static bool interpolate(mc_entity *p, mc_player_action_error *e) {
    mc_living_entity_fields *v = mc_entity_living_fields(p);
    if (v->an <= 0)
        return true;
    living_at(e, 19);
    if (!mc_action_fp(e, 19))
        return false;
    const double x = ac_add(p->base.x, ac_div(ac_sub(v->ao, p->base.x), (double)v->an));
    double y = ac_add(p->base.y, ac_div(ac_sub(v->ap, p->base.y), (double)v->an));
    const double z = ac_add(p->base.z, ac_div(ac_sub(v->aq, p->base.z), (double)v->an));
    double yaw_delta = ac_sub(v->ar, ac_widen(p->base.yaw));
    while (yaw_delta < -180.0) {
        living_at(e, 97);
        if (!mc_action_work(e, 97))
            return false;
        yaw_delta = ac_add(yaw_delta, 360.0);
    }
    while (yaw_delta >= 180.0) {
        living_at(e, 117);
        if (!mc_action_work(e, 117))
            return false;
        yaw_delta = ac_sub(yaw_delta, 360.0);
    }
    p->base.yaw = ac_narrow(ac_add(ac_widen(p->base.yaw), ac_div(yaw_delta, (double)v->an)));
    p->base.aT = ac_narrow(
        ac_add(ac_widen(p->base.aT), ac_div(ac_sub(v->as, ac_widen(p->base.aT)), (double)v->an)));
    v->an = mc_java_i32_sub(v->an, 1);
    living_at(e, 184);
    if (!position(p, x, y, z, e, 184))
        return false;
    living_at(e, 196);
    e->angle_bytecode = 4;
    p->base.yaw = mc_action_angle_remainder(p->base.yaw);
    e->angle_bytecode = 12;
    p->base.aT = mc_action_angle_remainder(p->base.aT);
    mc_world_runtime *world = p->base.world;
    mc_entity_bounds *contracted = NULL;
    mc_world_collision_list_view list = {0};
    bool okay = false;
    living_at(e, 215);
    if (p->bounds == NULL) {
        mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Interpolation contract null bounds", 215);
        goto done;
    }
    if (!entity_status(e,
                       mc_entity_bounds_contract(p->classes, p->bounds, .03125, 0.0, .03125,
                                                 &contracted, &e->bounds),
                       "Interpolation actual pooled bounds contract", 215))
        goto done;
    living_at(e, 218);
    if (!world_status(e,
                      mc_world_runtime_collision_boxes(world, p, contracted, &list, &e->collision),
                      "Interpolation actual World collision List", 218))
        goto done;
    size_t size;
    living_at(e, 225);
    if (!collision_count(&list, &size, e, 225))
        goto done;
    if (size > 0) {
        double highest = 0.0;
        int32_t index = 0;
        for (;;) {
            living_at(e, 243);
            if (!collision_count(&list, &size, e, 243))
                goto done;
            if (index >= 0 && (size_t)(uint32_t)index >= size)
                break;
            living_at(e, 255);
            if (!mc_action_work(e, 255))
                goto done;
            const mc_entity_bounds *box = NULL;
            if (index < 0 ||
                !world_status(
                    e, mc_world_collision_list_view_get(&list, (size_t)(uint32_t)index, &box),
                    "Interpolation live collision List get", 255)) {
                if (index < 0)
                    mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Interpolation negative List index",
                                   255);
                goto done;
            }
            living_at(e, 267);
            if (box == NULL) {
                mc_action_fail(e, MC_ENTITY_STATE_ERROR,
                               "Interpolation null collision box maxY receiver", 267);
                goto done;
            }
            if (!mc_action_fp(e, 272))
                goto done;
            if (box->max_y > highest)
                highest = box->max_y;
            index = mc_java_i32_add(index, 1);
        }
        living_at(e, 296);
        if (p->bounds == NULL) {
            mc_action_fail(e, MC_ENTITY_STATE_ERROR,
                           "Interpolation correction null permanent bounds", 296);
            goto done;
        }
        if (!mc_action_fp(e, 299))
            goto done;
        y = ac_add(y, ac_sub(highest, p->bounds->min_y));
        living_at(e, 307);
        if (!position(p, x, y, z, e, 307))
            goto done;
    }
    okay = true;
done:
    mc_world_collision_list_view_release(&list);
    if (contracted != NULL)
        mc_entity_bounds_release(p->classes, contracted);
    return okay;
}

static bool nearby(mc_entity *p, bool pickup, const mc_player_action_context *context,
                   mc_player_action_error *e) {
    mc_world_runtime *world = p->base.world;
    const unsigned int transform_pc = pickup ? 193u : 478u, query_pc = pickup ? 196u : 481u;
    const unsigned int count_pc = pickup ? 210u : 504u, get_pc = pickup ? 221u : 515u,
                       cast_pc = pickup ? 226u : 520u;
    mc_entity_bounds *expanded = NULL;
    mc_world_entity_list_view list = {0};
    mc_entity *held = NULL;
    bool okay = false;
    if (pickup)
        player_at(e, transform_pc);
    else
        living_at(e, transform_pc);
    if (p->bounds == NULL) {
        mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Action nearby null permanent bounds",
                       transform_pc);
        goto done;
    }
    const double extent = pickup ? 1.0 : ac_widen(.2f);
    if (!entity_status(e,
                       mc_entity_bounds_expand(p->classes, p->bounds, extent, 0.0, extent,
                                               &expanded, &e->bounds),
                       "Action actual nearby bounds expand", transform_pc))
        goto done;
    if (pickup)
        player_at(e, query_pc);
    else
        living_at(e, query_pc);
    if (!world_status(e, mc_world_runtime_nearby_entities(world, p, expanded, &list, &e->nearby),
                      "Action actual World nearby List", query_pc))
        goto done;
    if (list.identity == NULL) {
        okay = true;
        goto done;
    } /* Real source null check. */
    size_t size;
    if (!pickup) {
        living_at(e, 490);
        if (!world_status(e, mc_world_entity_list_view_count(&list, &size),
                          "Living nearby initial List size", 490))
            goto done;
        if (size == 0) {
            okay = true;
            goto done;
        }
    }
    int32_t index = 0;
    for (;;) {
        if (pickup)
            player_at(e, count_pc);
        else
            living_at(e, count_pc);
        if (!world_status(e, mc_world_entity_list_view_count(&list, &size),
                          "Action live nearby List size", count_pc))
            goto done;
        if (index >= 0 && (size_t)(uint32_t)index >= size)
            break;
        if (pickup)
            player_at(e, get_pc);
        else
            living_at(e, get_pc);
        if (!mc_action_work(e, get_pc))
            goto done;
        mc_entity *candidate = NULL;
        e->nearby_index = (size_t)(uint32_t)index;
        if (index < 0 ||
            !world_status(e,
                          mc_world_entity_list_view_get(&list, (size_t)(uint32_t)index, &candidate),
                          "Action live nearby List get", get_pc)) {
            if (index < 0)
                mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Action negative nearby List index",
                               get_pc);
            goto done;
        }
        if (!mc_action_candidate(candidate, p, e, cast_pc))
            goto done;
        if (candidate != NULL) {
            const mc_entity_status status = mc_entity_retain(candidate);
            if (!entity_status(e, status, "Action nearby local Entity hold", cast_pc))
                goto done;
            held = candidate;
        }
        if (pickup) {
            player_at(e, 233);
            if (candidate == NULL) {
                mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Player nearby null be receiver", 233);
                goto done;
            }
            if (!candidate->base.be) {
                player_at(e, 242);
                e->contact_bytecode = 2; /* Actual private gs.j(sn) invoke. */
                if (!mc_action_pickup(candidate, p, registry(context), e))
                    goto done;
            }
        } else {
            bool collidable;
            living_at(e, 527);
            if (!mc_action_collidable(candidate, &collidable, e))
                goto done;
            if (collidable) {
                living_at(e, 536);
                if (!mc_action_push(candidate, p, e))
                    goto done;
            }
        }
        mc_entity_release(held);
        held = NULL;
        index = mc_java_i32_add(index, 1);
    }
    okay = true;
done:
    mc_entity_release(held);
    mc_world_entity_list_view_release(&list);
    if (expanded != NULL)
        mc_entity_bounds_release(p->classes, expanded);
    return okay;
}

static bool living_body(mc_entity *p, const mc_player_action_context *context,
                        mc_player_action_error *e, mc_mob_tick_scope *scope) {
    mc_living_entity_fields *v = mc_entity_living_fields(p);
    living_at(e, 1);
    if (!interpolate(p, e))
        return false;
    living_at(e, 311);
    if (v->Y <= 0 || (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
                      p->local_player->fields.player.u)) {
        v->az = false;
        v->aw = 0.0f;
        v->ax = 0.0f;
        v->ay = 0.0f;
    } else if (!v->V) {
        living_at(e, 348);
        if (scope == NULL) {
            if (!mc_action_input(p, e))
                return false;
        } else if (!mc_mob_tick_behavior(scope))
            return false; /* The concrete scope retains its independent child. */
    }
    bool water, lava;
    living_at(e, 352);
    if (!entity_status(
            e,
            mc_entity_environment_in_water(p, registry(context),
                                           context == NULL ? NULL : context->travel.material_limits,
                                           &water, &e->environment),
            "Action actual cached-water virtual", 352))
        return false;
    living_at(e, 357);
    const mc_world_material_movement_limits *material =
        context == NULL ? NULL : context->travel.material_limits;
    if (!entity_status(
            e, mc_entity_environment_lava(p, registry(context), material, &lava, &e->environment),
            "Action actual lava virtual", 357))
        return false;
    if (v->az) {
        if (water || lava) {
            living_at(e, water ? 380u : 399u);
            if (!mc_action_fp(e, water ? 380u : 399u))
                return false;
            p->base.velocity_y = ac_add(p->base.velocity_y, ac_widen(.04f));
        } else if (p->base.aX) {
            living_at(e, 414);
            e->child_kind = MC_PLAYER_ACTION_CHILD_TRAVEL;
            if (scope == NULL) {
                if (!entity_status(e, mc_player_jump(p, travel_context(context), &e->travel),
                                   "Action actual Player jump virtual", 414))
                    return false;
            } else if (!mc_living_jump_entered(p, &e->travel))
                return false;
            e->child_kind = MC_PLAYER_ACTION_CHILD_NONE;
        }
    }
    living_at(e, 424);
    if (!mc_action_fp(e, 424))
        return false;
    v->aw = ac_fmul(v->aw, .98f);
    living_at(e, 435);
    v->ax = ac_fmul(v->ax, .98f);
    living_at(e, 446);
    v->ay = ac_fmul(v->ay, .9f);
    living_at(e, 459);
    e->child_kind = MC_PLAYER_ACTION_CHILD_TRAVEL;
    if (scope == NULL) {
        if (!entity_status(e,
                           mc_player_travel(p, travel_context(context), v->aw, v->ax, &e->travel),
                           "Action actual Player travel virtual", 459))
            return false;
    } else {
        mc_player_travel_error_init(&e->travel);
        if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SQUID)) {
            e->travel.source_class = "xt";
            e->travel.source_method = "a_(FF)V";
            e->travel.living_bytecode = 13;
            const mc_entity_status status = mc_entity_move_in_context(
                p, registry(context), p->base.velocity_x, p->base.velocity_y, p->base.velocity_z,
                context == NULL ? NULL : context->travel.movement_limits, &e->travel.movement);
            if (!entity_status(e, status, "Squid actual move-current-velocity", 459)) {
                e->travel.status = status;
                e->travel.cause = MC_PLAYER_TRAVEL_MOVEMENT;
                e->travel.operation = "Squid actual move-current-velocity";
                return false;
            }
        } else if (!mc_living_travel_mob_entered(scope, v->aw, v->ax, &e->travel)) {
            (void)entity_status(e, e->travel.status, "Action actual Living travel virtual", 459);
            return false;
        }
    }
    e->child_kind = MC_PLAYER_ACTION_CHILD_NONE;
    return nearby(p, false, context, e);
}

static bool player_body(mc_entity *p, const mc_player_action_context *context,
                        mc_player_action_error *e) {
    mc_living_entity_fields *v = &p->local_player->fields.living;
    mc_player_fields *player = &p->local_player->fields.player;
    player_at(e, 4);
    if (p->base.world == NULL)
        return mc_action_fail(e, MC_ENTITY_STATE_ERROR,
                              "Player action null World difficulty receiver", 4);
    if (p->base.world->fields.q == 0 && v->Y < 20 && mc_java_i32_mul(p->base.bt % 20, 12) == 0) {
        /* Actual ls.c(1), including its independent nonpositive-health guard. */
        if (v->Y > 0) {
            v->Y = mc_java_i32_add(v->Y, 1);
            if (v->Y > 20)
                v->Y = 20;
            p->base.by = v->E / 2;
        }
    }
    player_at(e, 41);
    const mc_inventory_tick_status inventory =
        mc_inventory_player_tick(player->c, registry(context),
                                 context == NULL ? NULL : context->inventory_limits, &e->inventory);
    if (inventory != MC_INVENTORY_TICK_OK)
        return mc_action_fail(e,
                              inventory == MC_INVENTORY_TICK_LIMIT ? MC_ENTITY_LIMIT
                              : inventory == MC_INVENTORY_TICK_ALLOCATION_FAILED
                                  ? MC_ENTITY_ALLOCATION_FAILED
                              : inventory == MC_INVENTORY_TICK_BUSY         ? MC_ENTITY_BUSY
                              : inventory == MC_INVENTORY_TICK_WORLD_FAILED ? MC_ENTITY_WORLD_FAILED
                                                                            : MC_ENTITY_STATE_ERROR,
                              "Player action actual InventoryPlayer tick", 41);
    player->h = player->i;
    player_at(e, 53);
    if (!living_body(p, context, e, NULL))
        return false;
    player_at(e, 64);
    if (!mc_action_fp(e, 64))
        return false;
    const float horizontal = ac_narrow(_mm_cvtsd_f64(_mm_sqrt_sd(
        _mm_setzero_pd(), _mm_set_sd(ac_add(ac_mul(p->base.velocity_x, p->base.velocity_x),
                                            ac_mul(p->base.velocity_z, p->base.velocity_z))))));
    player_at(e, 88);
    float vertical = ac_fmul(
        ac_narrow(mc_action_atan(ac_mul(ac_neg(p->base.velocity_y), ac_widen(.2f)))), 15.0f);
    float magnitude = horizontal;
    if (magnitude > .1f)
        magnitude = .1f;
    if (!p->base.aX || v->Y <= 0)
        magnitude = 0.0f;
    if (p->base.aX || v->Y <= 0)
        vertical = 0.0f;
    player->i = ac_fadd(player->i, ac_fmul(ac_fsub(magnitude, player->i), .4f));
    v->ag = ac_fadd(v->ag, ac_fmul(ac_fsub(vertical, v->ag), .8f));
    if (v->Y > 0)
        return nearby(p, true, context, e);
    return true;
}

static mc_entity_status run(mc_entity *p, const mc_player_action_context *context,
                            const mc_player_action_limits *limits, bool player,
                            mc_player_action_error *out) {
    /* Full nested World/Player causes need independent per-invocation storage,
     * including synchronous callback reentry. Native workspace failure precedes
     * source work and is not a fabricated Java allocation exception. */
    mc_player_action_error *frame = malloc(sizeof(*frame));
    if (frame == NULL) {
        mc_player_action_error_init(out);
        if (out != NULL) {
            out->status = MC_ENTITY_ALLOCATION_FAILED;
            out->operation = "Player action diagnostic workspace";
        }
        return MC_ENTITY_ALLOCATION_FAILED;
    }
    mc_player_action_error_init(frame);
    frame->max_loop_work =
        limits == NULL ? MC_PLAYER_ACTION_DEFAULT_LOOP_WORK : limits->max_loop_work;
    if (mc_action_enter(p, frame)) {
        if (player)
            (void)player_body(p, context, frame);
        else
            (void)living_body(p, context, frame, NULL);
        mc_entity_release(p);
    }
    const mc_entity_status status = mc_action_finish(frame, out);
    free(frame);
    return status;
}

mc_entity_status mc_player_action(mc_entity *p, const mc_player_action_context *c,
                                  const mc_player_action_limits *l, mc_player_action_error *e) {
    return run(p, c, l, true, e);
}

mc_entity_status mc_living_action_body(mc_entity *p, const mc_player_action_context *c,
                                       const mc_player_action_limits *l,
                                       mc_player_action_error *e) {
    return run(p, c, l, false, e);
}

bool mc_entity_mob_action_entered(mc_mob_tick_scope *scope, mc_player_action_error *error) {
    error->max_loop_work = scope->action_limits == NULL ? MC_PLAYER_ACTION_DEFAULT_LOOP_WORK
                                                        : scope->action_limits->max_loop_work;
    return living_body(scope->entity, scope->action_context, error, scope);
}
