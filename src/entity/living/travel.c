#include "mc/entity/living/travel.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../player/player_internal.h"
#include "../player/statistics_status_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/blocks.h"
#include "travel_math_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct travel_scope {
    mc_entity *entity;
    const mc_player_travel_context *context;
    mc_registry_classes *registry;
    bool registry_held;
} travel_scope;

void mc_player_travel_error_init(mc_player_travel_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->statistic_error.writer.index = SIZE_MAX;
    e->statistic_error.writer.text_offset = SIZE_MAX;
    mc_entity_environment_error_init(&e->environment);
    mc_entity_move_error_init(&e->movement);
    mc_entity_effect_error_init(&e->effect);
    e->registry = e->environment.registry;
}

mc_player_travel_error mc_player_travel_empty_error(void) {
    mc_player_travel_error e;
    mc_player_travel_error_init(&e);
    return e;
}

static bool fail(mc_player_travel_error *e, mc_entity_status status, mc_player_travel_cause cause,
                 const char *op) {
    e->status = status;
    e->cause = cause;
    e->operation = op;
    return false;
}

static bool fp(mc_player_travel_error *e) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_ENTITY_UNSUPPORTED_FP, MC_PLAYER_TRAVEL_FP, "Player travel FP admission");
}

static mc_entity_status registry_status(mc_registry_status s) {
    return s == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
           : s == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
           : s == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
           : s == MC_REGISTRY_INVALID_ARGUMENT  ? MC_ENTITY_INVALID_ARGUMENT
                                                : MC_ENTITY_STATE_ERROR;
}

static bool registry_result(mc_player_travel_error *e, mc_registry_status s) {
    return s == MC_REGISTRY_OK || fail(e, registry_status(s), MC_PLAYER_TRAVEL_REGISTRY,
                                       "Player travel actual code Registry");
}

static bool enter(travel_scope *s, mc_entity *entity, const mc_player_travel_context *context,
                  const char *source, const char *method, bool output, mc_player_travel_error *e) {
    e->source_class = source;
    e->source_method = method;
    e->player_method = strcmp(source, "gs") == 0 ? method : NULL;
    if (entity == NULL || !output)
        return fail(e, MC_ENTITY_INVALID_ARGUMENT, MC_PLAYER_TRAVEL_OWNER,
                    "Player travel arguments");
    if (entity->phase != MC_ENTITY_READY)
        return fail(e, MC_ENTITY_INVALID_PHASE, MC_PLAYER_TRAVEL_OWNER,
                    "Player travel READY admission");
    if (entity->classes->source != MC_ENTITY_CLIENT ||
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        entity->local_player == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, MC_PLAYER_TRAVEL_OWNER,
                    "Player travel actual canonical dc owner");
    const mc_entity_status held = mc_entity_retain(entity);
    if (held != MC_ENTITY_OK)
        return fail(e, held, MC_PLAYER_TRAVEL_OWNER, "Player travel Entity lifetime");
    s->entity = entity;
    s->context = context;
    s->registry = context == NULL ? NULL : context->code_registry;
    if (s->registry != NULL) {
        const mc_registry_status borrowed = mc_registry_classes_retain(
            s->registry, MC_REGISTRY_CLIENT, entity->classes->profile, &e->registry);
        if (!registry_result(e, borrowed)) {
            mc_entity_release(entity);
            s->entity = NULL;
            return false;
        }
        s->registry_held = true;
    }
    return true;
}

static mc_entity_status finish(travel_scope *s, mc_player_travel_error *e,
                               mc_player_travel_error *out) {
    if (s->registry_held && !mc_registry_runtime_borrow_release(s->registry))
        (void)fail(e, MC_ENTITY_STATE_ERROR, MC_PLAYER_TRAVEL_OWNER,
                   "Player travel Registry borrow retirement");
    if (s->entity != NULL)
        mc_entity_release(s->entity);
    if (e->status == MC_ENTITY_OK)
        mc_player_travel_error_init(e);
    if (out != NULL)
        *out = *e;
    return e->status;
}

static bool code_namespace(travel_scope *s, mc_player_travel_error *e) {
    if (s->registry == NULL || s->registry->entities != s->entity->classes)
        return fail(e, MC_ENTITY_STATE_ERROR, MC_PLAYER_TRAVEL_NAMESPACE,
                    "Player travel actual code namespace");
    return true;
}

static bool world_block(travel_scope *s, mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                        int32_t *out, mc_player_travel_error *e) {
    if (world != NULL && (mc_world_runtime_entities(world) != s->entity->classes ||
                          (s->registry != NULL && mc_world_runtime_registry(world) != s->registry)))
        return fail(e, MC_ENTITY_STATE_ERROR, MC_PLAYER_TRAVEL_NAMESPACE,
                    "Player travel World Entity namespace");
    const mc_world_storage_status status =
        mc_world_runtime_block_get(world, x, y, z, out, &e->world);
    return status == MC_WORLD_STORAGE_OK ||
           fail(e, MC_ENTITY_WORLD_FAILED,
                world == NULL ? MC_PLAYER_TRAVEL_NULL_REFERENCE : MC_PLAYER_TRAVEL_WORLD,
                "Player travel World block query");
}

static bool registry_null(mc_player_travel_error *e, const char *operation) {
    e->registry.status = MC_REGISTRY_NULL_REFERENCE;
    e->registry.message = operation;
    return fail(e, MC_ENTITY_STATE_ERROR, MC_PLAYER_TRAVEL_NULL_REFERENCE, operation);
}

static bool ladder(travel_scope *s, bool *out, mc_player_travel_error *e) {
    mc_entity *p = s->entity;
    e->ladder_bytecode = 4;
    if (!fp(e))
        return false;
    const int32_t x = mc_math_floor_double(p->base.x);
    e->ladder_bytecode = 12;
    if (p->bounds == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, MC_PLAYER_TRAVEL_NULL_REFERENCE,
                    "Ladder actual bounds receiver");
    const int32_t y = mc_math_floor_double(p->bounds->min_y), z = mc_math_floor_double(p->base.z);
    mc_world_runtime *world = p->base.world;
    int32_t id;
    e->ladder_bytecode = 34;
    if (!world_block(s, world, x, y, z, &id, e))
        return false;
    e->ladder_bytecode = 37;
    if (!code_namespace(s, e))
        return false;
    const mc_block *block;
    if (!registry_result(e, mc_registry_block_static_get(s->registry, "aG", &block, &e->registry)))
        return false;
    e->ladder_bytecode = 40;
    const mc_block_fields *fields = mc_block_fields_get(block);
    if (fields == NULL)
        return registry_null(e, "Ladder live Block.aG receiver");
    *out = id == fields->bn;
    e->ladder_bytecode = 0;
    return true;
}

static bool friction(travel_scope *s, unsigned getter, float *out, mc_player_travel_error *e) {
    mc_entity *p = s->entity;
    const bool second = getter == 404;
    mc_world_runtime *world = p->base.world;
    e->living_bytecode = second ? 382 : 283;
    if (!fp(e))
        return false;
    const int32_t x = mc_math_floor_double(p->base.x);
    e->living_bytecode = second ? 389 : 290;
    if (p->bounds == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, MC_PLAYER_TRAVEL_NULL_REFERENCE,
                    "Ground friction bounds receiver");
    const int32_t y = mc_java_i32_sub(mc_math_floor_double(p->bounds->min_y), 1);
    const int32_t z = mc_math_floor_double(p->base.z);
    int32_t id;
    e->living_bytecode = getter;
    if (!world_block(s, world, x, y, z, &id, e))
        return false;
    if (id > 0) {
        e->living_bytecode = second ? 414 : 315;
        if (!code_namespace(s, e))
            return false;
        mc_registry_block_m_view array;
        if (!registry_result(e, mc_registry_block_m_get(s->registry, &array, &e->registry)))
            return false;
        const mc_block *block;
        e->living_bytecode = second ? 419 : 320;
        if (!registry_result(e, mc_registry_block_m_at(&array, id, &block, &e->registry)))
            return false;
        e->living_bytecode = second ? 420 : 321;
        const mc_block_fields *fields = mc_block_fields_get(block);
        if (fields == NULL)
            return registry_null(e, "Ground friction captured Block.bB receiver");
        *out = mc_travel_fmul(fields->bB, 0.91f);
    }
    return true;
}

static bool environment_result(mc_player_travel_error *e, mc_entity_status status) {
    return status == MC_ENTITY_OK || fail(e, status, MC_PLAYER_TRAVEL_ENVIRONMENT,
                                          "Living travel actual environment descendant");
}

static bool accelerate(travel_scope *s, float strafe, float forward, float amount,
                       mc_player_travel_error *e) {
    return environment_result(
        e, mc_entity_environment_relative(s->entity, s->context == NULL ? NULL : s->context->trig,
                                          strafe, forward, amount, &e->environment));
}

static bool move(travel_scope *s, mc_player_travel_error *e) {
    mc_entity *p = s->entity;
    const mc_entity_status status = mc_entity_move_in_context(
        p, s->registry, p->base.velocity_x, p->base.velocity_y, p->base.velocity_z,
        s->context == NULL ? NULL : s->context->movement_limits, &e->movement);
    return status == MC_ENTITY_OK || fail(e, status, MC_PLAYER_TRAVEL_MOVEMENT,
                                          "Living travel actual dc movement descendant");
}

static bool escape(travel_scope *s, double previous_y, unsigned pc, mc_player_travel_error *e) {
    mc_entity *p = s->entity;
    if (!p->base.aY)
        return true;
    const double x = p->base.velocity_x;
    double y = mc_travel_dadd(p->base.velocity_y, 0.6000000238418579);
    y = mc_travel_dsub(y, p->base.y);
    y = mc_travel_dadd(y, previous_y);
    const double z = p->base.velocity_z;
    bool clear;
    e->living_bytecode = pc;
    if (!environment_result(e,
                            mc_entity_environment_clearance(
                                p, x, y, z, s->context == NULL ? NULL : s->context->material_limits,
                                &clear, &e->environment)))
        return false;
    if (clear)
        p->base.velocity_y = 0.30000001192092896;
    return true;
}

static bool living(travel_scope *s, float strafe, float forward, mc_player_travel_error *e) {
    mc_entity *p = s->entity;
    mc_living_entity_fields *state = &p->local_player->fields.living;
    bool water;
    e->living_bytecode = 1;
    if (!environment_result(e, mc_entity_environment_stored_in_water(p, &water, &e->environment)))
        return false;
    if (water) {
        const double previous_y = p->base.y;
        e->living_bytecode = 17;
        if (!accelerate(s, strafe, forward, 0.02f, e))
            return false;
        e->living_bytecode = 33;
        if (!move(s, e))
            return false;
        e->living_bytecode = 44;
        if (!fp(e))
            return false;
        p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, 0.800000011920929);
        p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, 0.800000011920929);
        p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, 0.800000011920929);
        p->base.velocity_y = mc_travel_dsub(p->base.velocity_y, 0.02);
        if (!escape(s, previous_y, 115, e))
            return false;
    } else {
        bool lava;
        e->living_bytecode = 132;
        if (!environment_result(e, mc_entity_environment_lava(
                                       p, s->registry,
                                       s->context == NULL ? NULL : s->context->material_limits,
                                       &lava, &e->environment)))
            return false;
        if (lava) {
            const double previous_y = p->base.y;
            e->living_bytecode = 148;
            if (!accelerate(s, strafe, forward, 0.02f, e))
                return false;
            e->living_bytecode = 164;
            if (!move(s, e))
                return false;
            e->living_bytecode = 175;
            if (!fp(e))
                return false;
            p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, 0.5);
            p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, 0.5);
            p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, 0.5);
            p->base.velocity_y = mc_travel_dsub(p->base.velocity_y, 0.02);
            if (!escape(s, previous_y, 246, e))
                return false;
        } else {
            float drag = 0.91f;
            if (p->base.aX) {
                drag = 0.54600006f;
                if (!friction(s, 305, &drag, e))
                    return false;
            }
            e->living_bytecode = 332;
            if (!fp(e))
                return false;
            const float square = mc_travel_fmul(drag, drag);
            const float cube = mc_travel_fmul(square, drag);
            const float factor = mc_travel_fdiv(0.16277136f, cube);
            const float amount = p->base.aX ? mc_travel_fmul(0.1f, factor) : 0.02f;
            e->living_bytecode = 358;
            if (!accelerate(s, strafe, forward, amount, e))
                return false;
            drag = 0.91f;
            if (p->base.aX) {
                drag = 0.54600006f;
                if (!friction(s, 404, &drag, e))
                    return false;
            }
            bool climbing;
            e->living_bytecode = 428;
            if (!ladder(s, &climbing, e))
                return false;
            if (climbing) {
                const double horizontal = (double)0.15f;
                if (p->base.velocity_x < -horizontal)
                    p->base.velocity_x = -horizontal;
                if (p->base.velocity_x > horizontal)
                    p->base.velocity_x = horizontal;
                if (p->base.velocity_z < -horizontal)
                    p->base.velocity_z = -horizontal;
                if (p->base.velocity_z > horizontal)
                    p->base.velocity_z = horizontal;
                p->base.bk = 0.0f;
                if (p->base.velocity_y < -0.15)
                    p->base.velocity_y = -0.15;
                bool sneaking;
                e->living_bytecode = 538;
                const mc_entity_status status = mc_entity_effect_sneaking(p, &sneaking, &e->effect);
                if (status != MC_ENTITY_OK)
                    return fail(e, status, MC_PLAYER_TRAVEL_ENVIRONMENT,
                                "Living ladder actual dc sneaking descendant");
                if (sneaking && p->base.velocity_y < 0.0)
                    p->base.velocity_y = 0.0;
            }
            e->living_bytecode = 571;
            if (!move(s, e))
                return false;
            if (p->base.aY) {
                e->living_bytecode = 582;
                if (!ladder(s, &climbing, e))
                    return false;
                if (climbing)
                    p->base.velocity_y = 0.2;
            }
            e->living_bytecode = 603;
            if (!fp(e))
                return false;
            p->base.velocity_y = mc_travel_dsub(p->base.velocity_y, 0.08);
            p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, 0.9800000190734863);
            p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, (double)drag);
            p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, (double)drag);
        }
    }
    state->ak = state->al;
    e->living_bytecode = 657;
    if (!fp(e))
        return false;
    const double dx = mc_travel_dsub(p->base.x, p->base.aJ),
                 dz = mc_travel_dsub(p->base.z, p->base.aL);
    const double square = mc_travel_dadd(mc_travel_dmul(dx, dx), mc_travel_dmul(dz, dz));
    float amount = mc_travel_fmul(mc_travel_sqrt_float(square), 4.0f);
    if (amount > 1.0f)
        amount = 1.0f;
    state->al = mc_travel_fadd(state->al, mc_travel_fmul(mc_travel_fsub(amount, state->al), 0.4f));
    state->am = mc_travel_fadd(state->am, state->al);
    return true;
}

static bool statistic_get(travel_scope *s, const char *field, size_t index, unsigned pc,
                          mc_player_travel_error *e) {
    e->player_bytecode = pc;
    if (!code_namespace(s, e))
        return false;
    if (!registry_result(e, mc_registry_classes_initialize_statistics(s->registry, &e->registry)))
        return false;
    e->statistic = s->registry->statistics.named[index];
    e->statistic_field = field;
    return true;
}

static bool statistic_dispatch(travel_scope *s, mc_player_travel_error *e, int32_t amount,
                               unsigned pc) {
    e->player_bytecode = pc;
    e->statistic_amount = amount;
    const mc_player_statistic_status status =
        mc_local_player_add_statistic(s->entity, e->statistic, amount, &e->statistic_error);
    e->statistic_bytecode = e->statistic_error.site.pc;
    return status == MC_PLAYER_STATISTIC_OK ||
           fail(e, mc_player_statistic_entity_status(&e->statistic_error),
                MC_PLAYER_TRAVEL_STATISTIC, "Actual LocalPlayer application statistic descendant");
}

static int32_t distance_amount(double x, double y, double z, bool vertical) {
    double square = mc_travel_dmul(x, x);
    if (vertical)
        square = mc_travel_dadd(square, mc_travel_dmul(y, y));
    square = mc_travel_dadd(square, mc_travel_dmul(z, z));
    return mc_travel_round_float(mc_travel_fmul(mc_travel_sqrt_float(square), 100.0f));
}

static bool statistics(travel_scope *s, double x, double y, double z, mc_player_travel_error *e) {
    e->player_method = "i(DDD)V";
    mc_entity *p = s->entity;
    if (p->base.aH != NULL)
        return true;
    e->player_bytecode = 9;
    if (!code_namespace(s, e))
        return false;
    mc_material *water;
    if (!registry_result(
            e, mc_registry_material_get(s->registry, MC_MATERIAL_g, &water, &e->registry)))
        return false;
    bool immersed;
    e->player_bytecode = 12;
    if (p->base.world != NULL && (mc_world_runtime_entities(p->base.world) != p->classes ||
                                  mc_world_runtime_registry(p->base.world) != s->registry))
        return fail(e, MC_ENTITY_STATE_ERROR, MC_PLAYER_TRAVEL_NAMESPACE,
                    "Player distance World code namespace");
    const mc_entity_status eye = mc_entity_inside_material(p, water, &immersed, &e->immersion);
    if (eye != MC_ENTITY_OK)
        return fail(e, eye, MC_PLAYER_TRAVEL_ENVIRONMENT,
                    "Player distance actual eye-material descendant");
    if (immersed) {
        e->player_bytecode = 20;
        if (!fp(e))
            return false;
        const int32_t amount = distance_amount(x, y, z, true);
        if (amount > 0) {
            if (!statistic_get(s, "q", 11, 48, e))
                return false;
            return statistic_dispatch(s, e, amount, 53);
        }
        return true;
    }
    if (p->base.bx) {
        e->player_bytecode = 68;
        if (!fp(e))
            return false;
        const int32_t amount = distance_amount(x, 0, z, false);
        if (amount > 0) {
            if (!statistic_get(s, "m", 7, 92, e))
                return false;
            return statistic_dispatch(s, e, amount, 97);
        }
        return true;
    }
    bool climbing;
    e->player_bytecode = 104;
    if (!ladder(s, &climbing, e))
        return false;
    if (climbing) {
        if (y > 0.0) {
            if (!statistic_get(s, "o", 9, 117, e))
                return false;
            e->player_bytecode = 124;
            if (!fp(e))
                return false;
            return statistic_dispatch(
                s, e, mc_java_i32_narrow(mc_travel_round_double(mc_travel_dmul(y, 100.0))), 129);
        }
        return true;
    }
    e->player_bytecode = p->base.aX ? 144 : 181;
    if (!fp(e))
        return false;
    const int32_t amount = distance_amount(x, 0, z, false);
    if (p->base.aX) {
        if (amount > 0) {
            if (!statistic_get(s, "l", 6, 168, e))
                return false;
            return statistic_dispatch(s, e, amount, 173);
        }
    } else if (amount > 25) {
        if (!statistic_get(s, "p", 10, 207, e))
            return false;
        return statistic_dispatch(s, e, amount, 212);
    }
    return true;
}

typedef enum travel_operation {
    LIVING_TRAVEL,
    PLAYER_TRAVEL,
    LADDER,
    LIVING_JUMP,
    PLAYER_JUMP,
    MOVEMENT_STATISTICS
} travel_operation;

static mc_entity_status invoke(mc_entity *p, const mc_player_travel_context *context,
                               travel_operation op, float strafe, float forward, double x, double y,
                               double z, bool *out, mc_player_travel_error *error) {
    /* This complete diagnostic graph can exceed200 KiB. Keep it off the
     * native call chain and independent across synchronous source callbacks. */
    mc_player_travel_error *e = malloc(sizeof(*e));
    if (e == NULL) {
        mc_player_travel_error_init(error);
        if (error != NULL) {
            error->status = MC_ENTITY_ALLOCATION_FAILED;
            error->cause = MC_PLAYER_TRAVEL_OWNER;
            error->operation = "Player travel diagnostic workspace";
        }
        return MC_ENTITY_ALLOCATION_FAILED;
    }
    mc_player_travel_error_init(e);
    travel_scope s = {0};
    bool value = false, completed = false;
    const bool base = op == LIVING_TRAVEL || op == LADDER || op == LIVING_JUMP;
    const char *method = op == LADDER                             ? "p()Z"
                         : op == LIVING_JUMP || op == PLAYER_JUMP ? "R()V"
                         : op == MOVEMENT_STATISTICS              ? "i(DDD)V"
                                                                  : "a_(FF)V";
    if (enter(&s, p, context, base ? "ls" : "gs", method, op != LADDER || out != NULL, e)) {
        if (op == LADDER)
            completed = ladder(&s, &value, e);
        else if (op == LIVING_JUMP || op == PLAYER_JUMP) {
            p->base.velocity_y = 0.41999998688697815;
            completed = true;
            if (op == PLAYER_JUMP) {
                completed = statistic_get(&s, "u", 15, 5, e);
                if (completed)
                    completed = statistic_dispatch(&s, e, 1, 9);
            }
        } else if (op == MOVEMENT_STATISTICS)
            completed = statistics(&s, x, y, z, e);
        else if (op == LIVING_TRAVEL)
            completed = living(&s, strafe, forward, e);
        else {
            const double old_x = p->base.x, old_y = p->base.y, old_z = p->base.z;
            e->player_bytecode = 20;
            completed = living(&s, strafe, forward, e);
            if (completed) {
                e->player_bytecode = 29;
                if (fp(e)) {
                    const double dx = mc_travel_dsub(p->base.x, old_x),
                                 dy = mc_travel_dsub(p->base.y, old_y),
                                 dz = mc_travel_dsub(p->base.z, old_z);
                    e->player_bytecode = 44;
                    completed = statistics(&s, dx, dy, dz, e);
                } else
                    completed = false;
            }
        }
    }
    const mc_entity_status status = finish(&s, e, error);
    if (status == MC_ENTITY_OK && completed && op == LADDER)
        *out = value;
    free(e);
    return status;
}

mc_entity_status mc_living_travel(mc_entity *p, const mc_player_travel_context *c, float a, float b,
                                  mc_player_travel_error *e) {
    return invoke(p, c, LIVING_TRAVEL, a, b, 0, 0, 0, NULL, e);
}

mc_entity_status mc_player_travel(mc_entity *p, const mc_player_travel_context *c, float a, float b,
                                  mc_player_travel_error *e) {
    return invoke(p, c, PLAYER_TRAVEL, a, b, 0, 0, 0, NULL, e);
}

mc_entity_status mc_living_on_ladder(mc_entity *p, const mc_player_travel_context *c, bool *out,
                                     mc_player_travel_error *e) {
    return invoke(p, c, LADDER, 0, 0, 0, 0, 0, out, e);
}

mc_entity_status mc_living_jump(mc_entity *p, mc_player_travel_error *e) {
    return invoke(p, NULL, LIVING_JUMP, 0, 0, 0, 0, 0, NULL, e);
}

mc_entity_status mc_player_jump(mc_entity *p, const mc_player_travel_context *c,
                                mc_player_travel_error *e) {
    return invoke(p, c, PLAYER_JUMP, 0, 0, 0, 0, 0, NULL, e);
}

mc_entity_status mc_player_movement_statistics(mc_entity *p, const mc_player_travel_context *c,
                                               double x, double y, double z,
                                               mc_player_travel_error *e) {
    return invoke(p, c, MOVEMENT_STATISTICS, 0, 0, x, y, z, NULL, e);
}
