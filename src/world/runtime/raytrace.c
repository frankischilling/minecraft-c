#include "../../block/registry/runtime_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "raytrace_internal.h"

#include <emmintrin.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(__x86_64__)
#error "World raytrace requires the qualified x86-64 strict FP profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "World raytrace requires strict floating transformations"
#endif
_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "World raytrace requires binary64 evaluation");

mc_world_raytrace_site mc_world_raytrace_core_site(unsigned int pc) {
    return (mc_world_raytrace_site){"fd", "a", "(Lbt;Lbt;ZZ)Lvf;", (int)pc};
}

mc_world_raytrace_status mc_world_raytrace_fail(mc_world_raytrace_work *w,
                                                mc_world_raytrace_status status,
                                                const char *operation, mc_world_raytrace_site at,
                                                bool native) {
    w->failure.status = status;
    w->failure.operation = operation;
    w->failure.site = at;
    if (at.class_name != NULL && strcmp(at.class_name, "fd") == 0)
        w->state.source_pc = (unsigned int)at.pc;
    w->failure.native_policy = native;
    w->failure.caller = at.class_name != NULL && strcmp(at.class_name, "fd") != 0
                            ? mc_world_raytrace_core_site(w->state.source_pc)
                            : w->wrapper;
    if (at.class_name != NULL && strcmp(at.class_name, "uu") == 0 &&
        w->block_parent.class_name != NULL)
        w->failure.caller = w->block_parent;
    w->state.failure = &w->failure;
    return status;
}

mc_world_raytrace_status mc_world_raytrace_null(mc_world_raytrace_work *w,
                                                mc_world_raytrace_site at) {
    w->failure.source_exception = true;
    w->failure.exception_class = "java.lang.NullPointerException";
    return mc_world_raytrace_fail(w, MC_WORLD_RAYTRACE_SOURCE_EXCEPTION,
                                  "World ray source null receiver", at, false);
}

static mc_world_raytrace_status native_status(mc_entity_status status) {
    return status == MC_ENTITY_LIMIT               ? MC_WORLD_RAYTRACE_LIMIT
           : status == MC_ENTITY_ALLOCATION_FAILED ? MC_WORLD_RAYTRACE_ALLOCATION
           : status == MC_ENTITY_BUSY              ? MC_WORLD_RAYTRACE_BUSY
           : status == MC_ENTITY_INVALID_ARGUMENT  ? MC_WORLD_RAYTRACE_ARGUMENT
           : status == MC_ENTITY_INVALID_PHASE || status == MC_ENTITY_STATE_ERROR
               ? MC_WORLD_RAYTRACE_STATE
               : MC_WORLD_RAYTRACE_CHILD_FAILED;
}

static bool capture(mc_world_raytrace_work *w, const mc_world_access_failure *f) {
    if (f->receipt == NULL)
        return true;
    for (size_t i = 0; i < w->capture_count; ++i)
        if (w->captures[i].receipt == f->receipt)
            return true;
    if (w->capture_count == sizeof(w->captures) / sizeof(w->captures[0]))
        return false;
    if (mc_world_access_failure_retain(f, &w->captures[w->capture_count]) != MC_WORLD_STORAGE_OK)
        return false;
    ++w->capture_count;
    const char *exception = NULL;
    if (mc_world_access_failure_source_exception(f, &exception)) {
        w->failure.source_exception = true;
        w->failure.exception_class = exception;
    }
    return true;
}

mc_world_raytrace_status mc_world_raytrace_collision_failed(mc_world_raytrace_work *w,
                                                            mc_world_raytrace_site at) {
    w->failure.collision = &w->collision;
    w->failure.world = &w->collision.nearby.query.cause;
    /* Capture the concrete ld value before any next getter or callback. */
    if (w->collision.nearby.query.source_chunk != NULL) {
        w->source_chunk = *w->collision.nearby.query.source_chunk;
        w->collision.nearby.query.source_chunk = &w->source_chunk;
        w->failure.source_chunk = &w->source_chunk;
        if (w->source_chunk.source.source_exception) {
            w->failure.source_exception = true;
            w->failure.exception_class = w->source_chunk.source.exception_class;
        }
    }
    const mc_live_tile_entity_error *t = &w->collision.nearby.tile_entity;
    if (!capture(w, &w->collision.nearby.batch.publication.world_access_failure) ||
        !capture(w, &t->world_access_failure) ||
        !capture(w, &t->publication.world_access_failure) ||
        !capture(w, &t->batch.publication.world_access_failure))
        return mc_world_raytrace_fail(w, MC_WORLD_RAYTRACE_BUSY,
                                      "World ray callback receipt capture", at, true);
    mc_world_storage_status status = w->collision.nearby.query.status;
    mc_world_raytrace_status mapped =
        status == MC_WORLD_STORAGE_LIMIT               ? MC_WORLD_RAYTRACE_LIMIT
        : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_WORLD_RAYTRACE_ALLOCATION
        : status == MC_WORLD_STORAGE_BUSY              ? MC_WORLD_RAYTRACE_BUSY
        : w->failure.source_exception                  ? MC_WORLD_RAYTRACE_SOURCE_EXCEPTION
                                                       : MC_WORLD_RAYTRACE_CHILD_FAILED;
    if (w->collision.block_class != NULL) {
        w->failure.child = (mc_world_raytrace_site){w->collision.block_class, "a", "(Lxp;III)V",
                                                    (int)w->collision.block_bytecode};
    }
    return mc_world_raytrace_fail(w, mapped, w->collision.nearby.query.operation, at,
                                  !w->failure.source_exception);
}

mc_world_raytrace_status mc_world_raytrace_vector_failed(mc_world_raytrace_work *w,
                                                         mc_world_raytrace_site at) {
    w->failure.vector = &w->vector;
    return mc_world_raytrace_fail(w, native_status(w->vector.status), w->vector.operation, at,
                                  true);
}

mc_world_raytrace_status mc_world_raytrace_geometry_failed(mc_world_raytrace_work *w,
                                                           mc_world_raytrace_site at) {
    w->failure.geometry = &w->geometry;
    w->failure.child =
        (mc_world_raytrace_site){w->geometry.site.declaring_class, w->geometry.site.method,
                                 w->geometry.site.descriptor, w->geometry.site.bytecode};
    if (w->geometry.status == MC_ENTITY_GEOMETRY_SOURCE_NULL_REFERENCE) {
        w->failure.source_exception = true;
        w->failure.exception_class = "java.lang.NullPointerException";
    }
    mc_entity_status child_status = w->geometry.native_status;
    if (w->geometry.status == MC_ENTITY_GEOMETRY_VECTOR_FAILED)
        child_status = w->geometry.vector.status;
    return mc_world_raytrace_fail(w,
                                  w->failure.source_exception ? MC_WORLD_RAYTRACE_SOURCE_EXCEPTION
                                                              : native_status(child_status),
                                  w->geometry.operation, at, w->geometry.native_policy);
}

mc_world_raytrace_status mc_world_raytrace_hit_failed(mc_world_raytrace_work *w,
                                                      mc_world_raytrace_site at) {
    w->failure.hit = &w->hit;
    w->failure.child = (mc_world_raytrace_site){w->hit.site.declaring_class, w->hit.site.method,
                                                w->hit.site.descriptor, w->hit.site.bytecode};
    if (w->hit.status == MC_ENTITY_HIT_SOURCE_NULL_REFERENCE) {
        w->failure.source_exception = true;
        w->failure.exception_class = "java.lang.NullPointerException";
    }
    return mc_world_raytrace_fail(w,
                                  w->failure.source_exception ? MC_WORLD_RAYTRACE_SOURCE_EXCEPTION
                                                              : native_status(w->hit.native.status),
                                  w->hit.operation, at, w->hit.native_policy);
}

static bool release_vector(mc_world_raytrace_work *w, mc_entity_vector **slot) {
    if (*slot == NULL)
        return true;
    const mc_entity_status status = mc_entity_vector_release(w->entities, *slot);
    if (status != MC_ENTITY_OK) {
        w->entity_cleanup =
            (mc_entity_error){.status = status, .operation = "World ray owned vector release"};
        return false;
    }
    *slot = NULL;
    return true;
}

mc_world_raytrace_status mc_world_raytrace_block_locals_retire(mc_world_raytrace_work *w) {
    for (size_t i = 0; i < 2; ++i)
        if (!release_vector(w, &w->local[i]))
            goto failed;
    for (size_t i = 0; i < 6; ++i)
        if (!release_vector(w, &w->generated[i]))
            goto failed;
    if (!release_vector(w, &w->point))
        goto failed;
    return MC_WORLD_RAYTRACE_OK;
failed:
    w->entity = w->entity_cleanup;
    w->failure.entity = &w->entity;
    return mc_world_raytrace_fail(w, MC_WORLD_RAYTRACE_CLEANUP_FAILED,
                                  "World ray completed Block locals retirement",
                                  mc_world_raytrace_core_site(w->state.source_pc), true);
}

static bool block_read(mc_world_raytrace_work *w, int32_t x, int32_t y, int32_t z, bool metadata,
                       unsigned int pc, int32_t *out) {
    w->state.source_pc = pc;
    mc_world_collision_error_init(&w->collision);
    if (mc_world_collision_block_value_held(w->world, x, y, z, metadata, out, &w->collision) ==
        MC_WORLD_STORAGE_OK)
        return true;
    (void)mc_world_raytrace_collision_failed(w, mc_world_raytrace_core_site(pc));
    return false;
}

static bool registry_failure(mc_world_raytrace_work *w, unsigned int pc) {
    w->failure.registry = &w->registry;
    if (w->registry.status == MC_REGISTRY_NULL_REFERENCE ||
        w->registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE) {
        w->failure.source_exception = true;
        w->failure.exception_class = w->registry.status == MC_REGISTRY_NULL_REFERENCE
                                         ? "java.lang.NullPointerException"
                                         : "java.lang.ArrayIndexOutOfBoundsException";
    }
    (void)mc_world_raytrace_fail(w,
                                 w->failure.source_exception ? MC_WORLD_RAYTRACE_SOURCE_EXCEPTION
                                                             : MC_WORLD_RAYTRACE_CHILD_FAILED,
                                 w->registry.message, mc_world_raytrace_core_site(pc),
                                 !w->failure.source_exception);
    return false;
}

static bool cell(mc_world_raytrace_work *w, bool initial) {
    w->block_parent = (mc_world_raytrace_site){0};
    int32_t id = 0, metadata = 0;
    const unsigned int id_pc = initial ? 125u : 838u;
    const unsigned int metadata_pc = initial ? 137u : 850u;
    ++w->state.cells_started;
    if (!block_read(w, w->state.x, w->state.y, w->state.z, false, id_pc, &id) ||
        !block_read(w, w->state.x, w->state.y, w->state.z, true, metadata_pc, &metadata))
        return false;
    ++w->state.cells_read;
    mc_registry_block_m_view array;
    const unsigned int m_pc = initial ? 142u : 855u;
    const unsigned int element_pc = initial ? 147u : 860u;
    w->state.source_pc = m_pc;
    if (mc_registry_block_m_get(w->classes->registry, &array, &w->registry) != MC_REGISTRY_OK)
        return registry_failure(w, m_pc);
    const mc_block *block = NULL;
    w->state.source_pc = element_pc;
    if (mc_registry_block_m_at(&array, id, &block, &w->registry) != MC_REGISTRY_OK)
        return registry_failure(w, element_pc);
    if (w->state.ignore_null_movement_box && block != NULL) {
        w->state.source_pc = initial ? 169u : 882u;
        ++w->state.movement_box_calls;
        mc_world_collision_error_init(&w->collision);
        if (mc_world_collision_block_box_held(w->world, block, w->state.x, w->state.y, w->state.z,
                                              &w->box, &w->collision) != MC_WORLD_STORAGE_OK) {
            (void)mc_world_raytrace_collision_failed(
                w, mc_world_raytrace_core_site(w->state.source_pc));
            return false;
        }
        if (w->box == NULL)
            return true;
        const mc_entity_status status = mc_entity_bounds_release(w->entities, w->box);
        if (status != MC_ENTITY_OK) {
            w->entity = (mc_entity_error){.status = status,
                                          .operation = "World ray optional bounds release"};
            w->failure.entity = &w->entity;
            (void)mc_world_raytrace_fail(w, MC_WORLD_RAYTRACE_CLEANUP_FAILED, w->entity.operation,
                                         mc_world_raytrace_core_site(w->state.source_pc), true);
            return false;
        }
        w->box = NULL;
    }
    if (id <= 0)
        return true;
    w->state.source_pc = initial ? 188u : 901u;
    ++w->state.predicate_calls;
    if (block == NULL) {
        (void)mc_world_raytrace_null(w, mc_world_raytrace_core_site(w->state.source_pc));
        return false;
    }
    bool collides = false;
    if (mc_world_raytrace_block_can_collide_held(w, block, metadata, w->state.stop_on_liquid,
                                                 &collides) != MC_WORLD_RAYTRACE_OK)
        return false;
    if (!collides)
        return true;
    w->state.source_pc = initial ? 205u : 918u;
    ++w->state.block_ray_calls;
    return mc_world_raytrace_block_held(w, block, w->state.x, w->state.y, w->state.z,
                                        w->state.start, w->state.end,
                                        &w->state.result) == MC_WORLD_RAYTRACE_OK;
}

static bool floating(mc_world_raytrace_work *w, unsigned int pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    (void)mc_world_raytrace_fail(w, MC_WORLD_RAYTRACE_STATE, "World ray strict FP admission",
                                 mc_world_raytrace_core_site(pc), true);
    return false;
}

static bool nan_start(mc_world_raytrace_work *w, unsigned int first) {
    mc_entity_vector *s = w->state.start;
    if (s == NULL) {
        (void)mc_world_raytrace_null(w, mc_world_raytrace_core_site(first));
        return false;
    }
    return isnan(s->x) || isnan(s->y) || isnan(s->z);
}

/* Explicit operations prevent FMA/reassociation under the strict build flags. */
static double add(double a, double b) {
    volatile double value = a + b;
    return value;
}

static double mul(double a, double b) {
    volatile double value = a * b;
    return value;
}

static bool advance(mc_world_raytrace_work *w) {
    mc_entity_vector *s = w->state.start;
    mc_entity_vector *e = w->state.end;
    bool enabled[3] = {true, true, true};
    double boundary[3] = {999.0, 999.0, 999.0};
    const int32_t current[3] = {w->state.x, w->state.y, w->state.z};
    const int32_t target[3] = {w->state.target_x, w->state.target_y, w->state.target_z};
    for (size_t i = 0; i < 3; ++i) {
        if (target[i] > current[i]) {
            if (!floating(w, i == 0 ? 321u : i == 1 ? 358u : 395u))
                return false;
            boundary[i] = add((double)current[i], 1.0);
        } else if (target[i] < current[i]) {
            if (!floating(w, i == 0 ? 338u : i == 1 ? 375u : 412u))
                return false;
            boundary[i] = add((double)current[i], 0.0);
        } else
            enabled[i] = false;
    }
    const double dx = e->x - s->x;
    const double dy = e->y - s->y;
    const double dz = e->z - s->z;
    double tx = 999.0, ty = 999.0, tz = 999.0;
    if (enabled[0])
        tx = (boundary[0] - s->x) / dx;
    if (enabled[1])
        ty = (boundary[1] - s->y) / dy;
    if (enabled[2])
        tz = (boundary[2] - s->z) / dz;
    if (tx < ty && tx < tz) {
        w->state.face = target[0] > current[0] ? 4 : 5;
        s->x = boundary[0];
        s->y = add(s->y, mul(dy, tx));
        s->z = add(s->z, mul(dz, tx));
    } else if (ty < tz) {
        w->state.face = target[1] > current[1] ? 0 : 1;
        s->x = add(s->x, mul(dx, ty));
        s->y = boundary[1];
        s->z = add(s->z, mul(dz, ty));
    } else {
        w->state.face = target[2] > current[2] ? 2 : 3;
        s->x = add(s->x, mul(dx, tz));
        s->y = add(s->y, mul(dy, tz));
        s->z = boundary[2];
    }
    w->state.source_pc = 715;
    if (mc_entity_vector_pool_get(w->entities, s->x, s->y, s->z, &w->pending_scratch, &w->vector) !=
        MC_ENTITY_OK) {
        (void)mc_world_raytrace_vector_failed(w, mc_world_raytrace_core_site(715));
        return false;
    }
    if (!release_vector(w, &w->scratch)) {
        /* The completed factory occurrence remains retained for cleanup. */
        w->entity = w->entity_cleanup;
        w->failure.entity = &w->entity;
        (void)mc_world_raytrace_fail(w, MC_WORLD_RAYTRACE_CLEANUP_FAILED,
                                     w->entity_cleanup.operation, mc_world_raytrace_core_site(718),
                                     true);
        return false;
    }
    w->scratch = w->pending_scratch;
    w->pending_scratch = NULL;
    mc_entity_vector *fresh = w->scratch;
    fresh->x = (double)mc_math_floor_double(s->x);
    w->state.x = mc_java_double_to_i32(fresh->x);
    if (w->state.face == 5) {
        w->state.x = mc_java_i32_sub(w->state.x, 1);
        fresh->x = add(fresh->x, 1.0);
    }
    fresh->y = (double)mc_math_floor_double(s->y);
    w->state.y = mc_java_double_to_i32(fresh->y);
    if (w->state.face == 1) {
        w->state.y = mc_java_i32_sub(w->state.y, 1);
        fresh->y = add(fresh->y, 1.0);
    }
    fresh->z = (double)mc_math_floor_double(s->z);
    w->state.z = mc_java_double_to_i32(fresh->z);
    if (w->state.face == 3) {
        w->state.z = mc_java_i32_sub(w->state.z, 1);
        fresh->z = add(fresh->z, 1.0);
    }
    return true;
}

static void execute(mc_world_raytrace_work *w) {
    if (nan_start(w, 1)) {
        w->state.source_pc = 30;
        return;
    }
    if (w->failure.status != MC_WORLD_RAYTRACE_OK)
        return;
    mc_entity_vector *e = w->state.end;
    if (e == NULL) {
        (void)mc_world_raytrace_null(w, mc_world_raytrace_core_site(33));
        return;
    }
    if (isnan(e->x) || isnan(e->y) || isnan(e->z)) {
        w->state.source_pc = 62;
        return;
    }
    w->state.target_x = mc_math_floor_double(e->x);
    w->state.target_y = mc_math_floor_double(e->y);
    w->state.target_z = mc_math_floor_double(e->z);
    w->state.x = mc_math_floor_double(w->state.start->x);
    w->state.y = mc_math_floor_double(w->state.start->y);
    w->state.z = mc_math_floor_double(w->state.start->z);
    if (!cell(w, true))
        return;
    if (w->state.result != NULL) {
        w->state.source_pc = 217;
        return;
    }
    for (int32_t remaining = 200; remaining-- >= 0;) {
        ++w->state.steps_started;
        if (nan_start(w, 232)) {
            w->state.source_pc = 261;
            return;
        }
        if (w->state.x == w->state.target_x && w->state.y == w->state.target_y &&
            w->state.z == w->state.target_z) {
            w->state.source_pc = 284;
            return;
        }
        if (!advance(w) || !cell(w, false))
            return;
        if (w->state.result != NULL) {
            w->state.source_pc = 930;
            return;
        }
    }
    w->state.source_pc = 934;
}

static mc_world_raytrace_status begin(mc_world_runtime *world, mc_entity_vector *start,
                                      mc_entity_vector *end, bool liquid, bool ignore,
                                      const char *wrapper_descriptor,
                                      mc_world_raytrace_call **out) {
    if (out == NULL || *out != NULL || world == NULL)
        return MC_WORLD_RAYTRACE_ARGUMENT;
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT || world->classes->registry == NULL ||
        world->classes->entities == NULL)
        return MC_WORLD_RAYTRACE_STATE;
    mc_world_raytrace_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return MC_WORLD_RAYTRACE_ALLOCATION;
    mc_world_raytrace_work *w = &call->work;
    w->world = world;
    w->classes = world->classes;
    w->entities = world->classes->entities;
    w->state.world = world;
    w->state.start = start;
    w->state.end = end;
    w->state.stop_on_liquid = liquid;
    w->state.ignore_null_movement_box = ignore;
    w->state.active = true;
    w->state.phase = MC_WORLD_RAYTRACE_ACTIVE;
    if (wrapper_descriptor != NULL)
        w->wrapper = (mc_world_raytrace_site){"fd", "a", wrapper_descriptor, 5};
    *out = call;
    if (!mc_world_runtime_binding_retain(world)) {
        const bool limit = world->persistent_bindings == SIZE_MAX;
        w->world_error = (mc_world_storage_error){limit ? MC_WORLD_STORAGE_LIMIT
                                                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                  "World ray actual World lifetime hold", 0};
        w->failure.world = &w->world_error;
        (void)mc_world_raytrace_fail(w, limit ? MC_WORLD_RAYTRACE_LIMIT : MC_WORLD_RAYTRACE_STATE,
                                     w->world_error.operation, mc_world_raytrace_core_site(0),
                                     true);
        goto done;
    }
    w->world_held = true;
    if (start != NULL) {
        const mc_entity_status status = mc_entity_vector_retain(w->entities, start);
        if (status != MC_ENTITY_OK) {
            w->entity = (mc_entity_error){.status = status, .operation = "World ray start hold"};
            w->failure.entity = &w->entity;
            (void)mc_world_raytrace_fail(w, native_status(status), w->entity.operation,
                                         mc_world_raytrace_core_site(0), true);
            goto done;
        }
        w->start_held = true;
    }
    if (end != NULL) {
        const mc_entity_status status = mc_entity_vector_retain(w->entities, end);
        if (status != MC_ENTITY_OK) {
            w->entity = (mc_entity_error){.status = status, .operation = "World ray end hold"};
            w->failure.entity = &w->entity;
            (void)mc_world_raytrace_fail(w, native_status(status), w->entity.operation,
                                         mc_world_raytrace_core_site(0), true);
            goto done;
        }
        w->end_held = true;
    }
    if (mc_world_runtime_call_enter(world, &w->world_error) != MC_WORLD_STORAGE_OK) {
        w->failure.world = &w->world_error;
        const mc_world_raytrace_status status =
            w->world_error.status == MC_WORLD_STORAGE_LIMIT  ? MC_WORLD_RAYTRACE_LIMIT
            : w->world_error.status == MC_WORLD_STORAGE_BUSY ? MC_WORLD_RAYTRACE_BUSY
                                                             : MC_WORLD_RAYTRACE_STATE;
        (void)mc_world_raytrace_fail(w, status, w->world_error.operation,
                                     mc_world_raytrace_core_site(0), true);
        goto done;
    }
    execute(w);
    mc_world_runtime_call_leave(world);
done:
    w->state.active = false;
    w->state.source_returned = w->failure.status == MC_WORLD_RAYTRACE_OK;
    w->state.phase =
        w->state.source_returned ? MC_WORLD_RAYTRACE_RETURNED : MC_WORLD_RAYTRACE_FAILED;
    return w->failure.status;
}

mc_world_raytrace_status mc_world_raytrace_begin(mc_world_runtime *world, mc_entity_vector *start,
                                                 mc_entity_vector *end,
                                                 mc_world_raytrace_call **out) {
    return begin(world, start, end, false, false, "(Lbt;Lbt;)Lvf;", out);
}

mc_world_raytrace_status mc_world_raytrace_liquid_begin(mc_world_runtime *world,
                                                        mc_entity_vector *start,
                                                        mc_entity_vector *end, bool liquid,
                                                        mc_world_raytrace_call **out) {
    return begin(world, start, end, liquid, false, "(Lbt;Lbt;Z)Lvf;", out);
}

mc_world_raytrace_status mc_world_raytrace_flags_begin(mc_world_runtime *world,
                                                       mc_entity_vector *start,
                                                       mc_entity_vector *end, bool liquid,
                                                       bool ignore, mc_world_raytrace_call **out) {
    return begin(world, start, end, liquid, ignore, NULL, out);
}

bool mc_world_raytrace_inspect(const mc_world_raytrace_call *call, mc_world_raytrace_state *out) {
    if (call == NULL || out == NULL)
        return false;
    *out = call->work.state;
    return true;
}

static mc_world_raytrace_status cleanup_failed(mc_world_raytrace_work *w, const char *operation) {
    w->cleanup.status = MC_WORLD_RAYTRACE_CLEANUP_FAILED;
    w->cleanup.operation = operation;
    w->cleanup.native_policy = true;
    w->state.cleanup = &w->cleanup;
    return w->cleanup.status;
}

mc_world_raytrace_status mc_world_raytrace_retire(mc_world_raytrace_call **slot) {
    if (slot == NULL)
        return MC_WORLD_RAYTRACE_ARGUMENT;
    if (*slot == NULL)
        return MC_WORLD_RAYTRACE_OK;
    mc_world_raytrace_work *w = &(*slot)->work;
    if (w->state.active)
        return MC_WORLD_RAYTRACE_BUSY;
    w->state.retiring = true;
    w->state.phase = MC_WORLD_RAYTRACE_RETIRING;
    memset(&w->cleanup, 0, sizeof(w->cleanup));
    w->state.cleanup = NULL;
    if (mc_entity_hit_result_release(&w->candidate, &w->hit_cleanup) != MC_ENTITY_HIT_OK ||
        mc_entity_hit_result_release(&w->state.result, &w->hit_cleanup) != MC_ENTITY_HIT_OK) {
        w->state.pending_result = w->candidate;
        w->cleanup.hit = &w->hit_cleanup;
        return cleanup_failed(w, "World ray owned HitResult release");
    }
    w->state.pending_result = NULL;
    for (size_t i = 0; i < 2; ++i)
        if (!release_vector(w, &w->local[i]))
            goto vector_failure;
    for (size_t i = 0; i < 6; ++i)
        if (!release_vector(w, &w->generated[i]))
            goto vector_failure;
    if (!release_vector(w, &w->point) || !release_vector(w, &w->scratch) ||
        !release_vector(w, &w->pending_scratch))
        goto vector_failure;
    if (w->box != NULL) {
        const mc_entity_status status = mc_entity_bounds_release(w->entities, w->box);
        if (status != MC_ENTITY_OK) {
            w->entity_cleanup = (mc_entity_error){
                .status = status, .operation = "World ray native optional box cleanup"};
            goto vector_failure;
        }
        w->box = NULL;
    }
    if (w->start_held) {
        const mc_entity_status status = mc_entity_vector_release(w->entities, w->state.start);
        if (status != MC_ENTITY_OK) {
            w->entity_cleanup =
                (mc_entity_error){.status = status, .operation = "World ray start cleanup"};
            goto vector_failure;
        }
        w->start_held = false;
        w->state.start = NULL;
    }
    if (w->end_held) {
        const mc_entity_status status = mc_entity_vector_release(w->entities, w->state.end);
        if (status != MC_ENTITY_OK) {
            w->entity_cleanup =
                (mc_entity_error){.status = status, .operation = "World ray end cleanup"};
            goto vector_failure;
        }
        w->end_held = false;
        w->state.end = NULL;
    }
    for (size_t i = 0; i < w->capture_count; ++i)
        mc_world_access_failure_release(&w->captures[i]);
    if (w->world_held)
        mc_world_runtime_binding_release(w->world);
    free(*slot);
    *slot = NULL;
    return MC_WORLD_RAYTRACE_OK;
vector_failure:
    w->cleanup.entity = &w->entity_cleanup;
    return cleanup_failed(w, w->entity_cleanup.operation);
}
