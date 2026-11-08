#include "mc/entity/movement/effects.h"
#include "../watcher_payload_internal.h"

#include "../mob/damage_internal.h"
#include "../mob/death_internal.h"
#include "../player/damage_alert_internal.h"
#include "../player/damage_internal.h"
#include "../player/damage_wake_internal.h"
#include "../player/death_internal.h"
#include "effects_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../client/input/movement_input_internal.h"
#include "../../world/runtime/collision/blocks_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../entity_internal.h"
#include "../mob/internal.h"
#include "../player/player_internal.h"
#include "../player/statistics_status_internal.h"
#include "mc/block/registry/step_sound.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

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

void mc_entity_effect_error_init(mc_entity_effect_error *error) {
    static const mc_entity_effect_error zero = {0};
    if (error == NULL)
        return;
    *error = zero;
    error->watcher_index = -1;
    error->statistic_error.writer.index = SIZE_MAX;
    error->statistic_error.writer.text_offset = SIZE_MAX;
    error->math.source_pc = -1;
    mc_inventory_armor_error_init(&error->armor);
    mc_world_movement_error_init(&error->world);
    error->registry = error->world.registry;
}

mc_entity_effect_error mc_entity_effect_empty_error(void) {
    mc_entity_effect_error error;
    mc_entity_effect_error_init(&error);
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
        !mc_entity_has_moving_particle(entity) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
        !(mc_entity_has_mob(entity) && mc_entity_is_kind(entity, entity->class_literal->kind))) {
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

static bool watcher_byte_failure(mc_entity_watcher_byte_error *error, mc_entity_effect_cause cause,
                                 const char *operation, uint16_t pc,
                                 mc_entity_watcher_byte_stage stage) {
    error->status = MC_ENTITY_STATE_ERROR;
    error->cause = cause;
    error->operation = operation;
    error->pc = pc;
    error->stage = stage;
    return false;
}

bool mc_entity_watcher_byte_read_entered(mc_entity *entity, int32_t index, int8_t *out,
                                         mc_entity_watcher_byte_error *error) {
    *error = (mc_entity_watcher_byte_error){.index = index};
    const mc_entity_watcher *watcher = entity->watcher;
    if (watcher == NULL)
        return watcher_byte_failure(error, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                                    "Entity flag null watcher", 5,
                                    MC_ENTITY_WATCHER_BYTE_STAGE_FLAG);
    const mc_entity_watch_map *map = watcher->map;
    if (map == NULL)
        return watcher_byte_failure(error, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                                    "Watcher null Map receiver", 8,
                                    MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER);
    const mc_entity_watch_node *node = map->head;
    size_t visited = 0;
    while (node != NULL) {
        /* Same native guard at the reached Map.get; no extra source read. */
        if (visited == map->size)
            return watcher_byte_failure(error, MC_ENTITY_EFFECT_CAUSE_OWNER,
                                        "Watcher Map node ownership invariant", 8,
                                        MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER);
        ++visited;
        if (node->key == index)
            break;
        node = node->next;
    }
    const mc_entity_watch_record *record = node == NULL ? NULL : node->record;
    if (record == NULL)
        return watcher_byte_failure(error, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                                    "Watcher null record value receiver", 16,
                                    MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER);
    const mc_entity_watch_payload_result payload =
        mc_entity_watch_payload_check(record, MC_ENTITY_WATCH_PAYLOAD_BYTE);
    if (payload != MC_ENTITY_WATCH_PAYLOAD_OK) {
        const uint16_t pc = payload == MC_ENTITY_WATCH_PAYLOAD_NULL_UNBOX ? 22 : 19;
        return watcher_byte_failure(
            error,
            payload == MC_ENTITY_WATCH_PAYLOAD_NULL_UNBOX ? MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE
            : payload == MC_ENTITY_WATCH_PAYLOAD_WRONG_CLASS
                ? MC_ENTITY_EFFECT_CAUSE_CLASS_CAST
                : MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_WATCHER_VALUE,
            payload == MC_ENTITY_WATCH_PAYLOAD_NULL_UNBOX ? "Watcher null boxed Byte unbox receiver"
            : payload == MC_ENTITY_WATCH_PAYLOAD_WRONG_CLASS
                ? "Watcher actual boxed Byte cast"
                : "Watcher unsupported typed Byte value projection",
            pc, MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER);
    }
    *out = record->byte_value;
    error->index = -1;
    return true;
}

void mc_entity_effect_watcher_byte_apply(const mc_entity_watcher_byte_error *leaf,
                                         mc_entity_effect_error *error) {
    error->watcher_index = leaf->index;
    if (leaf->status == MC_ENTITY_OK)
        return;
    (void)fail(error, leaf->status, leaf->cause, leaf->operation, leaf->pc);
    if (leaf->stage == MC_ENTITY_WATCHER_BYTE_STAGE_FLAG)
        error->flag_bytecode = leaf->pc;
    else if (leaf->stage == MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER)
        error->watcher_bytecode = leaf->pc;
}

bool mc_entity_effect_watcher_byte_entered(mc_entity *entity, int32_t index, int8_t *out,
                                           mc_entity_effect_error *error) {
    mc_entity_watcher_byte_error leaf;
    const bool result = mc_entity_watcher_byte_read_entered(entity, index, out, &leaf);
    mc_entity_effect_watcher_byte_apply(&leaf, error);
    return result;
}

static bool flag(mc_entity *entity, int32_t bit, bool *out, mc_entity_effect_error *error) {
    int8_t value;
    if (!mc_entity_effect_watcher_byte_entered(entity, 0, &value, error)) {
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
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (out == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Watcher Byte output", 0);
    else if (enter(entity, &frame)) {
        int8_t value;
        if (mc_entity_effect_watcher_byte_entered(entity, index, &value, &frame))
            *out = value;
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_flag(mc_entity *entity, int32_t bit, bool *out,
                                       mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
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
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (out == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Entity sneaking output", 0);
    else if (enter(entity, &frame)) {
        if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
            const struct mc_movement_input *input = entity->local_player->fields.input;
            if (input == NULL) {
                frame.source_class = "dc";
                fail(&frame, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                     "LocalPlayer sneaking null MovementInput", 4);
            } else
                *out = input->fields.sneak && !entity->local_player->fields.player.u;
        } else {
            bool value;
            if (flag(entity, 1, &value, &frame))
                *out = value;
            else
                frame.source_bytecode = 2;
        }
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_wet(mc_entity *entity, bool *out, mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
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

/* OpenJDK17 Math.round(D)J: the integer significand branch avoids the
 * double-rounding error of floor(a+.5). The source callers reach it only
 * after the actual statistic getstatic has completed. */
static int64_t round_double(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    const int32_t shift = 1074 - (int32_t)((bits >> 52) & 0x7ffu);
    if (shift >= 0 && shift < 64) {
        int64_t significand =
            (int64_t)((bits & UINT64_C(0x000fffffffffffff)) | UINT64_C(0x0010000000000000));
        if ((bits >> 63) != 0)
            significand = -significand;
        return mc_java_i64_shr(mc_java_i64_add(mc_java_i64_shr(significand, shift), 1), 1);
    }
    return mc_java_double_to_i64(value);
}

static bool statistic_get(mc_entity *entity, mc_registry_classes *classes, size_t index,
                          uint16_t pc, mc_entity_effect_error *error) {
    error->player_bytecode = pc;
    if (classes == NULL || classes->entities != entity->classes) {
        fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Player statistic actual code Registry namespace", pc);
        return false;
    }
    if (mc_registry_classes_initialize_statistics(classes, &error->registry) != MC_REGISTRY_OK) {
        fail(error, MC_ENTITY_WORLD_FAILED, MC_ENTITY_EFFECT_CAUSE_WORLD,
             "Player statistic getstatic", pc);
        return false;
    }
    error->statistic = classes->statistics.named[index];
    return true;
}

static bool statistic_dispatch(mc_entity *entity, mc_entity_effect_error *error, int32_t amount,
                               uint16_t pc) {
    error->statistic_amount = amount;
    error->player_bytecode = pc;
    const mc_player_statistic_status status =
        mc_local_player_add_statistic(entity, error->statistic, amount, &error->statistic_error);
    error->statistic_bytecode = (uint16_t)error->statistic_error.site.pc;
    if (status == MC_PLAYER_STATISTIC_OK) {
        if (pc == 152) {
            error->damage_statistic_returned = true;
            error->damage_statistic_amount = amount;
        }
        return true;
    }
    fail(error, mc_player_statistic_entity_status(&error->statistic_error),
         status == MC_PLAYER_STATISTIC_NULL_REFERENCE ? MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE
                                                      : MC_ENTITY_EFFECT_CAUSE_STATISTIC,
         "LocalPlayer actual application statistic descendant", pc);
    return false;
}

static bool base_fall(mc_entity *entity, mc_entity_effect_error *error) {
    if (entity->base.aG != NULL) {
        fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_RELATIONSHIP,
             "Entity fall unsupported nonnull passenger recursive virtual", 12);
        error->fall_callback_bytecode = 12;
        return false;
    }
    return true; /* Genuine inherited b(F)/a(F) return15. */
}

static bool damage(mc_entity *, mc_registry_classes *, mc_math_classes *, mc_entity *, int32_t,
                   bool *, mc_entity_effect_error *);

static bool landing_failure(mc_entity_effect_error *e, mc_entity_status status,
                            mc_entity_effect_cause cause, const char *operation, uint16_t pc) {
    e->source_class = "ls";
    e->living_bytecode = pc;
    (void)fail(e, status, cause, operation, pc);
    return false;
}

static bool landing_registry(mc_entity_effect_error *e, mc_registry_status status,
                             const char *operation, uint16_t pc) {
    if (status == MC_REGISTRY_OK)
        return true;
    return landing_failure(e,
                           status == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
                           : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                           : status == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
                                                                     : MC_ENTITY_WORLD_FAILED,
                           status == MC_REGISTRY_NULL_REFERENCE
                               ? MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE
                               : MC_ENTITY_EFFECT_CAUSE_REGISTRY,
                           operation, pc);
}

static bool landing_world(mc_entity_effect_error *e, mc_world_storage_status status,
                          const char *operation, uint16_t pc) {
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    return landing_failure(e,
                           status == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT
                           : status == MC_WORLD_STORAGE_ALLOCATION_FAILED
                               ? MC_ENTITY_ALLOCATION_FAILED
                           : status == MC_WORLD_STORAGE_BUSY ? MC_ENTITY_BUSY
                                                             : MC_ENTITY_WORLD_FAILED,
                           MC_ENTITY_EFFECT_CAUSE_WORLD, operation, pc);
}

static bool landing(mc_entity *entity, mc_registry_classes *classes,
                    mc_entity_effect_error *error) {
    /* Actual World receiver27 precedes all three coordinate floors. Reached
     * damage/sound callbacks may have changed both it and the current pose. */
    mc_world_runtime *world = entity->base.world;
    error->living_bytecode = 34;
    if (!fp(error, "Living landing coordinate arithmetic", 34))
        return false;
    const int32_t x = mc_math_floor_double(entity->base.x);
    const double below =
        _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(entity->base.y), _mm_set_sd(0.20000000298023224)));
    const double offset =
        _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(entity->base.offset)));
    const double y_value = _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(below), _mm_set_sd(offset)));
    const int32_t y = mc_math_floor_double(y_value);
    const int32_t z = mc_math_floor_double(entity->base.z);
    if (world == NULL)
        return landing_failure(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                               "Living landing null captured World receiver", 61);
    mc_world_movement_error_init(&error->world);
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &error->world.cause);
    int32_t id = 0;
    if (status == MC_WORLD_STORAGE_OK) {
        status =
            mc_world_collision_block_value_held(world, x, y, z, false, &id, &error->world.block);
        mc_world_runtime_call_leave(world);
    }
    error->world.status = status;
    if (!landing_world(error, status, "Living landing actual World block read", 61))
        return false;
    if (id <= 0)
        return true;
    if (classes == NULL || classes->entities != entity->classes)
        return landing_failure(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                               "Living landing actual code Registry namespace", 69);
    mc_registry_block_m_view array;
    if (!landing_registry(error, mc_registry_block_m_get(classes, &array, &error->registry),
                          "Living landing Block.m active use", 69))
        return false;
    const mc_block *block = NULL;
    if (!landing_registry(error, mc_registry_block_m_at(&array, id, &block, &error->registry),
                          "Living landing actual Block.m element", 73))
        return false;
    if (block == NULL)
        return landing_failure(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                               "Living landing null Block.by receiver", 74);
    const mc_block_sound *sound = block->fields.by;
    /* The sound World80 is captured before d/b/c dispatch. Keep the actual
     * newly built d() String alive through every WorldAccess recipient. */
    world = entity->base.world;
    mc_step_sound_name name = {0};
    if (!landing_registry(
            error, mc_registry_step_sound_name(classes, sound, false, &name, &error->registry),
            "Living landing actual StepSound.d", 86))
        return false;
    float volume = 0.0f, pitch = 0.0f;
    bool okay =
        landing_registry(error, mc_registry_step_sound_volume(sound, &volume, &error->registry),
                         "Living landing actual StepSound.b", 91);
    if (okay) {
        okay = fp(error, "Living landing sound volume arithmetic", 96);
        if (okay) {
            volume = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(volume), _mm_set_ss(.5f)));
            okay = landing_registry(error,
                                    mc_registry_step_sound_pitch(sound, &pitch, &error->registry),
                                    "Living landing actual StepSound.c", 99);
        }
    }
    if (okay) {
        pitch = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(pitch), _mm_set_ss(.75f)));
        if (world == NULL)
            okay =
                landing_failure(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                                "Living landing null captured sound World receiver", 105);
        else
            okay = landing_world(error,
                                 mc_world_runtime_entity_sound(world, entity, name.string, volume,
                                                               pitch, &error->world),
                                 "Living landing actual World Entity sound", 105);
    }
    /* Cleanup does not replace the reached gameplay result or child failure. */
    (void)mc_registry_step_sound_name_release(&name, NULL);
    return okay;
}

static bool fall(mc_entity *entity, mc_registry_classes *classes, mc_math_classes *math,
                 float distance, mc_entity_effect_error *error) {
    const bool player = mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER);
    const bool mob = mc_entity_has_mob(entity);
    if (!player && !mob)
        return base_fall(entity, error);
    if (mob && mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_CHICKEN))
        return true; /* Actual WW.b(float) literal return0. */
    error->source_class = player ? "gs" : "ls";
    if (player) {
        error->player_bytecode = 2;
        if (!fp(error, "Player fall-distance comparison", 2))
            return false;
    }
    if (player && distance >= 2.0f) {
        if (!statistic_get(entity, classes, 8, 7, error))
            return false;
        const double widened = _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(distance)));
        const double scaled = _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(widened), _mm_set_sd(100.0)));
        if (!statistic_dispatch(entity, error, mc_java_i32_narrow(round_double(scaled)), 20))
            return false;
    }
    /* gs invokespecial25 -> ls invokespecial2 -> actual sn passenger virtual. */
    if (player)
        error->player_bytecode = 25;
    error->living_bytecode = 2;
    if (!base_fall(entity, error))
        return false;
    if (!fp(error, "Living fall-distance arithmetic", 5))
        return false;
    error->living_bytecode = 10;
    const float reduced = _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(distance), _mm_set_ss(3.0f)));
    const double widened = _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(reduced)));
    const int32_t amount = mc_java_double_to_i32(ceil(widened));
    if (amount > 0) {
        bool ordinary;
        error->living_bytecode = 22;
        if (!damage(entity, classes, math, NULL, amount, &ordinary, error))
            return false;
        if (!landing(entity, classes, error))
            return false;
    }
    error->living_bytecode = 108;
    return true;
}

mc_entity_status mc_entity_effect_fall_with_math(mc_entity *entity, mc_registry_classes *classes,
                                                 mc_math_classes *math, float distance,
                                                 mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (enter(entity, &frame)) {
        (void)fall(entity, classes, math, distance, &frame);
        mc_entity_release(entity);
    }
    if (frame.status == MC_ENTITY_OK && !frame.living_damage.entered)
        mc_entity_effect_error_init(&frame);
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_fall_in_context(mc_entity *entity, mc_registry_classes *classes,
                                                  float distance, mc_entity_effect_error *error) {
    return mc_entity_effect_fall_with_math(entity, classes, NULL, distance, error);
}

mc_entity_status mc_entity_effect_fall(mc_entity *entity, float distance,
                                       mc_entity_effect_error *error) {
    return mc_entity_effect_fall_in_context(entity, NULL, distance, error);
}

mc_entity_status mc_entity_effect_fall_state_with_math(mc_entity *entity,
                                                       mc_registry_classes *classes,
                                                       mc_math_classes *math, double displacement,
                                                       bool grounded,
                                                       mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (enter(entity, &frame)) {
        if (grounded) {
            if (fp(&frame, "Entity grounded fall-distance comparison", 9)) {
                if (entity->base.bk > 0.0f) {
                    if (fall(entity, classes, math, entity->base.bk, &frame))
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
    if (frame.status == MC_ENTITY_OK && !frame.living_damage.entered)
        mc_entity_effect_error_init(&frame);
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_fall_state_in_context(mc_entity *entity,
                                                        mc_registry_classes *classes,
                                                        double displacement, bool grounded,
                                                        mc_entity_effect_error *error) {
    return mc_entity_effect_fall_state_with_math(entity, classes, NULL, displacement, grounded,
                                                 error);
}

mc_entity_status mc_entity_effect_fall_state(mc_entity *entity, double displacement, bool grounded,
                                             mc_entity_effect_error *error) {
    return mc_entity_effect_fall_state_in_context(entity, NULL, displacement, grounded, error);
}

void mc_entity_effect_attacked_entered(mc_entity *entity) {
    entity->base.bb = true; /* sn.ai/lq.af store2; inherited by Item/Falling. */
}

static bool dead_in_context(mc_entity *entity, mc_registry_classes *registry, mc_math_classes *math,
                            mc_entity_effect_error *error) {
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        error->source_class = "gs";
        if (mc_entity_effect_has_pending(entity)) {
            (void)mc_player_damage_wake_inspect(entity, &error->player_wake);
            (void)mc_player_damage_alert_inspect(entity, &error->player_alert);
            (void)mc_player_death_inspect(entity, &error->player_death);
            fail(error, MC_ENTITY_BUSY, MC_ENTITY_EFFECT_CAUSE_OWNER,
                 "Player K pending native child cleanup", 0);
            return false;
        }
        const bool returned = mc_player_dead_entered(entity, registry, math, error);
        error->close_bytecode = (uint16_t)error->player_death.site.pc;
        return returned;
    }
    if (mc_entity_has_mob(entity) && mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SLIME)) {
        error->source_class = "uw";
        error->close_bytecode = 1;
        fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL,
             "Slime K actual splitting/publication body", 1);
        return false;
    }
    entity->base.be = true; /* sn.K/lq.J store2; no collection removal. */
    return true;
}

static bool dead(mc_entity *entity, mc_entity_effect_error *error) {
    return dead_in_context(entity, NULL, NULL, error);
}

mc_entity_status mc_entity_effect_attacked(mc_entity *entity, mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (enter(entity, &frame)) {
        mc_entity_effect_attacked_entered(entity);
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_dead_with_context(mc_entity *entity,
                                                    mc_registry_classes *registry,
                                                    mc_math_classes *math,
                                                    mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (enter(entity, &frame)) {
        (void)dead_in_context(entity, registry, math, &frame);
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_dead(mc_entity *entity, mc_entity_effect_error *error) {
    return mc_entity_effect_dead_with_context(entity, NULL, NULL, error);
}

static bool instance_of(mc_entity *source, mc_entity_classes *classes, mc_entity_class_kind kind) {
    const mc_entity_class_literal *target = NULL;
    bool answer = false;
    return source != NULL && mc_entity_class_literal_get(classes, kind, &target) == MC_ENTITY_OK &&
           mc_entity_class_literal_is_assignable_from(target, source->class_literal, &answer) ==
               MC_ENTITY_OK &&
           answer;
}

static bool damage(mc_entity *entity, mc_registry_classes *classes, mc_math_classes *math,
                   mc_entity *source, int32_t amount, bool *out, mc_entity_effect_error *error) {
    if (mc_entity_effect_has_pending(entity)) {
        (void)mc_mob_death_inspect(entity, &error->death);
        (void)mc_player_death_inspect(entity, &error->player_death);
        (void)mc_player_damage_wake_inspect(entity, &error->player_wake);
        (void)mc_player_damage_alert_inspect(entity, &error->player_alert);
        fail(error, MC_ENTITY_BUSY, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Entity damage pending native child cleanup", 0);
        return false;
    }
    if (source == NULL &&
        (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ZOMBIE) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SKELETON) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_CREEPER)) &&
        entity->classes->source == MC_ENTITY_CLIENT)
        return mc_mob_monster_null_damage_entered(entity, classes, amount, out, error);
    if (entity->classes->source == MC_ENTITY_CLIENT &&
        (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SHEEP) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_PIG) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_COW) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_CHICKEN)))
        return mc_mob_living_damage_entered(entity, classes, math, source, amount, out, error);
    if (mc_entity_has_mob(entity)) {
        const mc_entity_class_kind kind = entity->class_literal->kind;
        error->damage_arguments_stored = true;
        error->damage_source_identity = source == NULL ? 0 : (uintptr_t)source;
        error->damage_amount = amount;
        error->source_class =
            kind == MC_ENTITY_CLASS_KIND_WOLF    ? "gi"
            : kind == MC_ENTITY_CLASS_KIND_SHEEP ? "dl"
            : kind == MC_ENTITY_CLASS_KIND_SPIDER || kind == MC_ENTITY_CLASS_KIND_ZOMBIE ||
                    kind == MC_ENTITY_CLASS_KIND_SKELETON || kind == MC_ENTITY_CLASS_KIND_CREEPER
                ? "gz"
                : "ls";
        error->damage_bytecode =
            kind == MC_ENTITY_CLASS_KIND_WOLF                                     ? 2
            : kind == MC_ENTITY_CLASS_KIND_SHEEP || error->source_class[0] == 'g' ? 3
                                                                                  : 4;
        fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL,
             "Living damage actual concrete hurt body", error->damage_bytecode);
        return false;
    }
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        mc_local_player_snapshot *fields = &entity->local_player->fields;
        error->source_class = "gs";
        fields->living.av = 0;
        if (fields->living.Y <= 0) {
            *out = false;
            return true;
        }
        if (fields->player.u) {
            if (entity->base.world == NULL) {
                fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                     "Player damage sleeping World B receiver", 25);
                return false;
            }
            if (!entity->base.world->fields.B) {
                error->source_bytecode = error->player_bytecode = 35;
                if (!mc_player_damage_wake_entered(entity, classes, error))
                    return false;
            }
        }
        /* Source instanceof first occurs at38, after the health/sleep prefix.
         * Native owners remain exclusive through this call. No relationship
         * is adopted, and the old Item/Falling bodies never inspect source. */
        if (source != NULL &&
            (source->classes == NULL || source->holds == 0 || source->phase != MC_ENTITY_READY)) {
            fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                 "Player damage source live Class identity", 39);
            return false;
        }
        if (instance_of(source, entity->classes, MC_ENTITY_CLASS_KIND_MONSTER) ||
            instance_of(source, entity->classes, MC_ENTITY_CLASS_KIND_ARROW)) {
            mc_world_runtime *world = entity->base.world;
            if (world == NULL) {
                fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                     "Player damage difficulty World receiver", 56);
                return false;
            }
            if (world->fields.q == 0)
                amount = 0;
            if (entity->base.world->fields.q == 1)
                amount = mc_java_i32_add(amount / 3, 1);
            if (entity->base.world->fields.q == 3)
                amount = mc_java_i32_mul(amount, 3) / 2;
        }
        if (amount == 0) {
            *out = false;
            return true;
        }
        if (instance_of(source, entity->classes, MC_ENTITY_CLASS_KIND_ARROW)) {
            fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_RELATIONSHIP,
                 "Player damage actual Arrow.c Living attacker", 117);
            return false;
        }
        if (instance_of(source, entity->classes, MC_ENTITY_CLASS_KIND_LIVING_ENTITY) &&
            !mc_player_damage_alert_entered(entity, source, error))
            return false;
        /* Every Living attacker enters the actual helper, including Player's
         * literal F=false return63. Null/non-Living skips the source call. */
        if (!statistic_get(entity, classes, 18, 148, error))
            return false;
        if (!statistic_dispatch(entity, error, amount, 152))
            return false;
        return mc_player_living_damage_entered(entity, classes, math, source, amount, out, error);
    }
    mc_entity_effect_attacked_entered(
        entity); /* Both actual damage bodies dispatch inherited ai/af at1. */
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM)) {
        entity->item.f = mc_java_i32_sub(entity->item.f, amount); /* isub10/store11. */
        if (entity->item.f <= 0)
            (void)dead(entity, error); /* Actual inherited K/J call22. */
    }
    *out = false;
    return true;
}

mc_entity_status mc_entity_effect_damage_with_math(mc_entity *entity, mc_registry_classes *classes,
                                                   mc_math_classes *math, mc_entity *source,
                                                   int32_t amount, bool *out,
                                                   mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (out == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Entity damage output", 0);
    else if (enter(entity, &frame)) {
        (void)damage(entity, classes, math, source, amount, out, &frame);
        mc_entity_release(entity);
    }
    if (frame.status == MC_ENTITY_OK && !frame.living_damage.entered)
        mc_entity_effect_error_init(&frame);
    return finish(&frame, error);
}

bool mc_entity_effect_has_pending(const mc_entity *entity) {
    return (mc_entity_has_mob(entity) && entity->mob->pending_death != NULL) ||
           (entity != NULL && entity->local_player != NULL &&
            (entity->local_player->pending_death != NULL ||
             entity->local_player->pending_damage_wake != NULL ||
             entity->local_player->pending_damage_alert != NULL));
}

mc_entity_status mc_entity_effect_cleanup(mc_entity *entity, mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (enter(entity, &frame)) {
        if (mc_entity_has_mob(entity))
            (void)mc_mob_death_cleanup(entity, &frame);
        else if (entity->local_player != NULL &&
                 mc_player_damage_wake_cleanup(entity, &frame) == MC_ENTITY_OK &&
                 mc_player_damage_alert_cleanup(entity, &frame) == MC_ENTITY_OK)
            (void)mc_player_death_cleanup(entity, &frame);
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

static void clear_wake_borrows(mc_entity_player_wake_state *state) {
    state->pending = state->active = false;
    state->receiver = NULL;
    state->error = NULL;
    state->cleanup = NULL;
    state->cleanup_status = MC_ENTITY_OK;
    state->cleanup_operation = NULL;
}

static void clear_alert_borrows(mc_entity_player_alert_state *state) {
    state->pending = state->active = false;
    state->receiver = NULL;
    state->error = NULL;
    state->cleanup = NULL;
    state->cleanup_status = MC_ENTITY_OK;
    state->cleanup_operation = NULL;
}

static void refresh_player_borrows(mc_entity_effect_error *error,
                                   const mc_entity_effect_error *cleanup, const mc_entity *receiver,
                                   mc_entity_status status) {
    if (error->player_wake.pending && error->player_wake.receiver == receiver) {
        if (cleanup->player_wake.entered)
            error->player_wake = cleanup->player_wake;
        if (status == MC_ENTITY_OK)
            clear_wake_borrows(&error->player_wake);
        else if (error->player_wake.pending) {
            error->player_wake.cleanup_status = status;
            error->player_wake.cleanup_operation = cleanup->operation;
        }
    }
    if (error->player_alert.pending && error->player_alert.receiver == receiver) {
        if (cleanup->player_alert.entered)
            error->player_alert = cleanup->player_alert;
        if (status == MC_ENTITY_OK)
            clear_alert_borrows(&error->player_alert);
        else if (error->player_alert.pending) {
            error->player_alert.cleanup_status = status;
            error->player_alert.cleanup_operation = cleanup->operation;
        }
    }
}

bool mc_entity_effect_error_retire(mc_entity_effect_error *error) {
    if (error == NULL)
        return false;
    if (!error->player_wake.pending && !error->player_alert.pending && !error->death.pending &&
        !error->player_death.pending)
        return true;
    mc_entity_effect_error cleanup;
    if (error->player_wake.pending) {
        mc_entity_effect_error_init(&cleanup);
        mc_entity *receiver = error->player_wake.receiver;
        const mc_entity_status status = mc_entity_effect_cleanup(receiver, &cleanup);
        refresh_player_borrows(error, &cleanup, receiver, status);
        if (status != MC_ENTITY_OK)
            return false;
    }
    if (error->player_alert.pending) {
        mc_entity_effect_error_init(&cleanup);
        mc_entity *receiver = error->player_alert.receiver;
        const mc_entity_status status = mc_entity_effect_cleanup(receiver, &cleanup);
        refresh_player_borrows(error, &cleanup, receiver, status);
        if (status != MC_ENTITY_OK)
            return false;
    }
    if (!error->death.pending && !error->player_death.pending)
        return true;
    mc_entity_effect_error_init(&cleanup);
    mc_entity *receiver =
        error->player_death.pending ? error->player_death.receiver : error->death.receiver;
    const mc_entity_status status = mc_entity_effect_cleanup(receiver, &cleanup);
    refresh_player_borrows(error, &cleanup, receiver, status);
    if (status != MC_ENTITY_OK) {
        error->player_death.cleanup_status = status;
        error->player_death.cleanup_operation = cleanup.operation;
        error->death.cleanup_status = status;
        error->death.cleanup_operation = cleanup.operation;
        return false;
    }
    error->death.pending = error->death.active = false;
    error->death.receiver = NULL;
    error->death.drop_error = NULL;
    error->death.cleanup_status = MC_ENTITY_OK;
    error->death.cleanup_operation = NULL;
    error->player_death.pending = error->player_death.active = false;
    error->player_death.receiver = NULL;
    error->player_death.drop_error = NULL;
    error->player_death.close_error = NULL;
    error->player_death.cleanup_status = MC_ENTITY_OK;
    error->player_death.cleanup_operation = NULL;
    return true;
}

mc_entity_status mc_entity_effect_death_with_math(mc_entity *entity, mc_registry_classes *classes,
                                                  mc_math_classes *math, mc_entity *source,
                                                  mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (enter(entity, &frame)) {
        frame.source_class = "ls";
        if (mc_entity_effect_has_pending(entity)) {
            (void)mc_mob_death_inspect(entity, &frame.death);
            (void)mc_player_death_inspect(entity, &frame.player_death);
            (void)mc_player_damage_wake_inspect(entity, &frame.player_wake);
            (void)mc_player_damage_alert_inspect(entity, &frame.player_alert);
            fail(&frame, MC_ENTITY_BUSY, MC_ENTITY_EFFECT_CAUSE_OWNER,
                 "Entity death pending native child cleanup", 0);
        } else if (entity->local_player != NULL) {
            frame.source_class = "gs";
            (void)mc_player_death_entered(entity, classes, math, source, &frame);
            frame.source_bytecode = (uint16_t)frame.player_death.site.pc;
        } else {
            (void)mc_mob_death_entered(entity, classes, source, &frame);
            frame.source_bytecode = frame.death.death_pc;
        }
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_death_in_context(mc_entity *entity, mc_registry_classes *classes,
                                                   mc_entity *source,
                                                   mc_entity_effect_error *error) {
    return mc_entity_effect_death_with_math(entity, classes, NULL, source, error);
}

mc_entity_status mc_entity_effect_damage_in_context(mc_entity *entity, mc_registry_classes *classes,
                                                    mc_entity *source, int32_t amount, bool *out,
                                                    mc_entity_effect_error *error) {
    return mc_entity_effect_damage_with_math(entity, classes, NULL, source, amount, out, error);
}

mc_entity_status mc_entity_effect_damage(mc_entity *entity, mc_entity *source, int32_t amount,
                                         bool *out, mc_entity_effect_error *error) {
    return mc_entity_effect_damage_in_context(entity, NULL, source, amount, out, error);
}

mc_entity_status mc_entity_effect_fire_damage_with_math(mc_entity *entity,
                                                        mc_registry_classes *classes,
                                                        mc_math_classes *math, int32_t amount,
                                                        mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (enter(entity, &frame)) {
        /* Item hl/ez invokes damage at3 without reading immunity. Falling
         * inherits sn/lq immunity1 and, when false, damage call10. */
        if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) || !entity->base.bC) {
            bool ordinary;
            if (!damage(entity, classes, math, NULL, amount, &ordinary, &frame))
                frame.fire_bytecode = mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) ? 3 : 10;
        }
        mc_entity_release(entity);
    }
    if (frame.status == MC_ENTITY_OK && !frame.living_damage.entered)
        mc_entity_effect_error_init(&frame);
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_fire_damage_in_context(mc_entity *entity,
                                                         mc_registry_classes *classes,
                                                         int32_t amount,
                                                         mc_entity_effect_error *error) {
    return mc_entity_effect_fire_damage_with_math(entity, classes, NULL, amount, error);
}

mc_entity_status mc_entity_effect_fire_damage(mc_entity *entity, int32_t amount,
                                              mc_entity_effect_error *error) {
    return mc_entity_effect_fire_damage_in_context(entity, NULL, amount, error);
}

mc_entity_status mc_entity_effect_walks(mc_entity *entity, bool *out,
                                        mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (out == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Entity walking-effect output", 0);
    else if (enter(entity, &frame)) {
        *out =
            mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
            (mc_entity_has_mob(entity) && !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SPIDER) &&
             !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_WOLF));
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}

mc_entity_status mc_entity_effect_contact_portal(mc_entity *entity, mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (enter(entity, &frame)) {
        if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
            mc_player_fields *fields = &entity->local_player->fields.player;
            if (fields->z > 0)
                fields->z = 10;
            else
                fields->A = true;
        }
        mc_entity_release(entity);
    }
    return finish(&frame, error);
}
