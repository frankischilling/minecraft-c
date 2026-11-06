#include "damage_internal.h"
#include "death_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../living/fields_internal.h"
#include "../movement/effects_internal.h"
#include "../player/damage_internal.h"
#include "../player/death_internal.h"
#include "../player/tick_math_internal.h"
#include "internal.h"
#include "mc/core/java/java.h"
#include "references_internal.h"

#include <float.h>
#include <limits.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Living damage requires the qualified x86-64/SSE2 target"
#endif
#if !defined(_MSC_VER) && !defined(__SSE2__)
#error "Living damage requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Living damage requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(CHAR_BIT == 8 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 &&
                   FLT_EVAL_METHOD == 0,
               "Living damage requires binary32/binary64 evaluation");

static void living_at(mc_entity_effect_error *e, uint16_t pc) {
    e->living_bytecode = e->living_damage.living_pc = pc;
}

static void child_at(mc_entity_effect_error *e, const char *name, const char *method,
                     const char *descriptor, uint16_t pc) {
    e->living_damage.child_site.class_name = name;
    e->living_damage.child_site.method = method;
    e->living_damage.child_site.descriptor = descriptor;
    e->living_damage.child_site.pc = pc;
}

static bool damage_fail(mc_entity_effect_error *e, mc_entity_status status,
                        mc_entity_effect_cause cause, const char *operation) {
    e->status = status;
    e->operation = operation;
    e->cause = cause;
    e->living_damage.native_policy = true;
    e->living_damage.source_exception = false;
    e->living_damage.exception_class = NULL;
    return false;
}

static bool damage_null(mc_entity_effect_error *e, const char *operation) {
    (void)damage_fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE, operation);
    e->living_damage.native_policy = false;
    e->living_damage.source_exception = true;
    e->living_damage.exception_class = "java/lang/NullPointerException";
    return false;
}

static bool damage_fp(mc_entity_effect_error *e) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           damage_fail(e, MC_ENTITY_UNSUPPORTED_FP, MC_ENTITY_EFFECT_CAUSE_FP,
                       "Living damage FP admission");
}

static float damage_i2f(int32_t a) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), a));
}

static void counter(uint64_t *value, mc_entity_effect_error *e) {
    if (*value == UINT64_MAX)
        e->living_damage.counters_saturated = true;
    else
        ++*value;
}

static bool shared_draw_at(mc_entity *p, uint16_t pc, double *out, mc_entity_effect_error *e) {
    living_at(e, pc);
    child_at(e, "java/lang/Math", "random", "()D", 0);
    counter(&e->living_damage.shared_attempts, e);
    double value;
    const mc_java_random_profile_status status = mc_java_math_random(p->classes->profile, &value);
    if (status == MC_JAVA_RANDOM_PROFILE_OK) {
        counter(&e->living_damage.shared_calls, e);
        e->living_damage.shared_value = value;
        *out = value;
        return true;
    }
    const mc_entity_status result =
        status == MC_JAVA_RANDOM_PROFILE_LIMIT               ? MC_ENTITY_LIMIT
        : status == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
        : status == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP    ? MC_ENTITY_UNSUPPORTED_FP
                                                             : MC_ENTITY_PROFILE_FAILED;
    e->entity = (mc_entity_error){.status = result,
                                  .operation = "Living damage actual Math.random",
                                  .bytecode = pc,
                                  .profile = status};
    return damage_fail(e, result, MC_ENTITY_EFFECT_CAUSE_RANDOM,
                       "Living damage actual Math.random");
}

static bool private_draw(mc_entity *p, uint16_t pc, size_t index, mc_entity_effect_error *e) {
    living_at(e, pc);
    child_at(e, "ls", "a", "(Lsn;I)Z", pc);
    ++e->living_damage.private_attempts;
    /* Each invocation evaluates a fresh actual bs field, including the second
     * draw. Its nullable receiver is checked only after the earlier operands. */
    mc_java_random *random = p->random;
    if (random == NULL)
        return damage_null(e, "Living damage null private Random receiver");
    e->living_damage.private_values[index] = mc_java_random_next_float(random);
    ++e->living_damage.private_calls;
    return true;
}

static bool sound(mc_entity *p, mc_registry_classes *registry, bool fatal,
                  mc_entity_effect_error *e) {
    /* The World receiver is captured BEFORE evaluating the actual virtual
     * name, volume and both Random operands. Do not reload it after those. */
    mc_world_runtime *world = p->base.world;
    e->living_damage.sound_world_captured = true;
    e->living_damage.sound_world_identity = (uintptr_t)world;
    const uint16_t name_pc = fatal ? 284u : 332u;
    living_at(e, name_pc);
    const bool skeleton = mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SKELETON);
    const bool zombie = mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ZOMBIE);
    const char *declaring_class = NULL, *text = NULL;
    const char *volume_class = "ls";
    uint16_t volume_pc = 1;
    float volume = 1.0f;
    if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        declaring_class = "ls";
        text = "random.hurt";
    } else if (skeleton) {
        declaring_class = "fr";
        text = "mob.skeletonhurt";
    } else if (zombie) {
        declaring_class = "uz";
        text = fatal ? "mob.zombiedeath" : "mob.zombiehurt";
    } else if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_CREEPER)) {
        declaring_class = "gb";
        text = fatal ? "mob.creeperdeath" : "mob.creeper";
    } else if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SHEEP)) {
        declaring_class = "dl";
        text = "mob.sheep";
    } else if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PIG)) {
        declaring_class = "wh";
        text = fatal ? "mob.pigdeath" : "mob.pig";
    } else if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_COW)) {
        declaring_class = volume_class = "bx";
        text = "mob.cowhurt";
        volume = .4f;
        volume_pc = 2;
    } else if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_CHICKEN)) {
        declaring_class = "ww";
        text = "mob.chickenhurt";
    } else
        return damage_fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL,
                           "Living damage actual concrete sound declaration");
    child_at(e, declaring_class, fatal ? "i" : "j_", "()Ljava/lang/String;", 0);
    /* FR.i and FR.j_ both return mob.skeletonhurt. Neither receiver is
     * replaced; only its actual virtual String declaration differs here. */
    const mc_registry_string *name = NULL;
    if (registry == NULL || registry->entities != p->classes)
        return damage_fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_REGISTRY,
                           skeleton ? "Skeleton damage actual code String namespace"
                           : zombie ? "Zombie damage actual code String namespace"
                                    : "Living damage actual concrete String namespace");
    const mc_registry_status code =
        mc_registry_runtime_literal_ascii(registry, text, &name, &e->registry);
    if (code != MC_REGISTRY_OK)
        return damage_fail(e,
                           code == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
                           : code == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                                                                   : MC_ENTITY_STATE_ERROR,
                           MC_ENTITY_EFFECT_CAUSE_REGISTRY,
                           skeleton ? "Skeleton damage actual code String literal"
                           : zombie ? "Zombie damage actual code String literal"
                                    : "Living damage actual concrete String literal");
    child_at(e, declaring_class, fatal ? "i" : "j_", "()Ljava/lang/String;", 2);
    e->living_damage.sound_name_returned = true;
    e->living_damage.sound_name_identity = (uintptr_t)name;
    e->living_damage.sound_literal = text;
    living_at(e, fatal ? 288u : 336u);
    child_at(e, volume_class, "k", "()F", volume_pc);
    e->living_damage.volume_returned = true;
    e->living_damage.volume = volume;
    if (!private_draw(p, fatal ? 295u : 343u, 0, e) || !private_draw(p, fatal ? 302u : 350u, 1, e))
        return false;
    living_at(e, fatal ? 305u : 353u);
    if (!damage_fp(e))
        return false;
    const __m128 difference = _mm_sub_ss(_mm_set_ss(e->living_damage.private_values[0]),
                                         _mm_set_ss(e->living_damage.private_values[1]));
    const __m128 scaled = _mm_mul_ss(difference, _mm_set_ss(.2f));
    const float pitch = _mm_cvtss_f32(_mm_add_ss(scaled, _mm_set_ss(1.0f)));
    living_at(e, fatal ? 311u : 359u);
    child_at(e, "ls", "a", "(Lsn;I)Z", e->living_bytecode);
    e->living_damage.sound_arguments_stored = true;
    e->living_damage.pitch = pitch;
    if (world == NULL)
        return damage_null(e, "Living damage null captured sound World receiver");
    const mc_world_storage_status status =
        mc_world_runtime_entity_sound(world, p, name, volume, pitch, &e->world);
    if (status != MC_WORLD_STORAGE_OK) {
        const mc_entity_status result = status == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT
                                        : status == MC_WORLD_STORAGE_ALLOCATION_FAILED
                                            ? MC_ENTITY_ALLOCATION_FAILED
                                        : status == MC_WORLD_STORAGE_BUSY ? MC_ENTITY_BUSY
                                                                          : MC_ENTITY_WORLD_FAILED;
        (void)damage_fail(e, result, MC_ENTITY_EFFECT_CAUSE_WORLD,
                          "Living damage actual World Entity sound child");
        child_at(e, "fd", "a", "(Lsn;Ljava/lang/String;FF)V", (uint16_t)e->world.bytecode);
        const char *exception = NULL;
        /* Native LIMIT/allocation/hold refusal outranks any borrowed earlier
         * Source receipt. Never infer a Throwable from a generic status. */
        if (status != MC_WORLD_STORAGE_LIMIT && status != MC_WORLD_STORAGE_ALLOCATION_FAILED &&
            status != MC_WORLD_STORAGE_BUSY &&
            mc_world_access_failure_source_exception(&e->world.world_access_failure, &exception)) {
            e->living_damage.native_policy = false;
            e->living_damage.source_exception = true;
            e->living_damage.exception_class = exception;
        }
        return false;
    }
    e->living_damage.sound_returned = true;
    return true;
}

static void knockback_at(mc_entity_effect_error *e, uint16_t pc) {
    e->living_damage.knockback_pc = pc;
    child_at(e, "ls", "a", "(Lsn;IDD)V", pc);
}

/* Actual inherited LS.a(SN,int,DD)V. Its source/amount operands are unused.
 * The same caller Math namespace is activated at9, before any velocity store;
 * no World field is consulted to discover another Math implementation. */
static bool knockback(mc_entity *p, mc_math_classes *math, double dx, double dz,
                      mc_entity_effect_error *e) {
    e->living_damage.knockback_entered = true;
    knockback_at(e, 2);
    if (!damage_fp(e))
        return false;
    const double squared = at_add(at_mul(dx, dx), at_mul(dz, dz));
    knockback_at(e, 9);
    if (math == NULL) {
        e->math = (mc_math_class_error){.status = MC_MATH_CLASS_INVALID_ARGUMENT,
                                        .operation = "Living knockback absent MathHelper namespace",
                                        .source_pc = -1};
        return damage_fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_MATH,
                           "Living knockback actual MathHelper namespace");
    }
    if (mc_math_classes_ensure(math, &e->math) != MC_MATH_CLASS_OK) {
        const mc_entity_status status =
            e->math.status == MC_MATH_CLASS_LIMIT               ? MC_ENTITY_LIMIT
            : e->math.status == MC_MATH_CLASS_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
            : e->math.status == MC_MATH_CLASS_BUSY              ? MC_ENTITY_BUSY
            : e->math.status == MC_MATH_CLASS_UNSUPPORTED_FP    ? MC_ENTITY_UNSUPPORTED_FP
                                                                : MC_ENTITY_STATE_ERROR;
        return damage_fail(e, status, MC_ENTITY_EFFECT_CAUSE_MATH,
                           "Living knockback actual MathHelper active use");
    }
    const float length = at_root(squared);
    e->living_damage.knockback_length = length;
    knockback_at(e, 26);
    if (!damage_fp(e))
        return false;
    p->base.velocity_x = at_div(p->base.velocity_x, 2.0);
    p->base.velocity_y = at_div(p->base.velocity_y, 2.0);
    p->base.velocity_z = at_div(p->base.velocity_z, 2.0);
    p->base.velocity_x =
        at_sub(p->base.velocity_x, at_mul(at_div(dx, at_widen(length)), at_widen(.4f)));
    p->base.velocity_y = at_add(p->base.velocity_y, .4000000059604645);
    p->base.velocity_z =
        at_sub(p->base.velocity_z, at_mul(at_div(dz, at_widen(length)), at_widen(.4f)));
    /* Actual dcmpl110/ifle111: unordered Y skips the cap. */
    if (p->base.velocity_y > .4000000059604645)
        p->base.velocity_y = .4000000059604645;
    knockback_at(e, 121);
    e->living_damage.knockback_returned = true;
    return true;
}

static bool source_direction(mc_entity *p, mc_math_classes *math, mc_entity *source,
                             bool *source_held, mc_entity_effect_error *e) {
    mc_entity_living_damage_state *r = &e->living_damage;
    r->direction_entered = true;
    living_at(e, 144);
    child_at(e, "ls", "a", "(Lsn;I)Z", 144);
    /* Admission occurs only where Source first dereferences the nonnull SN.
     * The caller supplies an independently live canonical owner; identity-only
     * receipts neither prove that lifetime nor create a target relationship. */
    if (source->classes != p->classes || source->holds == 0 || source->phase != MC_ENTITY_READY ||
        source->class_literal == NULL || !mc_entity_is_kind(source, source->class_literal->kind))
        return damage_fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                           "Living damage reached canonical source owner");
    const mc_entity_status held = mc_entity_retain(source);
    if (held != MC_ENTITY_OK) {
        e->entity = (mc_entity_error){
            .status = held, .operation = "Living damage source local occurrence", .bytecode = 144};
        return damage_fail(e, held, MC_ENTITY_EFFECT_CAUSE_OWNER,
                           "Living damage source local occurrence");
    }
    *source_held = true;
    r->source_hold_acquired = true;
    r->source_read = true;
    r->source_identity = (uintptr_t)source;
    const double source_x = source->base.x;
    const double own_x = p->base.x;
    living_at(e, 151);
    if (!damage_fp(e))
        return false;
    double dx = at_sub(source_x, own_x);
    r->direction_x = dx;
    const double source_z = source->base.z; /* Independent source155 and self159 reads. */
    const double own_z = p->base.z;
    double dz = at_sub(source_z, own_z);
    r->direction_z = dz;
    for (;;) {
        living_at(e, 169);
        if (!damage_fp(e))
            return false;
        /* dcmpg179 exits on NaN. Diagnostic saturation never controls this
         * original unbounded retry, nor changes its four Math.random calls. */
        if (!(at_add(at_mul(dx, dx), at_mul(dz, dz)) < .0001))
            break;
        counter(&r->direction_iterations, e);
        r->direction_draw_mask = 0;
        if (!shared_draw_at(p, 183, &r->direction_draws[0], e))
            return false;
        r->direction_draw_mask |= 1u;
        if (!shared_draw_at(p, 186, &r->direction_draws[1], e))
            return false;
        r->direction_draw_mask |= 2u;
        living_at(e, 189);
        if (!damage_fp(e))
            return false;
        dx = at_mul(at_sub(r->direction_draws[0], r->direction_draws[1]), .01);
        r->direction_x = dx;
        if (!shared_draw_at(p, 196, &r->direction_draws[2], e))
            return false;
        r->direction_draw_mask |= 4u;
        if (!shared_draw_at(p, 199, &r->direction_draws[3], e))
            return false;
        r->direction_draw_mask |= 8u;
        living_at(e, 202);
        if (!damage_fp(e))
            return false;
        dz = at_mul(at_sub(r->direction_draws[2], r->direction_draws[3]), .01);
        r->direction_z = dz;
    }
    living_at(e, 217);
    child_at(e, "java/lang/Math", "atan2", "(DD)D", 0);
    r->atan2_value = mc_player_tick_atan2(dz, dx);
    const double degrees = at_div(at_mul(r->atan2_value, 180.0), 3.1415927410125732);
    const float narrowed = at_narrow(degrees);
    mc_entity_living_fields(p)->ac = at_fsub(narrowed, p->base.yaw);
    living_at(e, 244);
    if (!knockback(p, math, dx, dz, e))
        return false;
    r->direction_returned = true;
    return true;
}

static bool health_damage(mc_entity *p, mc_registry_classes *registry, int32_t amount,
                          mc_entity_effect_error *e) {
    if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        return mc_player_health_damage_entered(p, registry, amount, e);
    child_at(e, "ls", "b", "(I)V", 7);
    mc_living_entity_fields *v = mc_entity_living_fields(p);
    v->Y = mc_java_i32_sub(v->Y, amount);
    e->living_damage.health_pc = 10;
    return true;
}

static bool living_body(mc_entity *p, mc_registry_classes *registry, mc_math_classes *math,
                        mc_entity *source, int32_t amount, bool *out, bool *source_held,
                        mc_entity_effect_error *e) {
    mc_living_entity_fields *v = mc_entity_living_fields(p);
    living_at(e, 4);
    child_at(e, "ls", "a", "(Lsn;I)Z", 4);
    mc_world_runtime *world = p->base.world;
    if (world == NULL)
        return damage_null(e, "Living damage null World B receiver");
    if (world->fields.B) {
        living_at(e, 11);
        *out = false;
        return true;
    }
    v->av = 0;
    living_at(e, 14);
    if (v->Y <= 0) {
        living_at(e, 25);
        *out = false;
        return true;
    }
    v->al = 1.5f;
    living_at(e, 38);
    child_at(e, "ls", "a", "(Lsn;I)Z", 38);
    if (!damage_fp(e))
        return false;
    const float by = damage_i2f(p->base.by);
    const float half = _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(damage_i2f(v->E)), _mm_set_ss(2.0f)));
    bool full_hit = true;
    if (by > half) {
        if (amount <= v->au) {
            living_at(e, 59);
            *out = false;
            return true;
        }
        living_at(e, 67);
        if (!health_damage(p, registry, mc_java_i32_sub(amount, v->au), e))
            return false;
        v->au = amount;
        full_hit = false;
    } else {
        v->au = amount;
        v->Z = v->Y;
        p->base.by = v->E;
        living_at(e, 103);
        if (!health_damage(p, registry, amount, e))
            return false;
        v->ab = 10;
        v->aa = 10;
    }
    e->living_damage.full_hit = full_hit;
    v->ac = 0.0f;
    living_at(e, 119);
    if (full_hit) {
        world = p->base.world;
        living_at(e, 132);
        child_at(e, "ls", "a", "(Lsn;I)Z", 132);
        if (world == NULL)
            return damage_null(e, "Living damage null captured status World receiver");
        /* Client FD.a(Entity,byte) has precisely the one-byte return0 body.
         * There is no WorldAccess callback, Entity read or hidden status work. */
        e->living_damage.status_returned = true;
        living_at(e, 136);
        mc_entity_effect_attacked_entered(p);
        e->living_damage.attacked_returned = true;
        if (source != NULL) {
            if (!source_direction(p, math, source, source_held, e))
                return false;
        } else {
            double shared;
            if (!shared_draw_at(p, 251, &shared, e))
                return false;
            const double twice = _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(shared), _mm_set_sd(2.0)));
            const int32_t direction = mc_java_i32_mul(mc_java_double_to_i32(twice), 180);
            v->ac = damage_i2f(direction);
            living_at(e, 264);
        }
    }
    if (v->Y <= 0) {
        if (full_hit && !sound(p, registry, true, e))
            return false;
        living_at(e, 316);
        child_at(e, "ls", "b", "(Lsn;)V", 0);
        e->living_damage.death_invoke_reached = true;
        if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
            child_at(e, "gs", "b", "(Lsn;)V", 2);
            if (!mc_player_death_entered(p, registry, math, source, e))
                return false;
        } else if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_CREEPER)) {
            /* GB.b starts with its actual inherited LS death invocation.
             * Its later Skeleton-killer record branch has not been reached. */
            child_at(e, "gb", "b", "(Lsn;)V", 2);
            return damage_fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL,
                               "Living damage actual Creeper death/drop body");
        } else if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ZOMBIE) ||
                   mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SKELETON))
            return damage_fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL,
                               "Living damage actual death/drop body");
        else if (!mc_mob_death_entered(p, registry, source, e))
            return false;
    } else if (full_hit && !sound(p, registry, false, e))
        return false;
    living_at(e, 363);
    *out = true;
    return true;
}

static bool living(mc_entity *p, mc_registry_classes *registry, mc_math_classes *math,
                   mc_entity *source, int32_t amount, bool *out, mc_entity_effect_error *e) {
    bool source_held = false;
    const bool returned = living_body(p, registry, math, source, amount, out, &source_held, e);
    if (source_held) {
        mc_entity_release(source);
        e->living_damage.source_hold_released = true;
    }
    return returned;
}

bool mc_player_living_damage_entered(mc_entity *p, mc_registry_classes *registry,
                                     mc_math_classes *math, mc_entity *source, int32_t amount,
                                     bool *out, mc_entity_effect_error *e) {
    if (e == NULL)
        return false;
    if (p == NULL || !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        p->classes->source != MC_ENTITY_CLIENT || p->phase != MC_ENTITY_READY || out == NULL)
        return damage_fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                           "Player damage entered canonical owner contract");
    e->damage_arguments_stored = true;
    e->damage_source_identity = (uintptr_t)source;
    e->damage_amount = amount;
    e->source_class = "gs";
    e->source_bytecode = e->damage_bytecode = e->player_bytecode = 158;
    e->living_damage.entered = true;
    e->living_damage.wrapper_pc = 158;
    bool answer = false;
    if (!living(p, registry, math, source, amount, &answer, e))
        return false;
    e->living_damage.living_returned = true;
    e->source_bytecode = e->player_bytecode = e->living_damage.wrapper_pc = 161;
    child_at(e, "gs", "a", "(Lsn;I)Z", 161);
    e->living_damage.returned = true;
    e->living_damage.result_stored = true;
    e->living_damage.result = answer;
    *out = answer;
    return true;
}

bool mc_mob_monster_null_damage_entered(mc_entity *p, mc_registry_classes *registry, int32_t amount,
                                        bool *out, mc_entity_effect_error *e) {
    /* This check is native structural admission, never a Source preflight of
     * future World/Random/target/health operands. The facade owns a live hold. */
    if (e == NULL)
        return false;
    if ((!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ZOMBIE) &&
         !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SKELETON) &&
         !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_CREEPER)) ||
        !mc_entity_has_mob(p) || p->classes->source != MC_ENTITY_CLIENT || out == NULL)
        return damage_fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                           "Monster NULL damage entered canonical owner contract");
    e->damage_arguments_stored = true;
    e->damage_source_identity = 0;
    e->damage_amount = amount;
    e->source_class = "gz";
    e->source_bytecode = e->damage_bytecode = 3;
    e->living_damage.entered = true;
    e->living_damage.wrapper_pc = 3;
    bool answer = false;
    /* The symbolic II.a invokespecial resolves inherited LS directly: there
     * is no II declaration, entered body or intermediate return to credit. */
    if (!living(p, registry, NULL, NULL, amount, &answer, e))
        return false;
    e->living_damage.living_returned = true;
    uint16_t pc = 40;
    if (answer) {
        /* Identity-only relationship comparisons do not dereference mounts.
         * Default NULL aG returns26 without reading aH or the II target slot. */
        if (p->base.aG == NULL)
            pc = 26;
        else if (p->base.aH == NULL)
            pc = 26;
        else {
            e->source_bytecode = e->living_damage.wrapper_pc = 34;
            child_at(e, "gz", "a", "(Lsn;I)Z", 34);
            const mc_entity_status status = mc_mob_creature_target_store(p, NULL, &e->entity);
            if (status != MC_ENTITY_OK)
                return damage_fail(e, status, MC_ENTITY_EFFECT_CAUSE_OWNER,
                                   "Monster damage actual target owning store");
            pc = 38;
        }
    }
    e->source_bytecode = e->living_damage.wrapper_pc = pc;
    child_at(e, "gz", "a", "(Lsn;I)Z", pc);
    e->living_damage.returned = true;
    e->living_damage.result_stored = true;
    e->living_damage.result = answer;
    *out = answer;
    return true;
}

bool mc_mob_living_damage_entered(mc_entity *p, mc_registry_classes *registry,
                                  mc_math_classes *math, mc_entity *source, int32_t amount,
                                  bool *out, mc_entity_effect_error *e) {
    if (e == NULL)
        return false;
    if (p == NULL || !mc_entity_has_mob(p) || p->classes->source != MC_ENTITY_CLIENT ||
        p->phase != MC_ENTITY_READY || out == NULL ||
        (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SHEEP) &&
         !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PIG) &&
         !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_COW) &&
         !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_CHICKEN)))
        return damage_fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                           "Living damage entered canonical animal owner contract");
    const bool sheep = mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SHEEP);
    e->damage_arguments_stored = true;
    e->damage_source_identity = (uintptr_t)source;
    e->damage_amount = amount;
    e->source_class = sheep ? "dl" : "ls";
    e->source_bytecode = e->damage_bytecode = sheep ? 3 : 4;
    e->living_damage.entered = true;
    e->living_damage.wrapper_pc = sheep ? 3 : 0;
    bool answer = false;
    if (!living(p, registry, math, source, amount, &answer, e)) {
        if (!sheep)
            e->source_bytecode = e->living_bytecode;
        return false;
    }
    e->living_damage.living_returned = true;
    if (sheep) {
        /* DL invokes symbolic BG.a3; BG and II have no declaration. The SAME
         * source argument enters LS directly, with no shearing/taming body. */
        e->source_bytecode = e->living_damage.wrapper_pc = 6;
        child_at(e, "dl", "a", "(Lsn;I)Z", 6);
    } else
        e->source_bytecode = e->living_bytecode;
    e->living_damage.returned = true;
    e->living_damage.result_stored = true;
    e->living_damage.result = answer;
    *out = answer;
    return true;
}
