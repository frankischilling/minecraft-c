#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../construction_internal.h"
#include "../living/travel_math_internal.h"
#include "mc/block/registry/registry.h"
#include "mc/block/registry/rendering.h"
#include "mc/block/registry/runtime.h"
#include "particle_internal.h"
#include <string.h>

static mc_entity_status digging_fail(mc_particle_error *e, mc_entity_status status,
                                     const char *operation, const char *method,
                                     const char *descriptor, unsigned int pc, bool native) {
    e->status = status;
    e->cause = native ? MC_PARTICLE_OWNER : MC_PARTICLE_NULL_REFERENCE;
    e->operation = operation;
    e->source_class = "qm";
    e->source_method = method;
    e->source_descriptor = descriptor;
    e->source_pc = e->item_pc = pc;
    e->native_policy = native;
    if (!native) {
        e->throwable = MC_PARTICLE_SOURCE_EXCEPTION;
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
    }
    return status;
}

mc_entity_status mc_digging_particle_allocate(mc_entity_classes *classes, mc_entity **out,
                                              mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE,
                                        "digging particle allocation", out, error);
}

mc_entity_status mc_digging_particle_construct_entered(
    mc_entity *p, mc_world_runtime *world, mc_registry_classes *registry, mc_math_classes *math,
    double x, double y, double z, double vx, double vy, double vz, const mc_block *block,
    int32_t face, int32_t metadata, mc_math_classes **reached_math, mc_particle_error *out) {
    if (out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_particle_constructor_error e = {.native_policy = true};
    mc_entity_status status = mc_entity_constructor_begin(
        p, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE, world, "digging particle constructor",
        "digging particle World profile", &e.entity);
    if (status != MC_ENTITY_OK)
        (void)mc_particle_constructor_fail(&e, status, MC_PARTICLE_OWNER, e.entity.operation, NULL,
                                           NULL, 0);
    else {
        if (mc_particle_construct_ancestor(p, world, x, y, z, vx, vy, vz, math, true, reached_math,
                                           &e)) {
            p->particle->digging_face = 0;
            p->particle->item_native.item_pc = 18;
            p->particle->digging_block = block;
            p->particle->item_native.item_pc = 24;
            if (block == NULL) {
                (void)mc_particle_constructor_fail(
                    &e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_NULL_REFERENCE,
                    "QM actual Block icon null receiver", "qm", "<init>", 33);
            } else if (block->classes != registry) {
                (void)mc_particle_constructor_fail(
                    &e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                    "QM actual Block Registry provenance", "qm", "<init>", 33);
            } else {
                e.registry.status = mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT,
                                                               p->classes->profile, &e.registry);
                if (e.registry.status == MC_REGISTRY_OK) {
                    p->particle->digging_registry = registry;
                    int32_t icon;
                    e.registry.status = mc_block_render_texture_metadata(block, 0, metadata, &icon);
                    if (e.registry.status == MC_REGISTRY_OK) {
                        mc_particle_fields *f = &p->particle->fields;
                        f->b = icon;
                        p->particle->item_native.item_pc = 36;
                        f->h = block->fields.bz;
                        p->particle->item_native.item_pc = 45;
                        f->k = .6f;
                        p->particle->item_native.item_pc = 54;
                        f->j = .6f;
                        p->particle->item_native.item_pc = 58;
                        f->i = .6f;
                        p->particle->item_native.item_pc = 61;
                        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
                            (void)mc_particle_constructor_fail(
                                &e, MC_ENTITY_UNSUPPORTED_FP, MC_PARTICLE_OWNER,
                                "QM reached size floating controls", "qm", "<init>", 70);
                        else {
                            f->g = mc_travel_fdiv(f->g, 2.0f);
                            p->particle->item_native.item_pc = 71;
                            p->particle->digging_face = face;
                            p->particle->item_native.item_pc = 77;
                            p->particle->item_native.item_pc = 80;
                        }
                    }
                }
                if (e.registry.status != MC_REGISTRY_OK) {
                    (void)mc_particle_constructor_fail(
                        &e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_REGISTRY,
                        "QM actual Block texture child", "qm", "<init>", 33);
                    mc_particle_registry_classification(registry, &e.registry, &e.throwable,
                                                        &e.exception_class);
                    e.native_policy = e.throwable == MC_PARTICLE_NO_THROWABLE;
                    e.source_exception = e.throwable == MC_PARTICLE_SOURCE_EXCEPTION;
                }
            }
        }
        if (e.source_class == NULL || strcmp(e.source_class, "qm") == 0)
            e.source_descriptor = "(Lfd;DDDDDDLuu;II)V";
        (void)mc_entity_constructor_finish(p, e.status, &e.entity);
        if (e.status == MC_ENTITY_OK) {
            e.source_class = "qm";
            e.source_method = "<init>";
            e.source_pc = e.item_pc = 80;
        }
    }
    mc_particle_constructor_error_publish(out, &e);
    return e.status;
}

mc_entity_status mc_digging_particle_construct(mc_entity *p, mc_world_runtime *world,
                                               mc_registry_classes *registry, mc_math_classes *math,
                                               double x, double y, double z, double vx, double vy,
                                               double vz, const mc_block *block, int32_t face,
                                               int32_t metadata, mc_particle_error *error) {
    return mc_digging_particle_construct_entered(p, world, registry, math, x, y, z, vx, vy, vz,
                                                 block, face, metadata, NULL, error);
}

static bool digging_ready(mc_entity *p, mc_particle_error *e, const char *method,
                          const char *descriptor) {
    mc_particle_error_init(e);
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE) || p->particle == NULL ||
        p->phase != MC_ENTITY_READY) {
        (void)digging_fail(e, MC_ENTITY_INVALID_ARGUMENT, "QM canonical READY receiver", method,
                           descriptor, 0, true);
        return false;
    }
    return true;
}

mc_entity_status mc_digging_particle_tint(mc_entity *p, int32_t x, int32_t y, int32_t z,
                                          mc_particle_error *e) {
    (void)x;
    (void)y;
    (void)z;
    if (e == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (!digging_ready(p, e, "a", "(III)Lqm;"))
        return e->status;
    const mc_block *grass = NULL;
    if (mc_registry_block_static_get(p->particle->digging_registry, "v", &grass, &e->registry) !=
        MC_REGISTRY_OK) {
        (void)digging_fail(e, MC_ENTITY_STATE_ERROR, "QM actual Grass static child", "a",
                           "(III)Lqm;", 4, true);
        e->cause = MC_PARTICLE_REGISTRY;
        mc_particle_registry_classification(p->particle->digging_registry, &e->registry,
                                            &e->throwable, &e->exception_class);
        e->native_policy = e->throwable == MC_PARTICLE_NO_THROWABLE;
        e->source_exception = e->throwable == MC_PARTICLE_SOURCE_EXCEPTION;
        return e->status;
    }
    const mc_block *block = p->particle->digging_block;
    if (block == grass)
        return MC_ENTITY_OK;
    if (block == NULL)
        return digging_fail(e, MC_ENTITY_STATE_ERROR, "QM actual Block color null receiver", "a",
                            "(III)Lqm;", 23, false);
    /* Actual inherited UU.b(XP,III) is literal white and does not read XP.
     * The two overriding families require their own reached World/color work. */
    if (block->kind == MC_BLOCK_LEAVES || block->kind == MC_BLOCK_TALL_GRASS)
        return digging_fail(e, MC_ENTITY_STATE_ERROR, "QM specialized Block World color child", "a",
                            "(III)Lqm;", 23, true);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return digging_fail(e, MC_ENTITY_UNSUPPORTED_FP, "QM tint floating controls", "a",
                            "(III)Lqm;", 45, true);
    p->particle->fields.i = mc_travel_fmul(p->particle->fields.i, mc_travel_fdiv(255.f, 255.f));
    p->particle->fields.j = mc_travel_fmul(p->particle->fields.j, mc_travel_fdiv(255.f, 255.f));
    p->particle->fields.k = mc_travel_fmul(p->particle->fields.k, mc_travel_fdiv(255.f, 255.f));
    e->source_class = "qm";
    e->source_method = "a";
    e->source_descriptor = "(III)Lqm;";
    e->source_pc = 92;
    return MC_ENTITY_OK;
}

mc_entity_status mc_digging_particle_velocity_scale(mc_entity *p, float scale,
                                                    mc_particle_error *e) {
    if (e == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (!digging_ready(p, e, "c", "(F)Lxw;"))
        return e->status;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return digging_fail(e, MC_ENTITY_UNSUPPORTED_FP, "XW velocity scale floating controls", "c",
                            "(F)Lxw;", 7, true);
    p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, (double)scale);
    p->base.velocity_y = mc_travel_dadd(
        mc_travel_dmul(mc_travel_dsub(p->base.velocity_y, 0.10000000149011612), (double)scale),
        0.10000000149011612);
    p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, (double)scale);
    e->source_class = "xw";
    e->source_method = "c";
    e->source_descriptor = "(F)Lxw;";
    e->source_pc = 42;
    return MC_ENTITY_OK;
}

mc_entity_status mc_digging_particle_size_scale(mc_entity *p, float scale, mc_particle_error *e) {
    if (e == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (!digging_ready(p, e, "d", "(F)Lxw;"))
        return e->status;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return digging_fail(e, MC_ENTITY_UNSUPPORTED_FP, "XW size scale floating controls", "d",
                            "(F)Lxw;", 4, true);
    const float width = mc_travel_fmul(.2f, scale), height = mc_travel_fmul(.2f, scale);
    const mc_entity_status sized = mc_entity_set_size(p, width, height);
    if (sized != MC_ENTITY_OK)
        return digging_fail(e, sized, "XW actual SN size child", "d", "(F)Lxw;", 9, true);
    p->particle->fields.g = mc_travel_fmul(p->particle->fields.g, scale);
    e->source_class = "xw";
    e->source_method = "d";
    e->source_descriptor = "(F)Lxw;";
    e->source_pc = 23;
    return MC_ENTITY_OK;
}

mc_entity_status mc_digging_particle_tick(mc_entity *p, mc_registry_classes *registry,
                                          const mc_entity_move_limits *limits,
                                          mc_item_particle_tick_error *error) {
    return mc_particle_tick_inherited(p, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE, registry, limits,
                                      error);
}

mc_entity_status mc_digging_particle_layer(const mc_entity *p, int32_t *out) {
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = 1;
    return MC_ENTITY_OK;
}

void mc_digging_particle_dispose(mc_entity *p) {
    if (p->particle->digging_registry != NULL)
        (void)mc_registry_runtime_borrow_release(p->particle->digging_registry);
}
