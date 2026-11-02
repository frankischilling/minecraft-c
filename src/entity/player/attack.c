#include "mc/entity/player/attack.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../inventory/internal.h"
#include "../../item/render/internal.h"
#include "../living/fields_internal.h"
#include "../mob/internal.h"
#include "../mob/references_internal.h"
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
    mc_world_runtime *alert_world;
    mc_entity_bounds *bounds, *expanded;
    mc_world_class_entity_list *wolves;
    bool player_held, target_held, registry_held, world_held, damage_entered;
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
    mc_item_stack_fields fields;
    if (mc_item_stack_fields_get(c->first_stack, &fields) != MC_ITEM_STACK_OK)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player attack actual first Stack id field");
    const mc_item *item = NULL;
    child(c, "iz", "a", "(Lsn;)I", 7);
    if (mc_registry_item_c_at(c->registry, array, fields.id, &item, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failure(c);
    child(c, "iz", "a", "(Lsn;)I", 9);
    if (item == NULL)
        return null_reference(c, "Stack attack null Item damage receiver");
    return fail(c, MC_ENTITY_STATE_ERROR, "Player attack actual Item damage override");
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
    return fail(c, MC_ENTITY_STATE_ERROR, "Player attack actual fresh Stack hit body");
}

static mc_entity_watch_record *watch(mc_player_attack_call *c, mc_entity *wolf, int32_t key,
                                     int32_t type) {
    if (wolf->watcher == NULL || wolf->watcher->map == NULL) {
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
        if (n->record == NULL) {
            (void)null_reference(c, "Player ally alert null Wolf watcher record receiver");
            return NULL;
        }
        if (n->record->type != type) {
            (void)source_exception(c, "java/lang/ClassCastException",
                                   "Player ally alert typed Wolf watcher value");
            return NULL;
        }
        return n->record;
    }
    (void)null_reference(c, "Player ally alert absent Wolf watcher record receiver");
    return NULL;
}

static bool flag(mc_player_attack_call *c, mc_entity *wolf, const char *method, int mask,
                 bool *out) {
    child(c, "gi", method, "()Z", 6);
    mc_entity_watch_record *r = watch(c, wolf, 16, 0);
    if (r == NULL)
        return false;
    *out = (r->byte_value & mask) != 0;
    return true;
}

static bool owner(mc_player_attack_call *c, mc_entity *wolf, bool *out) {
    const mc_player_string *name = c->state.player->local_player->fields.player.l;
    child(c, "gi", "A", "()Ljava/lang/String;", 6);
    mc_entity_watch_record *r = watch(c, wolf, 17, 4);
    if (r == NULL)
        return false;
    child(c, "java/lang/String", "equals", "(Ljava/lang/Object;)Z", 0);
    if (name == NULL)
        return null_reference(c, "Player ally alert null name equals receiver");
    mc_player_string_view a = {0}, b = {0};
    if (!mc_player_string_read(name, &a) ||
        (r->string_value != NULL && !mc_player_string_read(r->string_value, &b)))
        return fail(c, MC_ENTITY_STATE_ERROR, "Player ally alert actual String ownership");
    *out = r->string_value != NULL && a.length == b.length &&
           (a.length == 0 || memcmp(a.units, b.units, a.length * sizeof(*a.units)) == 0);
    return true;
}

static void alert_at(mc_player_attack_call *c, unsigned int pc) {
    c->error.child = (mc_player_site){"gs", "a", "(Lls;Z)V", pc};
}

static bool alert(mc_player_attack_call *c) {
    c->state.alert_entered = true;
    mc_entity *target = c->state.target;
    bool excluded;
    alert_at(c, 0);
    if (!instance(c, target, MC_ENTITY_CLASS_KIND_CREEPER, &excluded))
        return false;
    if (excluded)
        goto returned;
    if (!instance(c, target, MC_ENTITY_CLASS_KIND_GHAST, &excluded))
        return false;
    if (excluded)
        goto returned;
    if (mc_entity_is_kind(target, MC_ENTITY_CLASS_KIND_WOLF)) {
        bool tame, same;
        alert_at(c, 28);
        if (!flag(c, target, "D", 4, &tame))
            return false;
        if (tame) {
            alert_at(c, 42);
            if (!owner(c, target, &same))
                return false;
            if (same)
                goto returned;
        }
    }
    bool player;
    if (!instance(c, target, MC_ENTITY_CLASS_KIND_PLAYER, &player))
        return false;
    /* Canonical GS.F is the actual literal false body. */
    if (player)
        goto returned;
    mc_entity *p = c->state.player;
    c->alert_world = p->base.world; /* Captured field65 precedes all box work. */
    const mc_entity_class_literal *wolf_class = NULL;
    if (mc_entity_class_literal_get(p->classes, MC_ENTITY_CLASS_KIND_WOLF, &wolf_class) !=
        MC_ENTITY_OK)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player ally alert actual Wolf Class");
    alert_at(c, 100);
    if (!fp(c))
        return false;
    const double x1 = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(p->base.x), _mm_set_sd(1.0)));
    const double y1 = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(p->base.y), _mm_set_sd(1.0)));
    const double z1 = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(p->base.z), _mm_set_sd(1.0)));
    mc_entity_status status = mc_entity_bounds_pool_get(p->classes, p->base.x, p->base.y, p->base.z,
                                                        x1, y1, z1, &c->bounds, &c->error.bounds);
    if (status != MC_ENTITY_OK)
        return fail(c, status, "Player ally alert actual pooled bounds");
    alert_at(c, 112);
    status = mc_entity_bounds_expand(p->classes, c->bounds, 16.0, 4.0, 16.0, &c->expanded,
                                     &c->error.bounds);
    if (status != MC_ENTITY_OK)
        return fail(c, status, "Player ally alert actual bounds expansion");
    alert_at(c, 115);
    if (c->alert_world == NULL)
        return null_reference(c, "Player ally alert null captured World receiver");
    if (!mc_world_runtime_binding_retain(c->alert_world))
        return fail(c, MC_ENTITY_BUSY, "Player ally alert actual World occurrence");
    c->world_held = true;
    if (mc_world_runtime_class_entities(c->alert_world, wolf_class, c->expanded, &c->wolves,
                                        &c->error.nearby) != MC_WORLD_STORAGE_OK)
        return fail(c, MC_ENTITY_WORLD_FAILED, "Player ally alert actual Wolf query");
    size_t count;
    if (mc_world_class_entity_list_count(c->wolves, &count) != MC_WORLD_STORAGE_OK)
        return fail(c, MC_ENTITY_STATE_ERROR, "Player ally alert actual List iterator");
    for (size_t i = 0; i < count; ++i) {
        mc_entity *wolf = NULL, *present = NULL;
        alert_at(c, 139);
        if (mc_world_class_entity_list_get(c->wolves, i, &wolf) != MC_WORLD_STORAGE_OK ||
            !mc_entity_is_kind(wolf, MC_ENTITY_CLASS_KIND_WOLF))
            return fail(c, MC_ENTITY_STATE_ERROR, "Player ally alert actual Wolf List occurrence");
        if (c->state.wolves_examined != SIZE_MAX)
            ++c->state.wolves_examined;
        bool tame, same, sitting;
        alert_at(c, 158);
        if (!flag(c, wolf, "D", 4, &tame))
            return false;
        if (!tame)
            continue;
        alert_at(c, 166);
        child(c, "ii", "G", "()Lsn;", 1);
        if (mc_mob_creature_target_get(wolf, &present, &c->error.entity) != MC_ENTITY_OK)
            return fail(c, MC_ENTITY_STATE_ERROR, "Player ally alert actual Wolf target read");
        if (present != NULL)
            continue;
        alert_at(c, 181);
        if (!owner(c, wolf, &same))
            return false;
        if (!same)
            continue;
        alert_at(c, 193);
        if (!flag(c, wolf, "B", 1, &sitting))
            return false;
        if (sitting)
            continue; /* GS.a's attack=true excludes sitting Wolves. */
        alert_at(c, 202);
        child(c, "gi", "b", "(Z)V", 6);
        mc_entity_watch_record *old = watch(c, wolf, 16, 0);
        if (old == NULL)
            return false;
        const int8_t value = mc_java_i8_narrow(old->byte_value & -2);
        child(c, "gi", "b", "(Z)V", 46);
        mc_entity_watch_record *fresh = watch(c, wolf, 16, 0);
        if (fresh == NULL)
            return false;
        if (fresh->byte_value != value) {
            fresh->byte_value = value;
            fresh->dirty = wolf->watcher->dirty = true;
        }
        alert_at(c, 208);
        child(c, "ii", "c", "(Lsn;)V", 2);
        status = mc_mob_creature_target_store(wolf, target, &c->error.entity);
        if (status != MC_ENTITY_OK)
            return fail(c, status, "Player ally alert actual Wolf owning target store");
        if (c->state.wolves_alerted != SIZE_MAX)
            ++c->state.wolves_alerted;
    }
returned:
    c->state.alert_returned = true;
    alert_at(c, 214);
    return true;
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
        c->error.source_exception =
            e->living_damage.source_exception || e->cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE;
        c->error.exception_class = e->living_damage.exception_class;
        if (c->error.source_exception && c->error.exception_class == NULL)
            c->error.exception_class = "java/lang/NullPointerException";
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
    mc_world_class_entity_list_release(c->wolves);
    if (c->expanded != NULL)
        (void)mc_entity_bounds_release(c->state.player->classes, c->expanded);
    if (c->bounds != NULL)
        (void)mc_entity_bounds_release(c->state.player->classes, c->bounds);
    if (c->world_held)
        mc_world_runtime_binding_release(c->alert_world);
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
