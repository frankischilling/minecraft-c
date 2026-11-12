#include "death_execution_internal.h"
#include "death_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../inventory/runtime/scatter_internal.h"
#include "../../item/render/internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../living/fields_internal.h"
#include "combat_callbacks_internal.h"
#include "container_interaction.h"
#include "mc/block/registry/runtime.h"
#include "mc/item/registry/runtime.h"
#include "player_internal.h"
#include "statistics_status_internal.h"

#include <emmintrin.h>
#include <stdlib.h>

struct mc_player_death_work {
    mc_player_death_state state;
    mc_entity *actor, *source;
    mc_registry_classes *registry;
    mc_math_classes *math;
    mc_world_runtime *worlds[2];
    size_t world_count;
    bool registry_held, math_held;
    mc_item_stack *apple;
    mc_player_drop_call *drop;
    mc_player_drop_error drop_error, drop_cleanup;
    mc_player_container_call *close;
    mc_player_container_error close_error, close_cleanup;
    bool execution_owned, drop_returned, completed;
    mc_player_drop_execution_context execution;
    mc_world_runtime_execution_result drop_result;
    mc_player_drop_status drop_status;
};

static mc_player_death_state view(const struct mc_player_death_work *w) {
    mc_player_death_state s = w->state;
    s.receiver = s.pending ? w->actor : NULL;
    s.drop_error = s.pending && w->drop != NULL ? &w->drop_error : NULL;
    s.close_error = s.pending && w->close != NULL ? &w->close_error : NULL;
    return s;
}

static void project(const struct mc_player_death_work *w, mc_entity_effect_error *e) {
    e->player_death = view(w);
}

static void at(struct mc_player_death_work *w, uint16_t pc) {
    w->state.site = (mc_player_site){"gs", w->state.closing ? "K" : "b",
                                     w->state.closing ? "()V" : "(Lsn;)V", pc};
}

static void living_at(struct mc_player_death_work *w, uint16_t pc) {
    w->state.living_pc = pc;
    w->state.child = (mc_player_site){"ls", "b", "(Lsn;)V", pc};
}

static bool fail(struct mc_player_death_work *w, mc_entity_effect_error *e, mc_entity_status status,
                 mc_entity_effect_cause cause, const char *operation) {
    w->state.status = e->status = status;
    w->state.operation = e->operation = operation;
    w->state.native_policy = true;
    w->state.source_exception = w->state.source_error = false;
    w->state.exception_class = NULL;
    e->cause = cause;
    project(w, e);
    return false;
}

static bool null_reference(struct mc_player_death_work *w, mc_entity_effect_error *e,
                           const char *operation) {
    (void)fail(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE, operation);
    w->state.native_policy = false;
    w->state.source_exception = true;
    w->state.exception_class = "java/lang/NullPointerException";
    project(w, e);
    return false;
}

static bool fp(struct mc_player_death_work *w, mc_entity_effect_error *e) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(w, e, MC_ENTITY_UNSUPPORTED_FP, MC_ENTITY_EFFECT_CAUSE_FP,
                "Player death strict floating environment");
}

static bool registry_result(struct mc_player_death_work *w, mc_entity_effect_error *e,
                            mc_registry_status status, bool source) {
    if (status == MC_REGISTRY_OK)
        return true;
    (void)fail(w, e,
               status == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
               : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
               : status == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
                                                         : MC_ENTITY_STATE_ERROR,
               MC_ENTITY_EFFECT_CAUSE_REGISTRY, e->registry.message);
    if (source) {
        mc_item_render_throwable throwable;
        mc_item_render_registry_classification(w->registry, &e->registry, &throwable,
                                               &w->state.exception_class);
        w->state.source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
        w->state.source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
        w->state.native_policy = throwable == MC_ITEM_RENDER_NO_THROWABLE;
        project(w, e);
    }
    return false;
}

static bool registry(struct mc_player_death_work *w, mc_entity_effect_error *e) {
    if (w->registry_held)
        return true;
    if (w->registry == NULL || w->registry->entities != w->actor->classes)
        return fail(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Player death actual code Registry namespace");
    if (!registry_result(w, e,
                         mc_registry_classes_retain(w->registry, MC_REGISTRY_CLIENT,
                                                    w->actor->classes->profile, &e->registry),
                         false))
        return false;
    w->registry_held = true;
    return true;
}

static bool world(struct mc_player_death_work *w, mc_world_runtime *world,
                  mc_entity_effect_error *e) {
    if (world == NULL)
        return null_reference(w, e, "Player Living death null World receiver");
    if (w->world_count >= 2 || !mc_world_runtime_binding_retain(world))
        return fail(w, e, MC_ENTITY_LIMIT, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Player Living death actual World occurrence");
    w->worlds[w->world_count++] = world;
    return true;
}

static bool callback(struct mc_player_death_work *w, mc_entity *source, bool score,
                     mc_entity_effect_error *e) {
    living_at(w, score ? 17 : 26);
    if (w->source == NULL) {
        if (source->classes != w->actor->classes || source->holds == 0 ||
            source->phase != MC_ENTITY_READY || source->class_literal == NULL ||
            !mc_entity_is_kind(source, source->class_literal->kind))
            return fail(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                        "Player death actual attacker callback owner");
        const mc_entity_status held = mc_entity_retain(source);
        if (held != MC_ENTITY_OK)
            return fail(w, e, held, MC_ENTITY_EFFECT_CAUSE_OWNER,
                        "Player death attacker callback occurrence");
        w->source = source;
        w->state.source_held = true;
        w->state.source_identity = (uintptr_t)source;
    }
    if (mc_entity_is_kind(source, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        mc_player_site reached;
        if (!mc_player_death_callback(source, w->registry, w->actor, w->state.score, score,
                                      &reached, e)) {
            w->state.child = reached;
            w->state.status = e->status;
            w->state.operation = e->operation;
            w->state.source_exception = e->living_damage.source_exception;
            w->state.source_error = e->living_damage.source_error;
            w->state.native_policy = !w->state.source_exception && !w->state.source_error;
            w->state.exception_class = e->living_damage.exception_class;
            project(w, e);
            return false;
        }
        w->state.child = reached;
    } else {
        /* All 39 supplied-client literal declarations were inspected. Only GS
         * overrides SN.c(Entity,int) and SN.a(Living); every other represented
         * concrete receiver resolves to SN's literal return0 bodies. */
        _Static_assert(MC_ENTITY_CLASS_KIND_COUNT == 39,
                       "Audit death callbacks for new Entity kinds");
        w->state.child =
            (mc_player_site){"sn", score ? "c" : "a", score ? "(Lsn;I)V" : "(Lls;)V", 0};
    }
    if (score)
        w->state.score_returned = true;
    else
        w->state.kill_returned = true;
    return true;
}

static bool living(struct mc_player_death_work *w, mc_entity *source, mc_entity_effect_error *e) {
    living_at(w, 1);
    if (mc_entity_living_fields(w->actor)->T >= 0 && source != NULL) {
        w->state.score = mc_entity_living_fields(w->actor)->T;
        if (!callback(w, source, true, e))
            return false;
    }
    if (source != NULL && !callback(w, source, false, e))
        return false;
    mc_entity_living_fields(w->actor)->ah = true;
    w->state.ah_stored = true;
    living_at(w, 38);
    mc_world_runtime *captured = w->actor->base.world;
    if (!world(w, captured, e))
        return false;
    w->state.remote = captured->fields.B;
    w->state.remote_read = true;
    if (!w->state.remote) {
        living_at(w, 45);
        /* GS declares neither q() nor j(). LS.q invokes the actual inherited
         * LS.j return0, then returns38 without a Random or Item operation. */
        w->state.child = (mc_player_site){"ls", "j", "()I", 1};
    }
    captured = w->actor->base.world;
    living_at(w, 54);
    if (!world(w, captured, e))
        return false;
    /* Actual client FD.a(Entity,byte) is precisely return0. */
    living_at(w, 57);
    w->state.living_returned = true;
    return true;
}

static bool drop_failed(struct mc_player_death_work *w, mc_entity_effect_error *e,
                        mc_player_drop_status status) {
    if (status == MC_PLAYER_DROP_OK)
        return true;
    const mc_player_drop_error *d = &w->drop_error;
    (void)fail(w, e,
               status == MC_PLAYER_DROP_LIMIT        ? MC_ENTITY_LIMIT
               : status == MC_PLAYER_DROP_ALLOCATION ? MC_ENTITY_ALLOCATION_FAILED
               : status == MC_PLAYER_DROP_BUSY || status == MC_PLAYER_DROP_CLEANUP ? MC_ENTITY_BUSY
               : status == MC_PLAYER_DROP_FLOATING ? MC_ENTITY_UNSUPPORTED_FP
               : status == MC_PLAYER_DROP_WORLD    ? MC_ENTITY_WORLD_FAILED
                                                   : MC_ENTITY_STATE_ERROR,
               status == MC_PLAYER_DROP_WORLD       ? MC_ENTITY_EFFECT_CAUSE_WORLD
               : status == MC_PLAYER_DROP_STATISTIC ? MC_ENTITY_EFFECT_CAUSE_STATISTIC
               : status == MC_PLAYER_DROP_MATH      ? MC_ENTITY_EFFECT_CAUSE_MATH
               : status == MC_PLAYER_DROP_REGISTRY  ? MC_ENTITY_EFFECT_CAUSE_REGISTRY
                                                    : MC_ENTITY_EFFECT_CAUSE_OWNER,
               d->operation);
    w->state.child = d->site;
    w->state.native_policy = d->native_policy;
    w->state.source_exception = d->source_exception;
    w->state.source_error = d->source_error;
    w->state.exception_class = d->exception_class;
    project(w, e);
    return false;
}

static bool retire_drop(struct mc_player_death_work *w, mc_entity_effect_error *e) {
    if (w->execution_owned && w->drop != NULL) {
        const mc_world_storage_status closed = mc_local_player_drop_native_close(w->drop);
        if (closed != MC_WORLD_STORAGE_OK) {
            mc_world_runtime_execution_result_note(
                &w->drop_result, closed, "Player death completed owned scatter close",
                (mc_entity_tick_execution_site){"gs", "b", "(Lsn;)V", 61});
            project(w, e);
            return false;
        }
    }
    if (mc_local_player_drop_retire(&w->drop, &w->drop_cleanup) != MC_PLAYER_DROP_OK) {
        w->state.cleanup_status = MC_ENTITY_BUSY;
        w->state.cleanup_operation = w->drop_cleanup.operation;
        return fail(w, e, MC_ENTITY_BUSY, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Player death returned scatter child cleanup");
    }
    ++w->state.drops_retired;
    return true;
}

static bool apple(struct mc_player_death_work *w, mc_entity_effect_error *e) {
    at(w, 49);
    const mc_item_stack_status allocated = mc_item_stack_allocate(&w->apple);
    if (allocated != MC_ITEM_STACK_OK)
        return fail(
            w, e, allocated == MC_ITEM_STACK_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_ALLOCATION_FAILED,
            MC_ENTITY_EFFECT_CAUSE_OWNER, "Player death actual Apple Stack allocation");
    w->state.apple_allocated = true;
    at(w, 53);
    if (!registry(w, e))
        return false;
    const mc_item *item = NULL;
    if (!registry_result(w, e, mc_registry_item_static_get(w->registry, "h", &item, &e->registry),
                         true))
        return false;
    at(w, 57);
    w->state.child = (mc_player_site){"iz", "<init>", "(Lgm;I)V", 2};
    int32_t id;
    if (!registry_result(w, e, mc_registry_item_source_id(w->registry, item, &id, &e->registry),
                         true))
        return false;
    if (mc_item_stack_construct(w->apple, id, 1, 0) != MC_ITEM_STACK_OK)
        return fail(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Player death actual Apple Stack construction");
    at(w, 61);
    w->drop_returned = false;
    w->drop_status = MC_PLAYER_DROP_OK;
    if (w->execution_owned)
        w->drop_returned = mc_local_player_drop_stack_begin_in_execution(
            w->execution.execution, w->execution.code_classes, w->execution.code, w->registry,
            w->actor, w->apple, true, &w->drop, &w->drop_status, &w->drop_error, &w->drop_result);
    else {
        w->drop_status = mc_local_player_drop_stack_begin(w->registry, w->math, w->actor, w->apple,
                                                          true, &w->drop, &w->drop_error);
        w->drop_returned = w->drop_status == MC_PLAYER_DROP_OK;
    }
    if (!w->drop_returned) {
        (void)drop_failed(w, e, w->drop_status);
        if (w->drop_status == MC_PLAYER_DROP_OK) {
            w->state.child = w->drop_error.site;
            project(w, e);
        }
        return false;
    }
    w->state.apple_returned = true;
    ++w->state.drops_returned;
    if (!retire_drop(w, e))
        return false;
    mc_item_stack_release(w->apple);
    w->apple = NULL;
    return true;
}

static bool trig(struct mc_player_death_work *w, mc_entity_effect_error *e, float angle, bool sine,
                 float *out) {
    if (w->execution_owned) {
        mc_math_execution *math = w->execution.execution->math;
        mc_math_execution_failure failure;
        const mc_math_class_status status = mc_math_execution_require_recorded(
            math, (mc_math_execution_site){"gs", "b", "(Lsn;)V", (int)w->state.site.pc},
            sine ? "a" : "b", "(F)F", &failure);
        if (status != MC_MATH_CLASS_OK) {
            const mc_world_storage_status recorded = mc_world_runtime_execution_result_note_math(
                &w->drop_result, math, status, &failure);
            mc_world_runtime_execution_result_note(
                &w->drop_result, recorded, "Player death actual Math return recording",
                (mc_entity_tick_execution_site){"gs", "b", "(Lsn;)V", (int)w->state.site.pc});
            e->math = failure.cause;
            project(w, e);
            return false;
        }
        const mc_math_trig *table = mc_math_classes_table(math->owned);
        if (table == NULL) {
            mc_world_runtime_execution_result_note(
                &w->drop_result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "Player death owned actual Math table",
                (mc_entity_tick_execution_site){"gs", "b", "(Lsn;)V", (int)w->state.site.pc});
            project(w, e);
            return false;
        }
        *out = sine ? mc_math_sin(table, angle) : mc_math_cos(table, angle);
        return true;
    }
    if (!w->math_held) {
        if (mc_math_classes_retain(w->math, &e->math) != MC_MATH_CLASS_OK)
            return fail(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_MATH,
                        e->math.operation);
        w->math_held = true;
    }
    if (mc_math_classes_ensure(w->math, &e->math) != MC_MATH_CLASS_OK)
        return fail(w, e,
                    e->math.status == MC_MATH_CLASS_LIMIT ? MC_ENTITY_LIMIT
                    : e->math.status == MC_MATH_CLASS_ALLOCATION_FAILED
                        ? MC_ENTITY_ALLOCATION_FAILED
                    : e->math.status == MC_MATH_CLASS_UNSUPPORTED_FP ? MC_ENTITY_UNSUPPORTED_FP
                                                                     : MC_ENTITY_STATE_ERROR,
                    MC_ENTITY_EFFECT_CAUSE_MATH, e->math.operation);
    const mc_math_trig *table = mc_math_classes_table(w->math);
    if (table == NULL)
        return fail(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_MATH,
                    "Player death actual Math table");
    *out = sine ? mc_math_sin(table, angle) : mc_math_cos(table, angle);
    return true;
}

static float fadd(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float fmul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float angle(const mc_entity *p) {
    const mc_living_entity_fields *v = mc_entity_living_fields((mc_entity *)p);
    const float sum = fadd(v->ac, p->base.yaw);
    const float product = fmul(sum, 3.1415927f);
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(product), _mm_set_ss(180.0f)));
}

static bool body(struct mc_player_death_work *w, mc_entity *source, mc_entity_effect_error *e) {
    mc_entity *p = w->actor;
    at(w, 2);
    if (!living(w, source, e))
        return false;
    at(w, 10);
    p->base.width = p->base.height = .2f;
    w->state.size_stored = true;
    at(w, 26);
    if (!fp(w, e))
        return false;
    mc_entity_source_position(p, p->base.x, p->base.y, p->base.z);
    w->state.position_returned = true;
    at(w, 33);
    p->base.velocity_y = .10000000149011612;
    at(w, 42);
    const mc_player_string *name = p->local_player->fields.player.l;
    if (name == NULL)
        return null_reference(w, e, "Player death null username String.equals receiver");
    mc_player_string_view text;
    if (!mc_player_string_read(name, &text))
        return fail(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Player death actual username String storage");
    static const uint16_t notch[] = {'N', 'o', 't', 'c', 'h'};
    bool is_notch = text.length == 5;
    for (size_t i = 0; is_notch && i < 5; ++i)
        is_notch = text.units[i] == notch[i];
    if (is_notch && !apple(w, e))
        return false;
    at(w, 68);
    mc_entity_status scattered = MC_ENTITY_OK;
    bool scatter_returned;
    if (w->execution_owned)
        scatter_returned = mc_inventory_player_scatter_entered_in_execution(
            w->execution.execution, w->execution.code_classes, w->execution.code,
            p->local_player->fields.player.c, w->registry, &w->state, &w->drop, &w->drop_error,
            &w->drop_cleanup, &scattered, &w->drop_status, &w->drop_returned, &w->drop_result);
    else {
        scattered = mc_inventory_player_scatter_entered(p->local_player->fields.player.c,
                                                        w->registry, w->math, &w->state, &w->drop,
                                                        &w->drop_error, &w->drop_cleanup);
        scatter_returned = scattered == MC_ENTITY_OK;
    }
    if (!scatter_returned) {
        if (scattered == MC_ENTITY_OK) {
            w->state.child = w->drop_error.site;
            project(w, e);
            return false;
        }
        e->status = scattered;
        e->cause = w->state.source_exception && w->state.exception_class != NULL &&
                           w->state.drop_error == NULL
                       ? MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE
                       : MC_ENTITY_EFFECT_CAUSE_INVENTORY;
        e->operation = w->state.operation;
        project(w, e);
        return false;
    }
    if (source != NULL) {
        at(w, 91);
        if (!fp(w, e))
            return false;
        float value;
        if (!trig(w, e, angle(p), false, &value))
            return false;
        at(w, 99);
        p->base.velocity_x = (double)fmul(-value, .1f);
        at(w, 118);
        if (!trig(w, e, angle(p), true, &value))
            return false;
        at(w, 126);
        p->base.velocity_z = (double)fmul(-value, .1f);
    } else {
        at(w, 136);
        p->base.velocity_z = 0.0;
        at(w, 139);
        p->base.velocity_x = 0.0;
    }
    at(w, 145);
    p->base.offset = .1f;
    at(w, 148);
    if (!registry(w, e) ||
        !registry_result(w, e, mc_registry_classes_initialize_statistics(w->registry, &e->registry),
                         true))
        return false;
    e->statistic = w->registry->statistics.named[19];
    e->statistic_amount = 1;
    at(w, 153);
    const mc_player_statistic_status status =
        mc_local_player_add_statistic(p, e->statistic, 1, &e->statistic_error);
    e->statistic_bytecode = (uint16_t)e->statistic_error.site.pc;
    if (status != MC_PLAYER_STATISTIC_OK) {
        const bool null = status == MC_PLAYER_STATISTIC_NULL_REFERENCE ||
                          (status == MC_PLAYER_STATISTIC_WRITER_FAILED &&
                           e->statistic_error.writer.status == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
                           e->statistic_error.writer.exception == MC_CLIENT_STATS_NULL_REFERENCE);
        if (null)
            return null_reference(w, e, "Player death actual death statistic child");
        return fail(w, e, mc_player_statistic_entity_status(&e->statistic_error),
                    MC_ENTITY_EFFECT_CAUSE_STATISTIC, e->statistic_error.operation);
    }
    w->state.statistic_returned = true;
    at(w, 156);
    return true;
}

static bool close_body(struct mc_player_death_work *w, mc_entity_effect_error *e) {
    at(w, 1);
    w->actor->base.be = true; /* Actual invokespecial LS.K -> SN.K store2. */
    for (size_t i = 0; i < 2; ++i) {
        mc_container *container = i == 0 ? w->actor->local_player->fields.player.d
                                         : w->actor->local_player->fields.player.e;
        if (i != 0 && container == NULL)
            break;
        at(w, i == 0 ? 9 : 24);
        ++w->state.close_calls;
        const mc_entity_status status = mc_player_container_direct_close_begin(
            w->registry, w->math, w->actor, container, &w->close, &w->close_error);
        if (status != MC_ENTITY_OK) {
            (void)fail(w, e, status,
                       w->close_error.source_exception ? MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE
                                                       : MC_ENTITY_EFFECT_CAUSE_INVENTORY,
                       w->close_error.operation);
            w->state.child = w->close_error.site;
            w->state.native_policy = w->close_error.native_policy;
            w->state.source_exception = w->close_error.source_exception;
            w->state.source_error = w->close_error.source_error;
            w->state.exception_class = w->close_error.exception_class;
            project(w, e);
            return false;
        }
        ++w->state.closes_returned;
        const mc_entity_status cleaned = mc_player_container_retire(&w->close, &w->close_cleanup);
        if (cleaned != MC_ENTITY_OK) {
            w->state.cleanup_status = cleaned;
            w->state.cleanup_operation = w->close_cleanup.operation;
            return fail(w, e, cleaned, MC_ENTITY_EFFECT_CAUSE_OWNER,
                        "Player K completed Container native cleanup");
        }
    }
    at(w, 27);
    return true;
}

bool mc_player_death_inspect(const mc_entity *p, mc_player_death_state *out) {
    if (p == NULL || p->local_player == NULL || out == NULL ||
        p->local_player->pending_death == NULL)
        return false;
    *out = view(p->local_player->pending_death);
    return true;
}

bool mc_player_death_execution_inspect(mc_entity *p, mc_player_death_execution_view *out) {
    if (p == NULL || p->local_player == NULL || out == NULL ||
        p->local_player->pending_death == NULL)
        return false;
    struct mc_player_death_work *w = p->local_player->pending_death;
    if (!w->execution_owned)
        return false;
    *out = (mc_player_death_execution_view){.execution = w->execution.execution,
                                            .result = &w->drop_result,
                                            .drop = w->drop,
                                            .drop_status = w->drop_status,
                                            .drop_returned = w->drop_returned,
                                            .completed = w->completed};
    if (w->drop != NULL)
        out->has_child = mc_local_player_drop_execution_inspect(w->drop, &out->child);
    return true;
}

static mc_entity_status cleanup_failed(struct mc_player_death_work *w, mc_entity_effect_error *e,
                                       mc_entity_status status, const char *operation) {
    w->state.cleanup_status = status;
    w->state.cleanup_operation = operation;
    e->status = status;
    e->operation = operation;
    e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
    project(w, e);
    return status;
}

mc_entity_status mc_player_death_cleanup(mc_entity *p, mc_entity_effect_error *e) {
    if (p == NULL || p->local_player == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    struct mc_player_death_work *w = p->local_player->pending_death;
    if (w == NULL)
        return MC_ENTITY_OK;
    if (w->state.active)
        return cleanup_failed(w, e, MC_ENTITY_BUSY, "Player active death/K native cleanup");
    const bool had_drop = w->drop != NULL;
    if (w->execution_owned && had_drop) {
        const mc_world_storage_status closed = mc_local_player_drop_native_close(w->drop);
        if (closed != MC_WORLD_STORAGE_OK)
            return cleanup_failed(w, e,
                                  closed == MC_WORLD_STORAGE_BUSY    ? MC_ENTITY_BUSY
                                  : closed == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT
                                  : closed == MC_WORLD_STORAGE_ALLOCATION_FAILED
                                      ? MC_ENTITY_ALLOCATION_FAILED
                                      : MC_ENTITY_WORLD_FAILED,
                                  "Player death exact owned scatter native close");
    }
    if (mc_local_player_drop_retire(&w->drop, &w->drop_cleanup) != MC_PLAYER_DROP_OK)
        return cleanup_failed(w, e, MC_ENTITY_BUSY, w->drop_cleanup.operation);
    if (had_drop)
        ++w->state.drops_retired;
    const mc_entity_status closed = mc_player_container_retire(&w->close, &w->close_cleanup);
    if (closed != MC_ENTITY_OK)
        return cleanup_failed(w, e, closed, w->close_cleanup.operation);
    if (w->math_held && mc_math_classes_release(&w->math, &e->math) != MC_MATH_CLASS_OK)
        return cleanup_failed(w, e, MC_ENTITY_BUSY, e->math.operation);
    w->math_held = false;
    if (w->registry_held && !mc_registry_runtime_borrow_release(w->registry))
        return cleanup_failed(w, e, MC_ENTITY_BUSY, "Player death Registry occurrence cleanup");
    w->registry_held = false;
    mc_item_stack_release(w->apple);
    w->apple = NULL;
    mc_entity_release(w->source);
    w->source = NULL;
    w->state.source_held = false;
    for (size_t i = 0; i < w->world_count; ++i)
        mc_world_runtime_binding_release(w->worlds[i]);
    w->world_count = 0;
    w->state.pending = false;
    w->state.cleanup_status = MC_ENTITY_OK;
    w->state.cleanup_operation = NULL;
    project(w, e);
    p->local_player->pending_death = NULL;
    free(w);
    mc_entity_release(p);
    return MC_ENTITY_OK;
}

static bool entered(mc_entity *p, mc_registry_classes *registry, mc_math_classes *math,
                    mc_entity *source, bool closing, mc_entity_effect_error *e,
                    const mc_player_drop_execution_context *execution,
                    mc_world_runtime_execution_result *owned) {
    if (p == NULL || p->local_player == NULL ||
        !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) || p->phase != MC_ENTITY_READY ||
        p->classes->source != MC_ENTITY_CLIENT) {
        e->status = MC_ENTITY_STATE_ERROR;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Player death actual canonical client owner";
        return false;
    }
    if (p->local_player->pending_death != NULL) {
        (void)mc_player_death_inspect(p, &e->player_death);
        e->status = MC_ENTITY_BUSY;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Player death/K existing pending child";
        return false;
    }
    struct mc_player_death_work *w = calloc(1, sizeof(*w));
    if (w == NULL) {
        e->status = MC_ENTITY_ALLOCATION_FAILED;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Player death retained invocation allocation";
        return false;
    }
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK) {
        free(w);
        e->status = held;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Player death self occurrence";
        return false;
    }
    w->actor = p;
    w->registry = registry;
    w->math = math;
    if (execution != NULL) {
        w->execution_owned = true;
        w->execution = *execution;
        mc_world_runtime_execution_result_init(&w->drop_result);
    }
    w->state = (mc_player_death_state){.entered = true,
                                       .pending = true,
                                       .active = true,
                                       .native_policy = true,
                                       .closing = closing,
                                       .receiver_identity = (uintptr_t)p,
                                       .inventory_slot = -1};
    p->local_player->pending_death = w;
    at(w, 0);
    const bool returned = closing ? close_body(w, e) : body(w, source, e);
    w->completed = returned;
    w->state.active = false;
    w->state.returned = returned;
    project(w, e);
    if (w->execution_owned) {
        const mc_world_storage_status forwarded =
            mc_world_runtime_execution_result_forward(owned, &w->drop_result);
        if (forwarded != MC_WORLD_STORAGE_OK) {
            mc_world_runtime_execution_result_note(
                owned, forwarded, "Player death exact owned scatter result forwarding",
                (mc_entity_tick_execution_site){"gs", "b", "(Lsn;)V", 68});
            return false;
        }
    }
    if (!returned)
        return false;
    return mc_player_death_cleanup(p, e) == MC_ENTITY_OK;
}

bool mc_player_death_entered(mc_entity *p, mc_registry_classes *registry, mc_math_classes *math,
                             mc_entity *source, mc_entity_effect_error *e) {
    return entered(p, registry, math, source, false, e, NULL, NULL);
}

bool mc_player_dead_entered(mc_entity *p, mc_registry_classes *registry, mc_math_classes *math,
                            mc_entity_effect_error *e) {
    return entered(p, registry, math, NULL, true, e, NULL, NULL);
}

bool mc_player_death_entered_in_execution(mc_entity_tick_execution *execution,
                                          mc_world_runtime_classes *code_classes,
                                          mc_world_runtime_execution_code code, mc_entity *p,
                                          mc_registry_classes *registry, mc_entity *source,
                                          mc_entity_status *status, mc_entity_effect_error *e,
                                          mc_world_runtime_execution_result *owned) {
    if (owned != NULL)
        mc_world_runtime_execution_result_init(owned);
    if (execution == NULL || execution->identity != execution || execution->math == NULL ||
        code_classes == NULL || status == NULL || e == NULL || owned == NULL) {
        mc_world_runtime_execution_result_note(
            owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
            "Player death execution/code/outputs required",
            (mc_entity_tick_execution_site){"gs", "b", "(Lsn;)V", 0});
        return false;
    }
    const mc_player_drop_execution_context descriptor = {execution, code_classes, code};
    const bool returned = entered(p, registry, NULL, source, false, e, &descriptor, owned);
    *status = e->status;
    return returned && owned->ownership_status == MC_WORLD_STORAGE_OK;
}
