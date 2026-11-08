#include "internal.h"

#include "../../arrow/internal.h"
#include "mc/world/runtime/entities.h"

static void at(mc_mob_tick_scope *s, unsigned int pc) {
    mt_at(s, "fr", "a", "(Lsn;F)V", pc);
}

static bool returned(mc_mob_tick_scope *s) {
    at(s, 188);
    s->error->state.attack.returned = true;
    s->error->state.attack.caller_return_site = s->error->site;
    if (s->skeleton_capture != NULL)
        s->skeleton_capture->returned = true;
    return true;
}

static bool arrow_failure(mc_mob_tick_scope *s, mc_entity_status status) {
    const mc_arrow_error *e = &s->error->detail.arrow;
    const mc_arrow_site child = e->child.class_name == NULL ? e->site : e->child;
    s->error->child_site =
        (mc_mob_site){child.class_name, child.method, child.descriptor, child.pc};
    s->error->source_exception = e->source_exception;
    s->error->exception_class = e->exception_class;
    return mt_fail(s, status, e->operation);
}

static bool target_eye(mc_mob_tick_scope *s, mc_entity *target, float *out) {
    if (mc_entity_has_arrow(target)) {
        *out = 0.0f; /* Actual inherited SN.w: fconst_0; freturn. */
        return true;
    }
    const bool player = mc_entity_is_kind(target, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER);
    const bool wolf = mc_entity_is_kind(target, MC_ENTITY_CLASS_KIND_WOLF);
    s->error->child_site = (mc_mob_site){player ? "gs"
                                         : wolf ? "gi"
                                                : "ls",
                                         "w", "()F", player ? 2 : 6};
    if (!mt_eye(s, target, out))
        return false;
    s->error->child_site = (mc_mob_site){0};
    return true;
}

/* The runner or real II97 caller owns its evaluated target argument through
 * synchronous callbacks. This body owns only its actual NEW Arrow local. */
bool mt_skeleton_attack(mc_mob_tick_scope *s, mc_entity *target, float distance) {
    mc_entity *p = s->entity, *arrow = NULL;
    mt_skeleton_fire_observation *capture = s->skeleton_capture;
    bool okay = false;
    s->error->state.attack = (mc_mob_tick_attack_arguments){
        .stored = true, .target_identity = (uintptr_t)target, .distance = distance};
    if (capture != NULL) {
        capture->entered = true;
        capture->target_identity = (uintptr_t)target;
        capture->distance = distance;
    }
    at(s, 3);
    if (!mt_fp(s))
        goto done;
    if (!(distance < 10.0f)) { /* fcmpg3: NaN also takes return188. */
        okay = returned(s);
        goto done;
    }
    at(s, 8);
    if (target == NULL) {
        (void)mt_null(s, "Skeleton attack null target X field receiver");
        goto done;
    }
    const double target_x = target->base.x;
    const double dx = at_sub(target_x, p->base.x);
    at(s, 18);
    const double target_z = target->base.z;
    const double dz = at_sub(target_z, p->base.z);
    if (capture != NULL) {
        capture->dx = dx;
        capture->dz = dz;
    }
    at(s, 29);
    if (mt_fields(s)->ae == 0) {
        /* NEW precedes constructor argument evaluation, even null World40. */
        at(s, 35);
        s->error->child = MC_MOB_TICK_CHILD_ARROW;
        const mc_entity_status allocated =
            mc_arrow_allocate(p->classes, &arrow, &s->error->reference);
        if (allocated != MC_ENTITY_OK) {
            s->error->detail.arrow = (mc_arrow_error){.status = allocated,
                                                      .operation = s->error->reference.operation,
                                                      .native_policy = true,
                                                      .site = {NULL, NULL, NULL, -1},
                                                      .child = {NULL, NULL, NULL, -1},
                                                      .heading_site = {NULL, NULL, NULL, -1},
                                                      .math = {.source_pc = -1},
                                                      .entity = s->error->reference};
            s->error->child_site = (mc_mob_site){.pc = -1};
            (void)mt_fail(s, allocated, s->error->reference.operation);
            goto done;
        }
        if (capture != NULL) {
            capture->allocated = true;
            capture->arrow_identity = (uintptr_t)arrow;
        }
        mc_world_runtime *constructor_world = p->base.world; /* Actual field40. */
        if (capture != NULL) {
            capture->constructor_world_identity = (uintptr_t)constructor_world;
            capture->constructor_invoked = true;
        }
        at(s, 44);
        const mc_entity_status constructed =
            capture == NULL
                ? mc_arrow_construct_living(arrow, constructor_world, p, s->math,
                                            &s->error->detail.arrow)
                : mc_arrow_construct_living_observed(arrow, constructor_world, p, s->math,
                                                     &capture->constructor_arguments,
                                                     &s->error->detail.arrow);
        if (capture != NULL)
            capture->constructor = s->error->detail.arrow;
        if (constructed != MC_ENTITY_OK) {
            (void)arrow_failure(s, constructed);
            goto done;
        }
        if (capture != NULL) {
            capture->constructor_returned = true;
            capture->constructor_motion[0] = arrow->base.velocity_x;
            capture->constructor_motion[1] = arrow->base.velocity_y;
            capture->constructor_motion[2] = arrow->base.velocity_z;
            capture->constructor_angles[0] = arrow->base.yaw;
            capture->constructor_angles[1] = arrow->base.aT;
            capture->constructor_angles[2] = arrow->base.aU;
            capture->constructor_angles[3] = arrow->base.aV;
            capture->constructor_j = arrow->arrow->fields.j;
        }
        s->error->child = MC_MOB_TICK_CHILD_NONE;
        s->error->child_site = (mc_mob_site){0};
        at(s, 59);
        arrow->base.y = at_add(arrow->base.y, 1.399999976158142); /* No bounds rebuild. */
        at(s, 63);
        const double target_y = target->base.y;
        float eye;
        at(s, 67);
        if (!target_eye(s, target, &eye))
            goto done;
        const double dy =
            at_sub(at_sub(at_add(target_y, at_widen(eye)), .20000000298023224), arrow->base.y);
        if (capture != NULL) {
            capture->eye_returned = true;
            capture->eye_height = eye;
            capture->dy = dy;
        }
        at(s, 93);
        if (!mt_math(s))
            goto done;
        const float lift = at_fmul(at_root(at_add(at_mul(dx, dx), at_mul(dz, dz))), .2f);
        mc_world_runtime *sound_world = p->base.world; /* Capture nullable field102. */
        const mc_registry_string *sound;
        if (capture != NULL) {
            capture->lift = lift;
            capture->sound_world_identity = (uintptr_t)sound_world;
        }
        at(s, 106);
        if (!mt_literal(s, "random.bow", &sound))
            goto done;
        mc_java_random *random = p->random; /* Actual field111, one evaluated receiver. */
        if (capture != NULL) {
            capture->actor_random_identity = (uintptr_t)random;
            if (random != NULL) {
                capture->actor_random_captured = true;
                capture->actor_random_before = *random;
            }
        }
        at(s, 114);
        if (random == NULL) {
            (void)mt_null(s, "Skeleton attack null private Random receiver");
            goto done;
        }
        const float draw = mc_java_random_next_float(random);
        ++s->error->state.concrete_random_calls;
        if (capture != NULL) {
            capture->actor_random_after = *random;
            capture->actor_float = draw;
            capture->actor_float_returned = true;
        }
        const float pitch = at_fdiv(1.0f, at_fadd(at_fmul(draw, .4f), .8f));
        at(s, 124);
        if (capture != NULL) {
            capture->sound_volume = 1.0f;
            capture->sound_pitch = pitch;
            capture->sound_invoked = true;
        }
        if (!mt_sound_prepared(s, sound_world, sound, 1.0f, pitch))
            goto done;
        if (capture != NULL)
            capture->sound_returned = true;
        mc_world_runtime *publication_world = p->base.world; /* Fresh field128 after callback. */
        at(s, 133);
        if (capture != NULL) {
            capture->publication_world_identity = (uintptr_t)publication_world;
            capture->publication_invoked = true;
        }
        if (publication_world == NULL) {
            (void)mt_null(s, "Skeleton attack null captured publication World receiver");
            goto done;
        }
        bool published = false;
        s->error->child = MC_MOB_TICK_CHILD_ENTITY_PUBLICATION;
        const mc_world_storage_status inserted = mc_world_runtime_insert_entity(
            publication_world, arrow, &published, &s->error->detail.publication);
        if (inserted != MC_WORLD_STORAGE_OK) {
            const mc_world_entity_error *e = &s->error->detail.publication;
            const char *exception = NULL;
            if (mc_world_access_failure_source_exception(&e->world_access_failure, &exception)) {
                s->error->source_exception = true;
                s->error->exception_class = exception;
            }
            s->error->child_site = (mc_mob_site){"fd", "b", "(Lsn;)Z", (int32_t)e->bytecode};
            (void)mt_fail(s, MC_ENTITY_WORLD_FAILED, "Skeleton actual Arrow publication child");
            goto done;
        }
        if (capture != NULL) {
            capture->publication_returned = true;
            capture->publication_result = published;
        }
        at(s, 136); /* Pop either actual returned Boolean. */
        s->error->child = MC_MOB_TICK_CHILD_NONE;
        s->error->child_site = (mc_mob_site){0};
        const double heading_y = at_add(dy, at_widen(lift));
        at(s, 152);
        if (capture != NULL) {
            capture->heading_invoked = true;
            capture->heading_x = dx;
            capture->heading_y = heading_y;
            capture->heading_z = dz;
            capture->heading_speed = .6f;
            capture->heading_spread = 12.0f;
        }
        s->error->child = MC_MOB_TICK_CHILD_ARROW;
        const mc_entity_status headed =
            capture == NULL
                ? mc_arrow_heading(arrow, s->math, dx, heading_y, dz, .6f, 12.0f,
                                   &s->error->detail.arrow)
                : mc_arrow_heading_observed(arrow, s->math, dx, heading_y, dz, .6f, 12.0f,
                                            &capture->final_arguments, &s->error->detail.arrow);
        if (capture != NULL)
            capture->final_heading = s->error->detail.arrow;
        if (headed != MC_ENTITY_OK) {
            (void)arrow_failure(s, headed);
            goto done;
        }
        if (capture != NULL)
            capture->heading_returned = true;
        s->error->child = MC_MOB_TICK_CHILD_NONE;
        s->error->child_site = (mc_mob_site){0};
        at(s, 158);
        mt_fields(s)->ae = 30;
        if (capture != NULL)
            capture->cooldown_stored = true;
    }
    at(s, 165);
    if (!mt_fp(s))
        goto done;
    p->base.yaw = at_fsub(
        at_narrow(at_div(at_mul(mc_player_tick_atan2(dz, dx), 180.0), 3.1415927410125732)), 90.0f);
    if (capture != NULL)
        capture->facing_stored = true;
    at(s, 185);
    p->mob->creature_attacking = true;
    if (capture != NULL)
        capture->attacking_stored = true;
    okay = returned(s);
done:
    if (s->skeleton_arrow_output != NULL)
        *s->skeleton_arrow_output = arrow; /* Move this actual allocation occurrence. */
    else
        mc_entity_release(arrow);
    return okay;
}
