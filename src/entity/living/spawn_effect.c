#include "mc/entity/living/spawn_effect.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../entity_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

#include <emmintrin.h>
#include <float.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Living spawn effect requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Living spawn effect requires strict floating transformations"
#endif
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "Living spawn effect requires binary32/binary64 evaluation");

typedef struct spawn_effect_frame {
    mc_entity *entity;
    mc_registry_classes *registry;
    mc_living_spawn_effect_state *state;
    mc_living_spawn_effect_error *error;
    bool registry_held;
} spawn_effect_frame;

static double dadd(double a, double b) {
    uint64_t left, right;
    memcpy(&left, &a, sizeof(left));
    memcpy(&right, &b, sizeof(right));
    /* The repository's captured x86-64 interpreter DADD profile selects the
     * quieted right NaN. This is the same bounded compatibility operation as
     * World Entity update/bounds; no new LS.V/JIT payload observation is claimed. */
    if ((left & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        (right & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000)) {
        right |= UINT64_C(0x0008000000000000);
        memcpy(&b, &right, sizeof(b));
        return b;
    }
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static double dsub(double a, double b) {
    return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static double dmul(double a, double b) {
    return _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static float fmul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static void at(spawn_effect_frame *f, unsigned int pc) {
    const bool server = f->entity != NULL && f->entity->classes->source == MC_ENTITY_SERVER;
    f->state->source_pc = pc;
    f->error->site =
        (mc_living_spawn_effect_site){server ? "hl" : "ls", server ? "S" : "V", "()V", pc};
}

static bool fail(spawn_effect_frame *f, mc_world_storage_status status, const char *operation,
                 bool native, const char *exception) {
    f->error->status = status;
    f->error->operation = operation;
    f->error->native_policy = native;
    f->error->source_exception = !native;
    f->error->exception_class = exception;
    return false;
}

static bool random_draw(spawn_effect_frame *f, unsigned int pc, bool gaussian, double *out) {
    at(f, pc);
    if (f->entity->random == NULL)
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Living spawn effect null Random", false,
                    "java/lang/NullPointerException");
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Living spawn effect strict floating controls", true, NULL);
    if (gaussian) {
        *out = mc_java_random_next_gaussian(f->entity->random);
        ++f->state->gaussian_calls;
    } else {
        *out = (double)mc_java_random_next_float(f->entity->random);
        ++f->state->float_calls;
    }
    return true;
}

static bool literal(spawn_effect_frame *f, const mc_registry_string **out) {
    at(f, 55);
    if (f->registry == NULL || f->registry->entities != f->entity->classes)
        return fail(f, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                    "Living spawn effect defining Registry namespace", true, NULL);
    if (!f->registry_held) {
        const mc_registry_side side = f->entity->classes->source == MC_ENTITY_SERVER
                                          ? MC_REGISTRY_SERVER
                                          : MC_REGISTRY_CLIENT;
        const mc_registry_status held = mc_registry_classes_retain(
            f->registry, side, f->entity->classes->profile, &f->error->registry);
        if (held != MC_REGISTRY_OK)
            return fail(f,
                        held == MC_REGISTRY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                  : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        f->error->registry.message, true, NULL);
        f->registry_held = true;
    }
    const mc_registry_status status =
        mc_registry_runtime_literal_ascii(f->registry, "explode", out, &f->error->registry);
    return status == MC_REGISTRY_OK ||
           fail(f,
                status == MC_REGISTRY_LIMIT               ? MC_WORLD_STORAGE_LIMIT
                : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                          : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                f->error->registry.message, true, NULL);
}

static bool particle(spawn_effect_frame *f, mc_world_runtime *world, const mc_registry_string *name,
                     const double position[3], const double velocity[3]) {
    at(f, 149);
    if (world == NULL)
        return fail(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Living spawn effect null World", false,
                    "java/lang/NullPointerException");
    const mc_world_storage_status status =
        mc_world_runtime_particle(world, name, position[0], position[1], position[2], velocity[0],
                                  velocity[1], velocity[2], &f->error->particle);
    if (status == MC_WORLD_STORAGE_OK) {
        mc_world_runtime_binding_release(world);
        ++f->state->particles_returned;
        return true;
    }
    mc_living_spawn_effect_error *e = f->error;
    e->world_occurrence = world;
    const bool server = f->entity->classes->source == MC_ENTITY_SERVER;
    e->child =
        (mc_living_spawn_effect_site){server ? "dj" : "fd", "a", "(Ljava/lang/String;DDDDDD)V",
                                      e->particle.world_access_bytecode};
    (void)fail(f, status, e->particle.operation, true, NULL);
    const mc_world_access_failure *failure = &e->particle.world_access_failure;
    if (failure->receipt != NULL || failure->source_null_receiver) {
        e->world_access_capture_attempted = true;
        e->world_access_capture_status = mc_world_access_failure_retain(failure, &e->world_access);
        const char *exception = NULL;
        if (mc_world_access_failure_source_exception(failure, &exception)) {
            e->native_policy = false;
            e->source_exception = true;
            e->exception_class = exception;
            e->exception_borrowed = failure->receipt != NULL;
        }
    } else if (e->particle.world_access_source_fault != MC_WORLD_ACCESS_SOURCE_FAULT_NONE) {
        e->native_policy = false;
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
    }
    return false;
}

static bool body(spawn_effect_frame *f, size_t ceiling) {
    for (f->state->iteration = 0; f->state->iteration < 20; ++f->state->iteration) {
        at(f, 12);
        if (f->state->iterations_started == ceiling)
            return fail(f, MC_WORLD_STORAGE_LIMIT, "Living spawn effect native iteration ceiling",
                        true, NULL);
        ++f->state->iterations_started;
        double velocity[3], draw;
        static const unsigned int gaussian_pc[3] = {12, 24, 37};
        for (size_t i = 0; i < 3; ++i) {
            if (!random_draw(f, gaussian_pc[i], true, &draw))
                return false;
            velocity[i] = dmul(draw, .02);
        }
        at(f, 52);
        mc_world_runtime *world = f->entity->base.world; /* Actual captured getfield52. */
        if (world != NULL && !mc_world_runtime_binding_retain(world)) {
            f->error->world = world->last_error;
            return fail(f, f->error->world.status, f->error->world.operation, true, NULL);
        }
        const mc_registry_string *name = NULL;
        if (!literal(f, &name)) {
            mc_world_runtime_binding_release(world);
            return false;
        }
        double position[3];
        const double x = f->entity->base.x;
        if (!random_draw(f, 65, false, &draw)) {
            mc_world_runtime_binding_release(world);
            return false;
        }
        position[0] =
            dsub(dsub(dadd(x, (double)fmul(fmul((float)draw, f->entity->base.width), 2.0f)),
                      (double)f->entity->base.width),
                 dmul(velocity[0], 10.0));
        const double y = f->entity->base.y;
        if (!random_draw(f, 96, false, &draw)) {
            mc_world_runtime_binding_release(world);
            return false;
        }
        position[1] = dsub(dadd(y, (double)fmul((float)draw, f->entity->base.height)),
                           dmul(velocity[1], 10.0));
        const double z = f->entity->base.z;
        if (!random_draw(f, 120, false, &draw)) {
            mc_world_runtime_binding_release(world);
            return false;
        }
        position[2] =
            dsub(dsub(dadd(z, (double)fmul(fmul((float)draw, f->entity->base.width), 2.0f)),
                      (double)f->entity->base.width),
                 dmul(velocity[2], 10.0));
        if (!particle(f, world, name, position, velocity))
            return false;
    }
    at(f, 158);
    f->state->returned = true;
    return true;
}

mc_living_spawn_effect_limits mc_living_spawn_effect_default_limits(void) {
    return (mc_living_spawn_effect_limits){20};
}

mc_world_storage_status mc_living_spawn_effect(mc_entity *entity, mc_registry_classes *registry,
                                               const mc_living_spawn_effect_limits *limits,
                                               mc_living_spawn_effect_state *state,
                                               mc_living_spawn_effect_error *error) {
    if (state == NULL || error == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (error->world_access.receipt != NULL || error->world_access.source_null_receiver ||
        error->world_occurrence != NULL)
        return MC_WORLD_STORAGE_BUSY;
    *state = (mc_living_spawn_effect_state){0};
    *error = (mc_living_spawn_effect_error){0};
    spawn_effect_frame f = {entity, registry, state, error, false};
    const mc_entity_class_literal *living = NULL;
    bool assignable = false;
    if (entity == NULL ||
        mc_entity_class_literal_get(entity->classes, MC_ENTITY_CLASS_KIND_LIVING_ENTITY, &living) !=
            MC_ENTITY_OK ||
        mc_entity_class_literal_is_assignable_from(living, entity->class_literal, &assignable) !=
            MC_ENTITY_OK ||
        !assignable) {
        (void)fail(&f, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                   "Living spawn effect canonical Living receiver", true, NULL);
        return error->status;
    }
    error->entity = mc_entity_retain(entity);
    if (error->entity != MC_ENTITY_OK) {
        (void)fail(
            &f, error->entity == MC_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT : MC_WORLD_STORAGE_BUSY,
            "Living spawn effect native Entity occurrence", true, NULL);
        return error->status;
    }
    (void)body(&f, limits == NULL ? 20 : limits->max_iterations);
    if (f.registry_held)
        (void)mc_registry_classes_release(registry, NULL);
    mc_entity_release(entity);
    return error->status;
}

void mc_living_spawn_effect_error_retire(mc_living_spawn_effect_error *error) {
    if (error == NULL)
        return;
    mc_world_access_failure_release(&error->world_access);
    error->particle.world_access_failure = (mc_world_access_failure){0};
    if (error->exception_borrowed) {
        error->exception_class = NULL;
        error->exception_borrowed = false;
    }
    mc_world_runtime_binding_release(error->world_occurrence);
    error->world_occurrence = NULL;
}
