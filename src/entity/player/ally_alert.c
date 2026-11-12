#include "../watcher_payload_internal.h"
#include "ally_alert_execution_internal.h"
#include "ally_alert_occurrences_internal.h"

#include "../../item/render/internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../../world/runtime/world_access_internal.h"
#include "../bounds/bounds_internal.h"
#include "../mob/references_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "player_internal.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Player ally alert arithmetic requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Player ally alert arithmetic requires strict floating transformations"
#endif

typedef struct ally_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_world_runtime_execution_result *owned;
} ally_execution;

struct mc_player_ally_alert_call {
    mc_player_ally_alert_state state;
    mc_player_ally_alert_error error;
    mc_player_ally_alert_cleanup cleanup;
    mc_world_source_chunk_error source;
    mc_world_access_failure receipts[4];
    size_t receipt_count;
    bool player_held, target_held, world_held, query_failed, source_copied;
    bool execution_owned, scope_created, scope_open, query_returned, completed;
    mc_entity_tick_execution_world_token world_token;
    mc_world_storage_status query_status;
    mc_world_storage_status initial_close_status, native_close_status;
    bool close_attempted, close_locals_drained, close_failed, close_consumed;
    bool native_close_attempted;
    ally_execution owned;
};

static bool ally_scope_open(const mc_player_ally_alert_call *c) {
    if (!c->execution_owned || !c->scope_created)
        return false;
    const mc_entity_tick_execution *execution = c->owned.execution;
    return c->world_token.owner == execution &&
           c->world_token.slot < MC_ENTITY_TICK_EXECUTION_WORLDS &&
           execution->world_slots[c->world_token.slot].generation == c->world_token.generation &&
           execution->world_slots[c->world_token.slot].phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN;
}

/* A mismatched generation is NOT proof that this token was consumed. A
 * positive observation remains valid if that already-consumed slot is reused. */
static bool ally_scope_consumed(const mc_player_ally_alert_call *c) {
    if (!c->execution_owned || !c->scope_created)
        return false;
    const mc_entity_tick_execution *execution = c->owned.execution;
    if (c->world_token.owner != execution ||
        c->world_token.slot >= MC_ENTITY_TICK_EXECUTION_WORLDS || c->world_token.generation == 0 ||
        execution->world_slots[c->world_token.slot].generation != c->world_token.generation)
        return false;
    const mc_entity_tick_execution_world_slot_phase phase =
        execution->world_slots[c->world_token.slot].phase;
    return phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE ||
           phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED;
}

static bool ally_result_quiet(const mc_world_runtime_execution_result *owned) {
    if (owned->selection_ready || owned->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (owned->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (owned->closes[i].used)
            return false;
    return true;
}

static bool ally_close(mc_player_ally_alert_call *c, bool completed) {
    if (!c->execution_owned || !c->scope_created)
        return completed;
    if (completed && (c->owned.owned->ownership_status != MC_WORLD_STORAGE_OK ||
                      !ally_result_quiet(c->owned.owned))) {
        mc_world_runtime_execution_result_note(
            c->owned.owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "Player ally alert completed result not quiet",
            (mc_entity_tick_execution_site){"gs", "a", "(Lls;Z)V", 65});
        completed = false;
    }
    /* The SAME body returned: any reached query/Math/World calls and their
     * ordinary temporary calls have drained. Player/target/World/bounds/List
     * and producer rights intentionally remain owned by this retained call.
     * Record the actual original operand before the ONE initial checked pop. */
    c->close_locals_drained = true;
    c->close_failed = !completed;
    c->close_attempted = true;
    const mc_world_storage_status closed =
        mc_entity_tick_execution_pop_world(c->owned.execution, &c->world_token, c->close_failed);
    c->initial_close_status = closed;
    c->scope_open = ally_scope_open(c);
    c->close_consumed = ally_scope_consumed(c);
    if (closed != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(
            c->owned.owned, closed, "Player ally alert actual World checked close",
            (mc_entity_tick_execution_site){"gs", "a", "(Lls;Z)V", 65});
        return false;
    }
    return completed;
}

static mc_world_storage_status ally_native_close_refused(mc_player_ally_alert_call *c,
                                                         mc_world_storage_status status,
                                                         const char *operation) {
    mc_world_runtime_execution_result_note(
        c->owned.owned, status, operation,
        (mc_entity_tick_execution_site){"gs", "a", "(Lls;Z)V", 65});
    return status;
}

static bool ally_native_close_records_pending(const mc_world_runtime_execution_result *owned) {
    if (owned->root_pending)
        return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (owned->pending[i].used)
            return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (owned->closes[i].used)
            return true;
    return false;
}

mc_world_storage_status mc_player_ally_alert_native_close(mc_player_ally_alert_call *c) {
    if (c == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (!c->execution_owned || c->owned.execution == NULL ||
        c->owned.execution->identity != c->owned.execution || c->owned.owned == NULL ||
        c->world_token.owner != c->owned.execution ||
        c->world_token.slot >= MC_ENTITY_TICK_EXECUTION_WORLDS || c->world_token.generation == 0 ||
        !c->scope_created || !c->close_attempted)
        return ally_native_close_refused(c, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                         "Player ally alert actual recorded close/call required");
    if (c->state.active || !c->close_locals_drained)
        return ally_native_close_refused(c, MC_WORLD_STORAGE_BUSY,
                                         "Player ally alert actual locals not drained");

    /* No repeat pop after positive consumption, including a consumed non-OK
     * initial/Native close. This is only this token's completion, not proof
     * that Source, a result ledger, Math or the entire context is quiescent. */
    if (c->close_consumed)
        return MC_WORLD_STORAGE_OK;
    if (ally_scope_consumed(c)) {
        c->scope_open = false;
        c->close_consumed = true;
        return MC_WORLD_STORAGE_OK;
    }
    if (!c->scope_open || !ally_scope_open(c))
        return ally_native_close_refused(
            c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "Player ally alert recorded token is neither matching OPEN nor proved consumed");

    mc_entity_tick_execution_world_token current;
    if (!mc_entity_tick_execution_current_world(c->owned.execution, &current))
        return ally_native_close_refused(
            c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "Player ally alert checked current World for recorded close");
    if (current.owner != c->world_token.owner || current.slot != c->world_token.slot ||
        current.generation != c->world_token.generation)
        return ally_native_close_refused(
            c, MC_WORLD_STORAGE_BUSY,
            "Player ally alert recorded close awaits exact current token");
    if (ally_native_close_records_pending(c->owned.owned))
        return ally_native_close_refused(
            c, MC_WORLD_STORAGE_BUSY,
            "Player ally alert recorded close awaits explicit pending/child close completion");

    c->native_close_attempted = true;
    const mc_world_storage_status closed =
        mc_entity_tick_execution_pop_world(c->owned.execution, &c->world_token, c->close_failed);
    mc_world_runtime_execution_result_note(
        c->owned.owned, closed, "Player ally alert recorded Native checked close",
        (mc_entity_tick_execution_site){"gs", "a", "(Lls;Z)V", 65});
    c->native_close_status = closed;
    c->scope_open = ally_scope_open(c);
    c->close_consumed = ally_scope_consumed(c);
    if (closed != MC_WORLD_STORAGE_OK)
        return closed;
    if (!c->close_consumed)
        return ally_native_close_refused(
            c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "Player ally alert successful Native close lacks exact consumed token proof");
    return MC_WORLD_STORAGE_OK;
}

static void at(mc_player_ally_alert_call *c, unsigned int pc) {
    c->state.source_pc = pc;
    c->error.site = (mc_player_site){"gs", "a", "(Lls;Z)V", pc};
    c->error.child = (mc_player_site){0};
}

static void child(mc_player_ally_alert_call *c, const char *cl, const char *m, const char *d,
                  unsigned int pc) {
    c->error.child = (mc_player_site){cl, m, d, pc};
}

static bool fail(mc_player_ally_alert_call *c, mc_entity_status status, const char *operation) {
    if (c->error.status == MC_ENTITY_OK) {
        c->error.status = status;
        c->error.operation = operation;
        c->error.native_policy = !c->error.source_exception && !c->error.source_error;
    }
    return false;
}

static bool source_exception(mc_player_ally_alert_call *c, const char *name,
                             const char *operation) {
    c->error.source_exception = true;
    c->error.exception_class = name;
    return fail(c, MC_ENTITY_STATE_ERROR, operation);
}

static bool null_reference(mc_player_ally_alert_call *c, const char *operation) {
    return source_exception(c, "java/lang/NullPointerException", operation);
}

static bool fp(mc_player_ally_alert_call *c) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_ENTITY_UNSUPPORTED_FP,
                c->state.attack ? "Player attack strict floating environment"
                                : "Player ally alert strict floating environment");
}

static bool instance(mc_player_ally_alert_call *c, mc_entity *entity, mc_entity_class_kind kind,
                     bool *out) {
    *out = false;
    if (entity == NULL)
        return true;
    const mc_entity_class_literal *literal = NULL;
    if (mc_entity_class_literal_get(c->execution_owned ? c->owned.code_classes->entities
                                                       : c->state.player->classes,
                                    kind, &literal) != MC_ENTITY_OK ||
        mc_entity_class_literal_is_assignable_from(literal, entity->class_literal, out) !=
            MC_ENTITY_OK)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player attack actual Class identity");
    return true;
}

static mc_entity_watch_record *watch(mc_player_ally_alert_call *c, mc_entity *wolf, int32_t key,
                                     mc_entity_watch_payload_kind expected, bool setter) {
    if (wolf->watcher == NULL) {
        (void)null_reference(c, "Player ally alert null Wolf watcher/Map receiver");
        return NULL;
    }
    const char *method = setter ? "b" : expected == MC_ENTITY_WATCH_PAYLOAD_STRING ? "c" : "a";
    const char *descriptor = setter                                       ? "(ILjava/lang/Object;)V"
                             : expected == MC_ENTITY_WATCH_PAYLOAD_STRING ? "(I)Ljava/lang/String;"
                                                                          : "(I)B";
    child(c, "ud", method, descriptor, 8);
    if (wolf->watcher->map == NULL) {
        (void)null_reference(c, "Player ally alert null Wolf watcher/Map receiver");
        return NULL;
    }
    size_t seen = 0;
    for (mc_entity_watch_node *n = wolf->watcher->map->head; n != NULL; n = n->next) {
        if (seen++ >= wolf->watcher->map->size) {
            (void)fail(c, MC_ENTITY_STATE_ERROR, "Player ally alert watcher Map ownership");
            return NULL;
        }
        if (n->key != key)
            continue;
        child(c, "ud", method, descriptor, setter ? 19 : 16);
        if (n->record == NULL) {
            (void)null_reference(c, "Player ally alert null Wolf watcher record receiver");
            return NULL;
        }
        if (setter) {
            child(c, "ud", method, descriptor, 22);
            if (!mc_entity_watch_payload_known(n->record)) {
                (void)fail(c, MC_ENTITY_STATE_ERROR,
                           "Player ally alert unrepresented old watcher Object");
                return NULL;
            }
        } else {
            const mc_entity_watch_payload_result result =
                mc_entity_watch_payload_check(n->record, expected);
            if (result != MC_ENTITY_WATCH_PAYLOAD_OK) {
                child(c, "ud", method, descriptor,
                      result == MC_ENTITY_WATCH_PAYLOAD_NULL_UNBOX ? 22 : 19);
                if (result == MC_ENTITY_WATCH_PAYLOAD_NULL_UNBOX)
                    (void)null_reference(c, "Player ally alert null Wolf watcher unbox receiver");
                else if (result == MC_ENTITY_WATCH_PAYLOAD_WRONG_CLASS)
                    (void)source_exception(c, "java/lang/ClassCastException",
                                           "Player ally alert typed Wolf watcher value");
                else
                    (void)fail(c, MC_ENTITY_STATE_ERROR,
                               "Player ally alert unrepresented watcher Object");
                return NULL;
            }
        }
        return n->record;
    }
    child(c, "ud", method, descriptor, setter ? 19 : 16);
    (void)null_reference(c, "Player ally alert absent Wolf watcher record receiver");
    return NULL;
}

static bool flag(mc_player_ally_alert_call *c, mc_entity *wolf, const char *method, int mask,
                 bool *out) {
    child(c, "gi", method, "()Z", 6);
    mc_entity_watch_record *r = watch(c, wolf, 16, MC_ENTITY_WATCH_PAYLOAD_BYTE, false);
    if (r == NULL)
        return false;
    *out = (r->byte_value & mask) != 0;
    return true;
}

static bool owner(mc_player_ally_alert_call *c, mc_entity *wolf, bool *out) {
    const mc_player_string *name = c->state.player->local_player->fields.player.l;
    child(c, "gi", "A", "()Ljava/lang/String;", 6);
    mc_entity_watch_record *r = watch(c, wolf, 17, MC_ENTITY_WATCH_PAYLOAD_STRING, false);
    if (r == NULL)
        return false;
    child(c, "java/lang/String", "equals", "(Ljava/lang/Object;)Z", 0);
    if (name == NULL)
        return null_reference(c, "Player ally alert null name equals receiver");
    const mc_player_string *owner_value = mc_entity_watch_string_value(r);
    mc_player_string_view a = {0}, b = {0};
    if (!mc_player_string_read(name, &a) ||
        (owner_value != NULL && !mc_player_string_read(owner_value, &b)))
        return fail(c, MC_ENTITY_STATE_ERROR, "Player ally alert actual String ownership");
    *out = owner_value != NULL && a.length == b.length &&
           (a.length == 0 || memcmp(a.units, b.units, a.length * sizeof(*a.units)) == 0);
    return true;
}

static bool capture_query(mc_player_ally_alert_call *c) {
    if (c->execution_owned)
        return true; /* Actual owned lower captured before projection/release. */
    if (!c->query_failed)
        return true;
    mc_world_nearby_error *e = &c->error.nearby;
    if (e->query.source_chunk != NULL && !c->source_copied) {
        c->source = *e->query.source_chunk;
        c->source_copied = true;
        e->query.source_chunk = &c->source;
    }
    const mc_world_access_failure *receipts[] = {
        &e->batch.publication.world_access_failure, &e->tile_entity.world_access_failure,
        &e->tile_entity.publication.world_access_failure,
        &e->tile_entity.batch.publication.world_access_failure};
    for (size_t i = 0; i < sizeof(receipts) / sizeof(*receipts); ++i) {
        if (receipts[i]->receipt == NULL)
            continue;
        bool held = false;
        for (size_t j = 0; j < c->receipt_count; ++j)
            if (c->receipts[j].receipt == receipts[i]->receipt)
                held = true;
        if (held)
            continue;
        if (c->receipt_count == sizeof(c->receipts) / sizeof(*c->receipts)) {
            c->cleanup.status = MC_ENTITY_LIMIT;
            c->cleanup.operation = "Player ally alert WorldAccess receipt extent";
            c->cleanup.world = MC_WORLD_STORAGE_LIMIT;
            return false;
        }
        const mc_world_storage_status status =
            mc_world_access_failure_retain(receipts[i], &c->receipts[c->receipt_count]);
        if (status != MC_WORLD_STORAGE_OK) {
            c->cleanup.status =
                status == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR;
            c->cleanup.operation = "Player ally alert WorldAccess receipt capture";
            c->cleanup.world = status;
            return false;
        }
        ++c->receipt_count;
    }
    return true;
}

static void classify_query(mc_player_ally_alert_call *c) {
    const mc_world_nearby_error *e = &c->error.nearby;
    const char *exception = NULL;
    if (e->query.registry.status != MC_REGISTRY_OK &&
        mc_world_runtime_registry_status(e->query.registry.status) == e->query.status) {
        mc_item_render_throwable kind;
        mc_item_render_registry_classification(mc_world_runtime_registry(c->state.world),
                                               &e->query.registry, &kind, &exception);
        if (kind != MC_ITEM_RENDER_NO_THROWABLE) {
            c->error.source_exception = kind == MC_ITEM_RENDER_SOURCE_EXCEPTION;
            c->error.source_error = kind == MC_ITEM_RENDER_SOURCE_ERROR;
        }
    }
    if (c->source_copied && c->source.storage.status == e->query.status &&
        c->source.storage.operation == e->query.operation &&
        c->source.source.status != MC_WORLD_SOURCE_OK && c->source.source.source_exception &&
        !c->source.source.native_policy && c->source.source.exception_class != NULL)
        exception = c->source.source.exception_class;

    const struct {
        const mc_world_access_failure *receipt;
        mc_world_storage_status status;
        const char *operation;
    } receipts[] = {
        {&e->batch.publication.world_access_failure, e->batch.publication.status,
         e->batch.publication.operation},
        {&e->tile_entity.world_access_failure, e->tile_entity.status, e->tile_entity.operation},
        {&e->tile_entity.publication.world_access_failure, e->tile_entity.publication.status,
         e->tile_entity.publication.operation},
        {&e->tile_entity.batch.publication.world_access_failure,
         e->tile_entity.batch.publication.status, e->tile_entity.batch.publication.operation}};

    for (size_t i = 0; exception == NULL && i < sizeof(receipts) / sizeof(*receipts); ++i)
        if (receipts[i].status == e->query.status && receipts[i].operation == e->query.operation)
            (void)mc_world_access_failure_source_exception(receipts[i].receipt, &exception);
    if (exception != NULL) {
        c->error.exception_class = exception;
        c->error.native_policy = false;
        const size_t length = strlen(exception);
        c->error.source_error = length >= 5 && strcmp(exception + length - 5, "Error") == 0;
        c->error.source_exception = !c->error.source_error;
    }
}

static bool returned(mc_player_ally_alert_call *c, unsigned int pc) {
    at(c, pc);
    c->state.returned = true;
    return true;
}

static bool body(mc_player_ally_alert_call *c) {
    mc_entity *p = c->state.player, *target = c->state.target;
    at(c, 0);
    if (p == NULL)
        return null_reference(c, "Player ally alert null Player invocation receiver");
    bool excluded;
    if (!instance(c, target, MC_ENTITY_CLASS_KIND_CREEPER, &excluded))
        return false;
    if (excluded)
        return returned(c, 14);
    at(c, 7);
    if (!instance(c, target, MC_ENTITY_CLASS_KIND_GHAST, &excluded))
        return false;
    if (excluded)
        return returned(c, 14);
    at(c, 15);
    if (mc_entity_is_kind(target, MC_ENTITY_CLASS_KIND_WOLF)) {
        bool tame, same;
        at(c, 28);
        if (!flag(c, target, "D", 4, &tame))
            return false;
        if (tame) {
            at(c, 42);
            if (!owner(c, target, &same))
                return false;
            if (same)
                return returned(c, 48);
        }
    }
    at(c, 49);
    bool player;
    if (!instance(c, target, MC_ENTITY_CLASS_KIND_PLAYER, &player))
        return false;
    /* Canonical GS.F is the actual literal false body at the reached call57. */
    if (player)
        return returned(c, 63);
    at(c, 65);
    c->state.world = p->base.world; /* Captured field65 precedes all box work. */
    if (c->execution_owned) {
        const mc_world_storage_status captured = mc_entity_tick_execution_push_world(
            c->owned.execution, c->state.world,
            (mc_entity_tick_execution_site){"gs", "a", "(Lls;Z)V", 65}, &c->world_token);
        if (captured != MC_WORLD_STORAGE_OK) {
            mc_world_runtime_execution_result_note(
                c->owned.owned, captured, "Player ally alert evaluated World ownership",
                (mc_entity_tick_execution_site){"gs", "a", "(Lls;Z)V", 65});
            return false;
        }
        c->scope_created = c->scope_open = true;
    }
    if (c->state.world != NULL) {
        if (!mc_world_runtime_binding_retain(c->state.world))
            return fail(c, MC_ENTITY_BUSY, "Player ally alert actual World occurrence");
        c->world_held = true;
    }
    at(c, 68);
    const mc_entity_class_literal *wolf_class = NULL;
    if (mc_entity_class_literal_get(c->execution_owned ? c->owned.code_classes->entities
                                                       : p->classes,
                                    MC_ENTITY_CLASS_KIND_WOLF, &wolf_class) != MC_ENTITY_OK)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player ally alert actual Wolf Class");
    at(c, 100);
    const double x0 = p->base.x, y0 = p->base.y, z0 = p->base.z;
    if (!fp(c))
        return false;
    const double x1 = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(p->base.x), _mm_set_sd(1.0)));
    const double y1 = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(p->base.y), _mm_set_sd(1.0)));
    const double z1 = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(p->base.z), _mm_set_sd(1.0)));
    mc_entity_bounds *bounds = NULL;
    mc_entity_status status =
        mc_entity_bounds_pool_get(c->execution_owned ? c->owned.code_classes->entities : p->classes,
                                  x0, y0, z0, x1, y1, z1, &bounds, &c->error.bounds);
    c->state.bounds = bounds;
    if (status != MC_ENTITY_OK)
        return fail(c, status, "Player ally alert actual pooled bounds");
    at(c, 112);
    mc_entity_bounds *expanded = NULL;
    status =
        mc_entity_bounds_expand(c->execution_owned ? c->owned.code_classes->entities : p->classes,
                                bounds, 16.0, 4.0, 16.0, &expanded, &c->error.bounds);
    c->state.expanded = expanded;
    if (status != MC_ENTITY_OK)
        return fail(c, status, "Player ally alert actual bounds expansion");
    at(c, 115);
    if (c->state.world == NULL)
        return null_reference(c, "Player ally alert null captured World receiver");
    mc_world_class_entity_list *wolves = NULL;
    bool queried;
    if (c->execution_owned) {
        queried = mc_world_runtime_class_entities_in_execution(
            c->owned.execution, c->state.world, c->owned.code, wolf_class, expanded, &wolves,
            &c->error.nearby, &c->query_status, c->owned.owned);
        c->query_returned = true; /* Exact C entry return, never a Source-return witness. */
    } else {
        c->query_status = mc_world_runtime_class_entities(c->state.world, wolf_class, expanded,
                                                          &wolves, &c->error.nearby);
        queried = c->query_status == MC_WORLD_STORAGE_OK;
    }
    c->state.wolves = wolves;
    if (!queried) {
        c->query_failed = true;
        if (c->execution_owned && c->owned.owned->ownership_status != MC_WORLD_STORAGE_OK)
            return false; /* Exact lower/Native channels survive without invented Source cause. */
        (void)fail(c, MC_ENTITY_WORLD_FAILED, "Player ally alert actual Wolf query");
        if (c->execution_owned) {
            /* Exact current nearby/Math Source classification is a parent join.
             * Keep its typed frame, do not label an unproved cause Native. */
            c->error.native_policy = false;
        } else {
            (void)capture_query(c);
            classify_query(c);
        }
        return false;
    }
    at(c, 120);
    size_t count;
    if (mc_world_class_entity_list_count(wolves, &count) != MC_WORLD_STORAGE_OK)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player ally alert actual List iterator");
    for (size_t i = 0; i < count; ++i) {
        mc_entity *wolf = NULL, *present = NULL;
        at(c, 139);
        if (mc_world_class_entity_list_get(wolves, i, &wolf) != MC_WORLD_STORAGE_OK ||
            !mc_entity_is_kind(wolf, MC_ENTITY_CLASS_KIND_WOLF))
            return fail(c, MC_ENTITY_STATE_ERROR, "Player ally alert actual Wolf List occurrence");
        if (c->state.wolves_examined != SIZE_MAX)
            ++c->state.wolves_examined;
        bool tame, same;
        at(c, 158);
        if (!flag(c, wolf, "D", 4, &tame))
            return false;
        if (!tame)
            continue;
        at(c, 166);
        child(c, "ii", "G", "()Lsn;", 1);
        if (mc_mob_creature_target_get(wolf, &present, &c->error.entity) != MC_ENTITY_OK)
            return fail(c, MC_ENTITY_STATE_ERROR, "Player ally alert actual Wolf target read");
        if (present != NULL)
            continue;
        at(c, 181);
        if (!owner(c, wolf, &same))
            return false;
        if (!same)
            continue;
        at(c, 187);
        if (c->state.attack) {
            bool sitting;
            at(c, 193);
            if (!flag(c, wolf, "B", 1, &sitting))
                return false;
            if (sitting)
                continue;
        }
        at(c, 202);
        child(c, "gi", "b", "(Z)V", 6);
        mc_entity_watch_record *old = watch(c, wolf, 16, MC_ENTITY_WATCH_PAYLOAD_BYTE, false);
        if (old == NULL)
            return false;
        const int8_t value = mc_java_i8_narrow(old->byte_value & -2);
        child(c, "gi", "b", "(Z)V", 46);
        mc_entity_watch_record *fresh = watch(c, wolf, 16, MC_ENTITY_WATCH_PAYLOAD_BYTE, true);
        if (fresh == NULL)
            return false;
        if (mc_entity_watch_byte_replace(fresh, value))
            wolf->watcher->dirty = true;
        at(c, 208);
        child(c, "ii", "c", "(Lsn;)V", 2);
        status = mc_mob_creature_target_store(wolf, target, &c->error.entity);
        if (status != MC_ENTITY_OK)
            return fail(c, status, "Player ally alert actual Wolf owning target store");
        if (c->state.wolves_alerted != SIZE_MAX)
            ++c->state.wolves_alerted;
    }
    return returned(c, 214);
}

static mc_entity_status ally_alert_begin(mc_entity *player, mc_entity *target, bool attack,
                                         mc_player_ally_alert_call **out,
                                         const ally_execution *owned) {
    if (out == NULL || *out != NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (player != NULL && (player->classes == NULL || player->classes->source != MC_ENTITY_CLIENT ||
                           !mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
                           player->local_player == NULL))
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_player_ally_alert_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return MC_ENTITY_ALLOCATION_FAILED;
    mc_entity_status status = MC_ENTITY_OK;
    if (player != NULL) {
        status = mc_entity_retain(player);
        c->player_held = status == MC_ENTITY_OK;
    }
    if (status == MC_ENTITY_OK && player != NULL && target != NULL) {
        status = mc_entity_retain(target);
        c->target_held = status == MC_ENTITY_OK;
    }
    if (status != MC_ENTITY_OK) {
        if (c->player_held)
            mc_entity_release(player);
        free(c);
        return status;
    }
    c->state = (mc_player_ally_alert_state){.player = player,
                                            .target = target,
                                            .attack = attack,
                                            .active = true,
                                            .error = &c->error,
                                            .cleanup = &c->cleanup};
    mc_world_nearby_error_init(&c->error.nearby);
    if (owned != NULL) {
        c->execution_owned = true;
        c->owned = *owned;
    }
    *out = c;
    const bool returned = body(c);
    c->completed = ally_close(c, returned);
    c->state.active = false;
    return c->error.status;
}

mc_entity_status mc_player_ally_alert_begin(mc_entity *player, mc_entity *target, bool attack,
                                            mc_player_ally_alert_call **out) {
    return ally_alert_begin(player, target, attack, out, NULL);
}

bool mc_player_ally_alert_begin_in_execution(mc_entity_tick_execution *execution,
                                             mc_world_runtime_classes *code_classes,
                                             mc_world_runtime_execution_code code,
                                             mc_entity *player, mc_entity *target, bool attack,
                                             mc_player_ally_alert_call **out,
                                             mc_entity_status *status,
                                             mc_world_runtime_execution_result *owned) {
    if (owned != NULL)
        mc_world_runtime_execution_result_init(owned);
    const mc_entity_tick_execution_site site = {"gs", "a", "(Lls;Z)V", 0};
    if (execution == NULL || execution->identity != execution || execution->math == NULL ||
        code_classes == NULL || owned == NULL || out == NULL || *out != NULL || status == NULL) {
        mc_world_runtime_execution_result_note(owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "Player ally alert execution/code/outputs required",
                                               site);
        return false;
    }
    const ally_execution descriptor = {execution, code_classes, code, owned};
    *status = ally_alert_begin(player, target, attack, out, &descriptor);
    if (*out == NULL) {
        mc_world_runtime_execution_result_note(
            owned,
            *status == MC_ENTITY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "Player ally alert actual invocation admission", site);
        return false;
    }
    return (*out)->completed && owned->ownership_status == MC_WORLD_STORAGE_OK;
}

bool mc_player_ally_alert_execution_inspect(const mc_player_ally_alert_call *c,
                                            mc_player_ally_alert_execution_view *out) {
    if (c == NULL || !c->execution_owned || out == NULL)
        return false;
    *out = (mc_player_ally_alert_execution_view){c->owned.execution,
                                                 c->owned.owned,
                                                 c->world_token,
                                                 c->scope_created,
                                                 ally_scope_open(c),
                                                 c->query_returned,
                                                 c->completed,
                                                 c->query_status,
                                                 c->close_attempted,
                                                 c->close_locals_drained,
                                                 c->close_failed,
                                                 c->close_consumed,
                                                 c->native_close_attempted,
                                                 c->initial_close_status,
                                                 c->native_close_status};
    return true;
}

bool mc_player_ally_alert_inspect(const mc_player_ally_alert_call *c,
                                  mc_player_ally_alert_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

static mc_entity_status cleanup_failed(mc_player_ally_alert_call *c, mc_entity_status status,
                                       const char *operation, mc_player_ally_alert_cleanup *out) {
    c->cleanup.status = status;
    c->cleanup.operation = operation;
    if (out != NULL)
        *out = c->cleanup;
    return status;
}

mc_entity_status mc_player_ally_alert_retire(mc_player_ally_alert_call **slot,
                                             mc_player_ally_alert_cleanup *out) {
    if (slot == NULL) {
        if (out != NULL)
            *out = (mc_player_ally_alert_cleanup){MC_ENTITY_INVALID_ARGUMENT,
                                                  "Player ally alert retirement slot", 0};
        return MC_ENTITY_INVALID_ARGUMENT;
    }
    mc_player_ally_alert_call *c = *slot;
    if (c == NULL) {
        if (out != NULL)
            *out = (mc_player_ally_alert_cleanup){0};
        return MC_ENTITY_OK;
    }
    c->cleanup = (mc_player_ally_alert_cleanup){0};
    if (c->state.active)
        return cleanup_failed(c, MC_ENTITY_BUSY, "Player ally alert active retirement", out);
    if (c->execution_owned && ally_scope_open(c))
        return cleanup_failed(c, MC_ENTITY_BUSY, "Player ally alert actual owned World still open",
                              out);
    if (!capture_query(c)) {
        if (out != NULL)
            *out = c->cleanup;
        return c->cleanup.status;
    }
    if (!mc_player_ally_alert_occurrences_valid(&c->state, c->player_held, c->target_held) ||
        (c->world_held && c->state.world->persistent_bindings == 0))
        return cleanup_failed(c, MC_ENTITY_STATE_ERROR,
                              "Player ally alert retained owner occurrence metadata", out);
    mc_entity_classes *classes = c->execution_owned        ? c->owned.code_classes->entities
                                 : c->state.player == NULL ? NULL
                                                           : c->state.player->classes;
    const mc_entity_bounds *boxes[] = {c->state.bounds, c->state.expanded};
    const size_t box_count = (size_t)(boxes[0] != NULL) + (size_t)(boxes[1] != NULL);
    for (size_t i = 0; i < sizeof(boxes) / sizeof(*boxes); ++i)
        if (boxes[i] != NULL && (mc_entity_bounds_native_holds(classes, boxes[i]) <
                                     1u + (size_t)(boxes[0] == boxes[1]) ||
                                 classes->bounds_holds < box_count))
            return cleanup_failed(c, MC_ENTITY_STATE_ERROR,
                                  "Player ally alert retained bounds occurrence metadata", out);
    for (size_t i = 0; i < c->receipt_count; ++i)
        if (c->receipts[i].receipt->holds < 2 ||
            c->receipts[i].receipt->receiver != c->receipts[i].receiver ||
            c->receipts[i].receipt->render_global != c->receipts[i].render_global ||
            c->receipts[i].receiver->receipt_holds == 0)
            return cleanup_failed(c, MC_ENTITY_STATE_ERROR,
                                  "Player ally alert retained callback receipt metadata", out);
    mc_world_class_entity_list_release((mc_world_class_entity_list *)c->state.wolves);
    if (c->state.expanded != NULL)
        (void)mc_entity_bounds_release(classes, (mc_entity_bounds *)c->state.expanded);
    if (c->state.bounds != NULL)
        (void)mc_entity_bounds_release(classes, (mc_entity_bounds *)c->state.bounds);
    for (size_t i = 0; i < c->receipt_count; ++i)
        mc_world_access_failure_release(&c->receipts[i]);
    if (c->world_held)
        mc_world_runtime_binding_release(c->state.world);
    /* The slot may belong to the very Player whose final held occurrence is
     * about to be released. End that backedge before either Entity can die. */
    *slot = NULL;
    if (c->target_held)
        mc_entity_release(c->state.target);
    if (c->player_held)
        mc_entity_release(c->state.player);
    free(c);
    if (out != NULL)
        *out = (mc_player_ally_alert_cleanup){0};
    return MC_ENTITY_OK;
}
