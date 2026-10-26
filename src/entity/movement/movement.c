#include "mc/entity/movement/movement.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../world/runtime/collision/blocks_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../entity_internal.h"
#include "mc/block/registry/step_sound.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"

#include <float.h>
#include <limits.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64)
#error "Entity movement requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Entity movement requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 &&
                   DBL_MAX_EXP == 1024 && FLT_EVAL_METHOD == 0 &&
                   sizeof(float) == sizeof(uint32_t) && sizeof(double) == sizeof(uint64_t),
               "Entity movement requires binary32/binary64 storage and evaluation");

static double add(double a, double b) {
    uint64_t left, right;
    memcpy(&left, &a, sizeof(left));
    memcpy(&right, &b, sizeof(right));
    if ((left & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        (right & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000)) {
        right |= UINT64_C(0x0008000000000000);
        memcpy(&b, &right, sizeof(b));
        return b;
    }
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static double subtract(double a, double b) {
    return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static double multiply(double a, double b) {
    return _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static double widen(float value) {
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(value)));
}

static float negate(float value) {
    uint32_t raw;
    memcpy(&raw, &value, sizeof(raw));
    raw ^= UINT32_C(0x80000000);
    memcpy(&value, &raw, sizeof(value));
    return value;
}

void mc_entity_move_error_init(mc_entity_move_error *error) {
    if (error == NULL)
        return;
    memset(error, 0, sizeof(*error));
    error->collision = mc_world_collision_empty_error();
    error->block_read = mc_world_collision_empty_error();
    error->world = mc_world_movement_empty_error();
    error->contact = mc_world_entity_contact_empty_error();
    mc_world_step_error_init(&error->step);
    error->effect = mc_entity_effect_empty_error();
    error->registry = error->collision.nearby.query.registry;
}

mc_entity_move_error mc_entity_move_empty_error(void) {
    mc_entity_move_error error;
    mc_entity_move_error_init(&error);
    return error;
}

static bool fail(mc_entity_move_error *error, mc_entity_status status, const char *operation) {
    error->status = status;
    error->operation = operation;
    return false;
}

static bool world_result(mc_entity_move_error *error, mc_world_storage_status status,
                         const char *operation) {
    return status == MC_WORLD_STORAGE_OK ||
           fail(error, status == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_WORLD_FAILED,
                operation);
}

static bool work(mc_entity_move_error *error, unsigned int pc) {
    error->bytecode = pc;
    if (error->loop_work == error->max_loop_work)
        return fail(error, MC_ENTITY_LIMIT, "Entity movement loop-work ceiling");
    ++error->loop_work;
    return true;
}

static bool effect_result(mc_entity_move_error *error, mc_entity_status status) {
    return status == MC_ENTITY_OK || fail(error, status, "Entity movement effect");
}

static bool transform_query(mc_entity *entity, double x, double y, double z, bool shift,
                            unsigned int transform_pc, unsigned int query_pc,
                            mc_world_collision_list_view *list, mc_entity_move_error *error) {
    /* Source receiver is evaluated before the bounds argument; reload it for
     * each call, rather than retaining one World value for the whole move. */
    mc_world_runtime *const receiver = entity->base.world;
    mc_entity_bounds *argument = NULL;
    error->bytecode = transform_pc;
    const mc_entity_status transformed =
        shift ? mc_entity_bounds_shift(entity->classes, entity->bounds, x, y, z, &argument,
                                       &error->bounds)
              : mc_entity_bounds_sweep(entity->classes, entity->bounds, x, y, z, &argument,
                                       &error->bounds);
    if (transformed != MC_ENTITY_OK)
        return fail(error, transformed, "Entity movement collision argument");
    error->bytecode = query_pc;
    mc_world_collision_list_view next = {0};
    const mc_world_storage_status status =
        mc_world_runtime_collision_boxes(receiver, entity, argument, &next, &error->collision);
    mc_entity_bounds_release(entity->classes, argument);
    if (!world_result(error, status, "Entity movement collision query"))
        return false;
    mc_world_collision_list_view old = *list;
    *list = next;
    mc_world_collision_list_view_release(&old);
    return true;
}

static bool clip(mc_entity *entity, mc_world_collision_list_view *list, unsigned int axis,
                 double *delta, unsigned int count_pc, unsigned int get_pc, unsigned int clip_pc,
                 mc_entity_move_error *error) {
    for (size_t i = 0;; ++i) {
        error->bytecode = count_pc;
        size_t count;
        if (!world_result(error, mc_world_collision_list_view_count(list, &count),
                          "Entity movement collision List size"))
            return false;
        if (count > INT_MAX)
            return fail(error, MC_ENTITY_LIMIT, "Entity movement Java List extent");
        if (i >= count)
            return true;
        if (!work(error, get_pc))
            return false;
        const mc_entity_bounds *fixed = NULL;
        if (!world_result(error, mc_world_collision_list_view_get(list, i, &fixed),
                          "Entity movement collision List get"))
            return false;
        error->bytecode = clip_pc;
        if (fixed == NULL)
            return fail(error, MC_ENTITY_STATE_ERROR, "Entity movement null collision bounds");
        *delta = axis == 0   ? mc_entity_bounds_clip_x(fixed, entity->bounds, *delta)
                 : axis == 1 ? mc_entity_bounds_clip_y(fixed, entity->bounds, *delta)
                             : mc_entity_bounds_clip_z(fixed, entity->bounds, *delta);
    }
}

static bool ledge(mc_entity *entity, unsigned int axis, double *delta, double *original,
                  mc_world_collision_list_view *list, mc_entity_move_error *error) {
    const unsigned int query_pc = axis == 0 ? 232u : 313u;
    while (*delta != 0.0) {
        if (!work(error, query_pc - 3))
            return false;
        if (!transform_query(entity, axis == 0 ? *delta : 0.0, -1.0, axis == 2 ? *delta : 0.0, true,
                             query_pc - 3, query_pc, list, error))
            return false;
        error->bytecode = axis == 0 ? 235u : 316u;
        size_t count;
        if (!world_result(error, mc_world_collision_list_view_count(list, &count),
                          "Entity ledge collision List size"))
            return false;
        if (count != 0)
            break;
        if (*delta < 0.05 && *delta >= -0.05)
            *delta = 0.0;
        else if (*delta > 0.0)
            *delta = subtract(*delta, 0.05);
        else
            *delta = add(*delta, 0.05);
        *original = *delta;
    }
    return true;
}

static void position_from_bounds(mc_entity *entity) {
    entity->base.x = _mm_cvtsd_f64(
        _mm_div_sd(_mm_set_sd(add(entity->bounds->min_x, entity->bounds->max_x)), _mm_set_sd(2.0)));
    entity->base.y = subtract(add(entity->bounds->min_y, widen(entity->base.offset)),
                              widen(entity->base.y_size));
    entity->base.z = _mm_cvtsd_f64(
        _mm_div_sd(_mm_set_sd(add(entity->bounds->min_z, entity->bounds->max_z)), _mm_set_sd(2.0)));
}

static bool contacts(mc_entity *entity, mc_entity_move_error *error) {
    const int32_t min_x = mc_math_floor_double(add(entity->bounds->min_x, 0.001));
    const int32_t min_y = mc_math_floor_double(add(entity->bounds->min_y, 0.001));
    const int32_t min_z = mc_math_floor_double(add(entity->bounds->min_z, 0.001));
    const int32_t max_x = mc_math_floor_double(subtract(entity->bounds->max_x, 0.001));
    const int32_t max_y = mc_math_floor_double(subtract(entity->bounds->max_y, 0.001));
    const int32_t max_z = mc_math_floor_double(subtract(entity->bounds->max_z, 0.001));
    error->bytecode = 1792;
    bool loaded;
    if (!world_result(error,
                      mc_world_runtime_area_loaded(entity->base.world, min_x, min_y, min_z, max_x,
                                                   max_y, max_z, &loaded, &error->world),
                      "Entity contact loaded area"))
        return false;
    if (!loaded)
        return true;
    for (int32_t x = min_x; x <= max_x; x = mc_java_i32_add(x, 1))
        for (int32_t y = min_y; y <= max_y; y = mc_java_i32_add(y, 1))
            for (int32_t z = min_z; z <= max_z; z = mc_java_i32_add(z, 1)) {
                if (!work(error, 1841))
                    return false;
                mc_world_runtime *const world = entity->base.world;
                mc_world_storage_error admission = {0};
                error->block_read = mc_world_collision_empty_error();
                mc_world_storage_status status = mc_world_runtime_call_enter(world, &admission);
                int32_t id = 0;
                if (status == MC_WORLD_STORAGE_OK) {
                    status = mc_world_collision_block_value_held(world, x, y, z, false, &id,
                                                                 &error->block_read);
                    mc_world_runtime_call_leave(world);
                } else {
                    error->block_read.nearby.query.status = status;
                    error->block_read.nearby.query.operation = admission.operation;
                    error->block_read.nearby.query.cause = admission;
                }
                if (!world_result(error, status, "Entity contact Block ID"))
                    return false;
                if (id <= 0)
                    continue;
                mc_registry_block_m_view array;
                error->bytecode = 1851;
                if (world->classes == NULL || world->classes->entities != entity->classes)
                    return fail(error, MC_ENTITY_STATE_ERROR, "Entity contact class namespace");
                mc_registry_status registry = mc_registry_block_m_get(
                    mc_world_runtime_registry(world), &array, &error->registry);
                if (registry != MC_REGISTRY_OK)
                    return fail(error, MC_ENTITY_WORLD_FAILED, "Entity contact Block array");
                const mc_block *block = NULL;
                error->bytecode = 1856;
                registry = mc_registry_block_m_at(&array, id, &block, &error->registry);
                if (registry != MC_REGISTRY_OK)
                    return fail(error, MC_ENTITY_WORLD_FAILED, "Entity contact Block array index");
                error->bytecode = 1868;
                if (!world_result(error,
                                  mc_world_runtime_entity_contact(entity->base.world, block, x, y,
                                                                  z, entity, &error->contact),
                                  "Entity Block contact"))
                    return false;
            }
    return true;
}

static bool walking_block(mc_entity *entity, int32_t x, int32_t y, int32_t z, int32_t *out,
                          unsigned int pc, mc_entity_move_error *error) {
    error->bytecode = pc;
    mc_world_runtime *world = entity->base.world;
    mc_world_storage_error admission = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &admission);
    if (status == MC_WORLD_STORAGE_OK) {
        status =
            mc_world_collision_block_value_held(world, x, y, z, false, out, &error->block_read);
        mc_world_runtime_call_leave(world);
    } else {
        error->block_read = mc_world_collision_empty_error();
        error->block_read.nearby.query.status = status;
        error->block_read.nearby.query.operation = admission.operation;
        error->block_read.nearby.query.cause = admission;
    }
    return world_result(error, status, "Entity walking actual World block read");
}

static bool walking_code(mc_entity *entity, mc_registry_classes *classes, unsigned int pc,
                         mc_entity_move_error *error) {
    error->bytecode = pc;
    return (classes != NULL && classes->entities == entity->classes) ||
           fail(error, MC_ENTITY_STATE_ERROR, "Entity walking actual code Registry namespace");
}

static bool walking_static(mc_entity *entity, mc_registry_classes *classes, const char *name,
                           unsigned int capture_pc, unsigned int field_pc, const mc_block **out,
                           mc_entity_move_error *error) {
    if (!walking_code(entity, classes, capture_pc, error))
        return false;
    if (mc_registry_block_static_get(classes, name, out, &error->registry) != MC_REGISTRY_OK)
        return fail(error, MC_ENTITY_WORLD_FAILED, "Entity walking actual Block getstatic");
    error->bytecode = field_pc;
    return *out != NULL ||
           fail(error, MC_ENTITY_STATE_ERROR, "Entity walking null static Block field receiver");
}

static bool walking_element(mc_entity *entity, mc_registry_classes *classes, int32_t id,
                            unsigned int capture_pc, unsigned int element_pc, const mc_block **out,
                            mc_entity_move_error *error) {
    if (!walking_code(entity, classes, capture_pc, error))
        return false;
    mc_registry_block_m_view array;
    if (mc_registry_block_m_get(classes, &array, &error->registry) != MC_REGISTRY_OK)
        return fail(error, MC_ENTITY_WORLD_FAILED, "Entity walking Block.m active use");
    error->bytecode = element_pc;
    if (mc_registry_block_m_at(&array, id, out, &error->registry) != MC_REGISTRY_OK)
        return fail(error, MC_ENTITY_WORLD_FAILED, "Entity walking Block.m element");
    return true;
}

static bool walking_sound(mc_entity *entity, mc_registry_classes *classes,
                          const mc_block_sound *sound, bool snow, mc_entity_move_error *error) {
    /* World receiver and Entity argument are captured before d/b/c dispatch.
     * The fresh d() String stays the SAME identity throughout WorldAccess
     * iteration; a recipient may explicitly retain that identity. */
    mc_world_runtime *receiver = entity->base.world;
    mc_step_sound_name name = {0};
    error->bytecode = snow ? 1597u : 1641u;
    if (mc_registry_step_sound_name(classes, sound, false, &name, &error->registry) !=
        MC_REGISTRY_OK)
        return fail(error, MC_ENTITY_WORLD_FAILED, "Entity walking actual StepSound.d");
    float volume = 0.0f, pitch = 0.0f;
    error->bytecode = snow ? 1602u : 1646u;
    bool okay = mc_registry_step_sound_volume(sound, &volume, &error->registry) == MC_REGISTRY_OK;
    if (okay) {
        volume = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(volume), _mm_set_ss(0.15f)));
        error->bytecode = snow ? 1610u : 1654u;
        okay = mc_registry_step_sound_pitch(sound, &pitch, &error->registry) == MC_REGISTRY_OK;
    }
    if (!okay)
        fail(error, MC_ENTITY_WORLD_FAILED, "Entity walking actual StepSound b/c");
    else {
        error->bytecode = snow ? 1613u : 1657u;
        okay = world_result(error,
                            mc_world_runtime_entity_sound(receiver, entity, name.string, volume,
                                                          pitch, &error->world),
                            "Entity walking sound");
    }
    /* The frame keeps the reached source failure, not retirement diagnostics. */
    (void)mc_registry_step_sound_name_release(&name, NULL);
    return okay;
}

static bool walking(mc_entity *entity, mc_registry_classes *classes, double dx, double dz,
                    bool sneaking, mc_entity_move_error *error) {
    bool walks;
    error->bytecode = 1378;
    if (!effect_result(error, mc_entity_effect_walks(entity, &walks, &error->effect)))
        return false;
    if (!walks || sneaking || entity->base.aH != NULL)
        return true;
    const double squared = add(multiply(dx, dx), multiply(dz, dz));
    const double root = _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(squared)));
    const float narrowed = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(root)));
    entity->base.bj = _mm_cvtss_f32(_mm_cvtsd_ss(
        _mm_setzero_ps(), _mm_set_sd(add(widen(entity->base.bj), multiply(widen(narrowed), 0.6)))));
    const int32_t x = mc_math_floor_double(entity->base.x);
    const int32_t y = mc_math_floor_double(
        subtract(subtract(entity->base.y, 0.20000000298023224), widen(entity->base.offset)));
    const int32_t z = mc_math_floor_double(entity->base.z);
    int32_t id, below;
    if (!walking_block(entity, x, y, z, &id, 1473, error) ||
        !walking_block(entity, x, mc_java_i32_sub(y, 1), z, &below, 1490, error))
        return false;
    const mc_block *fence;
    if (!walking_static(entity, classes, "ba", 1493, 1496, &fence, error))
        return false;
    if (below == fence->fields.bn &&
        !walking_block(entity, x, mc_java_i32_sub(y, 1), z, &id, 1514, error))
        return false;
    const float counter = _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), entity->base.private_b));
    if (!(entity->base.bj > counter) || id <= 0)
        return true;
    entity->base.private_b = mc_java_i32_add(entity->base.private_b, 1);
    const mc_block *block;
    if (!walking_element(entity, classes, id, 1547, 1552, &block, error))
        return false;
    error->bytecode = 1553;
    if (block == NULL)
        return fail(error, MC_ENTITY_STATE_ERROR, "Entity walking null Block.by receiver");
    const mc_block_sound *sound = block->fields.by;
    int32_t above;
    if (!walking_block(entity, x, mc_java_i32_add(y, 1), z, &above, 1570, error))
        return false;
    const mc_block *snow;
    if (!walking_static(entity, classes, "aT", 1573, 1576, &snow, error))
        return false;
    if (above == snow->fields.bn) {
        if (!walking_static(entity, classes, "aT", 1582, 1585, &snow, error))
            return false;
        sound = snow->fields.by;
        if (!walking_sound(entity, classes, sound, true, error))
            return false;
    } else {
        if (!walking_element(entity, classes, id, 1619, 1624, &block, error))
            return false;
        error->bytecode = 1625;
        if (block == NULL)
            return fail(error, MC_ENTITY_STATE_ERROR, "Entity walking null Block.bA receiver");
        mc_material *material = block->fields.bA;
        error->bytecode = 1628;
        if (material == NULL)
            return fail(error, MC_ENTITY_STATE_ERROR, "Entity walking null Material.d receiver");
        if (!mc_material_d(material) && !walking_sound(entity, classes, sound, false, error))
            return false;
    }
    if (!walking_element(entity, classes, id, 1660, 1665, &block, error))
        return false;
    error->bytecode = 1677;
    return world_result(error,
                        mc_world_runtime_entity_step(entity->base.world, block, x, y, z, entity,
                                                     NULL, &error->step),
                        "Entity walking Block.b step-on");
}

static bool finish_environment(mc_entity *entity, mc_registry_classes *code_registry,
                               const mc_world_collision_list_view *list,
                               mc_entity_move_error *error) {
    bool wet;
    error->bytecode = 1890;
    if (!effect_result(error, mc_entity_effect_wet(entity, &wet, &error->effect)))
        return false;
    mc_world_runtime *const receiver = entity->base.world;
    mc_entity_bounds *argument = NULL;
    error->bytecode = 1912;
    const mc_entity_status contracted = mc_entity_bounds_contract(
        entity->classes, entity->bounds, 0.001, 0.001, 0.001, &argument, &error->bounds);
    if (contracted != MC_ENTITY_OK)
        return fail(error, contracted, "Entity burning bounds argument");
    error->bytecode = 1915;
    bool burning;
    const mc_world_storage_status status =
        mc_world_runtime_burning(receiver, argument, &burning, &error->world);
    mc_entity_bounds_release(entity->classes, argument);
    if (!world_result(error, status, "Entity burning query"))
        return false;
    if (burning) {
        error->bytecode = 1923;
        if (!effect_result(error, mc_entity_effect_fire_damage_in_context(entity, code_registry, 1,
                                                                          &error->effect)))
            return false;
        if (!wet) {
            entity->base.bv = mc_java_i32_add(entity->base.bv, 1);
            if (entity->base.bv == 0)
                entity->base.bv = 300;
        }
    } else if (entity->base.bv <= 0)
        entity->base.bv = mc_java_i32_sub(0, entity->base.bu);
    if (wet && entity->base.bv > 0) {
        mc_world_runtime *const sound_receiver = entity->base.world;
        error->bytecode = 1991;
        /* The held collision World supplies this actual loaded code namespace.
         * Constructor admission alone proves only Random-profile equality. */
        if (list->owner == NULL || list->owner->classes == NULL ||
            list->owner->classes->entities != entity->classes)
            return fail(error, MC_ENTITY_STATE_ERROR, "Entity fizz literal class namespace");
        const mc_registry_string *name = NULL;
        if (mc_registry_runtime_literal_ascii(mc_world_runtime_registry(list->owner), "random.fizz",
                                              &name, &error->registry) != MC_REGISTRY_OK)
            return fail(error, MC_ENTITY_WORLD_FAILED, "Entity fizz code literal");
        error->bytecode = 2001;
        if (entity->random == NULL)
            return fail(error, MC_ENTITY_STATE_ERROR, "Entity fizz Random receiver");
        const float first = mc_java_random_next_float(entity->random);
        error->bytecode = 2008;
        const float second = mc_java_random_next_float(entity->random);
        volatile float pitch = first - second;
        pitch = pitch * 0.4f;
        pitch = 1.6f + pitch;
        error->bytecode = 2016;
        if (!world_result(error,
                          mc_world_runtime_entity_sound(sound_receiver, entity, name, 0.7f, pitch,
                                                        &error->world),
                          "Entity extinguishing sound"))
            return false;
        entity->base.bv = mc_java_i32_sub(0, entity->base.bu);
    }
    return true;
}

mc_entity_status mc_entity_move_in_context(mc_entity *entity, mc_registry_classes *code_registry,
                                           double x, double y, double z,
                                           const mc_entity_move_limits *limits,
                                           mc_entity_move_error *error) {
    mc_entity_move_error frame;
    mc_entity_move_error_init(&frame);
    frame.max_loop_work = limits == NULL ? MC_ENTITY_MOVE_DEFAULT_LOOP_WORK : limits->max_loop_work;
    if (entity == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, "Entity movement owner");
    else if (entity->phase != MC_ENTITY_READY)
        fail(&frame, MC_ENTITY_INVALID_PHASE, "Entity movement phase");
    else if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
             !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND) &&
             !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        fail(&frame, MC_ENTITY_STATE_ERROR, "Entity movement concrete virtuals");
    else if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        fail(&frame, MC_ENTITY_UNSUPPORTED_FP, "Entity movement FP admission");
    else {
        const mc_entity_status held = mc_entity_retain(entity);
        if (held != MC_ENTITY_OK)
            fail(&frame, held, "Entity movement owner hold");
        else {
            mc_world_collision_list_view list = {0};
            mc_entity_bounds *initial = NULL, *flat = NULL;
            if (entity->base.bq) {
                frame.bytecode = 15;
                if (entity->bounds == NULL) {
                    fail(&frame, MC_ENTITY_STATE_ERROR, "Entity movement null permanent bounds");
                    goto done;
                }
                mc_entity_bounds_translate(entity->bounds, x, y, z);
                position_from_bounds(entity);
                frame.bytecode = 88;
                goto done;
            }
            entity->base.y_size =
                _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(entity->base.y_size), _mm_set_ss(0.4f)));
            const double old_x = entity->base.x, old_z = entity->base.z;
            if (entity->base.bc) {
                entity->base.bc = false;
                x = multiply(x, 0.25);
                y = multiply(y, 0.05000000074505806);
                z = multiply(z, 0.25);
                entity->base.velocity_x = 0.0;
                entity->base.velocity_y = 0.0;
                entity->base.velocity_z = 0.0;
            }
            double original_x = x, original_y = y, original_z = z;
            frame.bytecode = 173;
            if (entity->bounds == NULL) {
                fail(&frame, MC_ENTITY_STATE_ERROR, "Entity movement null permanent bounds");
                goto done;
            }
            mc_entity_status copied =
                mc_entity_bounds_copy(entity->classes, entity->bounds, &initial, &frame.bounds);
            if (copied != MC_ENTITY_OK) {
                fail(&frame, copied, "Entity initial pooled bounds copy");
                goto done;
            }
            bool sneaking = false;
            if (entity->base.aX) {
                frame.bytecode = 186;
                if (!effect_result(&frame,
                                   mc_entity_effect_sneaking(entity, &sneaking, &frame.effect)))
                    goto done;
            }
            if (sneaking && (!ledge(entity, 0, &x, &original_x, &list, &frame) ||
                             !ledge(entity, 2, &z, &original_z, &list, &frame)))
                goto done;
            if (!transform_query(entity, x, y, z, false, 391, 394, &list, &frame) ||
                !clip(entity, &list, 1, &y, 406, 418, 431, &frame))
                goto done;
            mc_entity_bounds_translate(entity->bounds, 0.0, y, 0.0);
            if (!entity->base.bd && original_y != y)
                x = y = z = 0.0;
            const bool grounded = entity->base.aX || (original_y != y && original_y < 0.0);
            if (!clip(entity, &list, 0, &x, 508, 520, 533, &frame))
                goto done;
            mc_entity_bounds_translate(entity->bounds, x, 0.0, 0.0);
            if (!entity->base.bd && original_x != x)
                x = y = z = 0.0;
            if (!clip(entity, &list, 2, &z, 582, 594, 608, &frame))
                goto done;
            mc_entity_bounds_translate(entity->bounds, 0.0, 0.0, z);
            if (!entity->base.bd && original_z != z)
                x = y = z = 0.0;
            if (entity->base.bp > 0.0f && grounded && (sneaking || entity->base.y_size < 0.05f) &&
                (original_x != x || original_z != z)) {
                const double flat_x = x, flat_y = y, flat_z = z;
                x = original_x;
                y = widen(entity->base.bp);
                z = original_z;
                frame.bytecode = 724;
                copied =
                    mc_entity_bounds_copy(entity->classes, entity->bounds, &flat, &frame.bounds);
                if (copied != MC_ENTITY_OK) {
                    fail(&frame, copied, "Entity flat pooled bounds copy");
                    goto done;
                }
                mc_entity_bounds_assign(entity->bounds, initial);
                if (!transform_query(entity, x, y, z, false, 751, 754, &list, &frame) ||
                    !clip(entity, &list, 1, &y, 766, 778, 791, &frame))
                    goto done;
                mc_entity_bounds_translate(entity->bounds, 0.0, y, 0.0);
                if (!entity->base.bd && original_y != y)
                    x = y = z = 0.0;
                if (!clip(entity, &list, 0, &x, 840, 852, 865, &frame))
                    goto done;
                mc_entity_bounds_translate(entity->bounds, x, 0.0, 0.0);
                if (!entity->base.bd && original_x != x)
                    x = y = z = 0.0;
                if (!clip(entity, &list, 2, &z, 914, 926, 940, &frame))
                    goto done;
                mc_entity_bounds_translate(entity->bounds, 0.0, 0.0, z);
                if (!entity->base.bd && original_z != z)
                    x = y = z = 0.0;
                if (!entity->base.bd && original_y != y)
                    x = y = z = 0.0;
                else {
                    y = widen(negate(entity->base.bp));
                    if (!clip(entity, &list, 1, &y, 1023, 1035, 1048, &frame))
                        goto done;
                    mc_entity_bounds_translate(entity->bounds, 0.0, y, 0.0);
                }
                if (add(multiply(flat_x, flat_x), multiply(flat_z, flat_z)) >=
                    add(multiply(x, x), multiply(z, z))) {
                    x = flat_x;
                    y = flat_y;
                    z = flat_z;
                    mc_entity_bounds_assign(entity->bounds, flat);
                } else {
                    const double fraction =
                        subtract(entity->bounds->min_y,
                                 (double)mc_java_double_to_i32(entity->bounds->min_y));
                    if (fraction > 0.0)
                        entity->base.y_size =
                            (float)add(widen(entity->base.y_size), add(fraction, 0.01));
                }
            }
            position_from_bounds(entity);
            entity->base.aY = original_x != x || original_z != z;
            entity->base.aZ = original_y != y;
            entity->base.aX = original_y != y && original_y < 0.0;
            entity->base.ba = entity->base.aY || entity->base.aZ;
            frame.bytecode = 1319;
            if (!effect_result(&frame, mc_entity_effect_fall_state_in_context(entity, code_registry,
                                                                              y, entity->base.aX,
                                                                              &frame.effect)))
                goto done;
            if (original_x != x)
                entity->base.velocity_x = 0.0;
            if (original_y != y)
                entity->base.velocity_y = 0.0;
            if (original_z != z)
                entity->base.velocity_z = 0.0;
            /* These two source locals are evaluated before n(), even though
             * the current concrete n=false bodies skip all walking effects. */
            volatile double walked_x = subtract(entity->base.x, old_x);
            volatile double walked_z = subtract(entity->base.z, old_z);
            if (!walking(entity, code_registry, walked_x, walked_z, sneaking, &frame))
                goto done;
            if (!contacts(entity, &frame) ||
                !finish_environment(entity, code_registry, &list, &frame))
                goto done;
            frame.bytecode = 2028;
        done:
            mc_world_collision_list_view_release(&list);
            if (flat != NULL)
                mc_entity_bounds_release(entity->classes, flat);
            if (initial != NULL)
                mc_entity_bounds_release(entity->classes, initial);
            mc_entity_release(entity);
        }
    }
    if (error != NULL)
        *error = frame;
    return frame.status;
}

mc_entity_status mc_entity_move(mc_entity *entity, double x, double y, double z,
                                const mc_entity_move_limits *limits, mc_entity_move_error *error) {
    return mc_entity_move_in_context(entity, NULL, x, y, z, limits, error);
}
