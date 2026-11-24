#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../construction_internal.h"
#include "../living/travel_math_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/world/runtime/world_access_error.h"
#include "particle_internal.h"

static const char constructor_descriptor[] = "(Lfd;DDDLgm;)V";

static const char *registry_source_exception(mc_registry_status status) {
    switch (status) {
        case MC_REGISTRY_NULL_REFERENCE:
            return "java/lang/NullPointerException";
        case MC_REGISTRY_INDEX_OUT_OF_RANGE:
            return "java/lang/ArrayIndexOutOfBoundsException";
        case MC_REGISTRY_ILLEGAL_ARGUMENT:
        case MC_REGISTRY_DUPLICATE_BLOCK:
        case MC_REGISTRY_DUPLICATE_STATISTIC:
            return "java/lang/IllegalArgumentException";
        default:
            return NULL;
    }
}

static bool same_registry_cause(const mc_registry_error *a, const mc_registry_error *b,
                                bool cached) {
    return (cached ? a->status == MC_REGISTRY_FAILED_INITIALIZATION : a->status == b->status) &&
           a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc && a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

void mc_particle_registry_classification(mc_registry_classes *classes,
                                         const mc_registry_error *error,
                                         mc_particle_throwable *kind, const char **name) {
    *kind = MC_PARTICLE_NO_THROWABLE;
    *name = NULL;
    const bool cached = error->status == MC_REGISTRY_FAILED_INITIALIZATION;
    const mc_registry_error *first = NULL;
    if (classes != NULL) {
        if (classes->blocks_phase == MC_REGISTRY_FAILED &&
            classes->block_states[MC_BLOCK_BASE].phase == MC_REGISTRY_FAILED &&
            same_registry_cause(&classes->block_failure,
                                &classes->block_states[MC_BLOCK_BASE].failure, false) &&
            same_registry_cause(error, &classes->block_failure, cached))
            first = &classes->block_failure;
        if (first == NULL) {
            for (size_t i = 0; i < (size_t)MC_BLOCK_KIND_COUNT; ++i)
                if (classes->block_states[i].phase == MC_REGISTRY_FAILED &&
                    same_registry_cause(error, &classes->block_states[i].failure, cached)) {
                    first = &classes->block_states[i].failure;
                    break;
                }
        }
        if (first == NULL) {
            for (size_t i = 0; i < (size_t)MC_ITEM_KIND_COUNT; ++i)
                if (classes->item_states[i].phase == MC_REGISTRY_FAILED &&
                    same_registry_cause(error, &classes->item_states[i].failure, cached)) {
                    first = &classes->item_states[i].failure;
                    break;
                }
        }
    }
    if (first != NULL && registry_source_exception(first->status) != NULL) {
        *kind = MC_PARTICLE_SOURCE_ERROR;
        *name = cached ? "java/lang/NoClassDefFoundError" : "java/lang/ExceptionInInitializerError";
    } else if (first != NULL && first->status == MC_REGISTRY_FAILED_INITIALIZATION &&
               classes != NULL) {
        /* A genuinely reached already erroneous dependency propagates its
         * Error unchanged. Match its actual immutable cause rather than
         * inferring a Throwable from FAILED_INITIALIZATION alone. */
        for (size_t i = 0; i < (size_t)MC_BLOCK_KIND_COUNT; ++i)
            if (classes->block_states[i].phase == MC_REGISTRY_FAILED &&
                same_registry_cause(first, &classes->block_states[i].failure, true) &&
                registry_source_exception(classes->block_states[i].failure.status) != NULL) {
                *kind = MC_PARTICLE_SOURCE_ERROR;
                *name = "java/lang/NoClassDefFoundError";
                return;
            }
        for (size_t i = 0; i < (size_t)MC_ITEM_KIND_COUNT; ++i)
            if (classes->item_states[i].phase == MC_REGISTRY_FAILED &&
                same_registry_cause(first, &classes->item_states[i].failure, true) &&
                registry_source_exception(classes->item_states[i].failure.status) != NULL) {
                *kind = MC_PARTICLE_SOURCE_ERROR;
                *name = "java/lang/NoClassDefFoundError";
                return;
            }
    } else if (!cached && error->active_initializer_class == NULL) {
        *name = registry_source_exception(error->status);
        if (*name != NULL)
            *kind = MC_PARTICLE_SOURCE_EXCEPTION;
    }
}

static mc_entity_status registry_status(mc_registry_status status) {
    return status == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
           : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
           : status == MC_REGISTRY_PROFILE_FAILED    ? MC_ENTITY_PROFILE_FAILED
                                                     : MC_ENTITY_STATE_ERROR;
}

static void item_at(mc_entity *p, mc_particle_constructor_error *e, unsigned int pc) {
    p->particle->item_native.item_pc = e->item_pc = pc;
}

static void item_fail(mc_particle_constructor_error *e, mc_entity_status status,
                      mc_particle_cause cause, const char *operation, unsigned int pc) {
    (void)mc_particle_constructor_fail(e, status, cause, operation, "pb", "<init>", pc);
    e->source_descriptor = constructor_descriptor;
    e->item_pc = pc;
}

static void registry_fail(mc_registry_classes *registry, mc_particle_constructor_error *e,
                          unsigned int pc) {
    item_fail(e, registry_status(e->registry.status), MC_PARTICLE_REGISTRY, e->registry.message,
              pc);
    mc_particle_registry_classification(registry, &e->registry, &e->throwable, &e->exception_class);
    e->native_policy = e->throwable == MC_PARTICLE_NO_THROWABLE;
    e->source_exception = e->throwable == MC_PARTICLE_SOURCE_EXCEPTION;
}

mc_entity_status mc_item_particle_allocate(mc_entity_classes *classes, mc_entity **out,
                                           mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_ITEM_PARTICLE,
                                        "item particle allocation", out, error);
}

mc_entity_status mc_item_particle_construct(mc_entity *p, mc_world_runtime *world,
                                            mc_registry_classes *registry, mc_math_classes *math,
                                            double x, double y, double z, const mc_item *item,
                                            mc_particle_error *out) {
    return mc_item_particle_construct_entered(p, world, registry, math, x, y, z, item, NULL, out);
}

mc_entity_status mc_item_particle_construct_entered(mc_entity *p, mc_world_runtime *world,
                                                    mc_registry_classes *registry,
                                                    mc_math_classes *math, double x, double y,
                                                    double z, const mc_item *item,
                                                    mc_math_classes **reached_math,
                                                    mc_particle_error *out) {
    if (out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_particle_constructor_error e = {.native_policy = true};
    const mc_entity_status entered = mc_entity_constructor_begin(
        p, MC_ENTITY_CLASS_KIND_ITEM_PARTICLE, world, "item particle constructor",
        "item particle World profile", &e.entity);
    if (entered != MC_ENTITY_OK)
        (void)mc_particle_constructor_fail(&e, entered, MC_PARTICLE_OWNER, e.entity.operation, NULL,
                                           NULL, 0);
    else {
        bool registry_held = false;
        item_at(p, &e, 10);
        if (mc_particle_construct_ancestor(p, world, x, y, z, 0.0, 0.0, 0.0, math, true,
                                           reached_math, &e)) {
            item_at(p, &e, 17);
            if (item == NULL)
                item_fail(&e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_NULL_REFERENCE,
                          "PB actual Item icon null receiver", 17);
            else {
                e.registry.status = mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT,
                                                               p->classes->profile, &e.registry);
                registry_held = e.registry.status == MC_REGISTRY_OK;
                if (!registry_held)
                    registry_fail(registry, &e, 17);
                else {
                    int32_t icon;
                    if (mc_registry_item_base_icon(registry, item, 0, &icon, &e.registry) !=
                        MC_REGISTRY_OK)
                        registry_fail(registry, &e, 17);
                    else {
                        mc_particle_fields *f = &p->particle->fields;
                        f->b = icon;
                        item_at(p, &e, 20);
                        f->k = 1.0f;
                        item_at(p, &e, 28);
                        f->j = 1.0f;
                        item_at(p, &e, 32);
                        f->i = 1.0f;
                        item_at(p, &e, 35);
                        const mc_block *snow = NULL;
                        item_at(p, &e, 39);
                        if (mc_registry_block_static_get(registry, "aV", &snow, &e.registry) !=
                            MC_REGISTRY_OK)
                            registry_fail(registry, &e, 39);
                        else if (snow == NULL)
                            item_fail(&e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_NULL_REFERENCE,
                                      "PB actual Snow Block gravity null receiver", 42);
                        else {
                            f->h = snow->fields.bz;
                            item_at(p, &e, 45);
                            f->g = mc_travel_fdiv(f->g, 2.0f);
                            item_at(p, &e, 55);
                            item_at(p, &e, 58);
                        }
                    }
                }
            }
        }
        if (registry_held && !mc_registry_runtime_borrow_release(registry))
            item_fail(&e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_OWNER,
                      "PB native Registry occurrence retirement", e.item_pc);
        (void)mc_entity_constructor_finish(p, e.status, &e.entity);
        if (e.status == MC_ENTITY_OK) {
            e.source_class = "pb";
            e.source_method = "<init>";
            e.source_descriptor = constructor_descriptor;
            e.source_pc = 58;
        }
    }
    mc_particle_constructor_error_publish(out, &e);
    return e.status;
}

static mc_entity_status tick_fail(mc_particle_error *e, mc_entity_status status,
                                  mc_particle_cause cause, const char *operation, unsigned int pc) {
    e->status = status;
    e->cause = cause;
    e->operation = operation;
    e->source_class = "xw";
    e->source_method = "w_";
    e->source_descriptor = "()V";
    e->source_pc = e->tick_pc = pc;
    return status;
}

void mc_item_particle_tick_error_init(mc_item_particle_tick_error *e) {
    if (e == NULL)
        return;
    mc_particle_error_init(&e->particle);
    mc_entity_move_error_init(&e->movement);
}

void mc_particle_movement_exception(mc_particle_error *e, const mc_entity_move_error *m,
                                    mc_registry_classes *registry) {
    if (m->source_null) {
        e->throwable = MC_PARTICLE_SOURCE_EXCEPTION;
        e->source_exception = true;
        e->native_policy = false;
        e->exception_class = "java/lang/NullPointerException";
        return;
    }
    const mc_world_access_failure *receipts[] = {
        &m->world.world_access_failure, &m->effect.world.world_access_failure,
        &m->contact.mutation.world_access_failure, &m->contact.effect.world.world_access_failure,
        &m->step.mutation.world_access_failure};
    for (size_t i = 0; i < sizeof(receipts) / sizeof(receipts[0]); ++i)
        if (mc_world_access_failure_source_exception(receipts[i], &e->exception_class)) {
            e->throwable = MC_PARTICLE_SOURCE_EXCEPTION;
            e->source_exception = true;
            e->native_policy = false;
            return;
        }
    if (m->effect.cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE ||
        m->effect.cause == MC_ENTITY_EFFECT_CAUSE_CLASS_CAST) {
        e->throwable = MC_PARTICLE_SOURCE_EXCEPTION;
        e->source_exception = true;
        e->native_policy = false;
        e->exception_class = m->effect.cause == MC_ENTITY_EFFECT_CAUSE_CLASS_CAST
                                 ? "java/lang/ClassCastException"
                                 : "java/lang/NullPointerException";
        return;
    }
    const mc_registry_error *causes[] = {&m->registry, &m->effect.registry, &m->world.registry,
                                         &m->contact.registry, &m->step.registry};
    for (size_t i = 0; i < sizeof(causes) / sizeof(causes[0]); ++i) {
        if (causes[i]->status == MC_REGISTRY_OK)
            continue;
        mc_particle_registry_classification(registry, causes[i], &e->throwable,
                                            &e->exception_class);
        if (e->throwable != MC_PARTICLE_NO_THROWABLE) {
            e->source_exception = e->throwable == MC_PARTICLE_SOURCE_EXCEPTION;
            e->native_policy = false;
            return;
        }
    }
}

mc_entity_status mc_particle_tick_inherited(mc_entity *p, mc_entity_class_kind expected,
                                            mc_registry_classes *registry,
                                            const mc_entity_move_limits *limits,
                                            mc_item_particle_tick_error *error) {
    if (error == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_item_particle_tick_error_init(error);
    mc_particle_error *e = &error->particle;
    if (!mc_entity_is_kind(p, expected) || p->particle == NULL)
        return tick_fail(e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                         "PB tick canonical native storage", 0);
    if (p->phase != MC_ENTITY_READY)
        return tick_fail(e, MC_ENTITY_INVALID_PHASE, MC_PARTICLE_OWNER,
                         "PB tick READY native owner", 0);
    if (p->particle->item_native.tick_active)
        return tick_fail(e, MC_ENTITY_BUSY, MC_PARTICLE_OWNER, "PB tick already active", 0);
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK)
        return tick_fail(e, held, MC_PARTICLE_OWNER, "PB tick native owner occurrence", 0);
    p->particle->item_native.tick_active = true;
    mc_particle_fields *f = &p->particle->fields;
    p->base.aJ = p->base.x;
    p->particle->item_native.tick_pc = 5;
    p->base.aK = p->base.y;
    p->particle->item_native.tick_pc = 13;
    p->base.aL = p->base.z;
    p->particle->item_native.tick_pc = 21;
    const int32_t old_age = f->e;
    f->e = mc_java_i32_add(f->e, 1);
    p->particle->item_native.tick_pc = 32;
    if (old_age >= f->f) {
        p->base.be = true; /* Actual inherited SN.K store2, then return5. */
        p->particle->item_native.tick_pc = 43;
    }
    p->particle->item_native.tick_pc = 58;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        (void)tick_fail(e, MC_ENTITY_UNSUPPORTED_FP, MC_PARTICLE_OWNER,
                        "PB tick reached gravity floating controls", 58);
        goto done;
    }
    const double gravity = mc_travel_dmul(0.04, (double)f->h);
    p->base.velocity_y = mc_travel_dsub(p->base.velocity_y, gravity);
    p->particle->item_native.tick_pc = 61;
    const double vx = p->base.velocity_x, vy = p->base.velocity_y, vz = p->base.velocity_z;
    p->particle->item_native.tick_pc = 77;
    e->status = mc_entity_move_in_context(p, registry, vx, vy, vz, limits, &error->movement);
    if (e->status != MC_ENTITY_OK) {
        (void)tick_fail(e, e->status, MC_PARTICLE_MOVEMENT, error->movement.operation, 77);
        mc_particle_movement_exception(e, &error->movement, registry);
    } else {
        p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, 0.9800000190734863);
        p->particle->item_native.tick_pc = 89;
        p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, 0.9800000190734863);
        p->particle->item_native.tick_pc = 101;
        p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, 0.9800000190734863);
        p->particle->item_native.tick_pc = 113;
        if (p->base.aX) {
            p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, 0.699999988079071);
            p->particle->item_native.tick_pc = 132;
            p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, 0.699999988079071);
            p->particle->item_native.tick_pc = 144;
        }
        p->particle->item_native.tick_pc = e->tick_pc = e->source_pc = 147;
        e->source_class = "xw";
        e->source_method = "w_";
        e->source_descriptor = "()V";
    }
done:
    p->particle->item_native.tick_active = false;
    mc_entity_release(p);
    return e->status;
}

mc_entity_status mc_item_particle_tick(mc_entity *p, mc_registry_classes *registry,
                                       const mc_entity_move_limits *limits,
                                       mc_item_particle_tick_error *error) {
    return mc_particle_tick_inherited(p, MC_ENTITY_CLASS_KIND_ITEM_PARTICLE, registry, limits,
                                      error);
}

mc_entity_status mc_item_particle_layer(const mc_entity *p, int32_t *out) {
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ITEM_PARTICLE) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = 2; /* Actual PB.c_ iconst2/ireturn1. */
    return MC_ENTITY_OK;
}
