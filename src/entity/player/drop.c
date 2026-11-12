#include "mc/entity/player/drop.h"
#include "drop_execution_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../item/render/internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/math/trig_double.h"
#include "mc/entity/player/statistics.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/world_access.h"
#include "player_internal.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

struct mc_player_drop_call {
    mc_player_drop_state state;
    mc_player_drop_error error, cleanup;
    mc_registry_classes *registry;
    mc_math_classes *math;
    mc_world_runtime *world, *publication_world;
    mc_world_entity_error publication;
    mc_world_access_failure receipt;
    mc_player_statistic_error statistic;
    bool player_held, stack_held, registry_held, math_held;
    bool world_held, publication_world_held, execution_owned, owned_completed;
    bool publication_execution_returned;
    mc_player_drop_execution_context execution;
    mc_world_runtime_execution_result owned;
    mc_world_entity_insert_execution insertion;
    mc_world_storage_status publication_status;
    mc_player_drop_world_execution_view constructor_scope, publication_scope;
};

static const char *const stack_descriptor = "(Liz;Z)V";

static mc_entity_tick_execution_site execution_site(bool publication) {
    return (mc_entity_tick_execution_site){"gs", "a", publication ? "(Lhl;)V" : stack_descriptor,
                                           publication ? 1 : 10};
}

static bool scope_open(const mc_player_drop_call *c,
                       const mc_player_drop_world_execution_view *scope) {
    const mc_entity_tick_execution *execution = c->execution.execution;
    return scope->created && scope->world.owner == execution &&
           scope->world.slot < MC_ENTITY_TICK_EXECUTION_WORLDS &&
           execution->world_slots[scope->world.slot].generation == scope->world.generation &&
           execution->world_slots[scope->world.slot].phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN;
}

static bool scope_consumed(const mc_player_drop_call *c,
                           const mc_player_drop_world_execution_view *scope) {
    const mc_entity_tick_execution *execution = c->execution.execution;
    if (!scope->created || scope->world.owner != execution ||
        scope->world.slot >= MC_ENTITY_TICK_EXECUTION_WORLDS || scope->world.generation == 0 ||
        execution->world_slots[scope->world.slot].generation != scope->world.generation)
        return false;
    const mc_entity_tick_execution_world_slot_phase phase =
        execution->world_slots[scope->world.slot].phase;
    return phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE ||
           phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED;
}

static bool records_pending(const mc_world_runtime_execution_result *owned) {
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

static bool capture_world(mc_player_drop_call *c, mc_world_runtime *world,
                          mc_player_drop_world_execution_view *scope, bool publication) {
    if (!c->execution_owned)
        return true;
    const mc_world_storage_status status = mc_entity_tick_execution_push_world(
        c->execution.execution, world, execution_site(publication), &scope->world);
    mc_world_runtime_execution_result_note(&c->owned, status,
                                           "Player drop actual evaluated World capture",
                                           execution_site(publication));
    if (status != MC_WORLD_STORAGE_OK)
        return false;
    scope->created = scope->open = true;
    return true;
}

static bool close_scope(mc_player_drop_call *c, mc_player_drop_world_execution_view *scope,
                        bool publication, bool completed) {
    if (!c->execution_owned || !scope->created || scope->close_attempted)
        return completed;
    if (completed && (c->owned.ownership_status != MC_WORLD_STORAGE_OK ||
                      c->owned.selection_ready || records_pending(&c->owned))) {
        mc_world_runtime_execution_result_note(&c->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                               "Player drop completed result not quiet",
                                               execution_site(publication));
        completed = false;
    }
    /* The SAME synchronous child/body returned. Retained call/producer rights
     * remain live. Record the genuine operand before this ONE initial pop. */
    scope->locals_drained = true;
    scope->failed = !completed;
    scope->close_attempted = true;
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(c->execution.execution, &scope->world, scope->failed);
    scope->initial_close_status = status;
    scope->open = scope_open(c, scope);
    scope->consumed = scope_consumed(c, scope);
    mc_world_runtime_execution_result_note(
        &c->owned, status, "Player drop actual World checked close", execution_site(publication));
    return status == MC_WORLD_STORAGE_OK && completed;
}

static mc_world_storage_status native_refused(mc_player_drop_call *c,
                                              mc_world_storage_status status, const char *operation,
                                              bool publication) {
    mc_world_runtime_execution_result_note(&c->owned, status, operation,
                                           execution_site(publication));
    return status;
}

static mc_world_storage_status native_close_scope(mc_player_drop_call *c,
                                                  mc_player_drop_world_execution_view *scope,
                                                  bool publication) {
    if (!scope->created || scope->consumed)
        return MC_WORLD_STORAGE_OK;
    if (!scope->close_attempted || !scope->locals_drained)
        return native_refused(c, MC_WORLD_STORAGE_BUSY,
                              "Player drop actual initial close/locals required", publication);
    if (scope_consumed(c, scope)) {
        scope->open = false;
        scope->consumed = true;
        return MC_WORLD_STORAGE_OK;
    }
    if (!scope->open || !scope_open(c, scope))
        return native_refused(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Player drop recorded token not OPEN or proved consumed",
                              publication);
    mc_entity_tick_execution_world_token current;
    if (!mc_entity_tick_execution_current_world(c->execution.execution, &current))
        return native_refused(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Player drop recorded close checked current token", publication);
    if (current.owner != scope->world.owner || current.slot != scope->world.slot ||
        current.generation != scope->world.generation)
        return native_refused(c, MC_WORLD_STORAGE_BUSY,
                              "Player drop recorded close awaits exact current token", publication);
    scope->native_close_attempted = true;
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(c->execution.execution, &scope->world, scope->failed);
    scope->native_close_status = status;
    scope->open = scope_open(c, scope);
    scope->consumed = scope_consumed(c, scope);
    mc_world_runtime_execution_result_note(&c->owned, status,
                                           "Player drop recorded Native checked close",
                                           execution_site(publication));
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (!scope->consumed)
        return native_refused(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Player drop successful close lacks consumed token proof",
                              publication);
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_local_player_drop_native_close(mc_player_drop_call *c) {
    if (c == NULL || !c->execution_owned || c->execution.execution == NULL ||
        c->execution.execution->identity != c->execution.execution)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (c->state.active)
        return native_refused(c, MC_WORLD_STORAGE_BUSY, "Player drop active native close", true);
    if (records_pending(&c->owned))
        return native_refused(c, MC_WORLD_STORAGE_BUSY,
                              "Player drop close awaits explicit pending/child continuation", true);
    const mc_world_storage_status status = native_close_scope(c, &c->publication_scope, true);
    return status == MC_WORLD_STORAGE_OK ? native_close_scope(c, &c->constructor_scope, false)
                                         : status;
}

static void at(mc_player_drop_call *c, const char *method, const char *descriptor,
               unsigned int pc) {
    c->error.site = (mc_player_site){"gs", method, descriptor, pc};
    c->state.source_pc = pc;
}

static bool fail(mc_player_drop_call *c, mc_player_drop_status status, const char *operation,
                 bool native) {
    c->error.status = status;
    c->error.operation = operation;
    c->error.native_policy = native;
    return false;
}

static bool null_reference(mc_player_drop_call *c, const char *operation) {
    c->error.source_exception = true;
    c->error.exception_class = "java/lang/NullPointerException";
    return fail(c, MC_PLAYER_DROP_NULL_REFERENCE, operation, false);
}

static bool fp(mc_player_drop_call *c, unsigned int pc) {
    at(c, "a", stack_descriptor, pc);
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_PLAYER_DROP_FLOATING, "Player drop strict floating environment", true);
}

static float add(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float sub(float a, float b) {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float mul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float divide(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static double dadd(double a, double b) {
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static double dsub(double a, double b) {
    return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static double dmul(double a, double b) {
    return _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static bool trig(mc_player_drop_call *c, float angle, bool sine, unsigned int pc, float *out) {
    at(c, "a", stack_descriptor, pc);
    const mc_math_trig *table;
    if (c->execution_owned) {
        mc_math_execution *math = c->execution.execution->math;
        mc_math_execution_failure failure;
        const mc_math_class_status status = mc_math_execution_require_recorded(
            math, (mc_math_execution_site){"gs", "a", stack_descriptor, (int)pc}, sine ? "a" : "b",
            "(F)F", &failure);
        if (status != MC_MATH_CLASS_OK) {
            const mc_world_storage_status recorded =
                mc_world_runtime_execution_result_note_math(&c->owned, math, status, &failure);
            mc_world_runtime_execution_result_note(
                &c->owned, recorded, "Player drop actual Math return recording",
                (mc_entity_tick_execution_site){"gs", "a", stack_descriptor, (int)pc});
            c->error.math = failure.cause;
            return false; /* Actual Math channel; raw Player status remains independent. */
        }
        table = mc_math_classes_table(math->owned);
        if (table == NULL) {
            mc_world_runtime_execution_result_note(
                &c->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "Player drop owned actual Math table",
                (mc_entity_tick_execution_site){"gs", "a", stack_descriptor, (int)pc});
            return false;
        }
    } else {
        if (!c->math_held) {
            if (mc_math_classes_retain(c->math, &c->error.math) != MC_MATH_CLASS_OK)
                return fail(c, MC_PLAYER_DROP_MATH, c->error.math.operation, true);
            c->math_held = true;
        }
        if (mc_math_classes_ensure(c->math, &c->error.math) != MC_MATH_CLASS_OK)
            return fail(c, MC_PLAYER_DROP_MATH, c->error.math.operation, true);
        table = mc_math_classes_table(c->math);
        if (table == NULL)
            return fail(c, MC_PLAYER_DROP_MATH, "Player drop actual Math table", true);
    }
    *out = sine ? mc_math_sin(table, angle) : mc_math_cos(table, angle);
    return true;
}

static bool random_float(mc_player_drop_call *c, unsigned int pc, float *out) {
    at(c, "a", stack_descriptor, pc);
    mc_java_random *random = c->state.player->random;
    if (random == NULL)
        return null_reference(c, "Player drop actual Random null receiver");
    *out = mc_java_random_next_float(random);
    c->state.draws[c->state.random_draws++] = *out;
    return true;
}

static bool registry_failure(mc_player_drop_call *c) {
    mc_item_render_throwable kind;
    mc_item_render_registry_classification(c->registry, &c->error.registry, &kind,
                                           &c->error.exception_class);
    c->error.source_exception = kind == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    c->error.source_error = kind == MC_ITEM_RENDER_SOURCE_ERROR;
    return fail(c, MC_PLAYER_DROP_REGISTRY, c->error.registry.message,
                kind == MC_ITEM_RENDER_NO_THROWABLE);
}

static bool statistic(mc_player_drop_call *c) {
    at(c, "a", stack_descriptor, 319);
    if (!c->registry_held) {
        if (c->registry == NULL)
            return fail(c, MC_PLAYER_DROP_REGISTRY, "Player drop actual code Registry", true);
        if (mc_registry_classes_retain(c->registry, MC_REGISTRY_CLIENT,
                                       c->registry->options.random_profile,
                                       &c->error.registry) != MC_REGISTRY_OK)
            return registry_failure(c);
        c->registry_held = true;
    }
    if (mc_registry_classes_initialize_statistics(c->registry, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failure(c);
    /* JL.f..B are the actual declared static identities. v is entry16. */
    const mc_statistic *value = c->registry->statistics.named[16];
    at(c, "a", stack_descriptor, 323);
    c->error.statistic = &c->statistic;
    if (mc_local_player_add_statistic(c->state.player, value, 1, &c->statistic) !=
        MC_PLAYER_STATISTIC_OK) {
        c->error.child = c->statistic.site;
        if (c->statistic.status == MC_PLAYER_STATISTIC_NULL_REFERENCE ||
            (c->statistic.status == MC_PLAYER_STATISTIC_WRITER_FAILED &&
             c->statistic.writer.status == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
             c->statistic.writer.exception == MC_CLIENT_STATS_NULL_REFERENCE)) {
            c->error.source_exception = true;
            c->error.exception_class = "java/lang/NullPointerException";
        }
        return fail(c, MC_PLAYER_DROP_STATISTIC, c->statistic.operation,
                    !c->error.source_exception);
    }
    c->state.statistic_returned = true;
    return true;
}

static bool publication_capture(mc_player_drop_call *c) {
    if (c->receipt.receipt != NULL || c->publication.world_access_failure.receipt == NULL)
        return true;
    if (mc_world_access_failure_retain(&c->publication.world_access_failure, &c->receipt) !=
        MC_WORLD_STORAGE_OK) {
        c->cleanup.status = MC_PLAYER_DROP_CLEANUP;
        c->cleanup.operation = "Player drop exact publication callback occurrence";
        c->cleanup.native_policy = true;
        return false;
    }
    return true;
}

static bool publication_failure_protected(const mc_player_drop_call *c) {
    if (!c->execution_owned)
        return true;
    const mc_player_drop_world_execution_view *scope = &c->publication_scope;
    const mc_entity_tick_execution *execution = c->execution.execution;
    const mc_world_access_failure *protected = NULL;
    return c->insertion.observer_returned && scope->created && scope->world.owner == execution &&
           scope->world.slot < MC_ENTITY_TICK_EXECUTION_WORLDS &&
           execution->world_slots[scope->world.slot].generation == scope->world.generation &&
           mc_world_access_failure_lease_inspect(&execution->leases[scope->world.slot],
                                                 &protected) &&
           protected->receipt == c->publication.world_access_failure.receipt &&
           protected->receiver == c->publication.world_access_failure.receiver;
}

static bool publish(mc_player_drop_call *c) {
    at(c, "a", stack_descriptor, 315);
    c->error.child = (mc_player_site){"gs", "a", "(Lhl;)V", 5};
    mc_world_runtime *world = c->state.player->base.world; /* Actual fresh aI1. */
    c->publication_world = world;
    if (!capture_world(c, world, &c->publication_scope, true))
        return false;
    if (world == NULL)
        return null_reference(c, "Player drop actual publication World null receiver");
    if (!mc_world_runtime_binding_retain(world))
        return fail(c, MC_PLAYER_DROP_BUSY, "Player drop publication World occurrence", true);
    c->publication_world_held = true;
    c->state.publication_entered = true;
    c->error.publication = &c->publication;
    bool completed;
    if (c->execution_owned) {
        mc_world_entity_insert_execution_init(&c->insertion, c->execution.execution,
                                              c->execution.code_classes, c->execution.code,
                                              execution_site(true), &c->owned);
        completed = mc_world_runtime_insert_entity_in_execution(&c->insertion, world, c->state.item,
                                                                &c->state.inserted, &c->publication,
                                                                &c->publication_status);
        c->publication_execution_returned = true;
    } else {
        c->publication_status = mc_world_runtime_insert_entity(world, c->state.item,
                                                               &c->state.inserted, &c->publication);
        completed = c->publication_status == MC_WORLD_STORAGE_OK;
    }
    const mc_world_storage_status status = c->publication_status;
    if (status != MC_WORLD_STORAGE_OK) {
        fail(c, MC_PLAYER_DROP_WORLD, c->publication.operation, true);
        const char *name = NULL;
        if (publication_failure_protected(c) &&
            mc_world_access_failure_source_exception(&c->publication.world_access_failure, &name)) {
            c->error.source_exception = true;
            c->error.exception_class = name;
            c->error.native_policy = false;
        }
    } else if (completed)
        c->state.publication_returned = true;
    if (!c->execution_owned && !publication_capture(c)) {
        if (status == MC_WORLD_STORAGE_OK)
            fail(c, MC_PLAYER_DROP_LIMIT, c->cleanup.operation, true);
        return false;
    }
    /* GS's original pop ignores an ordinary false insertion result. */
    return completed;
}

static bool velocity(mc_player_drop_call *c) {
    mc_entity_base_fields *item = &c->state.item->base;
    mc_entity_base_fields *player = &c->state.player->base;
    float a, b, first, second;
    if (c->state.scatter) {
        if (!random_float(c, 58, &first) || !fp(c, 63))
            return false;
        first = mul(first, .5f);
        if (!random_float(c, 70, &second) || !fp(c, 75))
            return false;
        second = mul(mul(second, 3.1415927f), 2.0f);
        if (!trig(c, second, true, 83, &a) || !fp(c, 89))
            return false;
        item->velocity_x = (double)mul(-a, first);
        if (!trig(c, second, false, 97, &a) || !fp(c, 102))
            return false;
        item->velocity_z = (double)mul(a, first);
        item->velocity_y = .20000000298023224;
        return true;
    }
    if (!fp(c, 128))
        return false;
    a = mul(divide(player->yaw, 180.0f), 3.1415927f);
    if (!trig(c, a, true, 132, &a) || !fp(c, 142))
        return false;
    b = mul(divide(player->aT, 180.0f), 3.1415927f);
    if (!trig(c, b, false, 146, &b) || !fp(c, 149))
        return false;
    item->velocity_x = (double)mul(mul(-a, b), .3f);
    if (!fp(c, 164))
        return false;
    a = mul(divide(player->yaw, 180.0f), 3.1415927f);
    if (!trig(c, a, false, 168, &a) || !fp(c, 177))
        return false;
    b = mul(divide(player->aT, 180.0f), 3.1415927f);
    if (!trig(c, b, false, 181, &b) || !fp(c, 184))
        return false;
    item->velocity_z = (double)mul(mul(a, b), .3f);
    if (!fp(c, 199))
        return false;
    a = mul(divide(player->aT, 180.0f), 3.1415927f);
    if (!trig(c, a, true, 203, &a) || !fp(c, 209))
        return false;
    item->velocity_y = (double)add(mul(-a, .3f), .1f);
    if (!random_float(c, 225, &first) || !fp(c, 230))
        return false;
    const float angle = mul(mul(first, 3.1415927f), 2.0f);
    c->state.perturbation_angle = angle;
    if (!random_float(c, 241, &second) || !fp(c, 244))
        return false;
    const float spread = mul(.02f, second);
    c->state.perturbation_spread = spread;
    c->state.perturbation_spread_returned = true;
    if (!fp(c, 255))
        return false;
    const double cosine = mc_math_native_double_cos((double)angle);
    c->state.perturbation_cosine = cosine;
    c->state.perturbation_cosine_returned = true;
    item->velocity_x = dadd(item->velocity_x, dmul(cosine, (double)spread));
    if (!random_float(c, 275, &first) || !random_float(c, 282, &second) || !fp(c, 285))
        return false;
    item->velocity_y = dadd(item->velocity_y, (double)mul(sub(first, second), .1f));
    if (!fp(c, 302))
        return false;
    const double sine = mc_math_native_double_sin((double)angle);
    c->state.perturbation_sine = sine;
    c->state.perturbation_sine_returned = true;
    item->velocity_z = dadd(item->velocity_z, dmul(sine, (double)spread));
    return true;
}

static bool execute_stack(mc_player_drop_call *c) {
    at(c, "a", stack_descriptor, c->state.stack == NULL ? 4u : 5u);
    if (c->state.stack == NULL)
        return true;
    c->error.entity = (mc_entity_error){0};
    if (mc_item_entity_allocate(c->state.player->classes, &c->state.item, &c->error.entity) !=
        MC_ENTITY_OK)
        return fail(c, MC_PLAYER_DROP_ENTITY, c->error.entity.operation, true);
    c->state.item_allocated = true;
    at(c, "a", stack_descriptor, 10);
    mc_world_runtime *world = c->state.player->base.world;
    c->world = world;
    if (!capture_world(c, world, &c->constructor_scope, false))
        return false;
    if (world != NULL) {
        if (!mc_world_runtime_binding_retain(world))
            return fail(c, MC_PLAYER_DROP_BUSY, "Player drop constructor World occurrence", true);
        c->world_held = true;
    }
    c->state.x = c->state.player->base.x;
    if (!fp(c, 24))
        return false;
    const double lowered = dsub(c->state.player->base.y, .30000001192092896);
    /* Actual DC receiver inherits GS.w(), literal .12f, at26. */
    if (!fp(c, 30))
        return false;
    c->state.y = dadd(lowered, (double).12f);
    c->state.z = c->state.player->base.z;
    at(c, "a", stack_descriptor, 36);
    const bool constructed =
        mc_item_entity_construct(c->state.item, world, c->state.x, c->state.y, c->state.z,
                                 c->state.stack, &c->error.entity) == MC_ENTITY_OK;
    if (!constructed)
        fail(c, MC_PLAYER_DROP_ENTITY, c->error.entity.operation, true);
    else
        c->state.item_constructed = true;
    if (!close_scope(c, &c->constructor_scope, false, constructed))
        return false;
    at(c, "a", stack_descriptor, 43);
    if (mc_entity_item_pickup_delay_set(c->state.item, 40) != MC_ENTITY_OK)
        return fail(c, MC_PLAYER_DROP_ENTITY, "Player drop actual Item pickup-delay field", true);
    c->state.pickup_delay_stored = true;
    if (!velocity(c))
        return false;
    c->state.velocity_x = c->state.item->base.velocity_x;
    c->state.velocity_y = c->state.item->base.velocity_y;
    c->state.velocity_z = c->state.item->base.velocity_z;
    const bool published = publish(c);
    if (!close_scope(c, &c->publication_scope, true, published) || !statistic(c))
        return false;
    at(c, "a", stack_descriptor, 326);
    return true;
}

static bool execute(mc_player_drop_call *c) {
    if (c->state.player == NULL) {
        at(c, c->state.selected_action ? "D" : "a",
           c->state.selected_action ? "()V" : stack_descriptor, 0);
        return null_reference(c, "Player drop actual null invocation receiver");
    }
    if (c->state.selected_action) {
        at(c, "D", "()V", 2);
        mc_inventory_player *array = c->state.player->local_player->fields.player.c;
        at(c, "D", "()V", 9);
        mc_inventory_player *index = c->state.player->local_player->fields.player.c;
        if (index == NULL)
            return null_reference(c, "Player drop selected Inventory field receiver");
        mc_inventory_player_state fields;
        if (!mc_inventory_player_read_state(index, &fields))
            return fail(c, MC_PLAYER_DROP_INVENTORY, "Player drop selected Inventory fields", true);
        c->state.selected = fields.selected;
        at(c, "D", "()V", 13);
        if (array == NULL)
            return null_reference(c, "Player drop Inventory split invocation receiver");
        if (mc_inventory_player_split(array, fields.selected, 1, &c->state.stack,
                                      &c->error.inventory) != MC_INVENTORY_OK) {
            const mc_inventory_error *e = &c->error.inventory;
            if (e->frame_count != 0) {
                const mc_inventory_site *child = &e->frames[e->frame_count - 1];
                c->error.child = (mc_player_site){child->class_name, child->method,
                                                  child->descriptor, (unsigned int)child->pc};
            }
            if (e->status == MC_INVENTORY_NULL_REFERENCE ||
                e->status == MC_INVENTORY_INDEX_OUT_OF_BOUNDS) {
                c->error.source_exception = true;
                c->error.exception_class = e->status == MC_INVENTORY_NULL_REFERENCE
                                               ? "java/lang/NullPointerException"
                                               : "java/lang/ArrayIndexOutOfBoundsException";
            }
            return fail(c, MC_PLAYER_DROP_INVENTORY, e->operation, !c->error.source_exception);
        }
        c->stack_held = c->state.stack != NULL;
        c->state.inventory_split_returned = true;
    }
    if (!execute_stack(c))
        return false;
    if (c->state.selected_action)
        at(c, "D", "()V", 20);
    return true;
}

static mc_player_drop_status admission(mc_player_drop_error *out, mc_player_drop_status status,
                                       const char *operation) {
    if (out != NULL)
        *out = (mc_player_drop_error){
            .status = status, .operation = operation, .native_policy = status != MC_PLAYER_DROP_OK};
    return status;
}

static mc_player_drop_status begin(mc_registry_classes *registry, mc_math_classes *math,
                                   mc_entity *player, mc_item_stack *stack, bool selected,
                                   bool scatter, mc_player_drop_call **out,
                                   mc_player_drop_error *error,
                                   const mc_player_drop_execution_context *execution) {
    if (out == NULL || *out != NULL ||
        (player != NULL && !mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)))
        return admission(error, MC_PLAYER_DROP_ARGUMENT, "Player drop actual owner/empty output");
    mc_player_drop_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return admission(error, MC_PLAYER_DROP_ALLOCATION, "Player drop retained invocation");
    c->registry = registry;
    c->math = math;
    c->state = (mc_player_drop_state){.player = player,
                                      .stack = stack,
                                      .selected_action = selected,
                                      .scatter = scatter,
                                      .active = true,
                                      .error = &c->error,
                                      .cleanup = &c->cleanup};
    if (player != NULL && mc_entity_retain(player) != MC_ENTITY_OK) {
        free(c);
        return admission(error, MC_PLAYER_DROP_LIMIT, "Player drop Player occurrence");
    }
    c->player_held = player != NULL;
    if (stack != NULL && mc_item_stack_retain(stack) != MC_ITEM_STACK_OK) {
        mc_entity_release(player);
        free(c);
        return admission(error, MC_PLAYER_DROP_LIMIT, "Player drop Stack occurrence");
    }
    c->stack_held = stack != NULL;
    if (execution != NULL) {
        c->execution_owned = true;
        c->execution = *execution;
        mc_world_runtime_execution_result_init(&c->owned);
    }
    *out = c;
    bool completed = execute(c);
    completed = close_scope(c, &c->publication_scope, true, completed);
    completed = close_scope(c, &c->constructor_scope, false, completed);
    c->state.returned = c->owned_completed = completed;
    c->state.active = false;
    if (error != NULL)
        *error = c->error;
    return c->error.status;
}

mc_player_drop_status mc_local_player_drop_selected_begin(mc_registry_classes *r,
                                                          mc_math_classes *m, mc_entity *p,
                                                          mc_player_drop_call **out,
                                                          mc_player_drop_error *e) {
    return begin(r, m, p, NULL, true, false, out, e, NULL);
}

mc_player_drop_status mc_local_player_drop_stack_begin(mc_registry_classes *r, mc_math_classes *m,
                                                       mc_entity *p, mc_item_stack *s, bool scatter,
                                                       mc_player_drop_call **out,
                                                       mc_player_drop_error *e) {
    return begin(r, m, p, s, false, scatter, out, e, NULL);
}

static bool begin_in_execution(mc_entity_tick_execution *execution,
                               mc_world_runtime_classes *code_classes,
                               mc_world_runtime_execution_code code, mc_registry_classes *registry,
                               mc_entity *player, mc_item_stack *stack, bool selected, bool scatter,
                               mc_player_drop_call **out, mc_player_drop_status *status,
                               mc_player_drop_error *error,
                               mc_world_runtime_execution_result *owned) {
    if (owned != NULL)
        mc_world_runtime_execution_result_init(owned);
    const mc_entity_tick_execution_site site = {"gs", selected ? "D" : "a",
                                                selected ? "()V" : stack_descriptor, 0};
    if (execution == NULL || execution->identity != execution || execution->math == NULL ||
        code_classes == NULL || out == NULL || *out != NULL || status == NULL || owned == NULL) {
        mc_world_runtime_execution_result_note(owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "Player drop execution/code/outputs required", site);
        return false;
    }
    const mc_player_drop_execution_context descriptor = {execution, code_classes, code};
    *status = begin(registry, NULL, player, stack, selected, scatter, out, error, &descriptor);
    if (*out == NULL) {
        mc_world_runtime_execution_result_note(owned,
                                               *status == MC_PLAYER_DROP_ALLOCATION
                                                   ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                               "Player drop actual invocation admission", site);
        return false;
    }
    const mc_world_storage_status forwarded =
        mc_world_runtime_execution_result_forward(owned, &(*out)->owned);
    if (forwarded != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(owned, forwarded,
                                               "Player drop exact result forwarding", site);
        return false;
    }
    return (*out)->owned_completed && owned->ownership_status == MC_WORLD_STORAGE_OK;
}

bool mc_local_player_drop_selected_begin_in_execution(
    mc_entity_tick_execution *execution, mc_world_runtime_classes *code_classes,
    mc_world_runtime_execution_code code, mc_registry_classes *registry, mc_entity *player,
    mc_player_drop_call **out, mc_player_drop_status *status, mc_player_drop_error *error,
    mc_world_runtime_execution_result *owned) {
    return begin_in_execution(execution, code_classes, code, registry, player, NULL, true, false,
                              out, status, error, owned);
}

bool mc_local_player_drop_stack_begin_in_execution(
    mc_entity_tick_execution *execution, mc_world_runtime_classes *code_classes,
    mc_world_runtime_execution_code code, mc_registry_classes *registry, mc_entity *player,
    mc_item_stack *stack, bool scatter, mc_player_drop_call **out, mc_player_drop_status *status,
    mc_player_drop_error *error, mc_world_runtime_execution_result *owned) {
    return begin_in_execution(execution, code_classes, code, registry, player, stack, false,
                              scatter, out, status, error, owned);
}

bool mc_local_player_drop_execution_inspect(mc_player_drop_call *c,
                                            mc_player_drop_execution_view *out) {
    if (c == NULL || !c->execution_owned || out == NULL)
        return false;
    *out =
        (mc_player_drop_execution_view){.execution = c->execution.execution,
                                        .result = &c->owned,
                                        .publication = &c->insertion,
                                        .constructor_value = c->world,
                                        .publication_value = c->publication_world,
                                        .constructor_world = c->constructor_scope,
                                        .publication_world = c->publication_scope,
                                        .publication_status = c->publication_status,
                                        .publication_returned = c->publication_execution_returned,
                                        .completed = c->owned_completed};
    out->constructor_world.open = scope_open(c, &c->constructor_scope);
    out->publication_world.open = scope_open(c, &c->publication_scope);
    return true;
}

bool mc_local_player_drop_inspect(const mc_player_drop_call *c, mc_player_drop_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

mc_player_drop_status mc_local_player_drop_retire(mc_player_drop_call **slot,
                                                  mc_player_drop_error *error) {
    if (slot == NULL)
        return admission(error, MC_PLAYER_DROP_ARGUMENT, "Player drop retirement slot");
    mc_player_drop_call *c = *slot;
    if (c == NULL)
        return admission(error, MC_PLAYER_DROP_OK, NULL);
    if (c->state.active)
        return admission(error, MC_PLAYER_DROP_BUSY, "Player drop active invocation retirement");
    if (c->execution_owned && (records_pending(&c->owned) ||
                               (c->constructor_scope.created && !c->constructor_scope.consumed) ||
                               (c->publication_scope.created && !c->publication_scope.consumed))) {
        c->cleanup.operation = "Player drop exact owned continuations/tokens not closed";
        goto busy;
    }
    if (!c->execution_owned && !publication_capture(c))
        goto busy;
    if (c->math_held && mc_math_classes_release(&c->math, &c->cleanup.math) != MC_MATH_CLASS_OK) {
        c->cleanup.operation = "Player drop Math occurrence retirement";
        goto busy;
    }
    c->math_held = false;
    if (c->registry_held &&
        mc_registry_classes_release(c->registry, &c->cleanup.registry) != MC_REGISTRY_OK) {
        c->cleanup.operation = "Player drop Registry occurrence retirement";
        goto busy;
    }
    c->registry_held = false;
    mc_world_access_failure_release(&c->receipt);
    mc_entity_release(c->state.item);
    if (c->stack_held)
        mc_item_stack_release(c->state.stack);
    if (c->player_held)
        mc_entity_release(c->state.player);
    if (c->world_held)
        mc_world_runtime_binding_release(c->world);
    if (c->publication_world_held)
        mc_world_runtime_binding_release(c->publication_world);
    free(c);
    *slot = NULL;
    return admission(error, MC_PLAYER_DROP_OK, NULL);
busy:
    c->cleanup.status = MC_PLAYER_DROP_CLEANUP;
    c->cleanup.native_policy = true;
    if (error != NULL)
        *error = c->cleanup;
    return MC_PLAYER_DROP_CLEANUP;
}
