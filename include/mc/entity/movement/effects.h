#ifndef MC_ENTITY_MOVEMENT_EFFECTS_H
#define MC_ENTITY_MOVEMENT_EFFECTS_H

#include "mc/entity/entity.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/movement.h"

typedef enum mc_entity_effect_cause {
    MC_ENTITY_EFFECT_CAUSE_NONE = 0,
    MC_ENTITY_EFFECT_CAUSE_OWNER,
    MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
    MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL,
    MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_RELATIONSHIP,
    MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_WATCHER_VALUE,
    MC_ENTITY_EFFECT_CAUSE_FP,
    MC_ENTITY_EFFECT_CAUSE_WORLD
} mc_entity_effect_cause;

/* Passive reached-operation frames, independent of constructor diagnostics.
 * source_bytecode belongs to the requested effect; child fields name its
 * actual delegated source bodies. Successful calls return a fresh empty error.
 * STATE_ERROR with an explicit unsupported cause is an unfinished reached
 * descendant, never an empty successful virtual or a synthesized Java Object. */
typedef struct mc_entity_effect_error {
    mc_entity_status status;
    const char *operation;
    mc_entity_effect_cause cause;
    const char *source_class;
    uint16_t source_bytecode, watcher_bytecode, flag_bytecode;
    uint16_t fall_bytecode, fall_callback_bytecode, damage_bytecode, fire_bytecode;
    uint16_t player_bytecode, living_bytecode, statistic_bytecode, close_bytecode;
    const mc_statistic *statistic;
    int32_t statistic_amount;
    mc_registry_error registry;
    int32_t watcher_index;
    mc_entity_error entity;
    mc_world_movement_error world;
} mc_entity_effect_error;

mc_entity_effect_error mc_entity_effect_empty_error(void);

/* Actual typed watcher Map lookup and signed Byte flag operation. No watcher
 * mutation or generic Object/boxing API is introduced. Missing/null record
 * reaches record-value16; unsupported typed value reaches Byte-cast19. The
 * source type metadata is not represented as a fabricated Java Object cast. */
mc_entity_status mc_entity_effect_watcher_byte(mc_entity *entity, int32_t index, int8_t *out,
                                               mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_flag(mc_entity *entity, int32_t bit, bool *out,
                                       mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_sneaking(mc_entity *entity, bool *out,
                                           mc_entity_effect_error *error);

mc_entity_status mc_entity_effect_wet(mc_entity *entity, bool *out, mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_fall_state(mc_entity *entity, double displacement, bool grounded,
                                             mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_fall_state_in_context(mc_entity *entity,
                                                        mc_registry_classes *code_registry,
                                                        double displacement, bool grounded,
                                                        mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_fall(mc_entity *entity, float distance,
                                       mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_fall_in_context(mc_entity *entity,
                                                  mc_registry_classes *code_registry,
                                                  float distance, mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_attacked(mc_entity *entity, mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_dead(mc_entity *entity, mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_damage(mc_entity *entity, mc_entity *source, int32_t amount,
                                         bool *out, mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_damage_in_context(mc_entity *entity,
                                                    mc_registry_classes *code_registry,
                                                    mc_entity *source, int32_t amount, bool *out,
                                                    mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_fire_damage(mc_entity *entity, int32_t amount,
                                              mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_fire_damage_in_context(mc_entity *entity,
                                                         mc_registry_classes *code_registry,
                                                         int32_t amount,
                                                         mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_walks(mc_entity *entity, bool *out,
                                        mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_contact_portal(mc_entity *entity, mc_entity_effect_error *error);

/* Caller supplies a real held READY canonical Item/Falling/LocalPlayer and exclusive
 * access through synchronous reentry. Each call retains that same owner until
 * its final use; admission/hold failures precede source effects. These are
 * native lifetime/phase guards, not Java instructions. Outputs/errors are
 * mutually disjoint from each other and all owners/input views. Failure leaves
 * outputs unchanged and preserves completed source stores. Constructors and
 * their diagnostics are untouched. Item/Falling damage leaves its nullable
 * source unread. LocalPlayer reads the actual Class identity only when reached.
 *
 * World/FP admission is delayed until a reached operation requires it. Wet's
 * actual bx=true branch needs no World or floating arithmetic. The current
 * fall callback returns for NULL passenger; a reached nonnull passenger stops
 * explicitly at its missing recursive virtual12 without dereferencing an
 * unowned relationship. LocalPlayer sneaking is actual input.e && !Player.u;
 * walks is inherited true, and portal dispatch stores Player.z/A. Item/Falling
 * retain their actual literal false/empty overrides.
 *
 * LocalPlayer fall executes gs->ls->sn in source order. A fall >=2 resolves
 * actual jl.n and computes the Java-rounded amount, then explicitly stops at
 * unfinished dc statistic/application dispatch. Smaller falls complete only
 * the genuinely reached base/Living body. Nonzero Player damage similarly
 * preserves av/health/sleep/type/difficulty prefixes, resolves jl.x, and stops
 * at that actual statistic call. Wake, Arrow ownership, other Living ally
 * handling and later Living hurt/armor/death remain explicit frontiers.
 * Player K stores be through super, then stops at its actual first Container
 * close invocation. These failures are not successful health/death substitutes.
 * Context arguments borrow the same actual Registry namespace independently
 * of nullable World; they are inspected only at reached code statics. Errors
 * may borrow the reached statistic identity while its Registry stays live. */

#endif
