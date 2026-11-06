#include "mc/entity/player/drop.h"

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
};

static const char *const stack_descriptor = "(Liz;Z)V";

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
    if (!c->math_held) {
        if (mc_math_classes_retain(c->math, &c->error.math) != MC_MATH_CLASS_OK)
            return fail(c, MC_PLAYER_DROP_MATH, c->error.math.operation, true);
        c->math_held = true;
    }
    if (mc_math_classes_ensure(c->math, &c->error.math) != MC_MATH_CLASS_OK)
        return fail(c, MC_PLAYER_DROP_MATH, c->error.math.operation, true);
    const mc_math_trig *table = mc_math_classes_table(c->math);
    if (table == NULL)
        return fail(c, MC_PLAYER_DROP_MATH, "Player drop actual Math table", true);
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

static bool publish(mc_player_drop_call *c) {
    at(c, "a", stack_descriptor, 315);
    c->error.child = (mc_player_site){"gs", "a", "(Lhl;)V", 5};
    mc_world_runtime *world = c->state.player->base.world; /* Actual fresh aI1. */
    if (world == NULL)
        return null_reference(c, "Player drop actual publication World null receiver");
    if (!mc_world_runtime_binding_retain(world))
        return fail(c, MC_PLAYER_DROP_BUSY, "Player drop publication World occurrence", true);
    c->publication_world = world;
    c->state.publication_entered = true;
    c->error.publication = &c->publication;
    const mc_world_storage_status status =
        mc_world_runtime_insert_entity(world, c->state.item, &c->state.inserted, &c->publication);
    if (status != MC_WORLD_STORAGE_OK) {
        fail(c, MC_PLAYER_DROP_WORLD, c->publication.operation, true);
        const char *name = NULL;
        if (mc_world_access_failure_source_exception(&c->publication.world_access_failure, &name)) {
            c->error.source_exception = true;
            c->error.exception_class = name;
            c->error.native_policy = false;
        }
    } else
        c->state.publication_returned = true;
    if (!publication_capture(c)) {
        if (status == MC_WORLD_STORAGE_OK)
            fail(c, MC_PLAYER_DROP_LIMIT, c->cleanup.operation, true);
        return false;
    }
    /* GS's original pop ignores an ordinary false insertion result. */
    return status == MC_WORLD_STORAGE_OK;
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
    if (world != NULL) {
        if (!mc_world_runtime_binding_retain(world))
            return fail(c, MC_PLAYER_DROP_BUSY, "Player drop constructor World occurrence", true);
        c->world = world;
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
    if (mc_item_entity_construct(c->state.item, world, c->state.x, c->state.y, c->state.z,
                                 c->state.stack, &c->error.entity) != MC_ENTITY_OK)
        return fail(c, MC_PLAYER_DROP_ENTITY, c->error.entity.operation, true);
    c->state.item_constructed = true;
    at(c, "a", stack_descriptor, 43);
    if (mc_entity_item_pickup_delay_set(c->state.item, 40) != MC_ENTITY_OK)
        return fail(c, MC_PLAYER_DROP_ENTITY, "Player drop actual Item pickup-delay field", true);
    c->state.pickup_delay_stored = true;
    if (!velocity(c))
        return false;
    c->state.velocity_x = c->state.item->base.velocity_x;
    c->state.velocity_y = c->state.item->base.velocity_y;
    c->state.velocity_z = c->state.item->base.velocity_z;
    if (!publish(c) || !statistic(c))
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
                                   mc_player_drop_error *error) {
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
    *out = c;
    c->state.returned = execute(c);
    c->state.active = false;
    if (error != NULL)
        *error = c->error;
    return c->error.status;
}

mc_player_drop_status mc_local_player_drop_selected_begin(mc_registry_classes *r,
                                                          mc_math_classes *m, mc_entity *p,
                                                          mc_player_drop_call **out,
                                                          mc_player_drop_error *e) {
    return begin(r, m, p, NULL, true, false, out, e);
}

mc_player_drop_status mc_local_player_drop_stack_begin(mc_registry_classes *r, mc_math_classes *m,
                                                       mc_entity *p, mc_item_stack *s, bool scatter,
                                                       mc_player_drop_call **out,
                                                       mc_player_drop_error *e) {
    return begin(r, m, p, s, false, scatter, out, e);
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
    if (!publication_capture(c))
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
    mc_world_runtime_binding_release(c->world);
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
