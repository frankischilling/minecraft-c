#include "internal.h"

#include <string.h>

static void following_at(mc_mob_tick_scope *s, unsigned int pc) {
    mt_at(s, "ii", "f_", "()V", pc);
}

static bool waypoint(mc_mob_tick_scope *s, unsigned int field_pc, unsigned int call_pc,
                     mc_entity_vector **out) {
    mc_path *path;
    following_at(s, field_pc);
    if (!mt_path_get(s, &path))
        return false;
    mc_path_error error;
    if (path != NULL && mc_path_retain(path, &error) != MC_PATH_OK)
        return mt_path_failed(s, &error, "Creature waypoint local Path occurrence");
    following_at(s, call_pc);
    const mc_path_status status = mc_path_waypoint(path, s->entity, out, &error);
    mc_path_release(path);
    if (status != MC_PATH_OK)
        return mt_path_failed(s, &error, "Creature actual Path waypoint");
    ++s->error->state.path_waypoints;
    return true;
}

static bool advance(mc_mob_tick_scope *s, bool *finished) {
    mc_path *path;
    following_at(s, 319);
    if (!mt_path_get(s, &path))
        return false;
    following_at(s, 322);
    if (!mt_work(s))
        return false;
    mc_path_error error;
    if (mc_path_advance(path, &error) != MC_PATH_OK)
        return mt_path_failed(s, &error, "Creature actual Path advance");
    ++s->error->state.path_advances;
    following_at(s, 326);
    if (!mt_path_get(s, &path))
        return false;
    following_at(s, 329);
    if (mc_path_finished(path, finished, &error) != MC_PATH_OK)
        return mt_path_failed(s, &error, "Creature actual Path completion");
    return true;
}

static bool distance(mc_mob_tick_scope *s, const mc_entity_vector *vector, double *out) {
    following_at(s, 306);
    mc_entity_geometry_error error;
    const mc_entity_geometry_status status = mc_entity_vector_distance_squared_to(
        s->entity->classes, vector, s->entity->base.x, vector->y, s->entity->base.z, out, &error);
    if (status == MC_ENTITY_GEOMETRY_OK)
        return true;
    s->error->child = MC_MOB_TICK_CHILD_PATH;
    memset(&s->error->detail.path, 0, sizeof(s->error->detail.path));
    s->error->detail.path.kind = MC_MOB_TICK_PATH_CHILD_GEOMETRY;
    s->error->detail.path.geometry = error;
    (void)mt_fail(s,
                  error.native_status == MC_ENTITY_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR,
                  "Creature actual waypoint squared distance");
    s->error->native_policy = error.native_policy;
    s->error->child_site = (mc_mob_site){error.site.declaring_class, error.site.method,
                                         error.site.descriptor, error.site.bytecode};
    if (status == MC_ENTITY_GEOMETRY_SOURCE_NULL_REFERENCE) {
        s->error->source_exception = true;
        s->error->native_policy = false;
        s->error->exception_class = "java/lang/NullPointerException";
    }
    return false;
}

static bool turn(mc_mob_tick_scope *s, float *difference) {
    while (*difference < -180.0f) {
        following_at(s, 453);
        if (!mt_work(s))
            return false;
        *difference = at_fadd(*difference, 360.0f);
    }
    while (*difference >= 180.0f) {
        following_at(s, 471);
        if (!mt_work(s))
            return false;
        *difference = at_fsub(*difference, 360.0f);
    }
    if (*difference > 30.0f)
        *difference = 30.0f;
    if (*difference < -30.0f)
        *difference = -30.0f;
    return true;
}

static bool attack_heading(mc_mob_tick_scope *s) {
    if (!s->entity->mob->creature_attacking)
        return true;
    mc_entity *target;
    following_at(s, 520);
    if (!mt_target_get(s, true, &target))
        return false;
    if (target == NULL)
        return true;
    following_at(s, 527);
    if (!mt_target_get(s, true, &target))
        return false;
    if (target == NULL)
        return mt_null(s, "Creature facing null target X receiver");
    const double x = at_sub(target->base.x, s->entity->base.x);
    following_at(s, 541);
    if (!mt_target_get(s, true, &target))
        return false;
    if (target == NULL)
        return mt_null(s, "Creature facing null target Z receiver");
    const double z = at_sub(target->base.z, s->entity->base.z);
    const float previous = s->entity->base.yaw;
    following_at(s, 565);
    s->entity->base.yaw = at_fsub(
        at_narrow(at_div(at_mul(mc_player_tick_atan2(z, x), 180.0), 3.1415927410125732)), 90.0f);
    const float difference = at_fdiv(
        at_fmul(at_fadd(at_fsub(previous, s->entity->base.yaw), 90.0f), 3.1415927f), 180.0f);
    following_at(s, 604);
    if (!mt_math(s))
        return false;
    mc_living_entity_fields *living = mt_fields(s);
    living->aw = at_fmul(
        at_fmul(at_fneg(mc_math_sin(mc_math_classes_table(s->math), difference)), living->ax),
        1.0f);
    following_at(s, 621);
    if (!mt_math(s))
        return false;
    living->ax =
        at_fmul(at_fmul(mc_math_cos(mc_math_classes_table(s->math), difference), living->ax), 1.0f);
    return true;
}

bool mt_follow_path(mc_mob_tick_scope *s, int32_t foot_y, bool water, bool lava) {
    mc_entity *p = s->entity;
    mc_living_entity_fields *living = mt_fields(s);
    mc_entity_vector *vector = NULL;
    bool ok = false;
    if (!waypoint(s, 268, 272, &vector))
        goto done;
    following_at(s, 282);
    if (!mt_fp(s))
        goto done;
    const double radius = at_widen(at_fmul(p->base.width, 2.0f));
    while (vector != NULL) {
        double squared;
        if (!distance(s, vector, &squared))
            goto done;
        if (!(squared < at_mul(radius, radius)))
            break;
        bool finished;
        if (!advance(s, &finished))
            goto done;
        if (finished) {
            /* Actual astore5 at336 precedes the source Path clear340. */
            (void)mc_entity_vector_release(p->classes, vector);
            vector = NULL;
            following_at(s, 340);
            if (!mt_path_store(s, NULL))
                goto done;
        } else {
            mc_entity_vector *next = NULL;
            if (!waypoint(s, 347, 351, &next))
                goto done;
            (void)mc_entity_vector_release(p->classes, vector);
            vector = next;
        }
    }
    following_at(s, 361);
    living->az = false;
    if (vector != NULL) {
        const double x = at_sub(vector->x, p->base.x), z = at_sub(vector->z, p->base.z);
        const double y = at_sub(vector->y, (double)foot_y);
        following_at(s, 407);
        const float heading = at_fsub(
            at_narrow(at_div(at_mul(mc_player_tick_atan2(z, x), 180.0), 3.1415927410125732)),
            90.0f);
        float difference = at_fsub(heading, p->base.yaw);
        following_at(s, 438);
        living->ax = living->aB;
        if (!turn(s, &difference))
            goto done;
        following_at(s, 509);
        p->base.yaw = at_fadd(p->base.yaw, difference);
        if (!attack_heading(s))
            goto done;
        if (y > 0.0) {
            following_at(s, 643);
            living->az = true;
        }
    }
    mc_entity *target;
    following_at(s, 647);
    if (!mt_target_get(s, true, &target))
        goto done;
    if (target != NULL) {
        following_at(s, 656);
        if (!mt_target_get(s, true, &target))
            goto done;
        following_at(s, 662);
        if (target != NULL) {
            const mc_entity_status status = mc_entity_retain(target);
            if (status != MC_ENTITY_OK) {
                (void)mt_fail(s, status, "Creature final facing local target occurrence");
                goto done;
            }
        }
        const bool looked = mt_look(s, target, 30.0f, 30.0f);
        mc_entity_release(target);
        if (!looked)
            goto done;
    }
    if (p->base.aY) {
        bool present;
        following_at(s, 673);
        if (!mt_path_present(s, &present))
            goto done;
        if (!present) {
            following_at(s, 681);
            living->az = true;
        }
    }
    following_at(s, 688);
    float draw;
    if (!mt_float(s, &draw))
        goto done;
    if (draw < .8f && (water || lava)) {
        following_at(s, 708);
        living->az = true;
    }
    following_at(s, 711);
    ok = true;
done:
    /* Every waypoint is a genuine held object in this same namespace. The
     * following body has no source callback between its acquisition and drop. */
    if (vector != NULL)
        (void)mc_entity_vector_release(p->classes, vector);
    return ok;
}
