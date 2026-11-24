#include "removal_execution_internal.h"

#include "../../world/runtime/runtime_internal.h"
#include "../../world/runtime/world_access_internal.h"
#include "../construction_internal.h"
#include "../movement/effects_internal.h"
#include "internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/entities.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

typedef struct removal_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
} removal_execution;

struct mc_slime_removal_work {
    mc_slime_removal_state state;
    mc_entity *actor, *child;
    mc_entity_effect_cause cause;
    mc_world_runtime *gate_world, *constructor_world, *publication_world;
    bool gate_held, constructor_held, publication_held, execution_owned, completed;
    removal_execution execution;
    mc_slime_removal_world_view constructor_scope, publication_scope;
    mc_world_entity_insert_execution insertion;
    mc_world_runtime_execution_result owned;
    mc_world_entity_error publication;
    mc_world_access_failure receipt;
    mc_entity_position_rotation_error pose;
};

static mc_mob_site site(const char *c, const char *m, const char *d, int32_t pc) {
    return (mc_mob_site){c, m, d, pc};
}

static mc_entity_tick_execution_site receiver_site(bool publication) {
    return (mc_entity_tick_execution_site){"uw", "K", "()V", publication ? 124 : 68};
}

static void at(struct mc_slime_removal_work *w, int32_t pc) {
    w->state.removal_pc = (uint16_t)pc;
    w->state.site = site("uw", "K", "()V", pc);
}

static mc_slime_removal_state view(const struct mc_slime_removal_work *w) {
    mc_slime_removal_state r = w->state;
    r.receiver = r.pending ? w->actor : NULL;
    r.current_child = r.pending ? w->child : NULL;
    r.publication = r.pending && r.publication_entered ? &w->publication : NULL;
    return r;
}

static void project(const struct mc_slime_removal_work *w, mc_entity_effect_error *e) {
    if (e != NULL)
        e->slime_removal = view(w);
}

static void apply(const struct mc_slime_removal_work *w, mc_entity_effect_error *e) {
    if (e == NULL)
        return;
    e->status = w->state.status;
    e->operation = w->state.operation;
    e->cause = w->cause;
    e->source_class = "uw";
    e->close_bytecode = w->state.removal_pc;
    e->watcher_index = w->state.watcher_index;
    e->watcher_bytecode = w->state.watcher_pc;
    project(w, e);
}

static bool failure(struct mc_slime_removal_work *w, mc_entity_status status,
                    mc_entity_effect_cause cause, const char *operation) {
    w->state.status = status;
    w->state.operation = operation;
    w->cause = cause;
    w->state.native_policy = true;
    w->state.source_exception = w->state.source_error = w->state.source_proven = false;
    w->state.source_unresolved = false;
    w->state.exception_class = NULL;
    return false;
}

static bool exception(struct mc_slime_removal_work *w, mc_entity_effect_cause cause,
                      const char *name, const char *operation) {
    (void)failure(w, MC_ENTITY_STATE_ERROR, cause, operation);
    w->state.native_policy = false;
    w->state.source_exception = w->state.source_proven = true;
    w->state.exception_class = name;
    return false;
}

static mc_entity_status storage_status(mc_world_storage_status status) {
    return status == MC_WORLD_STORAGE_LIMIT               ? MC_ENTITY_LIMIT
           : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_WORLD_STORAGE_BUSY              ? MC_ENTITY_BUSY
                                                          : MC_ENTITY_WORLD_FAILED;
}

static mc_entity_status mob_status(mc_mob_status status) {
    return status == MC_MOB_LIMIT            ? MC_ENTITY_LIMIT
           : status == MC_MOB_ALLOCATION     ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_MOB_BUSY           ? MC_ENTITY_BUSY
           : status == MC_MOB_UNSUPPORTED_FP ? MC_ENTITY_UNSUPPORTED_FP
                                             : MC_ENTITY_STATE_ERROR;
}

static bool mob_failure(struct mc_slime_removal_work *w) {
    const mc_mob_error *e = &w->state.mob;
    w->state.child = e->site;
    const mc_entity_status raw =
        e->status == MC_MOB_ENTITY_FAILED && e->entity.status != MC_ENTITY_OK
            ? e->entity.status
            : mob_status(e->status);
    (void)failure(w, raw,
                  e->status == MC_MOB_UNSUPPORTED_FP ? MC_ENTITY_EFFECT_CAUSE_FP
                                                     : MC_ENTITY_EFFECT_CAUSE_OWNER,
                  e->operation);
    w->state.native_policy = e->native_policy;
    w->state.source_exception = e->source_exception;
    w->state.source_proven = e->source_exception;
    w->state.exception_class = e->exception_class;
    if (e->source_exception && e->exception_class != NULL &&
        (strcmp(e->exception_class, "java/lang/ExceptionInInitializerError") == 0 ||
         strcmp(e->exception_class, "java/lang/NoClassDefFoundError") == 0)) {
        w->state.source_exception = false;
        w->state.source_error = true;
    }
    return false;
}

static bool fp(struct mc_slime_removal_work *w) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           failure(w, MC_ENTITY_UNSUPPORTED_FP, MC_ENTITY_EFFECT_CAUSE_FP,
                   "Slime removal reached strict floating environment");
}

static float fsub(float a, float b) {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float fmul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float fdiv(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static double dadd(double a, double b) {
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static bool records_pending(const mc_world_runtime_execution_result *r) {
    if (r->root_pending)
        return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (r->pending[i].used)
            return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (r->closes[i].used)
            return true;
    return false;
}

static bool scope_open(const struct mc_slime_removal_work *w,
                       const mc_slime_removal_world_view *s) {
    const mc_entity_tick_execution *e = w->execution.execution;
    return s->created && s->world.owner == e && s->world.slot < MC_ENTITY_TICK_EXECUTION_WORLDS &&
           s->world.generation != 0 &&
           e->world_slots[s->world.slot].generation == s->world.generation &&
           e->world_slots[s->world.slot].phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN;
}

static bool scope_consumed(const struct mc_slime_removal_work *w,
                           const mc_slime_removal_world_view *s) {
    const mc_entity_tick_execution *e = w->execution.execution;
    if (!s->created || s->world.owner != e || s->world.slot >= MC_ENTITY_TICK_EXECUTION_WORLDS ||
        s->world.generation == 0 || e->world_slots[s->world.slot].generation != s->world.generation)
        return false;
    const mc_entity_tick_execution_world_slot_phase p = e->world_slots[s->world.slot].phase;
    return p == MC_ENTITY_TICK_EXECUTION_WORLD_FREE || p == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED;
}

static bool capture(struct mc_slime_removal_work *w, mc_world_runtime *world, bool publication) {
    if (!w->execution_owned)
        return true;
    mc_slime_removal_world_view *s = publication ? &w->publication_scope : &w->constructor_scope;
    const mc_world_storage_status status = mc_entity_tick_execution_push_world(
        w->execution.execution, world, receiver_site(publication), &s->world);
    mc_world_runtime_execution_result_note(&w->owned, status, "Slime evaluated World capture",
                                           receiver_site(publication));
    if (status != MC_WORLD_STORAGE_OK)
        return false;
    s->created = s->open = true;
    return true;
}

static bool close_scope(struct mc_slime_removal_work *w, bool publication, bool completed) {
    mc_slime_removal_world_view *s = publication ? &w->publication_scope : &w->constructor_scope;
    if (!w->execution_owned || !s->created || s->close_attempted)
        return completed;
    if (completed && (w->owned.ownership_status != MC_WORLD_STORAGE_OK ||
                      w->owned.selection_ready || records_pending(&w->owned))) {
        mc_world_runtime_execution_result_note(&w->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                               "Slime completed child result not quiet",
                                               receiver_site(publication));
        completed = false;
    }
    s->locals_drained = true;
    s->failed = !completed;
    s->close_attempted = true;
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(w->execution.execution, &s->world, s->failed);
    s->initial_close_status = status;
    s->open = scope_open(w, s);
    s->consumed = scope_consumed(w, s);
    mc_world_runtime_execution_result_note(&w->owned, status, "Slime actual World checked close",
                                           receiver_site(publication));
    return status == MC_WORLD_STORAGE_OK && completed;
}

static mc_world_storage_status native_refused(struct mc_slime_removal_work *w,
                                              mc_world_storage_status status, const char *operation,
                                              bool publication) {
    mc_world_runtime_execution_result_note(&w->owned, status, operation,
                                           receiver_site(publication));
    return status;
}

static mc_world_storage_status native_scope(struct mc_slime_removal_work *w, bool publication) {
    mc_slime_removal_world_view *s = publication ? &w->publication_scope : &w->constructor_scope;
    if (!s->created || s->consumed)
        return MC_WORLD_STORAGE_OK;
    if (!s->close_attempted || !s->locals_drained)
        return native_refused(w, MC_WORLD_STORAGE_BUSY, "Slime initial close/locals required",
                              publication);
    if (scope_consumed(w, s)) {
        s->open = false;
        s->consumed = true;
        return MC_WORLD_STORAGE_OK;
    }
    mc_entity_tick_execution_world_token current;
    if (!s->open || !scope_open(w, s) ||
        !mc_entity_tick_execution_current_world(w->execution.execution, &current))
        return native_refused(w, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Slime recorded scope not OPEN or proved consumed", publication);
    if (current.owner != s->world.owner || current.slot != s->world.slot ||
        current.generation != s->world.generation)
        return native_refused(w, MC_WORLD_STORAGE_BUSY, "Slime recorded scope awaits exact current",
                              publication);
    s->native_close_attempted = true;
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(w->execution.execution, &s->world, s->failed);
    s->native_close_status = status;
    s->open = scope_open(w, s);
    s->consumed = scope_consumed(w, s);
    mc_world_runtime_execution_result_note(&w->owned, status, "Slime recorded Native checked close",
                                           receiver_site(publication));
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    return s->consumed
               ? MC_WORLD_STORAGE_OK
               : native_refused(w, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Slime successful close lacks consumed token proof", publication);
}

mc_world_storage_status mc_slime_removal_native_close(mc_entity *p) {
    if (!mc_entity_has_mob(p) || !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SLIME))
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (p->mob->pending_slime_removal == NULL)
        return MC_WORLD_STORAGE_OK;
    struct mc_slime_removal_work *w = p->mob->pending_slime_removal;
    if (!w->execution_owned || w->execution.execution == NULL ||
        w->execution.execution->identity != w->execution.execution)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (w->state.active || records_pending(&w->owned))
        return native_refused(w, MC_WORLD_STORAGE_BUSY,
                              "Slime active body or canonical pending/close work", true);
    const mc_world_storage_status status = native_scope(w, true);
    return status == MC_WORLD_STORAGE_OK ? native_scope(w, false) : status;
}

static bool hold_world(struct mc_slime_removal_work *w, mc_world_runtime *world, bool *held) {
    if (world == NULL)
        return true;
    if (!mc_world_runtime_binding_retain(world))
        return failure(w, MC_ENTITY_LIMIT, MC_ENTITY_EFFECT_CAUSE_OWNER,
                       "Slime captured World occurrence");
    *held = true;
    return true;
}

/* Preflight only OUR actual ordinary holds (and a sole child occurrence
 * about to destroy its World binding). Never infer an ambient close right. */
static bool releases_ready(const struct mc_slime_removal_work *w, bool final) {
    mc_world_runtime *worlds[5] = {w->constructor_held ? w->constructor_world : NULL,
                                   w->publication_held ? w->publication_world : NULL,
                                   final && w->gate_held ? w->gate_world : NULL,
                                   w->child != NULL && w->child->holds == 1 ? w->child->base.world
                                                                            : NULL,
                                   final && w->actor->holds == 1 ? w->actor->base.world : NULL};
    for (size_t i = 0; i < 5; ++i) {
        if (worlds[i] == NULL)
            continue;
        size_t count = 0;
        for (size_t j = 0; j < 5; ++j)
            count += worlds[j] == worlds[i];
        if (worlds[i]->persistent_bindings < count)
            return false;
    }
    return true;
}

static mc_entity_status cleanup_failure(struct mc_slime_removal_work *w, mc_entity_effect_error *e,
                                        mc_entity_status status, const char *operation) {
    w->state.cleanup_status = status;
    w->state.cleanup_operation = operation;
    if (e != NULL) {
        e->status = status;
        e->operation = operation;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
    }
    project(w, e);
    return status;
}

static bool current_release(struct mc_slime_removal_work *w, mc_entity_effect_error *e,
                            bool final) {
    if (!releases_ready(w, final)) {
        (void)cleanup_failure(w, e, MC_ENTITY_STATE_ERROR, "Slime recorded World releases blocked");
        return false;
    }
    if (w->child != NULL) {
        mc_entity_release(w->child); /* Releases OUR occurrence, never World membership. */
        w->child = NULL;
        ++w->state.children_released;
    }
    mc_world_access_failure_release(&w->receipt);
    if (w->constructor_held)
        mc_world_runtime_binding_release(w->constructor_world);
    if (w->publication_held)
        mc_world_runtime_binding_release(w->publication_world);
    w->constructor_held = w->publication_held = false;
    return true;
}

static bool protected_publication(struct mc_slime_removal_work *w, const char **name) {
    if (!w->execution_owned)
        return w->receipt.receipt != NULL &&
               mc_world_access_failure_source_exception(&w->receipt, name);
    const mc_slime_removal_world_view *s = &w->publication_scope;
    mc_entity_tick_execution *e = w->execution.execution;
    const mc_world_access_failure *held = NULL;
    const mc_world_access_failure *returned = &w->publication.world_access_failure;
    return w->insertion.observer_returned && s->created && s->world.owner == e &&
           s->world.slot < MC_ENTITY_TICK_EXECUTION_WORLDS && s->world.generation != 0 &&
           e->world_slots[s->world.slot].generation == s->world.generation &&
           mc_world_access_failure_lease_inspect(&e->leases[s->world.slot], &held) &&
           held->receipt == returned->receipt && held->receiver == returned->receiver &&
           held->render_global == returned->render_global &&
           e->leases[s->world.slot].callback_status == w->publication.cause.status &&
           e->leases[s->world.slot].callback_error.status == w->publication.cause.status &&
           e->leases[s->world.slot].callback_error.operation == w->publication.cause.operation &&
           e->leases[s->world.slot].callback_error.system_error ==
               w->publication.cause.system_error &&
           mc_world_access_failure_source_exception(held, name);
}

static void publication_throwable(struct mc_slime_removal_work *w, const char *name) {
    static const char *const exceptions[] = {"java/lang/NullPointerException",
                                             "java/lang/ClassCastException",
                                             "java/lang/IllegalStateException",
                                             "java/lang/IllegalArgumentException",
                                             "java/lang/IndexOutOfBoundsException",
                                             "java/lang/ArrayIndexOutOfBoundsException",
                                             "java/io/IOException",
                                             "java/io/EOFException",
                                             "java/io/UTFDataFormatException"};
    w->state.native_policy = false;
    w->state.source_proven = true;
    w->state.source_unresolved = true;
    w->state.exception_class = name;
    for (size_t i = 0; i < sizeof(exceptions) / sizeof(*exceptions); ++i)
        if (strcmp(name, exceptions[i]) == 0) {
            w->state.source_exception = true;
            w->state.source_unresolved = false;
        }
    if (strcmp(name, "java/lang/ExceptionInInitializerError") == 0 ||
        strcmp(name, "java/lang/NoClassDefFoundError") == 0) {
        w->state.source_error = true;
        w->state.source_unresolved = false;
    }
}

static bool publish(struct mc_slime_removal_work *w) {
    at(w, 124);
    mc_world_runtime *world = w->actor->base.world;
    w->publication_world = world;
    w->state.publication_worlds[w->state.index] = (uintptr_t)world;
    if (!capture(w, world, true) || !hold_world(w, world, &w->publication_held))
        return false;
    at(w, 129);
    if (world == NULL)
        return exception(w, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE, "java/lang/NullPointerException",
                         "Slime null publication World receiver");
    w->state.publication_entered = true;
    ++w->state.publications_invoked;
    bool completed;
    if (w->execution_owned) {
        mc_world_entity_insert_execution_init(&w->insertion, w->execution.execution,
                                              w->execution.code_classes, w->execution.code,
                                              receiver_site(true), &w->owned);
        completed = mc_world_runtime_insert_entity_in_execution(
            &w->insertion, world, w->child, &w->state.publication_result, &w->publication,
            &w->state.publication_status);
    } else {
        w->state.publication_status = mc_world_runtime_insert_entity(
            world, w->child, &w->state.publication_result, &w->publication);
        completed = w->state.publication_status == MC_WORLD_STORAGE_OK;
    }
    w->state.publication_returned = true;
    w->state.publication_continued = completed;
    w->state.inserted[w->state.index] = w->state.publication_result;
    const mc_world_storage_status raw = w->state.publication_status;
    if (raw != MC_WORLD_STORAGE_OK) {
        (void)failure(w, storage_status(raw), MC_ENTITY_EFFECT_CAUSE_WORLD,
                      w->publication.operation);
        w->state.child = site("fd", "b", "(Lsn;)Z", (int32_t)w->publication.bytecode);
    }
    if (!w->execution_owned && w->publication.world_access_failure.receipt != NULL) {
        const mc_world_storage_status retained =
            mc_world_access_failure_retain(&w->publication.world_access_failure, &w->receipt);
        if (retained != MC_WORLD_STORAGE_OK) {
            w->state.cleanup_status = storage_status(retained);
            w->state.cleanup_operation = "Slime actual legacy producer capture";
            return false;
        }
    }
    const char *name = NULL;
    if (raw != MC_WORLD_STORAGE_OK && w->publication.world_access_failure.source_null_receiver &&
        (!w->execution_owned || w->insertion.observer_returned))
        publication_throwable(w, "java/lang/NullPointerException");
    else if (raw != MC_WORLD_STORAGE_OK && protected_publication(w, &name))
        publication_throwable(w, name);
    else if (raw != MC_WORLD_STORAGE_OK && raw != MC_WORLD_STORAGE_LIMIT &&
             raw != MC_WORLD_STORAGE_ALLOCATION_FAILED && raw != MC_WORLD_STORAGE_BUSY &&
             (!w->execution_owned ||
              (w->owned.ownership_status == MC_WORLD_STORAGE_OK && !w->owned.math_failed))) {
        w->state.native_policy = false;
        w->state.source_unresolved = true;
    } else if (!completed && raw == MC_WORLD_STORAGE_OK) {
        w->state.native_policy = true;
        w->state.operation = "Slime owned insertion continuation refused";
    }
    return completed;
}

static bool body(struct mc_slime_removal_work *w, mc_entity_effect_error *e) {
    at(w, 1);
    int8_t size;
    mc_entity_watcher_byte_error read;
    if (!mc_entity_watcher_byte_read_entered(w->actor, 16, &size, &read)) {
        const bool receiver = read.stage == MC_ENTITY_WATCHER_BYTE_STAGE_FLAG;
        w->state.watcher_index = read.index;
        w->state.watcher_pc = receiver ? 6 : read.pc;
        w->state.child = receiver ? site("uw", "v", "()I", 6) : site("ud", "a", "(I)B", read.pc);
        if (read.cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE ||
            read.cause == MC_ENTITY_EFFECT_CAUSE_CLASS_CAST)
            return exception(w, read.cause,
                             read.cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE
                                 ? "java/lang/NullPointerException"
                                 : "java/lang/ClassCastException",
                             read.operation);
        return failure(w, read.status, read.cause, read.operation);
    }
    w->state.watcher_index = -1;
    w->state.size = (int32_t)size; /* Actual signed Byte.intValue. */
    w->state.size_read = true;
    at(w, 6);
    w->gate_world = w->actor->base.world;
    w->state.gate_world_identity = (uintptr_t)w->gate_world;
    if (!hold_world(w, w->gate_world, &w->gate_held))
        return false;
    at(w, 9);
    if (w->gate_world == NULL)
        return exception(w, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE, "java/lang/NullPointerException",
                         "Slime null remote World receiver");
    w->state.world_remote = w->gate_world->fields.B;
    w->state.world_b_read = true;
    if (!w->state.world_remote && w->state.size > 1) {
        at(w, 24);
        w->state.health = w->actor->mob->living.Y;
        w->state.health_read = true;
        if (w->state.health == 0)
            for (w->state.index = 0; w->state.index < 4; ++w->state.index) {
                const size_t i = (size_t)w->state.index;
                at(w, 40);
                if (!fp(w))
                    return false;
                w->state.offset_x[i] =
                    fdiv(fmul(fsub((float)(i % 2), .5f), (float)w->state.size), 4.f);
                w->state.offset_z[i] =
                    fdiv(fmul(fsub((float)(i / 2), .5f), (float)w->state.size), 4.f);
                w->state.child_allocated = w->state.constructor_invoked = false;
                w->state.constructor_returned = w->state.size_returned = w->state.pose_returned =
                    false;
                w->state.publication_entered = w->state.publication_returned = false;
                w->state.publication_continued = w->state.publication_result = false;
                w->state.publication_status = MC_WORLD_STORAGE_OK;
                memset(&w->state.mob, 0, sizeof(w->state.mob));
                w->publication = (mc_world_entity_error){0};
                w->insertion = (mc_world_entity_insert_execution){0};
                w->constructor_world = w->publication_world = NULL;
                w->constructor_scope = (mc_slime_removal_world_view){0};
                w->publication_scope = (mc_slime_removal_world_view){0};
                if (w->execution_owned)
                    mc_world_runtime_execution_result_init(&w->owned);
                at(w, 63);
                ++w->state.children_attempted;
                const mc_entity_class_literal *literal = NULL;
                const mc_entity_status kind = mc_entity_class_literal_get(
                    w->actor->classes, MC_ENTITY_CLASS_KIND_SLIME, &literal);
                if (kind != MC_ENTITY_OK)
                    return failure(w, kind, MC_ENTITY_EFFECT_CAUSE_OWNER,
                                   "Slime actual canonical child Class");
                if (mc_mob_allocate(w->actor->classes, NULL, literal, &w->child, &w->state.mob) !=
                    MC_MOB_OK)
                    return mob_failure(w);
                w->state.child_allocated = true;
                w->state.children[i] = (uintptr_t)w->child;
                at(w, 68);
                w->constructor_world = w->actor->base.world;
                w->state.constructor_worlds[i] = (uintptr_t)w->constructor_world;
                if (!capture(w, w->constructor_world, false) ||
                    !hold_world(w, w->constructor_world, &w->constructor_held))
                    return false;
                at(w, 71);
                w->state.constructor_invoked = true;
                const bool constructed =
                    mc_mob_construct(w->child, w->constructor_world, &w->state.mob) == MC_MOB_OK;
                w->state.constructor_returned = constructed;
                if (constructed)
                    ++w->state.constructors_returned;
                else
                    (void)mob_failure(w);
                if (!close_scope(w, false, constructed))
                    return false;
                at(w, 81);
                if (!mc_mob_slime_size_held(w->child, w->state.size / 2, &w->state.mob))
                    return mob_failure(w);
                w->state.size_returned = true;
                ++w->state.sizes_returned;
                at(w, 87);
                if (!fp(w))
                    return false;
                w->state.x[i] = dadd(w->actor->base.x, (double)w->state.offset_x[i]);
                at(w, 94);
                w->state.y[i] = dadd(w->actor->base.y, .5);
                at(w, 102);
                w->state.z[i] = dadd(w->actor->base.z, (double)w->state.offset_z[i]);
                at(w, 110);
                mc_java_random *random = w->actor->random;
                w->state.randoms[i] = (uintptr_t)random;
                at(w, 113);
                if (random == NULL)
                    return exception(w, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                                     "java/lang/NullPointerException",
                                     "Slime null parent yaw Random receiver");
                const float draw = mc_java_random_next_float(random);
                ++w->state.parent_draws;
                at(w, 118);
                if (!fp(w))
                    return false;
                w->state.yaw[i] = fmul(draw, 360.f);
                at(w, 120);
                w->state.pose_status = mc_entity_source_position_rotation(
                    w->child, w->state.x[i], w->state.y[i], w->state.z[i], w->state.yaw[i], 0.f,
                    &w->pose);
                w->state.pose_pc = w->pose.bytecode;
                w->state.position_pc = w->pose.position_bytecode;
                if (w->state.pose_status != MC_ENTITY_OK) {
                    w->state.child = site("sn", "c", "(DDDFF)V", (int32_t)w->pose.bytecode);
                    if (w->pose.source_null)
                        return exception(w, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                                         "java/lang/NullPointerException", w->pose.operation);
                    return failure(w, w->pose.status,
                                   w->pose.status == MC_ENTITY_UNSUPPORTED_FP
                                       ? MC_ENTITY_EFFECT_CAUSE_FP
                                       : MC_ENTITY_EFFECT_CAUSE_OWNER,
                                   w->pose.operation);
                }
                w->state.pose_returned = true;
                ++w->state.poses_returned;
                const bool published = publish(w);
                if (!close_scope(w, true, published))
                    return false;
                at(w, 132); /* Discard completed ordinary FD false too. */
                ++w->state.children_completed;
                if (!current_release(w, e, false)) {
                    if (w->execution_owned)
                        mc_world_runtime_execution_result_note(
                            &w->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            w->state.cleanup_operation,
                            (mc_entity_tick_execution_site){"uw", "K", "()V", 132});
                    else
                        (void)failure(w, w->state.cleanup_status, MC_ENTITY_EFFECT_CAUSE_OWNER,
                                      w->state.cleanup_operation);
                    return false;
                }
                at(w, 133);
            }
    }
    at(w, 140);
    w->state.child = site("sn", "K", "()V", 2);
    w->actor->base.be = true;
    w->state.be_stored = true;
    at(w, 143);
    w->state.returned = true;
    return true;
}

bool mc_slime_removal_inspect(const mc_entity *p, mc_slime_removal_state *out) {
    if (!mc_entity_has_mob(p) || out == NULL || p->mob->pending_slime_removal == NULL)
        return false;
    *out = view(p->mob->pending_slime_removal);
    return true;
}

bool mc_slime_removal_execution_inspect(mc_entity *p, mc_slime_removal_execution_view *out) {
    if (!mc_entity_has_mob(p) || out == NULL || p->mob->pending_slime_removal == NULL)
        return false;
    struct mc_slime_removal_work *w = p->mob->pending_slime_removal;
    if (!w->execution_owned)
        return false;
    *out = (mc_slime_removal_execution_view){
        .execution = w->execution.execution,
        .result = &w->owned,
        .publication = w->state.publication_entered ? &w->insertion : NULL,
        .current_child = w->child,
        .constructor_value = w->constructor_world,
        .publication_value = w->publication_world,
        .constructor_world = w->constructor_scope,
        .publication_world = w->publication_scope,
        .publication_status = w->state.publication_status,
        .publication_returned = w->state.publication_returned,
        .publication_continued = w->state.publication_continued,
        .completed = w->completed,
        .constructor_held = w->constructor_held,
        .publication_held = w->publication_held,
        .gate_held = w->gate_held};
    out->constructor_world.open = scope_open(w, &w->constructor_scope);
    out->publication_world.open = scope_open(w, &w->publication_scope);
    return true;
}

mc_entity_status mc_slime_removal_cleanup(mc_entity *p, mc_entity_effect_error *e) {
    if (p == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (!mc_entity_has_mob(p) || p->mob->pending_slime_removal == NULL)
        return MC_ENTITY_OK;
    struct mc_slime_removal_work *w = p->mob->pending_slime_removal;
    if (w->state.active)
        return cleanup_failure(w, e, MC_ENTITY_BUSY, "Slime active removal cleanup");
    if (w->execution_owned) {
        const mc_world_storage_status closed = mc_slime_removal_native_close(p);
        if (closed != MC_WORLD_STORAGE_OK)
            return cleanup_failure(w, e, storage_status(closed),
                                   "Slime exact recorded scopes cleanup");
    } else if (w->publication.world_access_failure.receipt != NULL && w->receipt.receipt == NULL) {
        const mc_world_storage_status captured =
            mc_world_access_failure_retain(&w->publication.world_access_failure, &w->receipt);
        if (captured != MC_WORLD_STORAGE_OK)
            return cleanup_failure(w, e, storage_status(captured),
                                   "Slime legacy producer cleanup capture");
    }
    if (!current_release(w, e, true))
        return w->state.cleanup_status;
    if (w->gate_held)
        mc_world_runtime_binding_release(w->gate_world);
    w->gate_held = false;
    w->state.pending = w->state.receiver_held = false;
    w->state.cleanup_status = MC_ENTITY_OK;
    w->state.cleanup_operation = NULL;
    /* Producer-backed class text expires with its actual receipt/scope graph;
     * direct fixed NPE/CCE strings remain ordinary static diagnostics. */
    if (w->state.publication_entered && w->state.source_proven)
        w->state.exception_class = NULL;
    project(w, e);
    p->mob->pending_slime_removal = NULL;
    free(w);
    mc_entity_release(p);
    return MC_ENTITY_OK;
}

static bool entered(mc_entity *p, mc_entity_effect_error *e, const removal_execution *execution,
                    mc_entity_status *raw, mc_world_runtime_execution_result *outer) {
    if (!mc_entity_has_mob(p) || !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SLIME) ||
        p->phase != MC_ENTITY_READY || p->classes->source != MC_ENTITY_CLIENT || p->holds == 0) {
        if (e != NULL) {
            e->status = MC_ENTITY_STATE_ERROR;
            e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
            e->operation = "Slime removal entered canonical client owner";
        }
        if (raw != NULL)
            *raw = MC_ENTITY_STATE_ERROR;
        return false;
    }
    if (mc_entity_effect_has_pending(p)) {
        if (e != NULL) {
            /* Rejection acquires no work. Inspect exposes the old call
             * separately; retiring this error must never consume it. */
            e->slime_removal = (mc_slime_removal_state){0};
            e->status = MC_ENTITY_BUSY;
            e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
            e->operation = "Slime removal existing pending work";
        }
        if (raw != NULL)
            *raw = MC_ENTITY_BUSY;
        return false;
    }
    struct mc_slime_removal_work *w = calloc(1, sizeof(*w));
    if (w == NULL) {
        if (e != NULL) {
            e->status = MC_ENTITY_ALLOCATION_FAILED;
            e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
            e->operation = "Slime reached removal work allocation";
        }
        if (raw != NULL)
            *raw = MC_ENTITY_ALLOCATION_FAILED;
        return false;
    }
    w->actor = p;
    w->state.entered = true;
    w->state.receiver_identity = (uintptr_t)p;
    w->state.watcher_index = -1;
    w->state.child.pc = -1;
    if (execution != NULL) {
        w->execution_owned = true;
        w->execution = *execution;
        mc_world_runtime_execution_result_init(&w->owned);
    }
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK) {
        (void)failure(w, held, MC_ENTITY_EFFECT_CAUSE_OWNER, "Slime removal receiver self-hold");
        apply(w, e);
        if (raw != NULL)
            *raw = held;
        free(w);
        return false;
    }
    w->state.pending = w->state.active = w->state.receiver_held = true;
    p->mob->pending_slime_removal = w;
    bool returned = body(w, e);
    returned = close_scope(w, true, returned);
    returned = close_scope(w, false, returned);
    w->completed = returned;
    w->state.active = false;
    if (!returned && w->state.status == MC_ENTITY_OK && !w->state.source_proven)
        w->state.native_policy = true;
    apply(w, e);
    if (raw != NULL)
        *raw = w->state.status;
    if (w->execution_owned) {
        const mc_world_storage_status forwarded =
            mc_world_runtime_execution_result_forward(outer, &w->owned);
        if (forwarded != MC_WORLD_STORAGE_OK) {
            mc_world_runtime_execution_result_note(outer, forwarded,
                                                   "Slime actual failed child VALUE forwarding",
                                                   receiver_site(true));
            return false;
        }
    }
    if (!returned)
        return false;
    const mc_entity_status cleaned = mc_slime_removal_cleanup(p, e);
    if (cleaned == MC_ENTITY_OK)
        return true;
    /* Refused cleanup keeps this exact work alive. Source may already have
     * stored be; preserve that prefix and raw return while recording the
     * actual final native refusal in the same canonical result. */
    w->completed = false;
    if (w->execution_owned) {
        const mc_world_storage_status native = cleaned == MC_ENTITY_BUSY    ? MC_WORLD_STORAGE_BUSY
                                               : cleaned == MC_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                               : cleaned == MC_ENTITY_ALLOCATION_FAILED
                                                   ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        mc_world_runtime_execution_result_note(
            &w->owned, native, w->state.cleanup_operation,
            (mc_entity_tick_execution_site){"uw", "K", "()V", (int32_t)w->state.removal_pc});
        const mc_world_storage_status forwarded =
            mc_world_runtime_execution_result_forward(outer, &w->owned);
        if (forwarded != MC_WORLD_STORAGE_OK)
            mc_world_runtime_execution_result_note(
                outer, forwarded, "Slime final native refusal VALUE forwarding",
                (mc_entity_tick_execution_site){"uw", "K", "()V", (int32_t)w->state.removal_pc});
    }
    return false;
}

bool mc_slime_removal_entered(mc_entity *p, mc_entity_effect_error *e) {
    return entered(p, e, NULL, NULL, NULL);
}

bool mc_slime_removal_entered_in_execution(mc_entity_tick_execution *execution,
                                           mc_world_runtime_classes *classes,
                                           mc_world_runtime_execution_code code, mc_entity *p,
                                           mc_entity_status *raw, mc_entity_effect_error *e,
                                           mc_world_runtime_execution_result *outer) {
    if (outer != NULL)
        mc_world_runtime_execution_result_init(outer);
    if (execution == NULL || execution->identity != execution || execution->math == NULL ||
        classes == NULL || raw == NULL || e == NULL || outer == NULL) {
        mc_world_runtime_execution_result_note(outer, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "Slime removal execution/code/output required",
                                               receiver_site(false));
        return false;
    }
    const removal_execution descriptor = {execution, classes, code};
    return entered(p, e, &descriptor, raw, outer);
}
