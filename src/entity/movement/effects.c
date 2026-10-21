#include "mc/entity/movement/effects.h"

#include "../entity_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"

#include <float.h>
#include <limits.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Entity movement effects require the captured x86-64/SSE2 target"
#endif
#if !defined(_MSC_VER) && !defined(__SSE2__)
#error "Entity movement effects require SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Entity movement effects require strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(CHAR_BIT == 8 && sizeof(float) == sizeof(uint32_t) &&
                   sizeof(double) == sizeof(uint64_t) && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 &&
                   FLT_EVAL_METHOD == 0,
               "strict binary32/binary64 storage and evaluation required");

mc_entity_effect_error mc_entity_effect_empty_error(void) {
    mc_entity_effect_error error = {0};
    error.watcher_index = -1;
    error.world = mc_world_movement_empty_error();
    return error;
}

static mc_entity_status fail(mc_entity_effect_error *error, mc_entity_status status,
                             mc_entity_effect_cause cause, const char *operation, uint16_t pc) {
    error->status = status;
    error->operation = operation;
    error->cause = cause;
    error->source_bytecode = pc;
    return status;
}

static bool enter(mc_entity *entity, mc_entity_effect_error *error) {
    if (entity == NULL) {
        fail(error, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Entity effect receiver", 0);
        return false;
    }
    if (entity->phase != MC_ENTITY_READY) {
        fail(error, MC_ENTITY_INVALID_PHASE, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Entity effect READY admission", 0);
        return false;
    }
    if (entity->classes == NULL || entity->holds == 0) {
        fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER, "Entity effect held owner",
             0);
        return false;
    }
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND)) {
        fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL,
             "Entity effect unsupported canonical virtual", 0);
        return false;
    }
    const mc_entity_status status = mc_entity_retain(entity);
    if (status != MC_ENTITY_OK) {
        error->entity = (mc_entity_error){.status = status, .operation = "Entity effect hold"};
        fail(error, status, MC_ENTITY_EFFECT_CAUSE_OWNER, "Entity effect hold", 0);
        return false;
    }
    return true;
}

static mc_entity_status finish(mc_entity_effect_error *frame, mc_entity_effect_error *error) {
    if (error != NULL)
        *error = *frame;
    return frame->status;
}

static bool fp(mc_entity_effect_error *error, const char *operation, uint16_t pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    fail(error, MC_ENTITY_UNSUPPORTED_FP, MC_ENTITY_EFFECT_CAUSE_FP, operation, pc);
    return false;
}

static bool watcher_byte(mc_entity *entity, int32_t index, int8_t *out,
                         mc_entity_effect_error *error) {
    error->watcher_index = index;
    const mc_entity_watcher *watcher = entity->watcher;
    if (watcher == NULL) {
        fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
             "Entity flag null watcher", 5);
        error->flag_bytecode = 5;
        return false;
    }
    const mc_entity_watch_map *map = watcher->map;
    if (map == NULL) {
        fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
             "Watcher null Map receiver", 8);
        error->watcher_bytecode = 8;
        return false;
    }
    const mc_entity_watch_node *node = map->head;
    size_t visited = 0;
    while (node != NULL) {
        /* Native typed Map invariant guard at the reached Map.get. This bounds
         * malformed private projections without inventing missing entries. */
        if (visited == map->size) {
            fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                 "Watcher Map node ownership invariant", 8);
            error->watcher_bytecode = 8;
            return false;
        }
        ++visited;
        if (node->key == index)
            break;
        node = node->next;
    }
    /* checkcast13 preserves null; only record.b()16 dereferences it. */
    const mc_entity_watch_record *record = node == NULL ? NULL : node->record;
    if (record == NULL) {
        fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
             "Watcher null record value receiver", 16);
        error->watcher_bytecode = 16;
        return false;
    }
    if (record->type != 0) {
        fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_WATCHER_VALUE,
             "Watcher unsupported typed Byte value projection", 19);
        error->watcher_bytecode = 19;
        return false;
    }
    /* Actual inline typed Byte value; no generic Object or nullable Byte box is
     * represented by this native owner. The successful unbox returns at25. */
    *out = record->byte_value;
    error->watcher_index = -1;
    return true;
}

static bool flag(mc_entity *entity, int32_t bit, bool *out, mc_entity_effect_error *error) {
    int8_t value;
    if (!watcher_byte(entity, 0, &value, error)) {
        error->flag_bytecode = 5;
        error->source_bytecode = 5;
        return false;
    }
    const int32_t promoted = (int32_t)value;
    const int32_t mask = mc_java_i32_shl(1, bit);
    *out = (promoted & mask) != 0;
    return true;
}

mc_entity_status mc_entity_effect_watcher_byte(mc_entity *entity, int32_t index, int8_t *out,
                                               mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    if (out == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Watcher Byte output", 0);
    else if (enter(entity, &frame)) {
        int8_t value;
        if (watcher_byte(entity, index, &value, &frame))
            *out = value;
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_flag(mc_entity *entity, int32_t bit, bool *out,
                                       mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    if (out == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_EFFECT_CAUSE_OWNER, "Entity flag output",
             0);
    else if (enter(entity, &frame)) {
        bool value;
        if (flag(entity, bit, &value, &frame))
            *out = value;
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_sneaking(mc_entity *entity, bool *out,
                                           mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    if (out == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Entity sneaking output", 0);
    else if (enter(entity, &frame)) {
        bool value;
        if (flag(entity, 1, &value, &frame))
            *out = value;
        else
            frame.source_bytecode = 2; /* Actual t/ah -> d(1). */
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_wet(mc_entity *entity, bool *out, mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    if (out == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_EFFECT_CAUSE_OWNER, "Entity wet output",
             0);
    else if (enter(entity, &frame)) {
        if (entity->base.bx)
            *out = true;
        else {
            /* Actual receiver load8 precedes argument evaluation15/22/29.
             * Floors use CURRENT fields; even a null World reaches all args. */
            mc_world_runtime *world = entity->base.world;
            if (fp(&frame, "Entity wet coordinate floor", 15)) {
                const int32_t x = mc_math_floor_double(entity->base.x);
                const int32_t y = mc_math_floor_double(entity->base.y);
                const int32_t z = mc_math_floor_double(entity->base.z);
                bool value;
                const mc_world_storage_status status =
                    mc_world_runtime_rain_at(world, x, y, z, &value, &frame.world);
                if (status == MC_WORLD_STORAGE_OK)
                    *out = value;
                else {
                    frame.entity = (mc_entity_error){.status = MC_ENTITY_WORLD_FAILED,
                                                     .operation = "Entity wet World rain-at call",
                                                     .bytecode = 32,
                                                     .world = status};
                    fail(&frame, MC_ENTITY_WORLD_FAILED,
                         world == NULL ? MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE
                                       : MC_ENTITY_EFFECT_CAUSE_WORLD,
                         "Entity wet World rain-at call", 32);
                }
            }
        }
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

static bool fall(mc_entity *entity, float distance, mc_entity_effect_error *error) {
    (void)distance; /* The actual base callback only forwards it to a passenger. */
    if (entity->base.aG != NULL) {
        fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_RELATIONSHIP,
             "Entity fall unsupported nonnull passenger recursive virtual", 12);
        error->fall_callback_bytecode = 12;
        return false;
    }
    return true; /* Genuine inherited b(F)/a(F) return15. */
}

mc_entity_status mc_entity_effect_fall(mc_entity *entity, float distance,
                                       mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    if (enter(entity, &frame)) {
        (void)fall(entity, distance, &frame);
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_fall_state(mc_entity *entity, double displacement, bool grounded,
                                             mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    if (enter(entity, &frame)) {
        if (grounded) {
            if (fp(&frame, "Entity grounded fall-distance comparison", 9)) {
                if (entity->base.bk > 0.0f) {
                    if (fall(entity, entity->base.bk, &frame))
                        entity->base.bk = 0.0f; /* Actual store23, only after callback. */
                    else {
                        frame.source_bytecode = 18;
                        frame.fall_bytecode = 18;
                    }
                }
            } else
                frame.fall_bytecode = 9;
        } else if (fp(&frame, "Entity descending fall-distance comparison", 31)) {
            if (displacement < 0.0) {
                /* Exact f2d40 / dsub42 / d2f43 / store44 sequence. */
                const __m128d old = _mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(entity->base.bk));
                const __m128d difference = _mm_sub_sd(old, _mm_set_sd(displacement));
                entity->base.bk = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), difference));
            }
        } else
            frame.fall_bytecode = 31;
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

static void attacked(mc_entity *entity) {
    entity->base.bb = true; /* sn.ai/lq.af store2; inherited by Item/Falling. */
}

static void dead(mc_entity *entity) {
    entity->base.be = true; /* sn.K/lq.J store2; no collection removal. */
}

mc_entity_status mc_entity_effect_attacked(mc_entity *entity, mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    if (enter(entity, &frame)) {
        attacked(entity);
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_dead(mc_entity *entity, mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    if (enter(entity, &frame)) {
        dead(entity);
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

static void damage(mc_entity *entity, int32_t amount) {
    attacked(entity); /* Both actual damage bodies dispatch inherited ai/af at1. */
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM)) {
        entity->item.f = mc_java_i32_sub(entity->item.f, amount); /* isub10/store11. */
        if (entity->item.f <= 0)
            dead(entity); /* Actual inherited K/J call22. */
    }
}

mc_entity_status mc_entity_effect_damage(mc_entity *entity, mc_entity *source, int32_t amount,
                                         bool *out, mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    (void)source; /* Genuinely unused by both complete current damage bodies. */
    if (out == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Entity damage output", 0);
    else if (enter(entity, &frame)) {
        damage(entity, amount);
        *out = false; /* Item return26; inherited Falling return5. */
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_fire_damage(mc_entity *entity, int32_t amount,
                                              mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    if (enter(entity, &frame)) {
        /* Item hl/ez invokes damage at3 without reading immunity. Falling
         * inherits sn/lq immunity1 and, when false, damage call10. */
        if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) || !entity->base.bC)
            damage(entity, amount);
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_walks(mc_entity *entity, bool *out,
                                        mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    if (out == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Entity walking-effect output", 0);
    else if (enter(entity, &frame)) {
        *out = false; /* Actual hl/ez and ju/gl n(): iconst_0; ireturn1. */
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_contact_portal(mc_entity *entity, mc_entity_effect_error *error) {
    mc_entity_effect_error frame = mc_entity_effect_empty_error();
    if (enter(entity, &frame)) {
        /* Both canonical concrete classes inherit sn.S/lq.P: literal return0.
         * The actual Block portal body owns its earlier relationship guards. */
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}
