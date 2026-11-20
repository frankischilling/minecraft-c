#include "mc/entity/player/attack.h"
#include "mc/entity/player/ally_alert.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../inventory/internal.h"
#include "../../inventory/player/source_internal.h"
#include "../../item/render/internal.h"
#include "../../item/stack/source_internal.h"
#include "../living/fields_internal.h"
#include "../mob/internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/item/registry/runtime.h"
#include "player_internal.h"
#include "statistics_status_internal.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

struct mc_player_attack_call {
    mc_player_attack_state state;
    mc_player_attack_error error;
    mc_registry_classes *registry;
    mc_math_classes *math;
    mc_item_stack *first_stack, *fresh_stack;
    mc_player_ally_alert_call *alert;
    mc_player_ally_alert_cleanup alert_cleanup;
    bool player_held, target_held, registry_held, damage_entered;
};

static void at(mc_player_attack_call *c, unsigned int pc) {
    c->state.source_pc = pc;
    c->error.site = (mc_player_site){"gs", "d", "(Lsn;)V", pc};
}

static void child(mc_player_attack_call *c, const char *cl, const char *m, const char *d,
                  unsigned int pc) {
    c->error.child = (mc_player_site){cl, m, d, pc};
}

static bool fail(mc_player_attack_call *c, mc_entity_status status, const char *operation) {
    c->error.status = status;
    c->error.operation = operation;
    c->error.native_policy = !c->error.source_exception;
    return false;
}

static bool null_reference(mc_player_attack_call *c, const char *operation) {
    c->error.source_exception = true;
    c->error.exception_class = "java/lang/NullPointerException";
    return fail(c, MC_ENTITY_STATE_ERROR, operation);
}

static bool source_exception(mc_player_attack_call *c, const char *name, const char *operation) {
    c->error.source_exception = true;
    c->error.exception_class = name;
    return fail(c, MC_ENTITY_STATE_ERROR, operation);
}

static bool fp(mc_player_attack_call *c) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_ENTITY_UNSUPPORTED_FP, "Player attack strict floating environment");
}

static bool instance(mc_player_attack_call *c, mc_entity *entity, mc_entity_class_kind kind,
                     bool *out) {
    *out = false;
    if (entity == NULL)
        return true;
    const mc_entity_class_literal *literal = NULL;
    if (mc_entity_class_literal_get(c->state.player->classes, kind, &literal) != MC_ENTITY_OK ||
        mc_entity_class_literal_is_assignable_from(literal, entity->class_literal, out) !=
            MC_ENTITY_OK)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player attack actual Class identity");
    return true;
}

static bool registry(mc_player_attack_call *c) {
    if (c->registry_held)
        return true;
    if (c->registry == NULL || c->registry->entities != c->state.player->classes)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player attack actual code Registry namespace");
    const mc_registry_status status = mc_registry_classes_retain(
        c->registry, MC_REGISTRY_CLIENT, c->state.player->classes->profile, &c->error.registry);
    if (status != MC_REGISTRY_OK)
        return fail(c, status == MC_REGISTRY_BUSY ? MC_ENTITY_BUSY : MC_ENTITY_STATE_ERROR,
                    "Player attack Registry occurrence");
    c->registry_held = true;
    return true;
}

static bool registry_failure(mc_player_attack_call *c) {
    mc_item_render_throwable kind;
    mc_item_render_registry_classification(c->registry, &c->error.registry, &kind,
                                           &c->error.exception_class);
    c->error.source_exception = kind != MC_ITEM_RENDER_NO_THROWABLE;
    return fail(c, MC_ENTITY_STATE_ERROR, c->error.registry.message);
}

static bool combat_failure(mc_player_attack_call *c) {
    const mc_item_combat_error *e = &c->error.combat;
    c->error.source_exception = e->source_exception || e->source_error;
    c->error.exception_class = e->exception_class;
    return fail(c, e->status, e->operation);
}

/* IX.a(SN) uses f_(c), rather than IX.b's hotbar-only branch. Its main and
 * armor descriptors have the actual constructor lengths36/4. Keep the null,
 * signed index and source array-choice order visible, including bad inputs. */
static bool inventory_damage(mc_player_attack_call *c) {
    at(c, 5);
    mc_inventory_player *inventory = c->state.player->local_player->fields.player.c;
    if (inventory == NULL)
        return null_reference(c, "Player attack null Inventory damage receiver");
    child(c, "ix", "a", "(Lsn;)I", 5);
    if (inventory->side != MC_REGISTRY_CLIENT || inventory->active)
        return fail(c, inventory->active ? MC_ENTITY_BUSY : MC_ENTITY_STATE_ERROR,
                    "Player attack actual Inventory owner");
    const int32_t selected = inventory->selected;
    mc_item_stack *const *slots = inventory->main_slots;
    child(c, "ix", "f_", "(I)Liz;", 7);
    if (slots == NULL)
        return null_reference(c, "Inventory attack null main array length receiver");
    int32_t index = selected;
    int32_t length = 36;
    if (index >= 36) {
        index = mc_java_i32_sub(index, 36);
        slots = inventory->armor_slots;
        length = 4;
    }
    child(c, "ix", "f_", "(I)Liz;", 23);
    if (slots == NULL)
        return null_reference(c, "Inventory attack null selected array receiver");
    if (index < 0 || index >= length)
        return source_exception(c, "java/lang/ArrayIndexOutOfBoundsException",
                                "Inventory attack selected array index");
    c->first_stack = slots[index];
    c->state.first_stack_identity = (uintptr_t)c->first_stack;
    if (c->first_stack == NULL) {
        c->state.damage = 1;
        return true;
    }
    const mc_item_stack_status held = mc_item_stack_retain(c->first_stack);
    if (held != MC_ITEM_STACK_OK) {
        c->first_stack = NULL;
        return fail(c, MC_ENTITY_LIMIT, "Player attack first Stack occurrence");
    }
    child(c, "iz", "a", "(Lsn;)I", 0);
    if (!registry(c))
        return false;
    const mc_registry_item_array *array = NULL;
    if (mc_registry_item_c_get(c->registry, &array, &c->error.registry) != MC_REGISTRY_OK)
        return registry_failure(c);
    int32_t id;
    c->error.stack = mc_item_stack_source_id_get(c->first_stack, &id);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player attack actual first Stack id field");
    const mc_item *item = NULL;
    child(c, "iz", "a", "(Lsn;)I", 7);
    if (mc_registry_item_c_at(c->registry, array, id, &item, &c->error.registry) != MC_REGISTRY_OK)
        return registry_failure(c);
    child(c, "iz", "a", "(Lsn;)I", 9);
    if (item == NULL)
        return null_reference(c, "Stack attack null Item damage receiver");
    if (mc_item_combat_damage(c->registry, item, c->state.target, &c->state.damage,
                              &c->error.combat) != MC_ENTITY_OK)
        return combat_failure(c);
    return true;
}

static bool used_statistic(mc_player_attack_call *c) {
    child(c, "iz", "a", "(Lls;Lgs;)V", 20);
    if (mc_registry_classes_initialize_statistics(c->registry, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failure(c);
    mc_statistic *const *captured = c->registry->statistics.E;
    child(c, "iz", "a", "(Lls;Lgs;)V", 24);
    int32_t id;
    c->error.stack = mc_item_stack_source_id_get(c->fresh_stack, &id);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ENTITY_STATE_ERROR, "Attack hit late Stack id field");
    child(c, "iz", "a", "(Lls;Lgs;)V", 27);
    if (captured == NULL)
        return null_reference(c, "Attack hit null captured used-statistic array");
    if (id < 0 || id >= 32000)
        return source_exception(c, "java/lang/ArrayIndexOutOfBoundsException",
                                "Attack hit used-statistic signed index");
    const mc_statistic *statistic = captured[id];
    child(c, "iz", "a", "(Lls;Lgs;)V", 29);
    if (mc_local_player_add_statistic(c->state.player, statistic, 1, &c->error.statistic) !=
        MC_PLAYER_STATISTIC_OK) {
        const mc_player_statistic_error *e = &c->error.statistic;
        if (e->status == MC_PLAYER_STATISTIC_NULL_REFERENCE ||
            (e->status == MC_PLAYER_STATISTIC_WRITER_FAILED &&
             e->writer.status == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
             e->writer.exception == MC_CLIENT_STATS_NULL_REFERENCE))
            return null_reference(c, "Attack hit actual used-statistic invocation");
        return fail(c, mc_player_statistic_entity_status(e),
                    "Attack hit actual used-statistic invocation");
    }
    c->state.item_used_returned = true;
    return true;
}

static bool clear_selected(mc_player_attack_call *c) {
    child(c, "gs", "H", "()V", 1);
    mc_entity *p = c->state.player;
    mc_inventory_player *destination = p->local_player->fields.player.c;
    c->state.clear_inventory_identity = (uintptr_t)destination;
    /* H captures its invocation receiver before independently reading the
     * current Inventory and selected field. Do not reuse GS.G's earlier slot. */
    child(c, "gs", "H", "()V", 8);
    mc_inventory_player *current = p->local_player->fields.player.c;
    if (current == NULL)
        return null_reference(c, "Player H null fresh Inventory selected-field receiver");
    c->state.cleared_slot = current->selected;
    child(c, "gs", "H", "()V", 12);
    if (destination == NULL)
        return null_reference(c, "Player H null captured Inventory setter receiver");
    const mc_inventory_status status =
        mc_inventory_player_source_set(destination, c->state.cleared_slot, NULL, &c->error.slots);
    if (status != MC_INVENTORY_OK) {
        const mc_inventory_error *e = &c->error.slots;
        if (e->frame_count != 0 && status == MC_INVENTORY_NULL_REFERENCE)
            return null_reference(c, e->operation);
        if (e->frame_count != 0 && status == MC_INVENTORY_INDEX_OUT_OF_BOUNDS)
            return source_exception(c, "java/lang/ArrayIndexOutOfBoundsException", e->operation);
        return fail(c,
                    status == MC_INVENTORY_BUSY                ? MC_ENTITY_BUSY
                    : status == MC_INVENTORY_LIMIT             ? MC_ENTITY_LIMIT
                    : status == MC_INVENTORY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                                                               : MC_ENTITY_STATE_ERROR,
                    e->operation);
    }
    c->state.clear_returned = true;
    child(c, "gs", "H", "()V", 15);
    return true;
}

static bool fresh_stack(mc_player_attack_call *c) {
    at(c, 33);
    child(c, "gs", "G", "()Liz;", 4);
    mc_inventory_player *inventory = c->state.player->local_player->fields.player.c;
    if (inventory == NULL)
        return null_reference(c, "Player attack fresh held Stack null Inventory receiver");
    const mc_item_stack *stack = NULL;
    const mc_inventory_status status =
        mc_inventory_player_current(inventory, &stack, &c->error.inventory);
    if (status != MC_INVENTORY_OK) {
        const mc_inventory_current_error *e = &c->error.inventory;
        child(c, e->site.class_name, e->site.method, e->site.descriptor, (unsigned int)e->site.pc);
        if (status == MC_INVENTORY_NULL_REFERENCE && !e->native_policy)
            return null_reference(c, e->operation);
        return fail(c, status == MC_INVENTORY_BUSY ? MC_ENTITY_BUSY : MC_ENTITY_STATE_ERROR,
                    e->operation);
    }
    c->state.fresh_stack_identity = (uintptr_t)stack;
    c->state.fresh_stack_read = true;
    if (stack == NULL)
        return true;
    if (mc_item_stack_retain((mc_item_stack *)stack) != MC_ITEM_STACK_OK)
        return fail(c, MC_ENTITY_LIMIT, "Player attack fresh Stack occurrence");
    c->fresh_stack = (mc_item_stack *)stack;
    bool living;
    if (!instance(c, c->state.target, MC_ENTITY_CLASS_KIND_LIVING_ENTITY, &living))
        return false;
    if (!living)
        return true;
    at(c, 54);
    child(c, "iz", "a", "(Lls;Lgs;)V", 0);
    if (!registry(c))
        return false;
    const mc_registry_item_array *array = NULL;
    if (mc_registry_item_c_get(c->registry, &array, &c->error.registry) != MC_REGISTRY_OK)
        return registry_failure(c);
    int32_t id;
    child(c, "iz", "a", "(Lls;Lgs;)V", 4);
    c->error.stack = mc_item_stack_source_id_get(c->fresh_stack, &id);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ENTITY_STATE_ERROR, "Attack hit fresh Stack id field");
    const mc_item *item = NULL;
    child(c, "iz", "a", "(Lls;Lgs;)V", 7);
    if (mc_registry_item_c_at(c->registry, array, id, &item, &c->error.registry) != MC_REGISTRY_OK)
        return registry_failure(c);
    child(c, "iz", "a", "(Lls;Lgs;)V", 11);
    if (item == NULL)
        return null_reference(c, "Attack hit null actual Item receiver");
    if (mc_item_combat_hit(c->registry, item, c->fresh_stack, c->state.target, c->state.player,
                           &c->state.item_hit_result, &c->error.combat) != MC_ENTITY_OK)
        return combat_failure(c);
    c->state.item_hit_returned = true;
    if (c->state.item_hit_result && !used_statistic(c))
        return false;
    child(c, "iz", "a", "(Lls;Lgs;)V", 32);
    at(c, 58);
    int32_t count;
    c->error.stack = mc_item_stack_source_count_get(c->fresh_stack, &count);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player attack late Stack count field");
    if (count <= 0) {
        c->state.exhausted_stack = true;
        at(c, 66);
        child(c, "iz", "a", "(Lgs;)V", 0); /* Actual literal return; no break effects. */
        c->state.destroy_returned = true;
        at(c, 70);
        if (!clear_selected(c))
            return false;
    }
    return true;
}

static bool alert(mc_player_attack_call *c) {
    c->state.alert_entered = true;
    const mc_entity_status status =
        mc_player_ally_alert_begin(c->state.player, c->state.target, true, &c->alert);
    mc_player_ally_alert_state state;
    if (!mc_player_ally_alert_inspect(c->alert, &state))
        return fail(c, status, "Player ally alert retained invocation admission");
    c->state.alert_returned = state.returned;
    c->state.wolves_examined = state.wolves_examined;
    c->state.wolves_alerted = state.wolves_alerted;
    const mc_player_ally_alert_error *e = state.error;
    c->error.child = e->child.class_name == NULL ? e->site : e->child;
    if (status == MC_ENTITY_OK)
        return true;
    c->error.source_exception = e->source_exception || e->source_error;
    c->error.exception_class = e->exception_class;
    c->error.entity = e->entity;
    c->error.bounds = e->bounds;
    c->error.nearby = e->nearby;
    return fail(c, status, e->operation);
}

static bool damage_statistic(mc_player_attack_call *c) {
    at(c, 97);
    if (!registry(c))
        return false;
    if (mc_registry_classes_initialize_statistics(c->registry, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failure(c);
    const mc_statistic *value = c->registry->statistics.named[17];
    at(c, 101);
    const mc_player_statistic_status status =
        mc_local_player_add_statistic(c->state.player, value, c->state.damage, &c->error.statistic);
    if (status != MC_PLAYER_STATISTIC_OK) {
        c->error.child = c->error.statistic.site;
        if (status == MC_PLAYER_STATISTIC_NULL_REFERENCE ||
            (status == MC_PLAYER_STATISTIC_WRITER_FAILED &&
             c->error.statistic.writer.status == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
             c->error.statistic.writer.exception == MC_CLIENT_STATS_NULL_REFERENCE))
            return null_reference(c, "Player attack actual damage-dealt statistic child");
        return fail(c, mc_player_statistic_entity_status(&c->error.statistic),
                    "Player attack actual damage-dealt statistic child");
    }
    c->state.statistic_returned = true;
    return true;
}

static bool body(mc_player_attack_call *c) {
    mc_entity *p = c->state.player;
    at(c, 1);
    if (p == NULL)
        return null_reference(c, "Player attack null Player Inventory field receiver");
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) || p->phase != MC_ENTITY_READY ||
        p->local_player == NULL)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player attack actual READY LocalPlayer");
    mc_entity_status status = mc_entity_retain(p);
    if (status != MC_ENTITY_OK)
        return fail(c, status, "Player attack Player occurrence");
    c->player_held = true;
    if (!inventory_damage(c))
        return false;
    if (c->state.damage <= 0)
        goto returned;
    at(c, 18);
    if (!fp(c))
        return false;
    if (p->base.velocity_y < 0.0) {
        c->state.damage = mc_java_i32_add(c->state.damage, 1);
        c->state.falling_bonus = true;
    }
    at(c, 28);
    mc_entity *target = c->state.target;
    if (target == NULL)
        return null_reference(c, "Player attack null target damage receiver");
    if (target->classes != p->classes || target->phase != MC_ENTITY_READY)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player attack actual target owner");
    status = mc_entity_retain(target);
    if (status != MC_ENTITY_OK)
        return fail(c, status, "Player attack target occurrence");
    c->target_held = true;
    /* This passive native observation identifies cleanup ownership only. The
     * actual damage call still executes and preserves its own BUSY receipt. A
     * rejected invocation cannot consume another producer's pending death. */
    c->damage_entered = !mc_entity_effect_has_pending(target);
    status = mc_entity_effect_damage_with_math(target, c->registry, c->math, p, c->state.damage,
                                               &c->state.damage_result, &c->error.effect);
    if (status != MC_ENTITY_OK) {
        const mc_entity_effect_error *e = &c->error.effect;
        c->error.source_exception = e->living_damage.source_exception ||
                                    e->cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE ||
                                    e->cause == MC_ENTITY_EFFECT_CAUSE_CLASS_CAST;
        c->error.exception_class = e->living_damage.exception_class;
        if (c->error.source_exception && c->error.exception_class == NULL)
            c->error.exception_class = e->cause == MC_ENTITY_EFFECT_CAUSE_CLASS_CAST
                                           ? "java/lang/ClassCastException"
                                           : "java/lang/NullPointerException";
        child(c, e->source_class, "a", "(Lsn;I)Z", e->source_bytecode);
        return fail(c, status, e->operation);
    }
    c->state.damage_returned = true; /* GS discards the Boolean at31. */
    if (!fresh_stack(c))
        return false;
    bool living;
    if (!instance(c, target, MC_ENTITY_CLASS_KIND_LIVING_ENTITY, &living))
        return false;
    if (living) {
        at(c, 80);
        const mc_living_entity_fields *v = mc_entity_living_fields(target);
        if (v == NULL)
            return fail(c, MC_ENTITY_STATE_ERROR, "Player attack actual Living fields");
        c->state.target_alive = !target->base.be && v->Y > 0;
        if (c->state.target_alive) {
            at(c, 93);
            if (!alert(c))
                return false;
        }
        if (!damage_statistic(c))
            return false;
    }
returned:
    at(c, 104);
    c->state.returned = true;
    return true;
}

mc_entity_status mc_local_player_attack_begin(mc_registry_classes *registry_value,
                                              mc_math_classes *math, mc_entity *player,
                                              mc_entity *target, mc_player_attack_call **out,
                                              mc_player_attack_error *error) {
    if (out == NULL || *out != NULL) {
        if (error != NULL)
            *error = (mc_player_attack_error){.status = MC_ENTITY_INVALID_ARGUMENT,
                                              .operation = "Player attack fresh output slot",
                                              .native_policy = true};
        return MC_ENTITY_INVALID_ARGUMENT;
    }
    mc_player_attack_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        if (error != NULL)
            *error = (mc_player_attack_error){.status = MC_ENTITY_ALLOCATION_FAILED,
                                              .operation = "Player attack invocation allocation",
                                              .native_policy = true};
        return MC_ENTITY_ALLOCATION_FAILED;
    }
    c->registry = registry_value;
    c->math = math;
    c->state = (mc_player_attack_state){
        .player = player, .target = target, .error = &c->error, .active = true};
    *out = c;
    (void)body(c);
    c->state.active = false;
    if (error != NULL)
        *error = c->error;
    return c->error.status;
}

bool mc_local_player_attack_inspect(const mc_player_attack_call *c, mc_player_attack_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

mc_entity_status mc_local_player_attack_retire(mc_player_attack_call **slot,
                                               mc_player_attack_error *error) {
    if (slot == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_player_attack_call *c = *slot;
    if (c == NULL) {
        if (error != NULL)
            *error = (mc_player_attack_error){0};
        return MC_ENTITY_OK;
    }
    if (c->state.active) {
        if (error != NULL)
            *error = (mc_player_attack_error){.status = MC_ENTITY_BUSY,
                                              .operation = "Player attack active retirement",
                                              .native_policy = true};
        return MC_ENTITY_BUSY;
    }
    if (c->damage_entered && !mc_entity_effect_error_retire(&c->error.effect)) {
        if (error != NULL)
            *error = c->error;
        return MC_ENTITY_BUSY;
    }
    if (c->registry_held && !mc_registry_runtime_borrow_release(c->registry)) {
        if (error != NULL)
            *error = c->error;
        return MC_ENTITY_BUSY;
    }
    c->registry_held = false;
    if (c->alert != NULL) {
        const mc_entity_status status = mc_player_ally_alert_retire(&c->alert, &c->alert_cleanup);
        if (status != MC_ENTITY_OK) {
            if (error != NULL) {
                *error = c->error;
                if (error->status == MC_ENTITY_OK) {
                    error->status = status;
                    error->operation = c->alert_cleanup.operation;
                    error->native_policy = true;
                }
            }
            return status;
        }
        /* The nested call owned copied loader diagnostics and source receipt
         * views. End those borrows immediately when its owner is consumed. */
        mc_world_nearby_error_init(&c->error.nearby);
    }
    mc_item_stack_release(c->first_stack);
    mc_item_stack_release(c->fresh_stack);
    if (c->target_held)
        mc_entity_release(c->state.target);
    if (c->player_held)
        mc_entity_release(c->state.player);
    free(c);
    *slot = NULL;
    if (error != NULL)
        *error = (mc_player_attack_error){0};
    return MC_ENTITY_OK;
}
