#include "mc/world/runtime/material_movement.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../entity/entity_internal.h"
#include "collision/blocks_internal.h"
#include "construction_internal.h"
#include "material_movement_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "runtime_internal.h"

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Material movement requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Material movement requires strict floating transformations"
#endif
#include <emmintrin.h>

void mc_world_material_movement_error_init(mc_world_material_movement_error *error) {
    static const mc_world_material_movement_error zero = {0};
    if (error == NULL)
        return;
    *error = zero;
    mc_world_movement_error_init(&error->area);
    mc_world_collision_error_init(&error->block);
    error->registry = error->area.registry;
    error->material.registry = error->registry;
}

mc_world_material_movement_error mc_world_material_movement_empty_error(void) {
    mc_world_material_movement_error error;
    mc_world_material_movement_error_init(&error);
    return error;
}

static bool fail(mc_world_material_movement_error *e, mc_world_storage_status status,
                 const char *op) {
    e->status = status;
    e->operation = op;
    e->cause = (mc_world_storage_error){status, op, 0};
    return false;
}

static bool cause(mc_world_material_movement_error *e, mc_world_storage_error c) {
    e->status = c.status;
    e->operation = c.operation;
    e->cause = c;
    return false;
}

static bool registry_fail(mc_world_runtime *w, mc_world_material_movement_error *e) {
    e->status = mc_world_runtime_registry_result(w, e->registry.status, &e->registry,
                                                 "Material movement Registry", &e->cause);
    e->operation = e->cause.operation;
    return false;
}

static bool null_registry(mc_world_runtime *w, mc_world_material_movement_error *e,
                          const char *op) {
    e->registry = e->area.registry;
    e->registry.status = MC_REGISTRY_NULL_REFERENCE;
    e->registry.message = op;
    return registry_fail(w, e);
}

static mc_world_storage_status entity_status(mc_entity_status s) {
    return s == MC_ENTITY_LIMIT               ? MC_WORLD_STORAGE_LIMIT
           : s == MC_ENTITY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : s == MC_ENTITY_BUSY              ? MC_WORLD_STORAGE_BUSY
                                              : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static bool vector_result(mc_world_material_movement_error *e, mc_entity_status s) {
    return s == MC_ENTITY_OK || fail(e, entity_status(s), e->vector.operation);
}

static bool floating(mc_world_material_movement_error *e) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Material movement FP admission");
}

static bool cell(mc_world_material_movement_error *e) {
    if (e->cell_work == e->max_cell_work)
        return fail(e, MC_WORLD_STORAGE_LIMIT, "Material movement cell-work limit");
    ++e->cell_work;
    return true;
}

static bool read_value(mc_world_runtime *w, int32_t x, int32_t y, int32_t z, bool metadata,
                       int32_t *out, mc_world_material_movement_error *e) {
    if (mc_world_collision_block_value_held(w, x, y, z, metadata, out, &e->block) ==
        MC_WORLD_STORAGE_OK)
        return true;
    return cause(e, e->block.nearby.query.cause);
}

static bool material(mc_world_runtime *w, int32_t x, int32_t y, int32_t z, mc_material **out,
                     mc_world_material_movement_error *e) {
    if (mc_world_runtime_material_get_detailed(w, x, y, z, out, &e->material) ==
        MC_WORLD_STORAGE_OK)
        return true;
    return cause(e, e->material.cause);
}

static bool selected_block(mc_world_runtime *w, int32_t x, int32_t y, int32_t z,
                           const mc_block **out, unsigned load, unsigned getter, unsigned index,
                           mc_world_material_movement_error *e) {
    mc_registry_block_m_view array;
    e->bytecode = load;
    if (mc_registry_block_m_get(w->classes->registry, &array, &e->registry) != MC_REGISTRY_OK)
        return registry_fail(w, e);
    e->bytecode = getter;
    int32_t id;
    if (!read_value(w, x, y, z, false, &id, e))
        return false;
    e->bytecode = index;
    if (mc_registry_block_m_at(&array, id, out, &e->registry) != MC_REGISTRY_OK)
        return registry_fail(w, e);
    return true;
}

static bool decay(mc_world_runtime *w, const mc_block *b, int32_t x, int32_t y, int32_t z,
                  int32_t *out, unsigned pc, mc_world_material_movement_error *e) {
    e->fluid_bytecode = pc;
    e->fluid_child_bytecode = 5;
    mc_material *m;
    if (!material(w, x, y, z, &m, e))
        return false;
    if (m != b->fields.bA) {
        *out = -1;
        e->fluid_child_bytecode = 0;
        return true;
    }
    e->fluid_child_bytecode = 24;
    if (!read_value(w, x, y, z, true, out, e))
        return false;
    if (*out >= 8)
        *out = 0;
    e->fluid_child_bytecode = 0;
    return true;
}

static bool side_solid(mc_world_runtime *w, const mc_block *b, int32_t x, int32_t y, int32_t z,
                       unsigned pc, bool *out, mc_world_material_movement_error *e) {
    e->fluid_bytecode = pc;
    e->fluid_child_bytecode = 5;
    mc_material *m;
    if (!material(w, x, y, z, &m, e))
        return false;
    if (m == b->fields.bA) {
        *out = false;
        e->fluid_child_bytecode = 0;
        return true;
    }
    e->fluid_child_bytecode = 25;
    mc_material *ice;
    if (mc_registry_material_get(w->classes->registry, MC_MATERIAL_s, &ice, &e->registry) !=
        MC_REGISTRY_OK)
        return registry_fail(w, e);
    if (m == ice) {
        *out = false;
        e->fluid_child_bytecode = 0;
        return true;
    }
    /* All reached directions are 2,3,4,5: invoke the actual super body rather
     * than the fluid's direction==1 early true. uu.d/na.b reloads Material. */
    e->fluid_child_bytecode = 49;
    if (!material(w, x, y, z, &m, e))
        return false;
    if (m == NULL)
        return null_registry(w, e, "Fluid side Material receiver");
    *out = mc_material_a(m);
    e->fluid_child_bytecode = 0;
    return true;
}

static bool replace_add(mc_entity_classes *classes, mc_entity_vector **v, double x, double y,
                        double z, mc_world_material_movement_error *e) {
    mc_entity_vector *next;
    if (!vector_result(e, mc_entity_vector_add(classes, *v, x, y, z, &next, &e->vector)))
        return false;
    mc_entity_vector_release(classes, *v);
    *v = next;
    return true;
}

static bool replace_normal(mc_entity_classes *classes, mc_entity_vector **v,
                           mc_world_material_movement_error *e) {
    mc_entity_vector *next;
    if (!vector_result(e, mc_entity_vector_normalize(classes, *v, &next, &e->vector)))
        return false;
    mc_entity_vector_release(classes, *v);
    *v = next;
    return true;
}

static bool fluid_vector(mc_world_runtime *w, const mc_block *b, int32_t x, int32_t y, int32_t z,
                         mc_entity_vector **out, mc_world_material_movement_error *e) {
    mc_entity_classes *classes = mc_world_runtime_entities(w);
    mc_entity_vector *v = NULL;
    e->fluid_bytecode = 3;
    if (!vector_result(e, mc_entity_vector_pool_get(classes, 0, 0, 0, &v, &e->vector)))
        return false;
    int32_t level;
    if (!decay(w, b, x, y, z, &level, 14, e))
        goto failed;
    for (int d = 0; d < 4; ++d) {
        int32_t nx = x, nz = z;
        if (d == 0)
            nx = mc_java_i32_sub(nx, 1);
        if (d == 1)
            nz = mc_java_i32_sub(nz, 1);
        if (d == 2)
            nx = mc_java_i32_add(nx, 1);
        if (d == 3)
            nz = mc_java_i32_add(nz, 1);
        int32_t neighbor;
        if (!decay(w, b, nx, y, nz, &neighbor, 81, e))
            goto failed;
        unsigned add_pc = 213;
        int32_t delta;
        if (neighbor < 0) {
            e->fluid_bytecode = 98;
            mc_material *m;
            if (!material(w, nx, y, nz, &m, e))
                goto failed;
            e->fluid_bytecode = 103;
            if (m == NULL) {
                null_registry(w, e, "Fluid flow Material.c receiver");
                goto failed;
            }
            if (mc_material_c(m))
                continue;
            if (!decay(w, b, nx, mc_java_i32_sub(y, 1), nz, &neighbor, 119, e))
                goto failed;
            if (neighbor < 0)
                continue;
            delta = mc_java_i32_sub(neighbor, mc_java_i32_sub(level, 8));
            add_pc = 166;
        } else
            delta = mc_java_i32_sub(neighbor, level);
        const double dx = (double)mc_java_i32_mul(mc_java_i32_sub(nx, x), delta);
        const double dz = (double)mc_java_i32_mul(mc_java_i32_sub(nz, z), delta);
        e->fluid_bytecode = add_pc;
        if (!replace_add(classes, &v, dx, 0, dz, e))
            goto failed;
    }
    e->fluid_bytecode = 229;
    int32_t metadata;
    if (!read_value(w, x, y, z, true, &metadata, e))
        goto failed;
    if (metadata >= 8) {
        static const int8_t offsets[8][3] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0},
                                             {0, 1, -1}, {0, 1, 1}, {-1, 1, 0}, {1, 1, 0}};
        static const unsigned pcs[8] = {256, 279, 302, 325, 350, 375, 400, 425};
        bool solid = false;
        for (unsigned i = 0; i < 8 && !solid; ++i) {
            if (!side_solid(w, b, mc_java_i32_add(x, offsets[i][0]),
                            mc_java_i32_add(y, offsets[i][1]), mc_java_i32_add(z, offsets[i][2]),
                            pcs[i], &solid, e))
                goto failed;
        }
        if (solid) {
            e->fluid_bytecode = 441;
            if (!replace_normal(classes, &v, e))
                goto failed;
            e->fluid_bytecode = 449;
            if (!replace_add(classes, &v, 0, -6, 0, e))
                goto failed;
        }
    }
    e->fluid_bytecode = 456;
    if (!replace_normal(classes, &v, e))
        goto failed;
    *out = v;
    e->fluid_bytecode = 0;
    return true;
failed:
    mc_entity_vector_release(classes, v);
    return false;
}

static bool accelerate_block(mc_world_runtime *w, const mc_block *b, int32_t x, int32_t y,
                             int32_t z, mc_entity_vector *sum,
                             mc_world_material_movement_error *e) {
    e->stair_bytecode = 0;
    e->delegation_depth = 0;
    while (b->kind == MC_BLOCK_STAIRS) {
        e->stair_bytecode = 13;
        if (!cell(e))
            return false;
        ++e->delegation_depth;
        b = b->subtype.ss.a;
        if (b == NULL)
            return null_registry(w, e, "Stair acceleration wrapped Block receiver");
    }
    if (b->kind < MC_BLOCK_BASE || b->kind >= MC_BLOCK_KIND_COUNT)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block acceleration concrete dispatch");
    /* Remaining admitted kinds inherit the base literal return or rp/kv.
     * om/jc and nx/is inherit the fluid body unchanged. */
    if (b->kind != MC_BLOCK_FLOWING_FLUID && b->kind != MC_BLOCK_STILL_FLUID) {
        e->stair_bytecode = 0;
        e->delegation_depth = 0;
        return true;
    }
    e->acceleration_bytecode = 6;
    mc_entity_vector *flow;
    if (!fluid_vector(w, b, x, y, z, &flow, e))
        return false;
    e->acceleration_bytecode = 23;
    sum->x = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(sum->x), _mm_set_sd(flow->x)));
    e->acceleration_bytecode = 38;
    sum->y = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(sum->y), _mm_set_sd(flow->y)));
    e->acceleration_bytecode = 53;
    sum->z = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(sum->z), _mm_set_sd(flow->z)));
    mc_entity_vector_release(mc_world_runtime_entities(w), flow);
    e->acceleration_bytecode = 0;
    e->stair_bytecode = 0;
    e->delegation_depth = 0;
    return true;
}

static bool execute(mc_world_runtime *w, const mc_entity_bounds *b, mc_material *m,
                    mc_entity *entity, int kind, bool *out, mc_world_material_movement_error *e) {
    e->bytecode = 1;
    if (b == NULL)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Material scan bounds receiver");
    e->bytecode = 4;
    if (!floating(e))
        return false;
    int32_t lx = mc_math_floor_double(b->min_x), hx = mc_math_floor_double(b->max_x + 1.0);
    int32_t ly = mc_math_floor_double(b->min_y), hy = mc_math_floor_double(b->max_y + 1.0);
    int32_t lz = mc_math_floor_double(b->min_z), hz = mc_math_floor_double(b->max_z + 1.0);
    if (kind == 1) {
        if (b->min_x < 0.0)
            lx = mc_java_i32_sub(lx, 1);
        if (b->min_y < 0.0)
            ly = mc_java_i32_sub(ly, 1);
        if (b->min_z < 0.0)
            lz = mc_java_i32_sub(lz, 1);
    }
    mc_entity_classes *classes = mc_world_runtime_entities(w);
    mc_entity_vector *sum = NULL;
    if (kind == 2) {
        e->bytecode = 73;
        bool loaded;
        if (mc_world_runtime_area_loaded(w, lx, ly, lz, hx, hy, hz, &loaded, &e->area) !=
            MC_WORLD_STORAGE_OK)
            return cause(e, e->area.cause);
        if (!loaded) {
            *out = false;
            return true;
        }
        e->bytecode = 87;
        if (!vector_result(e, mc_entity_vector_pool_get(classes, 0, 0, 0, &sum, &e->vector)))
            return false;
    }
    bool found = false, done = false;
    for (int32_t x = lx; x < hx; x = mc_java_i32_add(x, 1)) {
        for (int32_t y = ly; y < hy; y = mc_java_i32_add(y, 1)) {
            for (int32_t z = lz; z < hz; z = mc_java_i32_add(z, 1)) {
                e->bytecode = kind == 0 ? 91 : 125;
                if (!cell(e))
                    goto leave;
                const mc_block *block;
                if (!selected_block(w, x, y, z, &block, kind == 0 ? 91 : 125, kind == 0 ? 101 : 135,
                                    kind == 0 ? 104 : 138, e))
                    goto leave;
                if (block == NULL)
                    continue;
                if (kind == 1) {
                    e->bytecode = 151;
                    if (block->fields.bA == NULL) {
                        null_registry(w, e, "Liquid scan Material.d receiver");
                        goto leave;
                    }
                    if (!mc_material_d(block->fields.bA))
                        continue;
                    *out = true;
                    done = true;
                    goto leave;
                }
                if (block->fields.bA != m)
                    continue;
                if (kind == 0) {
                    *out = true;
                    done = true;
                    goto leave;
                }
                e->bytecode = 167;
                int32_t metadata;
                if (!read_value(w, x, y, z, true, &metadata, e))
                    goto leave;
                volatile float top = (float)mc_java_i32_add(y, 1);
                volatile float surface = top - mc_material_fluid_height(metadata);
                if ((double)hy < (double)surface)
                    continue;
                found = true;
                e->bytecode = 201;
                if (!accelerate_block(w, block, x, y, z, sum, e))
                    goto leave;
            }
        }
    }
    if (kind == 2) {
        e->bytecode = 224;
        double length;
        if (!vector_result(e, mc_entity_vector_length(sum, &length, &e->vector)))
            goto leave;
        if (length > 0.0) {
            e->bytecode = 234;
            if (!replace_normal(classes, &sum, e))
                goto leave;
            e->bytecode = 246;
            if (entity == NULL) {
                fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                     "Material acceleration Entity receiver");
                goto leave;
            }
            if (entity->holds == 0 || entity->classes != classes) {
                fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                     "Material acceleration Entity ownership");
                goto leave;
            }
            mc_entity_base_fields *f = mc_entity_runtime_fields(entity);
            volatile double dx = sum->x * 0.014;
            f->velocity_x = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(f->velocity_x), _mm_set_sd(dx)));
            e->bytecode = 263;
            volatile double dy = sum->y * 0.014;
            f->velocity_y = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(f->velocity_y), _mm_set_sd(dy)));
            e->bytecode = 280;
            volatile double dz = sum->z * 0.014;
            f->velocity_z = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(f->velocity_z), _mm_set_sd(dz)));
        }
    }
    *out = found;
    done = true;
leave:
    if (sum != NULL)
        mc_entity_vector_release(classes, sum);
    return done;
}

static mc_world_storage_status run(mc_world_runtime *w, const mc_entity_bounds *b, mc_material *m,
                                   mc_entity *entity, int kind,
                                   const mc_world_material_movement_limits *limits, bool *out,
                                   mc_world_material_movement_error *error) {
    mc_world_material_movement_error e;
    mc_world_material_movement_error_init(&e);
    e.max_cell_work =
        limits == NULL ? MC_WORLD_MATERIAL_MOVEMENT_DEFAULT_CELL_WORK : limits->max_cell_work;
    bool entered = false;
    if (w == NULL || out == NULL)
        fail(&e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Material movement arguments");
    else if (w->construction.mode != MC_WORLD_RUNTIME_GAME ||
             w->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        fail(&e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Material movement World phase");
    else if (mc_world_runtime_call_enter(w, &e.cause) != MC_WORLD_STORAGE_OK)
        cause(&e, e.cause);
    else {
        entered = true;
        (void)execute(w, b, m, entity, kind, out, &e);
    }
    if (entered) {
        if (e.status != MC_WORLD_STORAGE_OK)
            w->last_error = e.cause;
        mc_world_runtime_call_leave(w);
    }
    if (e.status == MC_WORLD_STORAGE_OK) {
        size_t work = e.cell_work, maximum = e.max_cell_work;
        mc_world_material_movement_error_init(&e);
        e.cell_work = work;
        e.max_cell_work = maximum;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_world_storage_status
mc_world_runtime_contains_material(mc_world_runtime *w, const mc_entity_bounds *b, mc_material *m,
                                   const mc_world_material_movement_limits *l, bool *out,
                                   mc_world_material_movement_error *e) {
    return run(w, b, m, NULL, 0, l, out, e);
}

mc_world_storage_status mc_world_runtime_contains_liquid(mc_world_runtime *w,
                                                         const mc_entity_bounds *b,
                                                         const mc_world_material_movement_limits *l,
                                                         bool *out,
                                                         mc_world_material_movement_error *e) {
    return run(w, b, NULL, NULL, 1, l, out, e);
}

mc_world_storage_status
mc_world_runtime_material_accelerate(mc_world_runtime *w, const mc_entity_bounds *b, mc_material *m,
                                     mc_entity *entity, const mc_world_material_movement_limits *l,
                                     bool *out, mc_world_material_movement_error *e) {
    return run(w, b, m, entity, 2, l, out, e);
}
