#include "mc/entity/movement/material.h"
#include "../../block/registry/runtime_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/material_movement_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../entity_internal.h"

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Entity material movement requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Entity material movement requires strict floating transformations"
#endif

static mc_entity_status fail(mc_entity_material_error *e, mc_entity_status s, const char *op,
                             unsigned pc) {
    e->status = s;
    e->operation = op;
    e->bytecode = pc;
    return s;
}

static bool registry_result(mc_entity_material_error *e, mc_registry_status s, unsigned pc) {
    if (s == MC_REGISTRY_OK)
        return true;
    fail(e,
         s == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
         : s == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                                              : MC_ENTITY_STATE_ERROR,
         "Entity immersion Block registry", pc);
    return false;
}

static bool execute(mc_entity *entity, mc_material *m, bool *out, mc_entity_material_error *e) {
    float eye_offset;
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) ||
        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND) ||
        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_MINECART))
        eye_offset = 0.0f;
    else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        eye_offset = 0.12f;
    else {
        fail(e, MC_ENTITY_STATE_ERROR, "Entity immersion unsupported eye-height dispatch", 5);
        return false;
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fail(e, MC_ENTITY_UNSUPPORTED_FP, "Entity immersion FP admission", 9);
        return false;
    }
    const mc_entity_base_fields *f = mc_entity_runtime_fields(entity);
    volatile double eye =
        _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(f->y), _mm_set_sd((double)eye_offset)));
    const int32_t x = mc_math_floor_double(f->x);
    const float narrowed = (float)mc_math_floor_double(eye);
    /* The input is already binary32. Widening it for the canonical floor
     * preserves in/fq.d(F)I at all narrowing saturation and unordered cases. */
    const int32_t y = mc_math_floor_double((double)narrowed);
    const int32_t z = mc_math_floor_double(f->z);
    mc_world_runtime *w = f->world;
    int32_t id;
    if (mc_world_runtime_block_get(w, x, y, z, &id, &e->world) != MC_WORLD_STORAGE_OK) {
        fail(e, MC_ENTITY_WORLD_FAILED, e->world.operation, 49);
        return false;
    }
    if (id == 0) {
        *out = false;
        return true;
    }
    if (mc_world_runtime_entities(w) != entity->classes) {
        fail(e, MC_ENTITY_STATE_ERROR, "Entity immersion code namespace", 59);
        return false;
    }
    mc_registry_block_m_view array;
    if (!registry_result(e, mc_registry_block_m_get(w->classes->registry, &array, &e->registry),
                         59))
        return false;
    const mc_block *b;
    if (!registry_result(e, mc_registry_block_m_at(&array, id, &b, &e->registry), 64))
        return false;
    const mc_block_fields *fields = mc_block_fields_get(b);
    if (fields == NULL) {
        e->registry.status = MC_REGISTRY_NULL_REFERENCE;
        e->registry.message = "Entity immersion Block.bA receiver";
        fail(e, MC_ENTITY_STATE_ERROR, e->registry.message, 65);
        return false;
    }
    if (fields->bA != m) {
        *out = false;
        return true;
    }
    int32_t metadata;
    if (mc_world_runtime_metadata_get(w, x, y, z, &metadata, &e->world) != MC_WORLD_STORAGE_OK) {
        fail(e, MC_ENTITY_WORLD_FAILED, e->world.operation, 82);
        return false;
    }
    volatile float fraction = mc_material_fluid_height(metadata) - 0.11111111f;
    volatile float top = (float)mc_java_i32_add(y, 1);
    volatile float surface = top - fraction;
    *out = eye < (double)surface;
    return true;
}

mc_entity_status mc_entity_inside_material(mc_entity *entity, mc_material *m, bool *out,
                                           mc_entity_material_error *error) {
    mc_entity_material_error e = {0};
    e.registry = (mc_registry_error){.initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    if (entity == NULL || out == NULL)
        fail(&e, MC_ENTITY_INVALID_ARGUMENT, "Entity immersion arguments", 0);
    else if (entity->phase != MC_ENTITY_READY)
        fail(&e, MC_ENTITY_INVALID_PHASE, "Entity immersion READY admission", 0);
    else {
        mc_entity_status status = mc_entity_retain(entity);
        if (status != MC_ENTITY_OK)
            fail(&e, status, "Entity immersion lifetime hold", 0);
        else {
            (void)execute(entity, m, out, &e);
            mc_entity_release(entity);
        }
    }
    if (e.status == MC_ENTITY_OK)
        e.bytecode = 0;
    if (error != NULL)
        *error = e;
    return e.status;
}
