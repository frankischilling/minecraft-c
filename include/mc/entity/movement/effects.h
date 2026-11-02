#ifndef MC_ENTITY_MOVEMENT_EFFECTS_H
#define MC_ENTITY_MOVEMENT_EFFECTS_H

#include "mc/core/math/classes.h"
#include "mc/entity/entity.h"
#include "mc/entity/mob/death.h"
#include "mc/entity/player/statistics.h"
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
    MC_ENTITY_EFFECT_CAUSE_WORLD,
    MC_ENTITY_EFFECT_CAUSE_STATISTIC,
    MC_ENTITY_EFFECT_CAUSE_RANDOM,
    MC_ENTITY_EFFECT_CAUSE_REGISTRY,
    MC_ENTITY_EFFECT_CAUSE_MATH
} mc_entity_effect_cause;

/* Value-only receipt of an actually entered canonical Living damage body.
 * No identity here owns an occurrence or may be dereferenced after its graph
 * retires. The existing Entity/Registry/World children retain the full causes.
 * Exact Source exceptions are distinguished from native limits/frontiers. */
typedef struct mc_entity_living_damage_state {
    bool entered, living_returned, returned, result_stored, result, full_hit;
    bool native_policy, source_exception;
    const char *exception_class;
    uint16_t wrapper_pc, living_pc, health_pc;

    struct {
        const char *class_name, *method, *descriptor;
        uint16_t pc;
    } child_site;

    uint64_t shared_attempts, shared_calls;
    uint8_t private_attempts, private_calls;
    bool counters_saturated;
    double shared_value;
    float private_values[2];
    bool status_returned, attacked_returned;
    bool sound_arguments_stored, sound_returned, death_invoke_reached;
    bool sound_world_captured, sound_name_returned, volume_returned;
    uintptr_t sound_world_identity, sound_name_identity;
    const char *sound_literal;
    float volume, pitch;
    /* The source retry is unbounded. Diagnostic counters saturate rather than
     * wrapping or controlling that loop; only its last four draw values are
     * retained. The mask identifies successful draws of the latest retry. */
    bool direction_entered, direction_returned, source_read;
    bool source_hold_acquired, source_hold_released;
    uintptr_t source_identity;
    uint64_t direction_iterations;
    uint8_t direction_draw_mask;
    double direction_draws[4], direction_x, direction_z, atan2_value;
    bool knockback_entered, knockback_returned;
    uint16_t knockback_pc;
    float knockback_length;
} mc_entity_living_damage_state;

/* Passive reached-operation frames, independent of constructor diagnostics.
 * source_bytecode belongs to the requested effect; child fields name its
 * actual delegated source bodies. Successful calls return a fresh empty error,
 * except the additive receipt of a genuinely entered Living damage/death body.
 * Failed death work belongs to the actual victim, even with NULL error output;
 * death's typed views borrow that work until checked cleanup. Value copies own
 * no resources and must not be reused after their borrowed graph retires.
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
    mc_player_statistic_error statistic_error;
    mc_math_class_error math;
    /* Actual evaluated operands at a reached, unfinished Mob hurt body.
     * The identity is comparison-only and owns no Entity occurrence. */
    bool damage_arguments_stored;
    uintptr_t damage_source_identity;
    int32_t damage_amount;
    mc_entity_living_damage_state living_damage;
    mc_mob_death_state death;
} mc_entity_effect_error;

/* Full passive initialization without a returned diagnostic copy. NULL is a no-op. */
void mc_entity_effect_error_init(mc_entity_effect_error *error);
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
/* Actual inherited client LS.b(Entity) death body on Sheep/Pig/Cow/Chicken/Wolf.
 * This does not invoke SN.K or reset health/death time. Failed native children
 * remain owned by the same actor, independently of optional value diagnostics.
 * Keep the actor/graph live and perform checked cleanup before retirement. */
mc_entity_status mc_entity_effect_death_in_context(mc_entity *, mc_registry_classes *,
                                                   mc_entity *source, mc_entity_effect_error *);
bool mc_entity_effect_has_pending(const mc_entity *);
mc_entity_status mc_entity_effect_cleanup(mc_entity *, mc_entity_effect_error *);
/* Consume the borrowed pending death named by this value receipt while its
 * graph is live. Success clears expired borrows and preserves the source cause.
 * False leaves the remaining producer and cleanup cause available for retry.
 * Copied receipts acquire no independent owner; their borrows expire together. */
bool mc_entity_effect_error_retire(mc_entity_effect_error *);
mc_entity_status mc_entity_effect_damage(mc_entity *entity, mc_entity *source, int32_t amount,
                                         bool *out, mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_damage_in_context(mc_entity *entity,
                                                    mc_registry_classes *code_registry,
                                                    mc_entity *source, int32_t amount, bool *out,
                                                    mc_entity_effect_error *error);
/* Same actual dispatch with the caller's exact MathHelper namespace. Math is
 * borrowed/exclusive for this synchronous call and activated only at a reached
 * inherited LS knockback sqrt. The older context entry supplies NULL Math;
 * remote/dead/repeated branches do not inspect it. A nonnull source is retained
 * at the first reached LS source-position read and kept through sound. Death
 * callbacks independently retain a reached source occurrence, including on a
 * stronger repeated fatal hit that skipped direction. Pending death cleanup
 * retains that occurrence after a failed call. Item/Falling and skipped Living
 * branches leave that argument unread. */
mc_entity_status mc_entity_effect_damage_with_math(mc_entity *, mc_registry_classes *,
                                                   mc_math_classes *, mc_entity *source,
                                                   int32_t amount, bool *out,
                                                   mc_entity_effect_error *);
mc_entity_status mc_entity_effect_fire_damage(mc_entity *entity, int32_t amount,
                                              mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_fire_damage_in_context(mc_entity *entity,
                                                         mc_registry_classes *code_registry,
                                                         int32_t amount,
                                                         mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_walks(mc_entity *entity, bool *out,
                                        mc_entity_effect_error *error);
mc_entity_status mc_entity_effect_contact_portal(mc_entity *entity, mc_entity_effect_error *error);

/* Caller supplies a real held READY canonical Item/Falling/LocalPlayer/Mob and exclusive
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
 * Mobs use the actual Byte0 sneaking flag and inherited SN push/walking leaves;
 * Spider and Wolf n return false. Chicken fall is its literal empty override;
 * other represented Living falls enter LS without Player statistics. Reached
 * canonical client Zombie/Skeleton NULL-source damage executes its actual GZ/LS
 * gates, health, direction and sound body. Client Sheep/Pig/Cow/Chicken execute
 * their actual inherited LS damage, including the nonnull-source direction
 * retry, atan2, knockback and concrete sound. Sheep's DL wrapper resolves LS
 * directly. Knockback requires the explicit Math context above. Fatal Living
 * damage stops at actual LS death invocation316 after any reached death sound.
 * GZ nonnull-source, Wolf/Slime hurt overrides and Slime splitting remain
 * explicit dependencies. No successful health/death substitute is supplied.
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
 * may borrow the reached statistic identity while its Registry stays live.
 * Zombie/Skeleton damage borrows the actual code String and exact WorldAccess producer
 * receipt under this same held, exclusive graph contract. Its completed receipt
 * remains available on ordinary true/false returns; it owns no extra resources. */

#endif
