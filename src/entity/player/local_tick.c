#include "active_internal.h"

static void at(mc_player_tick_error *e, unsigned int pc) {
    e->source_class = "dc";
    e->source_method = "o()V";
    e->local_bytecode = pc;
}

static bool achievement(mc_active_scope *s, const mc_statistic **out, mc_player_tick_error *e,
                        unsigned int pc) {
    at(e, pc);
    if (!mc_active_registry(s, e))
        return false;
    if (!mc_active_registry_result(
            e, mc_registry_classes_initialize_achievements(s->registry, &e->registry)))
        return false;
    mc_registry_achievement_view view;
    if (!mc_active_registry_result(
            e, mc_registry_classes_inspect_achievements(s->registry, &view, &e->registry)))
        return false;
    *out = view.named[0];
    return true;
}

static bool tutorial(mc_active_scope *s, mc_player_tick_error *e) {
    mc_entity *p = s->player;
    at(e, 4);
    client_application *app = p->local_player->fields.minecraft;
    if (app == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Local action null Minecraft I receiver");
    mc_client_stats_writer *writer = mc_client_application_live_stats_writer(app);
    const mc_statistic *statistic;
    if (!achievement(s, &statistic, e, 7))
        return false;
    at(e, 10);
    if (writer == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                              "Local action null statistics writer receiver");
    bool present;
    const mc_client_stats_status status =
        mc_client_stats_writer_contains(writer, statistic, &present, &e->writer);
    if (status != MC_CLIENT_STATS_OK)
        return mc_active_fail(e,
                              status == MC_CLIENT_STATS_LIMIT ? MC_ENTITY_LIMIT
                              : status == MC_CLIENT_STATS_ALLOCATION_FAILED
                                  ? MC_ENTITY_ALLOCATION_FAILED
                              : status == MC_CLIENT_STATS_BUSY ? MC_ENTITY_BUSY
                                                               : MC_ENTITY_STATE_ERROR,
                              "Local action actual achievement presence child");
    if (present)
        return true;
    at(e, 20);
    app = p->local_player->fields.minecraft;
    if (app == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                              "Local action null Minecraft notice receiver");
    mc_client_achievement_notice *notice = mc_client_application_live_achievement_notice(app);
    if (!achievement(s, &statistic, e, 23))
        return false;
    at(e, 26);
    if (notice == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                              "Local action null tutorial notice receiver");
    const mc_client_notice_status notice_status =
        mc_client_achievement_notice_remind(notice, statistic, &e->notice);
    return notice_status == MC_CLIENT_NOTICE_OK ||
           mc_active_fail(e,
                          notice_status == MC_CLIENT_NOTICE_ALLOCATION_FAILED
                              ? MC_ENTITY_ALLOCATION_FAILED
                              : MC_ENTITY_STATE_ERROR,
                          "Local action actual tutorial reminder child");
}

/* Exact sn.i(null), the only argument reached by this portal caller. */
static bool dismount(mc_active_scope *s, mc_player_tick_error *e) {
    mc_entity *p = s->player;
    e->dismount_bytecode = 2;
    p->base.private_d = 0.0;
    e->dismount_bytecode = 7;
    p->base.private_e = 0.0;
    mc_entity *mount = p->base.aH;
    if (mount == NULL)
        return true;
    const mc_entity_status held = mc_entity_retain(mount);
    if (held != MC_ENTITY_OK)
        return mc_active_fail(e, held, "Portal dismount actual mount local hold");
    bool okay = false;
    const double x = mount->base.x;
    e->dismount_bytecode = 36;
    if (mount->bounds == NULL) {
        mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Portal dismount null mount bounds");
        goto done;
    }
    e->dismount_bytecode = 47;
    if (!mc_active_fp(e))
        goto done;
    const double y = at_add(mount->bounds->min_y, at_widen(mount->base.height)), z = mount->base.z;
    const float yaw = p->base.yaw, pitch = p->base.aT;
    e->dismount_bytecode = 63;
    /* Actual sn.c(DDDFF), including its pre-position previous stores. */
    p->base.x = x;
    p->base.aJ = x;
    p->base.bl = x;
    p->base.y = at_add(y, at_widen(p->base.offset));
    p->base.aK = p->base.y;
    p->base.bm = p->base.y;
    p->base.z = z;
    p->base.aL = z;
    p->base.bn = z;
    p->base.yaw = yaw;
    p->base.aT = pitch;
    e->position_bytecode = 77;
    if (p->bounds == NULL) {
        mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Portal dismount null Player permanent bounds");
        goto done;
    }
    mc_entity_source_position(p, p->base.x, p->base.y, p->base.z);
    e->dismount_bytecode = 71;
    p->base.aH->base.aG = NULL;
    e->dismount_bytecode = 76;
    p->base.aH = NULL;
    okay = true;
done:
    mc_entity_release(mount);
    return okay;
}

static bool portals(mc_active_scope *s, mc_player_tick_error *e) {
    mc_entity *p = s->player;
    mc_player_fields *v = &p->local_player->fields.player;
    v->C = v->B;
    if (v->A) {
        at(e, 48);
        if (p->base.world == NULL)
            return mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Portal action null World B receiver");
        if (!p->base.world->fields.B && p->base.aH != NULL) {
            at(e, 63);
            if (!dismount(s, e))
                return false;
        }
        at(e, 70);
        if (p->local_player->fields.minecraft == NULL)
            return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                                  "Portal action null Minecraft screen receiver");
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                              "Portal action genuine Minecraft.r screen owner dependency");
    }
    at(e, 206);
    if (!mc_active_fp(e))
        return false;
    if (v->B > 0.0f) {
        at(e, 217);
        v->B = at_fsub(v->B, .05f);
    }
    if (v->B < 0.0f)
        v->B = 0.0f;
    if (v->z > 0)
        v->z = mc_java_i32_sub(v->z, 1);
    return true;
}

static bool corners(mc_active_scope *s, mc_player_tick_error *e) {
    mc_entity *p = s->player;
    static const unsigned int arithmetic[] = {299, 343, 387, 431}, bounds[] = {305, 349, 393, 437},
                              call[] = {326, 370, 414, 458};
    for (size_t i = 0; i < 4; ++i) {
        at(e, arithmetic[i]);
        if (!mc_active_fp(e))
            return false;
        const double offset = at_mul(at_widen(p->base.width), .35);
        const double x = i < 2 ? at_sub(p->base.x, offset) : at_add(p->base.x, offset);
        at(e, bounds[i]);
        if (p->bounds == NULL)
            return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                                  "Local push-out arguments null permanent bounds");
        const double y = at_add(p->bounds->min_y, .5);
        const double z = (i == 0 || i == 3)
                             ? at_add(p->base.z, at_mul(at_widen(p->base.width), .35))
                             : at_sub(p->base.z, at_mul(at_widen(p->base.width), .35));
        at(e, call[i]);
        e->pushout_call_bytecode = call[i];
        if (!mc_active_pushout(s, x, y, z, e))
            return false;
    }
    return true;
}

bool mc_active_local(mc_active_scope *s, mc_player_tick_error *e) {
    mc_entity *p = s->player;
    if (!tutorial(s, e) || !portals(s, e))
        return false;
    at(e, 257);
    mc_movement_input *input = p->local_player->fields.input;
    if (input == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Local action null Input update receiver");
    const mc_movement_input_status status = mc_movement_input_update(input, p, &e->input);
    if (status != MC_MOVEMENT_INPUT_OK)
        return mc_active_fail(e,
                              status == MC_MOVEMENT_INPUT_UNSUPPORTED_FP ? MC_ENTITY_UNSUPPORTED_FP
                              : status == MC_MOVEMENT_INPUT_LIMIT        ? MC_ENTITY_LIMIT
                              : status == MC_MOVEMENT_INPUT_ALLOCATION_FAILED
                                  ? MC_ENTITY_ALLOCATION_FAILED
                                  : MC_ENTITY_STATE_ERROR,
                              "Local action actual Input update child");
    at(e, 264);
    input = p->local_player->fields.input;
    if (input == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Local action null Input sneak receiver");
    if (input->fields.sneak) {
        at(e, 276);
        if (!mc_active_fp(e))
            return false;
        if (p->base.y_size < .2f)
            p->base.y_size = .2f;
    }
    if (!corners(s, e))
        return false;
    at(e, 463);
    e->child = MC_PLAYER_TICK_CHILD_ACTION;
    const mc_entity_status action = mc_player_action(
        p, s->context, s->limits == NULL ? NULL : &s->limits->action, &e->child_error.action);
    return action == MC_ENTITY_OK ||
           mc_active_fail(e, action, "Local action actual GS action body");
}
