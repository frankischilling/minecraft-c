#include "tick_internal.h"

static bool bounds_transform(mc_arrow_tick_call *c, const mc_entity_bounds *receiver, double x,
                             double y, double z, bool sweep, unsigned int pc,
                             mc_entity_bounds **out) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_BOUNDS, pc);
    if (receiver == NULL)
        return mc_arrow_tick_null(c, "Arrow null bounds virtual receiver");
    mc_entity_bounds *next = NULL;
    const mc_entity_status status =
        sweep ? mc_entity_bounds_sweep(c->state.receiver->classes, receiver, x, y, z, &next,
                                       &c->bounds_error)
              : mc_entity_bounds_expand(c->state.receiver->classes, receiver, x, y, z, &next,
                                        &c->bounds_error);
    if (status != MC_ENTITY_OK) {
        c->error.bounds = &c->bounds_error;
        return mc_arrow_tick_fail(
            c, status == MC_ENTITY_LIMIT ? MC_ARROW_TICK_LIMIT : MC_ARROW_TICK_CHILD_FAILED,
            c->bounds_error.operation, true, NULL);
    }
    c->replaced_bounds = *out;
    *out = next; /* Publish the completed source assignment before native cleanup. */
    if (!mc_arrow_tick_bounds_drop(c, &c->replaced_bounds))
        return false;
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

static bool hit_view(mc_arrow_tick_call *c, mc_entity_hit_result *hit, unsigned int pc,
                     mc_entity_hit_result_view *out) {
    mc_arrow_tick_at(c, pc);
    if (hit == NULL)
        return mc_arrow_tick_null(c, "Arrow null VF field receiver");
    if (mc_entity_hit_result_inspect(hit, out) != MC_ENTITY_HIT_OK ||
        out->phase != MC_ENTITY_HIT_READY) {
        c->work_tag = MC_ARROW_TICK_CHILD_HIT;
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_STATE, "Arrow actual READY VF source local",
                                  true, NULL);
    }
    return true;
}

static bool hit_point(mc_arrow_tick_call *c, mc_entity_hit_result *hit, unsigned int pc,
                      const mc_entity_vector **out) {
    mc_entity_hit_result_view view;
    if (!hit_view(c, hit, pc, &view))
        return false;
    if (view.fields.f == NULL)
        return mc_arrow_tick_null(c, "Arrow null current VF point field receiver");
    *out = view.fields.f;
    return true;
}

static bool held_local(mc_arrow_tick_call *c, mc_entity *value, unsigned int pc, mc_entity **out) {
    mc_arrow_tick_at(c, pc);
    if (value != NULL) {
        if (value->holds == 0) {
            c->work_tag = MC_ARROW_TICK_CHILD_OWNER;
            return mc_arrow_tick_fail(c, MC_ARROW_TICK_STATE,
                                      "Arrow actual held Entity source local", true, NULL);
        }
        const mc_entity_status status = mc_entity_retain(value);
        if (status != MC_ENTITY_OK) {
            c->entity_error = (mc_entity_error){.status = status,
                                                .operation = "Arrow evaluated Entity local hold"};
            c->error.entity = &c->entity_error;
            c->work_tag = MC_ARROW_TICK_CHILD_OWNER;
            return mc_arrow_tick_fail(
                c, status == MC_ENTITY_LIMIT ? MC_ARROW_TICK_LIMIT : MC_ARROW_TICK_CHILD_FAILED,
                c->entity_error.operation, true, NULL);
        }
    }
    mc_entity *old = *out;
    *out = value;
    mc_entity_release(old);
    return true;
}

static bool shooter(mc_arrow_tick_call *c, unsigned int pc, mc_entity **out) {
    mc_arrow_tick_at(c, pc);
    const mc_entity_status status = mc_arrow_shooter_get(c->state.receiver, out, &c->entity_error);
    if (status == MC_ENTITY_OK)
        return true;
    c->work_tag = MC_ARROW_TICK_CHILD_OWNER;
    c->error.entity = &c->entity_error;
    return mc_arrow_tick_fail(c, MC_ARROW_TICK_STATE, c->entity_error.operation, true, NULL);
}

static bool impact_sound(mc_arrow_tick_call *c, unsigned int world_pc, unsigned int random_pc,
                         unsigned int invoke_pc) {
    mc_entity *const p = c->state.receiver;
    mc_world_runtime *const world = p->base.world; /* Evaluated before literal and draw. */
    if (!mc_arrow_tick_world_begin(c, world, world_pc))
        return false;
    const mc_registry_string *name = NULL;
    if (!mc_arrow_tick_literal(c, "random.drr", world_pc + 4u, &name))
        return false;
    float value;
    if (!mc_arrow_tick_random(c, random_pc, &value) || !mc_arrow_tick_fp(c, random_pc + 5u))
        return false;
    const float pitch = at_fdiv(1.2f, at_fadd(at_fmul(value, .2f), .9f));
    return mc_arrow_tick_sound(c, world, name, pitch, invoke_pc);
}

static bool base(mc_arrow_tick_call *c) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_BASE, 1);
    c->state.base_wrapper_pc = 1;
    /* Required owned SAME-SN.U entry. No fallback to the legacy Math-only
     * helper: that would lose dynamic World/producer rights after failure. */
    c->query_live = true;
    c->error.query = &c->query_result;
    mc_tick_arrow_base_execution_init(&c->base_execution, &c->execution, c->code_classes,
                                      c->query_code, &c->query_result);
    const bool returned = mc_tick_arrow_base_body_in_execution(
        c->state.receiver, c->registry, &c->base_execution, &c->limits.base, &c->work.base);
    /* Exact VALUE token actually created by Base; never infer an ambient scope.
     * The same stable result is already the parent's existing unwind ledger. */
    c->direct_world = c->base_execution.world;
    c->direct_world_open = c->base_execution.world_open;
    c->query_unwind =
        c->direct_world_open ? MC_ARROW_TICK_UNWIND_OWN_CLOSE : MC_ARROW_TICK_UNWIND_QUIESCENT;
    c->state.base_particle_work = c->work.base.particle_work;
    if (!returned) {
        c->error.base = c->execution.capture_pending ? NULL : &c->work.base;
        c->error.execution = &c->execution;
        c->error.child_site = (mc_arrow_site){c->work.base.source_class, c->work.base.source_method,
                                              NULL, (int32_t)c->work.base.bytecode};
        const char *exception = NULL;
        const char *operation = NULL;
        if (mc_arrow_tick_base_source_failure(c, &operation, &exception))
            return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED, operation, false, exception);
        /* Base is first on this newly initialized Math execution: this is
         * its actual reached failure, not a historical context flag. */
        if (c->math.failed) {
            c->error.math = mc_math_execution_cause(&c->math);
            const mc_math_execution_site child = c->math.first.child;
            c->error.child_site =
                (mc_arrow_site){child.class_name, child.method, child.descriptor, child.pc};
            return mc_arrow_tick_fail(
                c,
                c->error.math->status == MC_MATH_CLASS_LIMIT ? MC_ARROW_TICK_LIMIT
                : c->error.math->status == MC_MATH_CLASS_ALLOCATION_FAILED
                    ? MC_ARROW_TICK_ALLOCATION
                : c->error.math->status == MC_MATH_CLASS_BUSY ? MC_ARROW_TICK_BUSY
                                                              : MC_ARROW_TICK_CHILD_FAILED,
                c->error.math->operation, true, NULL);
        }
        if (c->query_result.ownership_status != MC_WORLD_STORAGE_OK) {
            const mc_entity_tick_execution_error native = c->query_result.ownership_error;
            c->error.child_site = (mc_arrow_site){native.site.class_name, native.site.method,
                                                  native.site.descriptor, native.site.pc};
            return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED, native.operation, true, NULL);
        }
        return mc_arrow_tick_fail_classified(c, MC_ARROW_TICK_CHILD_FAILED, c->work.base.operation,
                                             MC_ARROW_TICK_FAILURE_UNRESOLVED, NULL);
    }
    c->query_live = false;
    c->error.query = NULL;
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

/* ONE complete SL.w_ body. Child helpers execute actual entered native
 * implementations and retain their returned/failed objects. Source-owned
 * fields are never reset to make cleanup pass. All three source returns are
 * explicit; K/dead never bypasses the shared tail. */
bool mc_arrow_tick_body(mc_arrow_tick_call *c) {
    mc_entity *const p = c->state.receiver;
    mc_arrow_fields *const a = &p->arrow->fields;
    if (!base(c))
        return false;

    if (p->base.aV == 0.0f && p->base.aU == 0.0f) {
        if (!mc_arrow_tick_fp(c, 30))
            return false;
        const double value = at_add(at_mul(p->base.velocity_x, p->base.velocity_x),
                                    at_mul(p->base.velocity_z, p->base.velocity_z));
        float horizontal;
        if (!mc_arrow_tick_math_root(c, value, 41, &horizontal))
            return false;
        mc_arrow_tick_at(c, 55);
        const float yaw = at_narrow(
            at_div(at_mul(mc_player_tick_atan2(p->base.velocity_x, p->base.velocity_z), 180.0),
                   3.1415927410125732));
        p->base.yaw = yaw; /* Source68 before71. */
        p->base.aU = yaw;
        mc_arrow_tick_at(c, 82);
        const float pitch = at_narrow(
            at_div(at_mul(mc_player_tick_atan2(p->base.velocity_y, at_widen(horizontal)), 180.0),
                   3.1415927410125732));
        p->base.aT = pitch;
        p->base.aV = pitch;
    }

    int32_t id;
    mc_world_runtime *world = p->base.world; /* Source102. */
    if (!mc_arrow_tick_world_begin(c, world, 102) ||
        !mc_arrow_tick_block_value(c, world, a->d, a->e, a->f, false, 117, &id))
        return false;
    if (id > 0) {
        const mc_block *block = NULL;
        if (!mc_arrow_tick_block_get(c, id, 125, &block))
            return false;
        world = p->base.world; /* Fresh source131 after the first live UU.m use. */
        if (!mc_arrow_tick_world_begin(c, world, 131) ||
            !mc_arrow_tick_block_shape(c, world, block, a->d, a->e, a->f, false, 146))
            return false;
        if (!mc_arrow_tick_block_get(c, id, 149, &block))
            return false;
        world = p->base.world; /* Separate source155. */
        if (!mc_arrow_tick_world_begin(c, world, 155) ||
            !mc_arrow_tick_block_shape(c, world, block, a->d, a->e, a->f, true, 170))
            return false;
        if (c->block_box != NULL) {
            if (!mc_arrow_tick_vector_new(c, p->base.x, p->base.y, p->base.z, 191, &c->point))
                return false;
            mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_GEOMETRY, 194);
            bool contains = false;
            const mc_entity_geometry_status status = mc_entity_bounds_contains_point(
                p->classes, c->block_box, c->point, &contains, &c->geometry_error);
            if (status != MC_ENTITY_GEOMETRY_OK) {
                c->error.geometry = &c->geometry_error;
                return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED,
                                          c->geometry_error.operation,
                                          c->geometry_error.native_policy,
                                          status == MC_ENTITY_GEOMETRY_SOURCE_NULL_REFERENCE
                                              ? "java/lang/NullPointerException"
                                              : NULL);
            }
            if (contains)
                a->i = true; /* False containment does not clear a previously true i. */
            if (!mc_arrow_tick_vector_drop(c, &c->point))
                return false;
            /* Actual source local1 retains the pooled Block box. */
            c->work_tag = MC_ARROW_TICK_CHILD_NONE;
        }
    }
    if (a->b > 0)
        a->b = mc_java_iadd(a->b, -1);

    if (a->i) {
        world = p->base.world;
        if (!mc_arrow_tick_world_begin(c, world, 230) ||
            !mc_arrow_tick_block_value(c, world, a->d, a->e, a->f, false, 245, &id))
            return false;
        int32_t metadata;
        world = p->base.world;
        if (!mc_arrow_tick_world_begin(c, world, 250) ||
            !mc_arrow_tick_block_value(c, world, a->d, a->e, a->f, true, 265, &metadata))
            return false;
        if (id != a->g || metadata != a->h) {
            a->i = false;
            float value;
            double motion = p->base.velocity_x; /* Source292 before bs296. */
            if (!mc_arrow_tick_random(c, 299, &value) || !mc_arrow_tick_fp(c, 304))
                return false;
            p->base.velocity_x = at_mul(motion, at_widen(at_fmul(value, .2f)));
            mc_arrow_tick_at(c, 307);
            motion = p->base.velocity_y;
            if (!mc_arrow_tick_random(c, 319, &value))
                return false;
            p->base.velocity_y = at_mul(motion, at_widen(at_fmul(value, .2f)));
            mc_arrow_tick_at(c, 327);
            motion = p->base.velocity_z;
            if (!mc_arrow_tick_random(c, 339, &value))
                return false;
            p->base.velocity_z = at_mul(motion, at_widen(at_fmul(value, .2f)));
            a->j = 0;
            a->k = 0;
            mc_arrow_tick_at(c, 360);
            return true;
        }
        a->j = mc_java_iadd(a->j, 1);
        if (a->j == 1200 && !mc_arrow_tick_dead(c, 382))
            return false;
        mc_arrow_tick_at(c, 385);
        return true;
    }

    a->k = mc_java_iadd(a->k, 1); /* Source393, before rays or candidates. */
    if (!mc_arrow_tick_vector_new(c, p->base.x, p->base.y, p->base.z, 408, &c->start) ||
        !mc_arrow_tick_fp(c, 420))
        return false;
    double x = at_add(p->base.x, p->base.velocity_x);
    double y = at_add(p->base.y, p->base.velocity_y);
    double z = at_add(p->base.z, p->base.velocity_z);
    if (!mc_arrow_tick_vector_new(c, x, y, z, 439, &c->end))
        return false;
    world = p->base.world; /* Source444, after actual endpoint factories. */
    if (!mc_arrow_tick_world_begin(c, world, 444) || !mc_arrow_tick_ray(c, world))
        return false;
    if (!mc_arrow_tick_vector_new(c, p->base.x, p->base.y, p->base.z, 467, &c->start) ||
        !mc_arrow_tick_fp(c, 479))
        return false;
    x = at_add(p->base.x, p->base.velocity_x);
    y = at_add(p->base.y, p->base.velocity_y);
    z = at_add(p->base.z, p->base.velocity_z);
    if (!mc_arrow_tick_vector_new(c, x, y, z, 498, &c->end))
        return false;
    if (c->hit != NULL) {
        const mc_entity_vector *point;
        if (!hit_point(c, c->hit, 510, &point) ||
            !mc_arrow_tick_vector_new(c, point->x, point->y, point->z, 527, &c->end))
            return false;
    }

    world = p->base.world; /* Source535 precedes bounds/velocity argument evaluation. */
    if (!mc_arrow_tick_world_begin(c, world, 535) ||
        !bounds_transform(c, p->bounds, p->base.velocity_x, p->base.velocity_y, p->base.velocity_z,
                          true, 555, &c->swept) ||
        !bounds_transform(c, c->swept, 1.0, 1.0, 1.0, false, 561, &c->query_bounds) ||
        !mc_arrow_tick_bounds_drop(c, &c->swept) || !mc_arrow_tick_nearby(c, world))
        return false;
    if (!mc_arrow_tick_bounds_drop(c, &c->query_bounds))
        return false;
    c->state.nearest = 0.0;
    int32_t index = 0;
    for (;;) {
        mc_arrow_tick_at(c, 579);
        if (c->nearby.identity == NULL)
            return mc_arrow_tick_null(c, "Arrow null captured nearby List size receiver");
        size_t count;
        const mc_world_storage_status counted = mc_world_entity_list_view_count(&c->nearby, &count);
        if (counted != MC_WORLD_STORAGE_OK)
            return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED,
                                      "Arrow actual live nearby List size", true, NULL);
        if (count > INT32_MAX)
            return mc_arrow_tick_fail(c, MC_ARROW_TICK_STATE,
                                      "Arrow List size outside represented Java int domain", true,
                                      NULL);
        if (index >= 0 && (size_t)index >= count)
            break;
        if (c->state.candidates_started == c->limits.max_candidates)
            return mc_arrow_tick_fail(c, MC_ARROW_TICK_LIMIT,
                                      "Arrow pending candidate occurrence ceiling", true, NULL);
        ++c->state.candidates_started;
        mc_entity *candidate = NULL;
        mc_arrow_tick_at(c, 591);
        if (mc_world_entity_list_view_get(&c->nearby, (size_t)(uint32_t)index, &candidate) !=
            MC_WORLD_STORAGE_OK)
            return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED,
                                      "Arrow actual typed nearby SN occurrence", true, NULL);
        if (!held_local(c, candidate, 599, &c->candidate))
            return false;
        mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_HIT, 603);
        if (candidate == NULL)
            return mc_arrow_tick_null(c, "Arrow null candidate h_ receiver");
        bool pickable = false;
        const mc_entity_hit_status picked = mc_entity_pickable(candidate, &pickable, &c->hit_error);
        if (picked != MC_ENTITY_HIT_OK) {
            c->error.hit = &c->hit_error;
            return mc_arrow_tick_fail(
                c, MC_ARROW_TICK_CHILD_FAILED, c->hit_error.operation, c->hit_error.native_policy,
                picked == MC_ENTITY_HIT_SOURCE_NULL_REFERENCE ? "java/lang/NullPointerException"
                                                              : NULL);
        }
        if (pickable) {
            mc_entity *current_shooter;
            if (!shooter(c, 612, &current_shooter))
                return false;
            if (candidate != current_shooter || a->k >= 5) {
                const double border = at_widen(.3f);
                if (!bounds_transform(c, candidate->bounds, border, border, border, false, 647,
                                      &c->candidate_bounds) ||
                    !mc_arrow_tick_intercept(c))
                    return false;
                if (c->intercept_hit != NULL) {
                    const mc_entity_vector *point;
                    if (!hit_point(c, c->intercept_hit, 669, &point))
                        return false;
                    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_GEOMETRY, 672);
                    double distance;
                    const mc_entity_geometry_status status = mc_entity_vector_distance(
                        p->classes, c->start, point, &distance, &c->geometry_error);
                    if (status != MC_ENTITY_GEOMETRY_OK) {
                        c->error.geometry = &c->geometry_error;
                        return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED,
                                                  c->geometry_error.operation,
                                                  c->geometry_error.native_policy,
                                                  status == MC_ENTITY_GEOMETRY_SOURCE_NULL_REFERENCE
                                                      ? "java/lang/NullPointerException"
                                                      : NULL);
                    }
                    if (distance < c->state.nearest || c->state.nearest == 0.0) {
                        if (!held_local(c, candidate, 692, &c->selected))
                            return false;
                        c->state.nearest = distance;
                    }
                }
                /* Source locals11/12 survive skipped later candidates and
                 * failures before the next completed assignments650/659. */
            }
        }
        /* Source Entity local9 survives until the next assignment599. */
        index = mc_java_iadd(index, 1);
    }
    /* Keep actual List local5 and its live identity lease through this call. */
    if (c->selected != NULL) {
        mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_HIT, 711);
        const mc_entity_hit_status allocated =
            mc_entity_hit_result_allocate(p->classes, &c->pending_hit, &c->hit_error);
        if (allocated != MC_ENTITY_HIT_OK) {
            c->error.hit = &c->hit_error;
            return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED, c->hit_error.operation,
                                      c->hit_error.native_policy, NULL);
        }
        mc_arrow_tick_at(c, 717);
        const mc_entity_hit_status constructed =
            mc_entity_hit_result_construct_entity(c->pending_hit, c->selected, &c->hit_error);
        if (constructed != MC_ENTITY_HIT_OK) {
            c->error.hit = &c->hit_error;
            return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED, c->hit_error.operation,
                                      c->hit_error.native_policy,
                                      constructed == MC_ENTITY_HIT_SOURCE_NULL_REFERENCE
                                          ? "java/lang/NullPointerException"
                                          : NULL);
        }
        c->old_hit = c->hit;
        c->hit = c->pending_hit; /* Actual astore_3@720, not a reconstructed hit. */
        c->pending_hit = NULL;
        mc_arrow_tick_at(c, 720);
        if (!mc_arrow_tick_hit_drop(c, &c->old_hit))
            return false;
    }
    if (c->hit != NULL) {
        mc_entity_hit_result_view view;
        if (!hit_view(c, c->hit, 726, &view))
            return false;
        if (view.fields.g != NULL) {
            if (!held_local(c, view.fields.g, 733, &c->hurt_target))
                return false;
            mc_entity *current_shooter;
            if (!shooter(c, 737, &current_shooter) ||
                !held_local(c, current_shooter, 737, &c->hurt_source))
                return false;
            bool accepted = false;
            if (!mc_arrow_tick_hurt(c, c->hurt_target, c->hurt_source, &accepted))
                return false;
            mc_arrow_tick_entity_drop(&c->hurt_source);
            mc_arrow_tick_entity_drop(&c->hurt_target);
            if (accepted) {
                if (!impact_sound(c, 748, 761, 771) || !mc_arrow_tick_dead(c, 775))
                    return false;
            } else {
                if (!mc_arrow_tick_fp(c, 789))
                    return false;
                p->base.velocity_x = at_mul(p->base.velocity_x, -0.10000000149011612);
                p->base.velocity_y = at_mul(p->base.velocity_y, -0.10000000149011612);
                p->base.velocity_z = at_mul(p->base.velocity_z, -0.10000000149011612);
                p->base.yaw = at_fadd(p->base.yaw, 180.0f);
                p->base.aU = at_fadd(p->base.aU, 180.0f);
                a->k = 0;
            }
        } else {
            a->d = view.fields.b;
            a->e = view.fields.c;
            a->f = view.fields.d;
            world = p->base.world;
            if (!mc_arrow_tick_world_begin(c, world, 873) ||
                !mc_arrow_tick_block_value(c, world, a->d, a->e, a->f, false, 888, &id))
                return false;
            a->g = id; /* Actual source store891, after the completed child. */
            world = p->base.world;
            int32_t hit_metadata;
            if (!mc_arrow_tick_world_begin(c, world, 896) ||
                !mc_arrow_tick_block_value(c, world, a->d, a->e, a->f, true, 911, &hit_metadata))
                return false;
            a->h = hit_metadata; /* Source914. */
            const mc_entity_vector *point;
            if (!hit_point(c, c->hit, 922, &point) || !mc_arrow_tick_fp(c, 929))
                return false;
            p->base.velocity_x = at_widen(at_narrow(at_sub(point->x, p->base.x)));
            if (!hit_point(c, c->hit, 940, &point))
                return false;
            p->base.velocity_y = at_widen(at_narrow(at_sub(point->y, p->base.y)));
            if (!hit_point(c, c->hit, 958, &point))
                return false;
            p->base.velocity_z = at_widen(at_narrow(at_sub(point->z, p->base.z)));
            const double length_squared =
                at_add(at_add(at_mul(p->base.velocity_x, p->base.velocity_x),
                              at_mul(p->base.velocity_y, p->base.velocity_y)),
                       at_mul(p->base.velocity_z, p->base.velocity_z));
            float length;
            if (!mc_arrow_tick_math_root(c, length_squared, 1000, &length))
                return false;
            p->base.x = at_sub(p->base.x, at_mul(at_div(p->base.velocity_x, at_widen(length)),
                                                 0.05000000074505806));
            p->base.y = at_sub(p->base.y, at_mul(at_div(p->base.velocity_y, at_widen(length)),
                                                 0.05000000074505806));
            p->base.z = at_sub(p->base.z, at_mul(at_div(p->base.velocity_z, at_widen(length)),
                                                 0.05000000074505806));
            if (!impact_sound(c, 1069, 1082, 1092))
                return false;
            a->i = true;
            a->b = 7;
        }
    }

    /* Common tail1106 executes even after accepted-hit K775. */
    if (!mc_arrow_tick_fp(c, 1115))
        return false;
    p->base.x = at_add(p->base.x, p->base.velocity_x);
    p->base.y = at_add(p->base.y, p->base.velocity_y);
    p->base.z = at_add(p->base.z, p->base.velocity_z);
    float horizontal;
    const double horizontal_squared = at_add(at_mul(p->base.velocity_x, p->base.velocity_x),
                                             at_mul(p->base.velocity_z, p->base.velocity_z));
    if (!mc_arrow_tick_math_root(c, horizontal_squared, 1164, &horizontal))
        return false;
    mc_arrow_tick_at(c, 1178);
    p->base.yaw = at_narrow(
        at_div(at_mul(mc_player_tick_atan2(p->base.velocity_x, p->base.velocity_z), 180.0),
               3.1415927410125732));
    mc_arrow_tick_at(c, 1201);
    p->base.aT = at_narrow(
        at_div(at_mul(mc_player_tick_atan2(p->base.velocity_y, at_widen(horizontal)), 180.0),
               3.1415927410125732));
    while (at_fsub(p->base.aT, p->base.aV) < -180.0f) {
        if (!mc_arrow_tick_angle_work(c, 1231))
            return false;
        p->base.aV = at_fsub(p->base.aV, 360.0f);
    }
    while (at_fsub(p->base.aT, p->base.aV) >= 180.0f) {
        if (!mc_arrow_tick_angle_work(c, 1260))
            return false;
        p->base.aV = at_fadd(p->base.aV, 360.0f);
    }
    while (at_fsub(p->base.yaw, p->base.aU) < -180.0f) {
        if (!mc_arrow_tick_angle_work(c, 1289))
            return false;
        p->base.aU = at_fsub(p->base.aU, 360.0f);
    }
    while (at_fsub(p->base.yaw, p->base.aU) >= 180.0f) {
        if (!mc_arrow_tick_angle_work(c, 1318))
            return false;
        p->base.aU = at_fadd(p->base.aU, 360.0f);
    }
    p->base.aT = at_fadd(p->base.aV, at_fmul(at_fsub(p->base.aT, p->base.aV), .2f));
    p->base.yaw = at_fadd(p->base.aU, at_fmul(at_fsub(p->base.yaw, p->base.aU), .2f));
    float drag = .99f;
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_ENVIRONMENT, 1383);
    bool water = false;
    const mc_entity_status wet =
        mc_entity_environment_stored_in_water(p, &water, &c->work.environment);
    if (wet != MC_ENTITY_OK) {
        c->error.environment = &c->work.environment;
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED, c->work.environment.operation,
                                  true, NULL);
    }
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    if (water) {
        for (unsigned int i = 0; i < 4; ++i) {
            world = p->base.world; /* Fresh1403 for every source iteration. */
            if (!mc_arrow_tick_world_begin(c, world, 1403))
                return false;
            const mc_registry_string *name = NULL;
            if (!mc_arrow_tick_literal(c, "bubble", 1406, &name) || !mc_arrow_tick_fp(c, 1419))
                return false;
            x = at_sub(p->base.x, at_mul(p->base.velocity_x, at_widen(.25f)));
            y = at_sub(p->base.y, at_mul(p->base.velocity_y, at_widen(.25f)));
            z = at_sub(p->base.z, at_mul(p->base.velocity_z, at_widen(.25f)));
            const double vx = p->base.velocity_x, vy = p->base.velocity_y, vz = p->base.velocity_z;
            if (!mc_arrow_tick_particle(c, world, name, x, y, z, vx, vy, vz))
                return false;
        }
        drag = .8f; /* Source1468 only after all four actual callbacks. */
    }
    if (!mc_arrow_tick_fp(c, 1480))
        return false;
    p->base.velocity_x = at_mul(p->base.velocity_x, at_widen(drag));
    p->base.velocity_y = at_mul(p->base.velocity_y, at_widen(drag));
    p->base.velocity_z = at_mul(p->base.velocity_z, at_widen(drag));
    p->base.velocity_y = at_sub(p->base.velocity_y, at_widen(.03f));
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_POSITION, 1533);
    const mc_entity_status positioned =
        mc_entity_source_position_checked(p, p->base.x, p->base.y, p->base.z, &c->position_error);
    if (positioned != MC_ENTITY_OK) {
        c->error.position = &c->position_error;
        c->error.child_site =
            (mc_arrow_site){"sn", "e", "(DDD)V", (int32_t)c->position_error.bytecode};
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED, c->position_error.operation,
                                  c->position_error.native_policy,
                                  c->position_error.source_null ? "java/lang/NullPointerException"
                                                                : NULL);
    }
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    mc_arrow_tick_at(c, 1536);
    return true;
}
