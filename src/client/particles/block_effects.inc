/* Actual DN hit/destruction bodies. The retained record owns the current NEW,
 * reached Math occurrence and complete acquisition diagnostics. */
#include "../../block/registry/registry_internal.h"
#include "../../entity/living/travel_math_internal.h"
#include "../../entity/particles/digging_color_internal.h"
#include "../../entity/particles/particle_internal.h"
#include "mc/block/registry/registry.h"
#include "mc/block/registry/runtime.h"
#include "mc/world/runtime/blocks.h"

struct mc_effect_block_call {
    mc_effect_block_state state;
    mc_effect_renderer_error failure, append, cleanup;
    mc_particle_error particle;
    mc_digging_color_error color;
    mc_world_storage_error world;
    mc_world_runtime_acquisition_error acquisition;
    mc_registry_error registry;
    mc_math_class_error math_error, math_cleanup;
    mc_math_classes *math;
    mc_entity *candidate;
    mc_world_access_failure receipts[4];
    size_t receipt_count;
};

static bool block_capture_receipt(struct mc_effect_block_call *w,
                                  const mc_world_access_failure *source) {
    if (source->receipt == NULL)
        return true;
    for (size_t i = 0; i < w->receipt_count; ++i)
        if (w->receipts[i].receipt == source->receipt)
            return true;
    if (w->receipt_count == sizeof(w->receipts) / sizeof(*w->receipts))
        return false;
    if (mc_world_access_failure_retain(source, &w->receipts[w->receipt_count]) !=
        MC_WORLD_STORAGE_OK)
        return false;
    ++w->receipt_count;
    return true;
}

/* The acquiring scalar getter can fail during Entity/TileEntity activation.
 * Each distinct concrete callback receipt keeps its entire producer graph
 * alive, just as the inherited particle tick's captured movement children do.
 * A failed retain is retried only as native cleanup; source work never reruns. */
static bool block_capture_acquisition(struct mc_effect_block_call *w) {
    if (w->state.acquisition == NULL)
        return true;
    const mc_world_runtime_acquisition_error *a = w->state.acquisition;
    return block_capture_receipt(w, &a->batch.publication.world_access_failure) &&
           block_capture_receipt(w, &a->tile_entity.world_access_failure) &&
           block_capture_receipt(w, &a->tile_entity.publication.world_access_failure) &&
           block_capture_receipt(w, &a->tile_entity.batch.publication.world_access_failure);
}

static mc_effect_renderer_status block_cleanup_failure(struct mc_effect_block_call *w,
                                                       const char *operation,
                                                       mc_effect_renderer_error *error) {
    error_init(&w->cleanup);
    full_fail(&w->cleanup, MC_EFFECT_RENDERER_BUSY, operation, NULL, NULL, 0);
    w->cleanup.block = &w->state;
    if (error != NULL)
        *error = w->cleanup;
    return MC_EFFECT_RENDERER_BUSY;
}

static bool block_fail(struct mc_effect_block_call *w, mc_effect_renderer_status status,
                       const char *operation, unsigned int pc, bool native) {
    full_fail(&w->failure, status, operation, "a", w->state.hit ? "(IIII)V" : "(IIIII)V", pc);
    w->failure.native_policy = native;
    w->failure.block = &w->state;
    w->state.source_pc = pc;
    return false;
}

static bool block_fp(struct mc_effect_block_call *w, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           block_fail(w, MC_EFFECT_RENDERER_ENTITY_FAILED, "DN block-particle floating controls",
                      pc, true);
}

static bool block_random(mc_client_effect_renderer *r, struct mc_effect_block_call *w,
                         unsigned int pc) {
    if (r->random != NULL)
        return true;
    return block_fail(w, MC_EFFECT_RENDERER_NULL_REFERENCE, "DN actual Random null receiver", pc,
                      false);
}

static bool block_registry(struct mc_effect_block_call *w, mc_registry_classes *registry,
                           int32_t id, const mc_block **out, unsigned int pc) {
    w->state.registry = &w->registry;
    const mc_registry_status status = mc_registry_block_get(registry, id, out, &w->registry);
    if (status == MC_REGISTRY_OK)
        return true;
    const bool native =
        status != MC_REGISTRY_NULL_REFERENCE && status != MC_REGISTRY_INDEX_OUT_OF_RANGE;
    (void)block_fail(w, MC_EFFECT_RENDERER_ENTITY_FAILED, "DN actual Block.m array access", pc,
                     native);
    if (!native) {
        w->failure.source_exception = true;
        w->failure.exception_class = status == MC_REGISTRY_NULL_REFERENCE
                                         ? "java/lang/NullPointerException"
                                         : "java/lang/ArrayIndexOutOfBoundsException";
    }
    return false;
}

static bool block_world(mc_client_effect_renderer *r, struct mc_effect_block_call *w, int32_t *out,
                        bool metadata, unsigned int pc) {
    if (r->world == NULL)
        return block_fail(w, MC_EFFECT_RENDERER_NULL_REFERENCE, "DN actual World null receiver", pc,
                          false);
    w->state.world = &w->world;
    w->state.acquisition = &w->acquisition;
    const mc_world_storage_status status =
        metadata
            ? mc_world_runtime_metadata_get_detailed(r->world, w->state.x, w->state.y, w->state.z,
                                                     out, &w->world, &w->acquisition)
            : mc_world_runtime_block_get_detailed(r->world, w->state.x, w->state.y, w->state.z, out,
                                                  &w->world, &w->acquisition);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    (void)block_fail(w, MC_EFFECT_RENDERER_WORLD_FAILED, w->world.operation, pc, true);
    if (!block_capture_acquisition(w))
        (void)block_cleanup_failure(w, "DN failed acquisition producer capture", NULL);
    return false;
}

static bool block_particle(struct mc_effect_block_call *w, mc_entity_status status,
                           unsigned int pc) {
    w->state.particle = &w->particle;
    if (status == MC_ENTITY_OK)
        return true;
    w->failure.particle = w->particle;
    w->failure.entity = status;
    if (!block_fail(w, MC_EFFECT_RENDERER_ENTITY_FAILED, w->particle.operation, pc,
                    w->particle.native_policy)) {
        w->failure.source_exception = w->particle.source_exception;
        w->failure.exception_class = w->particle.exception_class;
    }
    return false;
}

static bool block_tint(mc_client_effect_renderer *r, struct mc_effect_block_call *w,
                       unsigned int pc) {
    const mc_entity_status status =
        mc_digging_particle_tint_entered(w->candidate, r->render_engine, w->state.x, w->state.y,
                                         w->state.z, &w->color, &w->particle);
    if (w->color.reached)
        w->state.color = &w->color;
    if (status != MC_ENTITY_OK && w->color.failed && w->color.child == MC_DIGGING_COLOR_METADATA &&
        w->color.acquisition_reached) {
        w->state.world = &w->color.world;
        w->state.acquisition = &w->color.acquisition;
        if (!block_capture_acquisition(w))
            (void)block_cleanup_failure(w, "DN failed color acquisition producer capture", NULL);
    }
    return block_particle(w, status, pc);
}

static bool block_append(mc_client_effect_renderer *r, struct mc_effect_block_call *w,
                         unsigned int pc) {
    effect_error_scope work;
    mc_effect_error *e = error_begin(r, &w->append, &work);
    add_entered(r, w->candidate, e);
    const mc_effect_renderer_status status = error_end(e, &work);
    w->state.append = &w->append;
    if (status != MC_EFFECT_RENDERER_OK)
        return block_fail(w, status, w->append.operation, pc, w->append.native_policy);
    ++w->state.appended;
    mc_entity_release(w->candidate);
    w->candidate = NULL;
    w->state.candidate = NULL;
    return true;
}

static bool block_new(mc_client_effect_renderer *r, struct mc_effect_block_call *w,
                      unsigned int pc) {
    mc_entity_error error;
    const mc_entity_status status = mc_digging_particle_allocate(r->classes, &w->candidate, &error);
    w->state.candidate = w->candidate;
    if (status == MC_ENTITY_OK)
        return true;
    w->failure.entity = status;
    return block_fail(w, MC_EFFECT_RENDERER_ENTITY_FAILED, error.operation, pc, true);
}

static bool block_construct(mc_client_effect_renderer *r, struct mc_effect_block_call *w,
                            mc_registry_classes *registry, mc_math_classes *math,
                            const mc_block *block, double vx, double vy, double vz,
                            unsigned int pc) {
    const mc_entity_status status = mc_digging_particle_construct_entered(
        w->candidate, r->world, registry, math, w->state.position[0], w->state.position[1],
        w->state.position[2], vx, vy, vz, block, w->state.face, w->state.metadata, &w->math,
        &w->particle);
    if (!block_particle(w, status, pc))
        return false;
    ++w->state.constructed;
    if (w->math != NULL && mc_math_classes_release(&w->math, &w->math_error) != MC_MATH_CLASS_OK)
        return block_fail(w, MC_EFFECT_RENDERER_BUSY, "DN reached Math occurrence release", pc,
                          true);
    return true;
}

static bool destroy_body(mc_client_effect_renderer *r, struct mc_effect_block_call *w,
                         mc_registry_classes *registry, mc_math_classes *math) {
    if (w->state.block_id == 0) {
        w->state.source_pc = 5;
        w->state.returned = true;
        return true;
    }
    const mc_block *block = NULL;
    if (!block_registry(w, registry, w->state.block_id, &block, 11))
        return false;
    for (int32_t a = 0; a < 4; ++a)
        for (int32_t b = 0; b < 4; ++b)
            for (int32_t c = 0; c < 4; ++c) {
                if (!block_fp(w, 55))
                    return false;
                w->state.position[0] = mc_travel_dadd(
                    (double)w->state.x, mc_travel_ddiv(mc_travel_dadd((double)a, .5), 4.));
                w->state.position[1] = mc_travel_dadd(
                    (double)w->state.y, mc_travel_ddiv(mc_travel_dadd((double)b, .5), 4.));
                w->state.position[2] = mc_travel_dadd(
                    (double)w->state.z, mc_travel_ddiv(mc_travel_dadd((double)c, .5), 4.));
                if (!block_random(r, w, 101))
                    return false;
                (void)mc_java_random_next_bounded(r->random, 6, &w->state.face);
                w->state.destruction_faces[w->state.random_calls++] = w->state.face;
                if (!block_new(r, w, 107))
                    return false;
                const double vx =
                    mc_travel_dsub(mc_travel_dsub(w->state.position[0], (double)w->state.x), .5);
                const double vy =
                    mc_travel_dsub(mc_travel_dsub(w->state.position[1], (double)w->state.y), .5);
                const double vz =
                    mc_travel_dsub(mc_travel_dsub(w->state.position[2], (double)w->state.z), .5);
                if (!block_construct(r, w, registry, math, block, vx, vy, vz, 154) ||
                    !block_tint(r, w, 160) || !block_append(r, w, 163))
                    return false;
            }
    w->state.source_pc = 184;
    w->state.returned = true;
    return true;
}

static bool hit_body(mc_client_effect_renderer *r, struct mc_effect_block_call *w,
                     mc_registry_classes *registry, mc_math_classes *math) {
    if (!block_world(r, w, &w->state.block_id, false, 7))
        return false;
    if (w->state.block_id == 0) {
        w->state.source_pc = 17;
        w->state.returned = true;
        return true;
    }
    const mc_block *block = NULL;
    if (!block_registry(w, registry, w->state.block_id, &block, 23))
        return false;
    const int32_t coordinates[3] = {w->state.x, w->state.y, w->state.z};
    const unsigned int random_pc[3] = {36, 76, 116};
    for (size_t axis = 0; axis < 3; ++axis) {
        if (!block_random(r, w, random_pc[axis]))
            return false;
        const double random = mc_java_random_next_double(r->random);
        w->state.hit_random[axis] = random;
        ++w->state.random_calls;
        /* The actual Random draw precedes the first Block field dereference. */
        if (block == NULL)
            return block_fail(w, MC_EFFECT_RENDERER_NULL_REFERENCE,
                              "DN actual hit Block bounds null receiver", random_pc[axis] + 5u,
                              false);
        if (!block_fp(w, random_pc[axis] + 13u))
            return false;
        const mc_block_fields *f = mc_block_fields_get(block);
        const double low = axis == 0 ? f->bs : axis == 1 ? f->bt : f->bu;
        const double high = axis == 0 ? f->bv : axis == 1 ? f->bw : f->bx;
        double value = mc_travel_dsub(mc_travel_dsub(high, low), (double)mc_travel_fmul(.1f, 2.f));
        value = mc_travel_dmul(random, value);
        value = mc_travel_dadd((double)coordinates[axis], value);
        value = mc_travel_dadd(value, (double).1f);
        w->state.position[axis] = mc_travel_dadd(value, low);
    }
    const mc_block_fields *f = mc_block_fields_get(block);
    const int32_t face = w->state.face;
    if (face == 0)
        w->state.position[1] =
            mc_travel_dsub(mc_travel_dadd((double)w->state.y, f->bt), (double).1f);
    if (face == 1)
        w->state.position[1] =
            mc_travel_dadd(mc_travel_dadd((double)w->state.y, f->bw), (double).1f);
    if (face == 2)
        w->state.position[2] =
            mc_travel_dsub(mc_travel_dadd((double)w->state.z, f->bu), (double).1f);
    if (face == 3)
        w->state.position[2] =
            mc_travel_dadd(mc_travel_dadd((double)w->state.z, f->bx), (double).1f);
    if (face == 4)
        w->state.position[0] =
            mc_travel_dsub(mc_travel_dadd((double)w->state.x, f->bs), (double).1f);
    if (face == 5)
        w->state.position[0] =
            mc_travel_dadd(mc_travel_dadd((double)w->state.x, f->bv), (double).1f);
    if (!block_new(r, w, 270) || !block_world(r, w, &w->state.metadata, true, 298) ||
        !block_construct(r, w, registry, math, block, 0., 0., 0., 301) || !block_tint(r, w, 307) ||
        !block_particle(w, mc_digging_particle_velocity_scale(w->candidate, .2f, &w->particle),
                        312) ||
        !block_particle(w, mc_digging_particle_size_scale(w->candidate, .6f, &w->particle), 317) ||
        !block_append(r, w, 320))
        return false;
    w->state.source_pc = 323;
    w->state.returned = true;
    return true;
}

static mc_effect_renderer_status block_begin(mc_client_effect_renderer *r,
                                             mc_registry_classes *registry, mc_math_classes *math,
                                             int32_t x, int32_t y, int32_t z, int32_t face,
                                             int32_t id, int32_t metadata, bool hit,
                                             mc_effect_renderer_error *error) {
    effect_error_scope scope;
    mc_effect_error *e = error_begin(r, error, &scope);
    if (!enter(r, e))
        return error_end(e, &scope);
    struct mc_effect_block_call *w = allocate(r, sizeof(*w), e, "DN retained block-particle work",
                                              "a", hit ? "(IIII)V" : "(IIIII)V", 0);
    if (w == NULL) {
        r->entered = false;
        return error_end(e, &scope);
    }
    memset(w, 0, sizeof(*w));
    r->block_call = w;
    w->state = (mc_effect_block_state){
        .hit = hit, .x = x, .y = y, .z = z, .face = face, .block_id = id, .metadata = metadata};
    w->failure.block = &w->state;
    (void)(hit ? hit_body(r, w, registry, math) : destroy_body(r, w, registry, math));
    r->entered = false;
    if (error != NULL)
        *error = w->failure;
    return w->failure.status;
}

mc_effect_renderer_status mc_client_effect_renderer_hit_block(mc_client_effect_renderer *r,
                                                              mc_registry_classes *registry,
                                                              mc_math_classes *math, int32_t x,
                                                              int32_t y, int32_t z, int32_t face,
                                                              mc_effect_renderer_error *error) {
    return block_begin(r, registry, math, x, y, z, face, 0, 0, true, error);
}

mc_effect_renderer_status mc_client_effect_renderer_destroy_block(
    mc_client_effect_renderer *r, mc_registry_classes *registry, mc_math_classes *math, int32_t x,
    int32_t y, int32_t z, int32_t id, int32_t metadata, mc_effect_renderer_error *error) {
    return block_begin(r, registry, math, x, y, z, 0, id, metadata, false, error);
}

mc_effect_renderer_status mc_client_effect_renderer_block_cleanup(mc_client_effect_renderer *r,
                                                                  mc_effect_renderer_error *error) {
    if (error != NULL)
        error_init(error);
    if (r == NULL) {
        if (error != NULL)
            full_fail(error, MC_EFFECT_RENDERER_INVALID_ARGUMENT, "DN block cleanup receiver", NULL,
                      NULL, 0);
        return MC_EFFECT_RENDERER_INVALID_ARGUMENT;
    }
    if (r->entered) {
        if (error != NULL)
            full_fail(error, MC_EFFECT_RENDERER_BUSY, "DN block cleanup entered", NULL, NULL, 0);
        return MC_EFFECT_RENDERER_BUSY;
    }
    struct mc_effect_block_call *w = r->block_call;
    if (w == NULL)
        return MC_EFFECT_RENDERER_OK;
    if (!block_capture_acquisition(w))
        return block_cleanup_failure(w, "DN acquisition receipt cleanup capture", error);
    if (w->math != NULL && mc_math_classes_release(&w->math, &w->math_cleanup) != MC_MATH_CLASS_OK)
        return block_cleanup_failure(w, "DN retained Math native cleanup", error);
    while (w->receipt_count != 0)
        mc_world_access_failure_release(&w->receipts[--w->receipt_count]);
    mc_entity_release(w->candidate);
    w->candidate = NULL;
    r->allocation_bytes -= sizeof(*w);
    free(w);
    r->block_call = NULL;
    return MC_EFFECT_RENDERER_OK;
}
