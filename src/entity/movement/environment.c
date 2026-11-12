#include "../tick/arrow_base_execution_internal.h"
#include "environment_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/material_movement_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../entity_internal.h"
#include "../mob/internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/math/math.h"

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Entity environment requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Entity environment requires strict floating transformations"
#endif
#include <emmintrin.h>

void mc_entity_environment_error_init(mc_entity_environment_error *error) {
    static const mc_entity_environment_error zero = {0};
    if (error == NULL)
        return;
    *error = zero;
    mc_world_material_movement_error_init(&error->material);
    mc_world_collision_error_init(&error->collision);
    error->registry = error->material.registry;
}

mc_entity_environment_error mc_entity_environment_empty_error(void) {
    mc_entity_environment_error error;
    mc_entity_environment_error_init(&error);
    return error;
}

static bool fail(mc_entity_environment_error *e, mc_entity_status status,
                 mc_entity_environment_cause cause, const char *operation, unsigned pc) {
    e->status = status;
    e->cause = cause;
    e->operation = operation;
    e->bytecode = pc;
    return false;
}

static mc_entity_status result(mc_entity_environment_error *out,
                               const mc_entity_environment_error *e) {
    if (out != NULL)
        *out = *e;
    return e->status;
}

static bool enter_domain(mc_entity *entity, bool output, mc_entity_environment_error *e,
                         bool inherited_arrow) {
    if (entity == NULL || !output)
        return fail(e, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_ENVIRONMENT_OWNER,
                    "Entity environment arguments", 0);
    if (entity->phase != MC_ENTITY_READY)
        return fail(e, MC_ENTITY_INVALID_PHASE, MC_ENTITY_ENVIRONMENT_OWNER,
                    "Entity environment READY admission", 0);
    const bool arrow = inherited_arrow && mc_entity_has_arrow(entity);
    if (arrow && entity->holds == 0)
        return fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_ENVIRONMENT_OWNER,
                    "Entity environment held Arrow owner", 0);
    if (!arrow && !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_MINECART) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
        !(mc_entity_has_mob(entity) && mc_entity_is_kind(entity, entity->class_literal->kind)))
        return fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_ENVIRONMENT_UNSUPPORTED_VIRTUAL,
                    "Entity environment concrete virtual", 0);
    const mc_entity_status status = mc_entity_retain(entity);
    if (status != MC_ENTITY_OK)
        return fail(e, status, MC_ENTITY_ENVIRONMENT_OWNER, "Entity environment lifetime hold", 0);
    e->source_class = entity->classes->source == MC_ENTITY_CLIENT ? "sn" : "lq";
    return true;
}

static bool enter(mc_entity *entity, bool output, mc_entity_environment_error *e) {
    return enter_domain(entity, output, e, false);
}

static bool enter_arrow_base(mc_entity *entity, bool output, mc_entity_environment_error *e) {
    return enter_domain(entity, output, e, true);
}

static bool floating(mc_entity_environment_error *e, unsigned pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_ENTITY_UNSUPPORTED_FP, MC_ENTITY_ENVIRONMENT_FP,
                "Entity environment FP admission", pc);
}

static bool bounds_result(mc_entity_environment_error *e, mc_entity_status status, unsigned pc) {
    return status == MC_ENTITY_OK ||
           fail(e, status, MC_ENTITY_ENVIRONMENT_BOUNDS, "Entity environment pooled bounds", pc);
}

static bool registry_result(mc_entity_environment_error *e, mc_registry_status status,
                            unsigned pc) {
    if (status == MC_REGISTRY_OK)
        return true;
    const mc_entity_status mapped =
        status == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
        : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
        : status == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
        : status == MC_REGISTRY_INVALID_ARGUMENT  ? MC_ENTITY_INVALID_ARGUMENT
                                                  : MC_ENTITY_STATE_ERROR;
    return fail(e, mapped, MC_ENTITY_ENVIRONMENT_REGISTRY, "Entity environment code Registry", pc);
}

static bool namespace_ok(mc_entity *entity, mc_world_runtime *world, mc_registry_classes *registry,
                         mc_entity_environment_error *e, unsigned pc) {
    if (world == NULL)
        return true; /* The actual reached World invocation fails. */
    if (mc_world_runtime_entities(world) != entity->classes ||
        (registry != NULL && world->classes->registry != registry))
        return fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_ENVIRONMENT_NAMESPACE,
                    "Entity environment World namespace", pc);
    return true;
}

static bool world_result(mc_entity_environment_error *e, mc_world_storage_status status,
                         mc_world_runtime *world, unsigned pc) {
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    e->entity = (mc_entity_error){.status = MC_ENTITY_WORLD_FAILED,
                                  .operation = "Entity environment World call",
                                  .bytecode = (uint16_t)pc,
                                  .world = status};
    return fail(e, MC_ENTITY_WORLD_FAILED,
                world == NULL ? MC_ENTITY_ENVIRONMENT_NULL_REFERENCE : MC_ENTITY_ENVIRONMENT_WORLD,
                "Entity environment World call", pc);
}

static bool liquid_probe(mc_entity *entity, mc_registry_classes *registry, bool water,
                         const mc_world_material_movement_limits *limits, bool *out,
                         mc_entity_environment_error *e, mc_math_execution *execution,
                         mc_tick_arrow_base_execution *state) {
    mc_world_runtime *const receiver = entity->base.world; /* Source load1 precedes args. */
    if (!mc_tick_arrow_base_world_begin(
            state, receiver, (mc_entity_tick_execution_site){"sn", water ? "k_" : "ah", "()Z", 1},
            MC_TICK_ARROW_BASE_ENVIRONMENT))
        return false;
    const bool item = water && mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM);
    if (item)
        e->source_class = entity->classes->source == MC_ENTITY_CLIENT ? "hl" : "ez";
    mc_entity_bounds *query = entity->bounds, *first = NULL, *second = NULL;
    bool completed = false;
    if (!item) {
        const unsigned expand_pc = water ? 13 : 17;
        if (query == NULL) {
            fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_ENVIRONMENT_NULL_REFERENCE,
                 "Entity environment bounds receiver", expand_pc);
            goto leave;
        }
        if (!floating(e, expand_pc))
            goto leave;
        const double horizontal = water ? 0.0 : -0.10000000149011612;
        if (!bounds_result(e,
                           mc_entity_bounds_expand(entity->classes, query, horizontal,
                                                   -0.4000000059604645, horizontal, &first,
                                                   &e->bounds),
                           expand_pc))
            goto leave;
        query = first;
        if (water) {
            if (!bounds_result(e,
                               mc_entity_bounds_contract(entity->classes, query, 0.001, 0.001,
                                                         0.001, &second, &e->bounds),
                               25))
                goto leave;
            query = second;
        }
    }
    const unsigned material_pc = item ? 8 : water ? 28 : 20;
    const unsigned world_pc = item ? 12 : water ? 32 : 23;
    if (execution != NULL && (registry == NULL || registry->entities != entity->classes ||
                              registry->options.side != MC_REGISTRY_CLIENT ||
                              registry->options.random_profile != entity->classes->profile)) {
        fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_ENVIRONMENT_NAMESPACE,
             "Arrow environment actual defining Registry", material_pc);
        goto leave;
    }
    mc_material *material = NULL;
    if (!registry_result(e,
                         mc_registry_material_get(registry, water ? MC_MATERIAL_g : MC_MATERIAL_h,
                                                  &material, &e->registry),
                         material_pc))
        goto leave;
    if (!namespace_ok(entity, receiver, registry, e, world_pc))
        goto leave;
    bool value;
    if (state != NULL) {
        const bool scanned = mc_world_runtime_material_scan_in_execution(
            state->execution, state->code_classes, receiver, state->code, query, material,
            water ? entity : NULL,
            water ? MC_WORLD_MATERIAL_SCAN_ACCELERATE : MC_WORLD_MATERIAL_SCAN_MATCH, limits,
            &value, &e->material, &state->scan, state->owned);
        state->child_returned = true;
        if (!scanned) {
            if (state->owned->ownership_status == MC_WORLD_STORAGE_OK)
                (void)world_result(e, e->material.status, receiver, world_pc);
            goto leave;
        }
        *out = value;
        completed = true;
    } else {
        const mc_world_storage_status status =
            water ? mc_world_runtime_material_accelerate_in_execution(
                        receiver, query, material, entity, limits, &value, &e->material, execution)
                  : mc_world_runtime_contains_material_in_execution(
                        receiver, query, material, limits, &value, &e->material, execution);
        if (world_result(e, status, receiver, world_pc)) {
            *out = value;
            completed = true;
        }
    }
leave:
    if (second != NULL)
        mc_entity_bounds_release(entity->classes, second);
    if (first != NULL)
        mc_entity_bounds_release(entity->classes, first);
    return mc_tick_arrow_base_world_end(state, completed);
}

static mc_entity_status liquid(mc_entity *entity, mc_registry_classes *registry, bool water,
                               const mc_world_material_movement_limits *limits, bool *out,
                               mc_entity_environment_error *error) {
    mc_entity_environment_error e;
    mc_entity_environment_error_init(&e);
    bool value = false, completed = false;
    if (enter(entity, out != NULL, &e)) {
        const mc_registry_status held = mc_registry_classes_retain(
            registry,
            entity->classes->source == MC_ENTITY_CLIENT ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
            entity->classes->profile, &e.registry);
        if (registry_result(&e, held, 0)) {
            completed = liquid_probe(entity, registry, water, limits, &value, &e, NULL, NULL);
            /* Exact native borrow retirement, including an admitted synchronous
             * initializer flow. This does not initialize or replace a class. */
            if (!mc_registry_runtime_borrow_release(registry)) {
                completed = false;
                fail(&e, MC_ENTITY_STATE_ERROR, MC_ENTITY_ENVIRONMENT_OWNER,
                     "Entity environment Registry borrow retirement", 0);
            }
        }
        mc_entity_release(entity);
    }
    if (completed)
        *out = value;
    if (e.status == MC_ENTITY_OK)
        mc_entity_environment_error_init(&e);
    return result(error, &e);
}

static mc_entity_status liquid_held_in_execution(mc_entity *entity, mc_registry_classes *registry,
                                                 bool water, mc_math_execution *execution,
                                                 const mc_world_material_movement_limits *limits,
                                                 bool *out, mc_entity_environment_error *e,
                                                 mc_tick_arrow_base_execution *state,
                                                 bool *continued) {
    if (continued != NULL)
        *continued = false;
    if (e == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_entity_environment_error_init(e);
    if (out == NULL || execution == NULL)
        fail(e, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_ENVIRONMENT_OWNER,
             "Arrow held environment arguments", 0);
    else if (!mc_entity_has_arrow(entity))
        fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_ENVIRONMENT_OWNER,
             "Arrow held environment actual storage", 0);
    else if (entity->phase != MC_ENTITY_READY)
        fail(e, MC_ENTITY_INVALID_PHASE, MC_ENTITY_ENVIRONMENT_OWNER,
             "Arrow held environment READY owner", 0);
    else if (entity->holds == 0)
        fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_ENVIRONMENT_OWNER,
             "Arrow held environment live owner", 0);
    else {
        e->source_class = "sn";
        bool value;
        const bool completed =
            liquid_probe(entity, registry, water, limits, &value, e, execution, state);
        if (continued != NULL)
            *continued = completed;
        if (completed)
            *out = value;
    }
    if (e->status == MC_ENTITY_OK &&
        (state == NULL || state->owned->ownership_status == MC_WORLD_STORAGE_OK))
        mc_entity_environment_error_init(e);
    return e->status;
}

mc_entity_status mc_entity_environment_water_held_in_execution(
    mc_entity *entity, mc_registry_classes *registry, mc_math_execution *execution,
    const mc_world_material_movement_limits *limits, bool *out, mc_entity_environment_error *e) {
    return liquid_held_in_execution(entity, registry, true, execution, limits, out, e, NULL, NULL);
}

mc_entity_status mc_entity_environment_lava_held_in_execution(
    mc_entity *entity, mc_registry_classes *registry, mc_math_execution *execution,
    const mc_world_material_movement_limits *limits, bool *out, mc_entity_environment_error *e) {
    return liquid_held_in_execution(entity, registry, false, execution, limits, out, e, NULL, NULL);
}

bool mc_entity_environment_liquid_in_tick_execution(mc_entity *entity,
                                                    mc_registry_classes *registry, bool water,
                                                    mc_tick_arrow_base_execution *state,
                                                    const mc_world_material_movement_limits *limits,
                                                    bool *out, mc_entity_environment_error *error) {
    if (state == NULL)
        return false;
    if (state->execution == NULL || state->execution->math == NULL || state->owned == NULL ||
        error == NULL) {
        mc_world_runtime_execution_result_note(state->owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "Arrow Base liquid required execution/result/error",
                                               state->receiver_site);
        return false; /* Required owned entry never chooses the legacy branch. */
    }
    bool completed = false;
    const mc_entity_status status = liquid_held_in_execution(
        entity, registry, water, state->execution->math, limits, out, error, state, &completed);
    /* Preserve the exact shared probe/checked-close bool. A raw OK status and
     * ownership channel never reconstruct a discarded continuation result. */
    return completed && status == MC_ENTITY_OK &&
           state->owned->ownership_status == MC_WORLD_STORAGE_OK;
}

mc_entity_status mc_entity_environment_water(mc_entity *entity, mc_registry_classes *registry,
                                             const mc_world_material_movement_limits *limits,
                                             bool *out, mc_entity_environment_error *e) {
    return liquid(entity, registry, true, limits, out, e);
}

mc_entity_status mc_entity_environment_lava(mc_entity *entity, mc_registry_classes *registry,
                                            const mc_world_material_movement_limits *limits,
                                            bool *out, mc_entity_environment_error *e) {
    return liquid(entity, registry, false, limits, out, e);
}

mc_entity_status mc_entity_environment_stored_in_water(mc_entity *entity, bool *out,
                                                       mc_entity_environment_error *error) {
    mc_entity_environment_error e;
    mc_entity_environment_error_init(&e);
    if (enter_arrow_base(entity, out != NULL, &e)) {
        if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SQUID))
            fail(&e, MC_ENTITY_STATE_ERROR, MC_ENTITY_ENVIRONMENT_NAMESPACE,
                 "Squid ag requires its actual defining Registry context", 16);
        else
            *out = entity->base.bx;
        mc_entity_release(entity);
    }
    if (e.status == MC_ENTITY_OK)
        mc_entity_environment_error_init(&e);
    return result(error, &e);
}

mc_entity_status mc_entity_environment_in_water(mc_entity *entity, mc_registry_classes *registry,
                                                const mc_world_material_movement_limits *limits,
                                                bool *out, mc_entity_environment_error *error) {
    if (!mc_entity_has_mob(entity) || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SQUID))
        return mc_entity_environment_stored_in_water(entity, out, error);
    mc_entity_environment_error e;
    mc_entity_environment_error_init(&e);
    bool completed = false, value = false;
    if (enter(entity, out != NULL, &e)) {
        e.source_class = "xt";
        mc_world_runtime *const world = entity->base.world;
        mc_entity_bounds *query = NULL;
        if (entity->bounds == NULL)
            fail(&e, MC_ENTITY_STATE_ERROR, MC_ENTITY_ENVIRONMENT_NULL_REFERENCE,
                 "Squid ag null permanent bounds receiver", 13);
        else if (floating(&e, 13) &&
                 bounds_result(&e,
                               mc_entity_bounds_expand(entity->classes, entity->bounds, 0.0,
                                                       -0.6000000238418579, 0.0, &query, &e.bounds),
                               13)) {
            mc_material *water = NULL;
            if (registry_result(
                    &e, mc_registry_material_get(registry, MC_MATERIAL_g, &water, &e.registry),
                    16) &&
                namespace_ok(entity, world, registry, &e, 20))
                completed =
                    world_result(&e,
                                 mc_world_runtime_material_accelerate(world, query, water, entity,
                                                                      limits, &value, &e.material),
                                 world, 20);
        }
        if (query != NULL)
            mc_entity_bounds_release(entity->classes, query);
        mc_entity_release(entity);
    }
    if (completed)
        *out = value;
    if (e.status == MC_ENTITY_OK)
        mc_entity_environment_error_init(&e);
    return result(error, &e);
}

static bool clearance(mc_entity *entity, double x, double y, double z,
                      const mc_world_material_movement_limits *limits, bool *out,
                      mc_entity_environment_error *e) {
    mc_entity_bounds *query = NULL;
    mc_world_collision_list_view list = {0};
    bool leased = false, complete = false;
    if (entity->bounds == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_ENVIRONMENT_NULL_REFERENCE,
                    "Entity clearance bounds receiver", 8);
    if (!floating(e, 8))
        return false;
    if (!bounds_result(
            e, mc_entity_bounds_shift(entity->classes, entity->bounds, x, y, z, &query, &e->bounds),
            8))
        return false;
    mc_world_runtime *receiver = entity->base.world;
    if (!namespace_ok(entity, receiver, NULL, e, 20))
        goto leave;
    if (!world_result(
            e, mc_world_runtime_collision_boxes(receiver, entity, query, &list, &e->collision),
            receiver, 20))
        goto leave;
    leased = true;
    size_t count;
    const mc_world_storage_status counted = mc_world_collision_list_view_count(&list, &count);
    if (!world_result(e, counted, receiver, 27))
        goto leave;
    if (count > 0) {
        *out = false;
        complete = true;
        goto leave;
    }
    receiver = entity->base.world; /* Distinct source field reload38. */
    if (!namespace_ok(entity, receiver, NULL, e, 43))
        goto leave;
    bool value;
    if (world_result(
            e, mc_world_runtime_contains_liquid(receiver, query, limits, &value, &e->material),
            receiver, 43)) {
        *out = !value;
        complete = true;
    }
leave:
    if (leased)
        mc_world_collision_list_view_release(&list);
    mc_entity_bounds_release(entity->classes, query);
    return complete;
}

mc_entity_status mc_entity_environment_clearance(mc_entity *entity, double x, double y, double z,
                                                 const mc_world_material_movement_limits *limits,
                                                 bool *out, mc_entity_environment_error *error) {
    mc_entity_environment_error e;
    mc_entity_environment_error_init(&e);
    bool value = false, complete = false;
    if (enter(entity, out != NULL, &e)) {
        complete = clearance(entity, x, y, z, limits, &value, &e);
        mc_entity_release(entity);
    }
    if (complete)
        *out = value;
    if (e.status == MC_ENTITY_OK)
        mc_entity_environment_error_init(&e);
    return result(error, &e);
}

static float addf(float a, float b) {
    /* The captured interpreter evaluates commutative bytecodes with the top
     * (right) operand in the destination register. Keep that register order
     * for NaN payload selection; finite arithmetic is unchanged. */
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(b), _mm_set_ss(a)));
}

static float subf(float a, float b) {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float mulf(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(b), _mm_set_ss(a)));
}

static float divf(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static bool relative(mc_entity *entity, const mc_math_trig *trig, float strafe, float forward,
                     float friction, mc_entity_environment_error *e) {
    if (trig == NULL)
        return fail(e, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_ENVIRONMENT_OWNER,
                    "Entity relative trig context", 0);
    if (!floating(e, 2))
        return false;
    const float square = addf(mulf(strafe, strafe), mulf(forward, forward));
    const __m128d root =
        _mm_sqrt_sd(_mm_setzero_pd(), _mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(square)));
    float magnitude = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), root));
    if (magnitude < 0.01f)
        return true;
    if (magnitude < 1.0f)
        magnitude = 1.0f;
    const float factor = divf(friction, magnitude);
    strafe = mulf(strafe, factor);
    forward = mulf(forward, factor);
    const float sine = mc_math_sin(trig, divf(mulf(entity->base.yaw, 3.1415927f), 180.0f));
    const float cosine = mc_math_cos(trig, divf(mulf(entity->base.yaw, 3.1415927f), 180.0f));
    const float dx = subf(mulf(strafe, cosine), mulf(forward, sine));
    entity->base.velocity_x =
        _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd((double)dx), _mm_set_sd(entity->base.velocity_x)));
    const float dz = addf(mulf(forward, cosine), mulf(strafe, sine));
    entity->base.velocity_z =
        _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd((double)dz), _mm_set_sd(entity->base.velocity_z)));
    return true;
}

mc_entity_status mc_entity_environment_relative(mc_entity *entity, const mc_math_trig *trig,
                                                float strafe, float forward, float friction,
                                                mc_entity_environment_error *error) {
    mc_entity_environment_error e;
    mc_entity_environment_error_init(&e);
    if (enter(entity, true, &e)) {
        (void)relative(entity, trig, strafe, forward, friction, &e);
        mc_entity_release(entity);
    }
    if (e.status == MC_ENTITY_OK)
        mc_entity_environment_error_init(&e);
    return result(error, &e);
}
