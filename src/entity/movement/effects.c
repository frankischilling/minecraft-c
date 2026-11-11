#include "mc/entity/movement/effects.h"
#include "../arrow/references_internal.h"
#include "../watcher_payload_internal.h"

#include "../mob/damage_internal.h"
#include "../mob/death_execution_internal.h"
#include "../mob/death_internal.h"
#include "../mob/removal_internal.h"
#include "../mob/wolf_damage_execution_internal.h"
#include "../mob/wolf_damage_internal.h"
#include "../player/damage_alert_execution_internal.h"
#include "../player/damage_alert_internal.h"
#include "../player/damage_internal.h"
#include "../player/damage_wake_internal.h"
#include "../player/death_execution_internal.h"
#include "../player/death_internal.h"
#include "../player/drop_execution_internal.h"
#include "damage_execution_internal.h"
#include "effects_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../client/input/movement_input_internal.h"
#include "../../item/render/internal.h"
#include "../../world/runtime/collision/blocks_internal.h"
#include "../../world/runtime/mutation/drops/execution_internal.h"
#include "../../world/runtime/mutation/notifications_execution_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../../world/runtime/world_access_internal.h"
#include "../entity_internal.h"
#include "../mob/internal.h"
#include "../player/player_internal.h"
#include "../player/statistics_status_internal.h"
#include "mc/block/registry/step_sound.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/placement.h"

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

static bool enter_domain(mc_entity *entity, mc_entity_effect_error *error, bool inherited_arrow) {
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
        !(inherited_arrow && mc_entity_has_arrow(entity)) &&
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

static bool enter(mc_entity *entity, mc_entity_effect_error *error) {
    return enter_domain(entity, error, false);
}

static bool enter_arrow_base(mc_entity *entity, mc_entity_effect_error *error) {
    return enter_domain(entity, error, true);
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
    else if (enter_arrow_base(entity, &frame)) {
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
        if (mc_entity_effect_has_pending(entity)) {
            /* The rejected call cannot grant retirement of earlier work. */
            fail(error, MC_ENTITY_BUSY, MC_ENTITY_EFFECT_CAUSE_OWNER,
                 "Slime K pending native child cleanup", 0);
            return false;
        }
        const bool returned = mc_slime_removal_entered(entity, error);
        error->close_bytecode = (uint16_t)error->slime_removal.site.pc;
        return returned;
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
    if (enter_arrow_base(entity, &frame)) {
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
    if (enter_arrow_base(entity, &frame)) {
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

static bool damage_body(mc_entity *entity, mc_registry_classes *classes, mc_math_classes *math,
                        mc_entity *source, int32_t amount, bool *out, mc_entity_effect_error *error,
                        mc_entity_damage_execution *owned) {
    if (mc_entity_effect_has_pending(entity)) {
        (void)mc_mob_death_inspect(entity, &error->death);
        (void)mc_mob_wolf_damage_inspect(entity, &error->wolf_damage);
        (void)mc_player_death_inspect(entity, &error->player_death);
        (void)mc_player_damage_wake_inspect(entity, &error->player_wake);
        (void)mc_player_damage_alert_inspect(entity, &error->player_alert);
        fail(error, MC_ENTITY_BUSY, MC_ENTITY_EFFECT_CAUSE_OWNER,
             "Entity damage pending native child cleanup", 0);
        return false;
    }
    if ((mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SPIDER) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ZOMBIE) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SKELETON) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_CREEPER)) &&
        entity->classes->source == MC_ENTITY_CLIENT)
        return owned == NULL ? mc_mob_monster_damage_entered(entity, classes, math, source, amount,
                                                             out, error)
                             : mc_mob_monster_damage_entered_in_execution(entity, source, amount,
                                                                          out, error, owned);
    if (entity->classes->source == MC_ENTITY_CLIENT &&
        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_WOLF))
        return owned == NULL
                   ? mc_mob_wolf_damage_entered(entity, classes, math, source, amount, out, error)
                   : mc_mob_wolf_damage_entered_in_execution(entity, source, amount, out, error,
                                                             owned);
    if (entity->classes->source == MC_ENTITY_CLIENT &&
        (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SHEEP) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_PIG) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_COW) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_CHICKEN) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SLIME) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SQUID) ||
         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_GHAST)))
        return owned == NULL
                   ? mc_mob_living_damage_entered(entity, classes, math, source, amount, out, error)
                   : mc_mob_living_damage_entered_in_execution(entity, source, amount, out, error,
                                                               owned);
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
                if (owned != NULL) {
                    error->living_damage.source_exception = true;
                    error->living_damage.exception_class = "java/lang/NullPointerException";
                }
                fail(error, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                     "Player damage sleeping World B receiver", 25);
                return false;
            }
            if (!entity->base.world->fields.B) {
                error->source_bytecode = error->player_bytecode = 35;
                if (owned == NULL) {
                    if (!mc_player_damage_wake_entered(entity, classes, error))
                        return false;
                } else {
                    mc_entity_damage_execution_select(owned, MC_ENTITY_DAMAGE_CHILD_WAKE);
                    if (!mc_player_damage_wake_entered_in_execution(entity, classes, error,
                                                                    owned)) {
                        if (entity->local_player->pending_damage_wake != NULL) {
                            owned->work_kind = MC_ENTITY_DAMAGE_WORK_WAKE;
                            owned->acquired_work = entity->local_player->pending_damage_wake;
                        }
                        return false;
                    }
                }
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
                if (owned != NULL) {
                    error->living_damage.source_exception = true;
                    error->living_damage.exception_class = "java/lang/NullPointerException";
                }
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
        mc_entity *attacker = source; /* The source local3 is separate from argument1. */
        if (instance_of(attacker, entity->classes, MC_ENTITY_CLASS_KIND_ARROW)) {
            mc_entity *shooter = NULL;
            mc_entity_status read = mc_arrow_shooter_get(attacker, &shooter, &error->entity);
            if (read != MC_ENTITY_OK) {
                fail(error, read, MC_ENTITY_EFFECT_CAUSE_OWNER, "Player damage first Arrow.c read",
                     117);
                return false;
            }
            if (shooter != NULL) {
                /* The source performs another field read at127, rather than
                 * using the nonnull value tested by its first read117. */
                read = mc_arrow_shooter_get(attacker, &shooter, &error->entity);
                if (read != MC_ENTITY_OK) {
                    fail(error, read, MC_ENTITY_EFFECT_CAUSE_OWNER,
                         "Player damage second Arrow.c read", 127);
                    return false;
                }
                attacker = shooter;
            }
        }
        if (instance_of(attacker, entity->classes, MC_ENTITY_CLASS_KIND_LIVING_ENTITY)) {
            bool returned;
            if (owned == NULL)
                returned = mc_player_damage_alert_entered(entity, attacker, error);
            else {
                mc_entity_damage_execution_select(owned, MC_ENTITY_DAMAGE_CHILD_ALERT);
                const void *prior = entity->local_player->pending_damage_alert;
                returned = mc_player_damage_alert_entered_in_execution(
                    owned->execution, owned->code_classes, owned->code, entity, attacker,
                    &owned->child_status, error, &owned->alert_result);
                mc_entity_damage_execution_returned(owned);
                mc_entity_damage_execution_note(owned, &owned->alert_result);
                if (prior == NULL && entity->local_player->pending_damage_alert != NULL) {
                    owned->work_kind = MC_ENTITY_DAMAGE_WORK_ALERT;
                    owned->acquired_work = entity->local_player->pending_damage_alert;
                }
                if (!returned && owned->alert_result.ownership_status == MC_WORLD_STORAGE_OK &&
                    !error->player_alert.native_policy && !error->player_alert.source_exception &&
                    !error->player_alert.source_error)
                    owned->unresolved = true;
            }
            if (!returned)
                return false;
        }
        /* Every Living attacker enters the actual helper, including Player's
         * literal F=false return63. Null/non-Living skips the source call. */
        if (!statistic_get(entity, classes, 18, 148, error))
            return false;
        if (!statistic_dispatch(entity, error, amount, 152))
            return false;
        return owned == NULL ? mc_player_living_damage_entered(entity, classes, math, source,
                                                               amount, out, error)
                             : mc_player_living_damage_entered_in_execution(entity, source, amount,
                                                                            out, error, owned);
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

static bool damage(mc_entity *entity, mc_registry_classes *classes, mc_math_classes *math,
                   mc_entity *source, int32_t amount, bool *out, mc_entity_effect_error *error) {
    return damage_body(entity, classes, math, source, amount, out, error, NULL);
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
    else if (enter_arrow_base(entity, &frame)) {
        (void)damage(entity, classes, math, source, amount, out, &frame);
        mc_entity_release(entity);
    }
    if (frame.status == MC_ENTITY_OK && !frame.living_damage.entered)
        mc_entity_effect_error_init(&frame);
    return finish(&frame, error);
}

void mc_entity_damage_execution_init(mc_entity_damage_execution *frame,
                                     mc_entity_tick_execution *execution,
                                     mc_world_runtime_classes *code_classes,
                                     mc_world_runtime_execution_code code,
                                     mc_registry_classes *registry) {
    if (frame == NULL)
        return;
    *frame = (mc_entity_damage_execution){
        .execution = execution, .code_classes = code_classes, .code = code, .registry = registry};
    mc_world_runtime_execution_result_init(&frame->owned);
    mc_world_runtime_execution_result_init(&frame->sound_result);
    mc_world_runtime_execution_result_init(&frame->knockback_result);
    mc_world_runtime_execution_result_init(&frame->alert_result);
    mc_world_runtime_execution_result_init(&frame->death_result);
}

void mc_entity_damage_execution_select(mc_entity_damage_execution *frame,
                                       mc_entity_damage_child child) {
    frame->child = child;
    frame->child_returned = false;
}

void mc_entity_damage_execution_note(mc_entity_damage_execution *frame,
                                     const mc_world_runtime_execution_result *result) {
    if (result->ownership_status != MC_WORLD_STORAGE_OK)
        mc_world_runtime_execution_result_note(&frame->owned, result->ownership_status,
                                               result->ownership_error.operation,
                                               result->ownership_error.site);
    if (result->math_failed) {
        const mc_entity_tick_execution_site site = {
            result->math_failure.caller.class_name, result->math_failure.caller.method,
            result->math_failure.caller.descriptor, result->math_failure.caller.pc};
        if (result->math_owner != frame->execution->math) {
            mc_world_runtime_execution_result_note(
                &frame->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "Damage exact child Math proof owner mismatch", site);
            return;
        }
        const mc_world_storage_status noted = mc_world_runtime_execution_result_note_math(
            &frame->owned, result->math_owner, result->math_status, &result->math_failure);
        if (noted != MC_WORLD_STORAGE_OK) {
            mc_world_runtime_execution_result_note(
                &frame->owned, noted, "Damage exact child Math receipt forwarding", site);
            return;
        }
        frame->math_failed = true;
        frame->math_owner = result->math_owner;
        frame->math_status = result->math_status;
        frame->math_failure = result->math_failure;
    }
}

void mc_entity_damage_execution_returned(mc_entity_damage_execution *frame) {
    frame->child_returned = true;
}

static mc_world_storage_status damage_native_note(mc_entity_damage_execution *frame,
                                                  mc_world_storage_status status,
                                                  const char *operation) {
    mc_world_runtime_execution_result_note(
        &frame->owned, status, operation,
        (mc_entity_tick_execution_site){"native.damage", "owned", "(Lsn;I)Z", -1});
    return status;
}

static bool damage_work_matches(const mc_entity_damage_execution *frame) {
    const mc_entity *p = frame->victim;
    switch (frame->work_kind) {
        case MC_ENTITY_DAMAGE_WORK_NONE:
            return frame->acquired_work == NULL;
        case MC_ENTITY_DAMAGE_WORK_WOLF:
            return mc_entity_has_mob(p) && p->mob->pending_wolf_damage == frame->acquired_work;
        case MC_ENTITY_DAMAGE_WORK_MOB_DEATH:
            return mc_entity_has_mob(p) && p->mob->pending_death == frame->acquired_work;
        case MC_ENTITY_DAMAGE_WORK_WAKE:
            return p->local_player != NULL &&
                   p->local_player->pending_damage_wake == frame->acquired_work;
        case MC_ENTITY_DAMAGE_WORK_ALERT:
            return p->local_player != NULL &&
                   p->local_player->pending_damage_alert == frame->acquired_work;
        case MC_ENTITY_DAMAGE_WORK_PLAYER_DEATH:
            return p->local_player != NULL &&
                   p->local_player->pending_death == frame->acquired_work;
    }
    return false;
}

bool mc_entity_effect_damage_in_execution(mc_entity_damage_execution *frame, mc_entity *victim,
                                          mc_entity *source, int32_t amount, bool *out,
                                          mc_entity_effect_error *error, mc_entity_status *status) {
    if (frame == NULL)
        return false;
    if (frame->execution == NULL || frame->execution->identity != frame->execution ||
        frame->execution->math == NULL || frame->code_classes == NULL || error == NULL ||
        status == NULL || out == NULL || frame->entered || frame->active) {
        (void)damage_native_note(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                 "Damage required fresh stable context/code/outputs");
        return false;
    }
    if (frame->owned.ownership_status != MC_WORLD_STORAGE_OK ||
        frame->execution->phase != MC_ENTITY_TICK_EXECUTION_OPEN ||
        frame->execution->math->phase != MC_MATH_EXECUTION_OPEN || frame->execution->math->failed) {
        (void)damage_native_note(frame, MC_WORLD_STORAGE_BUSY,
                                 "Damage shared execution already failed or retired");
        return false;
    }
    frame->entered = frame->active = true;
    frame->victim = victim;
    frame->error = error;
    mc_entity_effect_error_init(error);
    bool returned = false;
    if (enter_arrow_base(victim, error)) {
        frame->prior_pending = mc_entity_effect_has_pending(victim);
        frame->body_invoked = true;
        returned = damage_body(victim, frame->registry, NULL, source, amount, out, error, frame);
        mc_entity_release(victim);
    } else
        (void)damage_native_note(frame,
                                 error->status == MC_ENTITY_LIMIT
                                     ? MC_WORLD_STORAGE_LIMIT
                                     : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "Damage actual victim native admission");
    frame->active = false;
    *status = error->status; /* Actual raw C channel; never a continuation witness. */
    frame->completed = returned && frame->owned.ownership_status == MC_WORLD_STORAGE_OK;
    return frame->completed;
}

bool mc_entity_damage_execution_inspect(mc_entity_damage_execution *frame,
                                        mc_entity_damage_execution_view *out) {
    if (frame == NULL || out == NULL || !frame->entered || frame->active || frame->cleaned ||
        !damage_work_matches(frame))
        return false;
    mc_entity_damage_execution_view view = {.execution = frame->execution,
                                            .result = &frame->owned,
                                            .child = frame->child,
                                            .acquired_work = frame->acquired_work,
                                            .completed = frame->completed,
                                            .unresolved = frame->unresolved};
    switch (frame->child) {
        case MC_ENTITY_DAMAGE_CHILD_SOUND:
            view.result = &frame->sound_result;
            view.sound = &frame->sound;
            view.world = frame->sound.world;
            view.world_created = frame->sound.scope_created;
            view.world_open = frame->sound.scope_open;
            break;
        case MC_ENTITY_DAMAGE_CHILD_KNOCKBACK:
            view.result = &frame->knockback_result;
            break;
        case MC_ENTITY_DAMAGE_CHILD_ALERT:
            view.result = &frame->alert_result;
            if (frame->work_kind == MC_ENTITY_DAMAGE_WORK_ALERT) {
                view.alert = frame->victim->local_player->pending_damage_alert;
                mc_player_ally_alert_execution_view child;
                if (!mc_player_ally_alert_execution_inspect(view.alert, &child))
                    return false;
                view.result = child.owned;
                view.world = child.world;
                view.world_created = child.scope_created;
                view.world_open = child.scope_open;
                view.query_returned = child.query_returned;
                view.query_status = child.query_status;
                mc_player_ally_alert_state state;
                if (!mc_player_ally_alert_inspect(view.alert, &state))
                    return false;
                view.nearby = &state.error->nearby;
            }
            break;
        case MC_ENTITY_DAMAGE_CHILD_WAKE:
            if (frame->work_kind == MC_ENTITY_DAMAGE_WORK_WAKE) {
                view.wake = mc_player_damage_wake_call(frame->victim);
                mc_world_runtime_execution_result *result =
                    mc_player_wake_execution_result(view.wake);
                if (result != NULL)
                    view.result = result;
            }
            break;
        case MC_ENTITY_DAMAGE_CHILD_WOLF_QUERY: {
            if (frame->work_kind != MC_ENTITY_DAMAGE_WORK_WOLF)
                break; /* Completed Wolf work has already retired its own query. */
            mc_wolf_damage_execution_view child;
            if (!mc_mob_wolf_damage_execution_inspect(frame->victim, &child))
                return false;
            view.result = child.result;
            view.nearby = child.nearby;
            view.world = child.world;
            view.world_created = child.created;
            view.world_open = child.open;
            view.query_status = child.query_status;
            view.query_returned = child.query_returned;
            break;
        }
        case MC_ENTITY_DAMAGE_CHILD_MOB_DEATH: {
            view.result = &frame->death_result;
            mc_mob_death_execution_view child;
            if (frame->death_work != NULL &&
                (!mc_entity_has_mob(frame->victim) ||
                 frame->victim->mob->pending_death != frame->death_work))
                return false;
            if (frame->death_work != NULL && mc_entity_has_mob(frame->victim) &&
                frame->victim->mob->pending_death == frame->death_work &&
                mc_mob_death_execution_inspect(frame->victim, &child)) {
                view.mob_drop = child.drop;
                view.result = child.has_child ? child.child.result : child.result;
            }
            break;
        }
        case MC_ENTITY_DAMAGE_CHILD_PLAYER_DEATH: {
            view.result = &frame->death_result;
            mc_player_death_execution_view child;
            if (frame->death_work != NULL &&
                (frame->victim->local_player == NULL ||
                 frame->victim->local_player->pending_death != frame->death_work))
                return false;
            if (frame->death_work != NULL && frame->victim->local_player != NULL &&
                frame->victim->local_player->pending_death == frame->death_work &&
                mc_player_death_execution_inspect(frame->victim, &child)) {
                view.player_drop = child.drop;
                view.result = child.has_child ? child.child.result : child.result;
            }
            break;
        }
        default:
            break;
    }
    *out = view;
    return true;
}

static mc_entity_damage_failure_kind damage_named_kind(const char *name) {
    static const char *const exceptions[] = {"java/lang/NullPointerException",
                                             "java/lang/ClassCastException",
                                             "java/lang/IllegalStateException",
                                             "java/lang/IllegalArgumentException",
                                             "java/lang/IndexOutOfBoundsException",
                                             "java/lang/ArrayIndexOutOfBoundsException",
                                             "java/util/ConcurrentModificationException",
                                             "java/util/NoSuchElementException",
                                             "java/io/IOException",
                                             "java/io/EOFException",
                                             "java/io/UTFDataFormatException"};
    if (name != NULL) {
        for (size_t i = 0; i < sizeof(exceptions) / sizeof(*exceptions); ++i)
            if (strcmp(name, exceptions[i]) == 0)
                return MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION;
        if (strcmp(name, "java/lang/ExceptionInInitializerError") == 0 ||
            strcmp(name, "java/lang/NoClassDefFoundError") == 0)
            return MC_ENTITY_DAMAGE_FAILURE_SOURCE_ERROR;
    }
    return MC_ENTITY_DAMAGE_FAILURE_UNRESOLVED;
}

static bool damage_protected_source(mc_entity_tick_execution *execution,
                                    mc_entity_tick_execution_world_token token,
                                    const mc_world_access_failure *active,
                                    const mc_world_storage_error *cause, const char **name) {
    if (active->source_null_receiver)
        return active->receiver == NULL && active->receipt == NULL &&
               active->render_global == NULL &&
               mc_world_access_failure_source_exception(active, name);
    if (token.owner != execution || token.slot >= MC_ENTITY_TICK_EXECUTION_WORLDS ||
        token.generation == 0 || !execution->lease_used[token.slot] ||
        execution->world_slots[token.slot].generation != token.generation)
        return false;
    const mc_world_access_failure *protected = NULL;
    const mc_world_access_failure_lease *node = &execution->leases[token.slot];
    if (!mc_world_access_failure_lease_inspect(node, &protected) ||
        protected->receiver != active->receiver || protected->receipt != active->receipt ||
        protected->render_global != active->render_global ||
        protected->source_null_receiver != active->source_null_receiver)
        return false;
    const mc_world_storage_error actual =
        mc_world_access_callback_error(node->callback_status, node->callback_error);
    return actual.status == cause->status && actual.operation == cause->operation &&
           actual.system_error == cause->system_error &&
           mc_world_access_failure_source_exception(protected, name);
}

static void damage_named_failure(mc_entity_damage_failure *failure, const char *name,
                                 const char *operation, mc_entity_tick_execution_site site) {
    failure->source_proven = true;
    failure->exception_class = name;
    failure->operation = operation;
    failure->site = site;
    failure->kind = damage_named_kind(name);
    failure->unresolved = failure->kind == MC_ENTITY_DAMAGE_FAILURE_UNRESOLVED;
}

bool mc_entity_damage_execution_source_failure(mc_entity_damage_execution *frame,
                                               mc_entity_damage_failure *out) {
    mc_entity_damage_execution_view view;
    if (out == NULL || !mc_entity_damage_execution_inspect(frame, &view) || frame->completed)
        return false;
    const mc_entity_effect_error *error = frame->error;
    mc_entity_damage_failure failure = {
        .kind = MC_ENTITY_DAMAGE_FAILURE_UNRESOLVED,
        .operation = error->operation,
        .site = {error->living_damage.child_site.class_name, error->living_damage.child_site.method,
                 error->living_damage.child_site.descriptor, error->living_damage.child_site.pc},
        .math = frame->math_failed ? &frame->math_failure : NULL,
        .math_owner = frame->math_failed ? frame->math_owner : NULL,
        .math_status = frame->math_failed ? frame->math_status : MC_MATH_CLASS_OK,
        .unresolved = true};
    if (failure.site.class_name == NULL)
        failure.site = (mc_entity_tick_execution_site){error->source_class, "a", "(Lsn;I)Z",
                                                       error->source_bytecode};
    if (frame->child == MC_ENTITY_DAMAGE_CHILD_SOUND && frame->sound.world_returned &&
        (error->world.world_access_fault == MC_WORLD_ACCESS_SOURCE_FAULT_NULL_SIZE_LIST ||
         error->world.world_access_fault == MC_WORLD_ACCESS_SOURCE_FAULT_NULL_GET_LIST)) {
        damage_named_failure(
            &failure, "java/lang/NullPointerException", error->world.cause.operation,
            (mc_entity_tick_execution_site){"fd", "a", "(Lsn;Ljava/lang/String;FF)V",
                                            (int)error->world.world_access_bytecode});
        *out = failure;
        return true;
    }
    const char *name = NULL;
    if (frame->child == MC_ENTITY_DAMAGE_CHILD_SOUND && frame->sound.world_returned &&
        frame->sound.world_status != MC_WORLD_STORAGE_OK &&
        damage_protected_source(frame->execution, frame->sound.world,
                                &error->world.world_access_failure, &error->world.cause, &name)) {
        damage_named_failure(&failure, name, error->world.cause.operation,
                             (mc_entity_tick_execution_site){"fd", "a",
                                                             "(Lsn;Ljava/lang/String;FF)V",
                                                             (int)error->world.bytecode});
        *out = failure;
        return true;
    }
    if (view.mob_drop != NULL) {
        mc_entity_drop_state state;
        mc_entity_drop_execution_view child;
        if (mc_entity_drop_inspect(view.mob_drop, &state) && state.error != NULL &&
            state.error->publication != NULL &&
            mc_entity_drop_execution_inspect(view.mob_drop, &child) && child.publication_returned &&
            child.publication_status != MC_WORLD_STORAGE_OK) {
            const mc_world_entity_error *publication = state.error->publication;
            if (damage_protected_source(frame->execution, child.publication_world.world,
                                        &publication->world_access_failure, &publication->cause,
                                        &name)) {
                damage_named_failure(
                    &failure, name, publication->cause.operation,
                    (mc_entity_tick_execution_site){"fd", "c", "(Lsn;)V",
                                                    (int)publication->world_access_bytecode});
                *out = failure;
                return true;
            }
        }
    }
    if (view.player_drop != NULL) {
        mc_player_drop_state state;
        mc_player_drop_execution_view child;
        if (mc_local_player_drop_inspect(view.player_drop, &state) && state.error != NULL &&
            state.error->publication != NULL &&
            mc_local_player_drop_execution_inspect(view.player_drop, &child) &&
            child.publication_returned && child.publication_status != MC_WORLD_STORAGE_OK) {
            const mc_world_entity_error *publication = state.error->publication;
            if (damage_protected_source(frame->execution, child.publication_world.world,
                                        &publication->world_access_failure, &publication->cause,
                                        &name)) {
                damage_named_failure(
                    &failure, name, publication->cause.operation,
                    (mc_entity_tick_execution_site){"fd", "c", "(Lsn;)V",
                                                    (int)publication->world_access_bytecode});
                *out = failure;
                return true;
            }
        }
    }
    if (view.wake != NULL) {
        const mc_entity_player_wake_state *wake = &error->player_wake;
        const mc_world_mutation_drop_execution *drop =
            mc_player_wake_execution_failed_drop(view.wake);
        if (drop != NULL && drop->publication_returned && drop->insertion.observer_returned &&
            drop->publication_status != MC_WORLD_STORAGE_OK &&
            damage_protected_source(frame->execution, drop->publication_scope.world,
                                    &drop->publication.world_access_failure,
                                    &drop->publication.cause, &name)) {
            damage_named_failure(
                &failure, name, drop->publication.cause.operation,
                (mc_entity_tick_execution_site){"fd", "c", "(Lsn;)V",
                                                (int)drop->publication.world_access_bytecode});
            *out = failure;
            return true;
        }
        if (wake->source_exception || wake->source_error) {
            const mc_player_site *site =
                wake->child.class_name == NULL ? &wake->site : &wake->child;
            damage_named_failure(&failure, wake->exception_class, wake->operation,
                                 (mc_entity_tick_execution_site){site->class_name, site->method,
                                                                 site->descriptor, (int)site->pc});
            *out = failure;
            return true;
        }
        mc_entity_tick_execution_site mutation_site;
        if (mc_player_wake_execution_source_null(view.wake, &mutation_site)) {
            damage_named_failure(&failure, "java/lang/NullPointerException", wake->operation,
                                 mutation_site);
            *out = failure;
            return true;
        }
        const mc_world_notification_execution *observer =
            mc_player_wake_execution_failure_observer(view.wake);
        const mc_world_mutation_error *mutation =
            wake->error != NULL && wake->error->occupancy != NULL ? wake->error->occupancy->mutation
                                                                  : NULL;
        if (observer != NULL && observer->callback_status != MC_WORLD_STORAGE_OK &&
            mutation != NULL &&
            damage_protected_source(frame->execution, observer->world,
                                    &mutation->world_access_failure, &mutation->cause, &name)) {
            damage_named_failure(
                &failure, name, mutation->cause.operation,
                (mc_entity_tick_execution_site){"fd", observer->range ? "b" : "j",
                                                observer->range ? "(IIIIII)V" : "(III)V",
                                                (int)mutation->world_access_bytecode});
            *out = failure;
            return true;
        }
    }
    /* Query Source classification remains unresolved below unless a current
     * direct typed fault or independent actual native/Math return proves more.
     * No legacy copied query classifier runs. */
    if (error->living_damage.source_exception || error->living_damage.source_error) {
        damage_named_failure(&failure, error->living_damage.exception_class, error->operation,
                             failure.site);
        *out = failure;
        return true;
    }
    if (frame->work_kind == MC_ENTITY_DAMAGE_WORK_ALERT &&
        (error->player_alert.source_exception || error->player_alert.source_error)) {
        damage_named_failure(&failure, error->player_alert.exception_class,
                             error->player_alert.operation,
                             (mc_entity_tick_execution_site){error->player_alert.child.class_name,
                                                             error->player_alert.child.method,
                                                             error->player_alert.child.descriptor,
                                                             (int)error->player_alert.child.pc});
        *out = failure;
        return true;
    }
    if (frame->death_work != NULL && frame->child == MC_ENTITY_DAMAGE_CHILD_PLAYER_DEATH &&
        (error->player_death.source_exception || error->player_death.source_error)) {
        damage_named_failure(&failure, error->player_death.exception_class,
                             error->player_death.operation,
                             (mc_entity_tick_execution_site){error->player_death.child.class_name,
                                                             error->player_death.child.method,
                                                             error->player_death.child.descriptor,
                                                             (int)error->player_death.child.pc});
        *out = failure;
        return true;
    }
    if (error->registry.status != MC_REGISTRY_OK &&
        (error->cause == MC_ENTITY_EFFECT_CAUSE_REGISTRY || error->player_bytecode == 148)) {
        mc_item_render_throwable kind = MC_ITEM_RENDER_NO_THROWABLE;
        mc_item_render_registry_classification(frame->registry, &error->registry, &kind, &name);
        failure.registry = &error->registry;
        if (kind != MC_ITEM_RENDER_NO_THROWABLE) {
            damage_named_failure(&failure, name, error->operation, failure.site);
            failure.kind = kind == MC_ITEM_RENDER_SOURCE_ERROR
                               ? MC_ENTITY_DAMAGE_FAILURE_SOURCE_ERROR
                               : MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION;
            failure.unresolved = false;
            *out = failure;
            return true;
        }
    }
    if ((error->cause == MC_ENTITY_EFFECT_CAUSE_STATISTIC ||
         error->cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE) &&
        error->statistic_error.status == MC_PLAYER_STATISTIC_NULL_REFERENCE) {
        damage_named_failure(&failure, "java/lang/NullPointerException",
                             error->statistic_error.operation,
                             (mc_entity_tick_execution_site){error->statistic_error.site.class_name,
                                                             error->statistic_error.site.method,
                                                             error->statistic_error.site.descriptor,
                                                             (int)error->statistic_error.site.pc});
        *out = failure;
        return true;
    }
    if (frame->owned.ownership_status != MC_WORLD_STORAGE_OK || frame->math_failed ||
        error->cause == MC_ENTITY_EFFECT_CAUSE_OWNER || error->cause == MC_ENTITY_EFFECT_CAUSE_FP ||
        error->cause == MC_ENTITY_EFFECT_CAUSE_RANDOM ||
        error->cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL) {
        failure.kind = MC_ENTITY_DAMAGE_FAILURE_NATIVE;
        failure.unresolved = false;
        if (frame->owned.ownership_status != MC_WORLD_STORAGE_OK) {
            failure.operation = frame->owned.ownership_error.operation;
            failure.site = frame->owned.ownership_error.site;
        } else if (frame->math_failed) {
            failure.operation = frame->math_failure.cause.operation;
            failure.site = (mc_entity_tick_execution_site){
                frame->math_failure.caller.class_name, frame->math_failure.caller.method,
                frame->math_failure.caller.descriptor, frame->math_failure.caller.pc};
        }
    }
    *out = failure;
    return true;
}

mc_world_storage_status mc_entity_damage_execution_native_close(mc_entity_damage_execution *frame) {
    if (frame == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (frame->active)
        return damage_native_note(frame, MC_WORLD_STORAGE_BUSY, "Damage actual body still active");
    if (!frame->entered || frame->cleaned)
        return MC_WORLD_STORAGE_OK;
    if (!damage_work_matches(frame))
        return damage_native_note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Damage exact acquired actor work identity changed");
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    if (frame->child == MC_ENTITY_DAMAGE_CHILD_MOB_DEATH) {
        mc_mob_death_execution_view child;
        if (frame->death_work != NULL && (!mc_entity_has_mob(frame->victim) ||
                                          frame->victim->mob->pending_death != frame->death_work))
            return damage_native_note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Damage exact acquired Mob death identity changed");
        if (frame->death_work != NULL && mc_entity_has_mob(frame->victim) &&
            frame->victim->mob->pending_death == frame->death_work &&
            mc_mob_death_execution_inspect(frame->victim, &child) && child.drop != NULL)
            status = mc_entity_drop_native_close(child.drop);
    } else if (frame->child == MC_ENTITY_DAMAGE_CHILD_PLAYER_DEATH) {
        mc_player_death_execution_view child;
        if (frame->death_work != NULL &&
            (frame->victim->local_player == NULL ||
             frame->victim->local_player->pending_death != frame->death_work))
            return damage_native_note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Damage exact acquired Player death identity changed");
        if (frame->death_work != NULL && frame->victim->local_player != NULL &&
            frame->victim->local_player->pending_death == frame->death_work &&
            mc_player_death_execution_inspect(frame->victim, &child) && child.drop != NULL)
            status = mc_local_player_drop_native_close(child.drop);
    } else if (frame->work_kind == MC_ENTITY_DAMAGE_WORK_WAKE)
        status = mc_player_wake_execution_native_close(mc_player_damage_wake_call(frame->victim));
    else if (frame->work_kind == MC_ENTITY_DAMAGE_WORK_ALERT)
        status =
            mc_player_ally_alert_native_close(frame->victim->local_player->pending_damage_alert);
    else if (frame->work_kind == MC_ENTITY_DAMAGE_WORK_WOLF &&
             frame->child == MC_ENTITY_DAMAGE_CHILD_WOLF_QUERY)
        status = mc_mob_wolf_damage_native_close(frame->victim);
    else if (frame->sound.entered && frame->sound.scope_created)
        status = mc_living_damage_sound_native_close(&frame->sound);
    frame->native_close_status = status;
    if (status != MC_WORLD_STORAGE_OK)
        (void)damage_native_note(frame, status, "Damage exact child native close");
    return status;
}

mc_entity_status mc_entity_damage_execution_cleanup(mc_entity_damage_execution *frame,
                                                    mc_entity_effect_error *error) {
    if (frame == NULL || error == NULL || (frame->entered && frame->error != error))
        return MC_ENTITY_INVALID_ARGUMENT;
    if (frame->cleaned)
        return MC_ENTITY_OK;
    const mc_world_storage_status closed = mc_entity_damage_execution_native_close(frame);
    if (closed != MC_WORLD_STORAGE_OK)
        return closed == MC_WORLD_STORAGE_BUSY ? MC_ENTITY_BUSY : MC_ENTITY_STATE_ERROR;
    if (frame->acquired_work != NULL && !mc_entity_effect_error_retire(error))
        return MC_ENTITY_BUSY;
    frame->acquired_work = NULL;
    frame->death_work = NULL;
    frame->work_kind = MC_ENTITY_DAMAGE_WORK_NONE;
    frame->cleaned = true;
    return MC_ENTITY_OK;
}

bool mc_entity_effect_has_pending(const mc_entity *entity) {
    return (mc_entity_has_mob(entity) &&
            (entity->mob->pending_death != NULL || entity->mob->pending_wolf_damage != NULL ||
             entity->mob->pending_slime_removal != NULL)) ||
           (entity != NULL && entity->local_player != NULL &&
            (entity->local_player->pending_death != NULL ||
             entity->local_player->pending_damage_wake != NULL ||
             entity->local_player->pending_damage_alert != NULL));
}

mc_entity_status mc_entity_effect_cleanup(mc_entity *entity, mc_entity_effect_error *error) {
    mc_entity_effect_error frame;
    mc_entity_effect_error_init(&frame);
    if (enter(entity, &frame)) {
        if (mc_entity_has_mob(entity)) {
            if (entity->mob->pending_slime_removal != NULL)
                (void)mc_slime_removal_cleanup(entity, &frame);
            else if (entity->mob->pending_wolf_damage != NULL)
                (void)mc_mob_wolf_damage_cleanup(entity, &frame);
            else
                (void)mc_mob_death_cleanup(entity, &frame);
        } else if (entity->local_player != NULL &&
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
    if (error->slime_removal.pending) {
        mc_entity_effect_error cleanup;
        mc_entity_effect_error_init(&cleanup);
        const mc_entity_status status =
            mc_slime_removal_cleanup(error->slime_removal.receiver, &cleanup);
        if (cleanup.slime_removal.entered)
            error->slime_removal = cleanup.slime_removal;
        if (status != MC_ENTITY_OK) {
            error->slime_removal.cleanup_status = status;
            error->slime_removal.cleanup_operation = cleanup.operation;
            return false;
        }
    }
    if (error->wolf_damage.pending) {
        mc_entity_effect_error cleanup;
        mc_entity_effect_error_init(&cleanup);
        const mc_entity_status status =
            mc_mob_wolf_damage_cleanup(error->wolf_damage.receiver, &cleanup);
        if (cleanup.wolf_damage.entered)
            error->wolf_damage = cleanup.wolf_damage;
        if (cleanup.death.entered)
            error->death = cleanup.death;
        if (status != MC_ENTITY_OK) {
            error->wolf_damage.cleanup_status = status;
            error->wolf_damage.cleanup_operation = cleanup.operation;
            return false;
        }
        /* The wrapper's private parent owns and retires any newly acquired
         * inherited failure. Earlier passive copies must not dereference it. */
        error->death.pending = error->death.active = false;
        error->death.receiver = NULL;
        error->death.drop_error = NULL;
    }
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
            (void)mc_mob_wolf_damage_inspect(entity, &frame.wolf_damage);
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
