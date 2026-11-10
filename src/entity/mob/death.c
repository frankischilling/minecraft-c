#include "../watcher_payload_internal.h"
#include "death_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../living/fields_internal.h"
#include "../movement/effects_internal.h"
#include "../player/combat_callbacks_internal.h"
#include "internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/entity/drop.h"
#include "mc/item/registry/runtime.h"

#include <stdlib.h>
#include <string.h>

struct mc_mob_death_work {
    mc_mob_death_state state;
    mc_entity *actor, *source;
    mc_registry_classes *registry;
    mc_item_stack *stack;
    mc_entity_drop_call *drop;
    mc_entity_drop_error drop_error, drop_cleanup;
    mc_world_runtime *worlds[2];
    size_t world_count;
    bool drop_reached;
};

static mc_mob_death_site site(const char *c, const char *m, const char *d, int32_t pc) {
    return (mc_mob_death_site){c, m, d, pc};
}

static bool animal(const mc_entity *p) {
    return mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SHEEP) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PIG) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_COW) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_CHICKEN) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_WOLF);
}

static bool monster(const mc_entity *p) {
    return mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ZOMBIE) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SPIDER) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SKELETON) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_CREEPER);
}

static bool remaining_living(const mc_entity *p) {
    return mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SLIME) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SQUID) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_GHAST);
}

static mc_mob_death_state view(const struct mc_mob_death_work *w) {
    mc_mob_death_state r = w->state;
    r.receiver = r.pending ? w->actor : NULL;
    if (w->drop_reached)
        r.drop_error = &w->drop_error;
    mc_entity_drop_state drop;
    if (w->drop != NULL && mc_entity_drop_inspect(w->drop, &drop)) {
        const size_t i = (size_t)r.drops_attempted - 1u;
        r.drop_error = drop.error;
        if (i < 4) {
            r.items[i] = (uintptr_t)drop.item;
            r.stacks[i] = (uintptr_t)drop.stack;
            r.constructor_worlds[i] = (uintptr_t)drop.constructor_world;
            r.publication_worlds[i] = (uintptr_t)drop.publication_world;
            r.inserted[i] = drop.inserted;
        }
    }
    return r;
}

static void project(const struct mc_mob_death_work *w, mc_entity_effect_error *e) {
    if (e != NULL)
        e->death = view(w);
}

static void death_at(struct mc_mob_death_work *w, mc_entity_effect_error *e, uint16_t pc) {
    w->state.death_pc = pc;
    w->state.site = site("ls", "b", "(Lsn;)V", pc);
    if (e->living_damage.entered) {
        e->living_damage.child_site.class_name = "ls";
        e->living_damage.child_site.method = "b";
        e->living_damage.child_site.descriptor = "(Lsn;)V";
        e->living_damage.child_site.pc = pc;
    }
}

static void q_at(struct mc_mob_death_work *w, const char *c, uint16_t pc) {
    w->state.q_pc = pc;
    w->state.site = site(c, "q", "()V", pc);
}

static void j_at(struct mc_mob_death_work *w, const char *c, uint16_t pc) {
    w->state.j_pc = pc;
    w->state.site = site(c, "j", "()I", pc);
}

static bool failure(struct mc_mob_death_work *w, mc_entity_effect_error *e, mc_entity_status status,
                    mc_entity_effect_cause cause, const char *op) {
    w->state.status = e->status = status;
    w->state.operation = e->operation = op;
    e->cause = cause;
    w->state.native_policy = e->living_damage.native_policy = true;
    w->state.source_exception = e->living_damage.source_exception = false;
    w->state.exception_class = e->living_damage.exception_class = NULL;
    project(w, e);
    return false;
}

static bool exception(struct mc_mob_death_work *w, mc_entity_effect_error *e,
                      mc_entity_effect_cause cause, const char *name, const char *op) {
    (void)failure(w, e, MC_ENTITY_STATE_ERROR, cause, op);
    w->state.native_policy = e->living_damage.native_policy = false;
    w->state.source_exception = e->living_damage.source_exception = true;
    w->state.exception_class = e->living_damage.exception_class = name;
    project(w, e);
    return false;
}

static bool null_reference(struct mc_mob_death_work *w, mc_entity_effect_error *e, const char *op) {
    return exception(w, e, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE, "java/lang/NullPointerException",
                     op);
}

static mc_entity_status registry_status(mc_registry_status status) {
    return status == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
           : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
                                                     : MC_ENTITY_STATE_ERROR;
}

static const char *registry_source(mc_registry_status status) {
    switch (status) {
        case MC_REGISTRY_NULL_REFERENCE:
            return "java/lang/NullPointerException";
        case MC_REGISTRY_INDEX_OUT_OF_RANGE:
            return "java/lang/ArrayIndexOutOfBoundsException";
        case MC_REGISTRY_ILLEGAL_ARGUMENT:
        case MC_REGISTRY_DUPLICATE_BLOCK:
        case MC_REGISTRY_DUPLICATE_STATISTIC:
            return "java/lang/IllegalArgumentException";
        default:
            return NULL;
    }
}

/* Classify only an exact actual class latch/cause, as the Mob tick registry
 * join does. A generic failed status alone never manufactures a Throwable. */
static bool same_cause(const mc_registry_error *a, const mc_registry_error *b, bool cached) {
    return (cached ? a->status == MC_REGISTRY_FAILED_INITIALIZATION : a->status == b->status) &&
           a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc && a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

static bool registry_failure(struct mc_mob_death_work *w, mc_entity_effect_error *e,
                             mc_registry_status status, const char *op, bool active_use) {
    w->state.registry = e->registry;
    (void)failure(w, e, registry_status(status), MC_ENTITY_EFFECT_CAUSE_REGISTRY, op);
    if (!active_use)
        return false;
    const mc_registry_classes *classes = w->registry;
    const mc_registry_error *block = NULL, *item = NULL;
    if (classes != NULL) {
        if (classes->blocks_phase == MC_REGISTRY_FAILED &&
            classes->block_states[MC_BLOCK_BASE].phase == MC_REGISTRY_FAILED &&
            same_cause(&classes->block_failure, &classes->block_states[MC_BLOCK_BASE].failure,
                       false))
            block = &classes->block_failure;
        if (classes->item_states[MC_ITEM_BASE].phase == MC_REGISTRY_FAILED)
            item = &classes->item_states[MC_ITEM_BASE].failure;
    }
    const bool cached = e->registry.status == MC_REGISTRY_FAILED_INITIALIZATION;
    const mc_registry_error *owner =
        block != NULL && same_cause(&e->registry, block, cached) ? block
        : item != NULL && same_cause(&e->registry, item, cached) ? item
                                                                 : NULL;
    const char *name = NULL;
    if (owner != NULL) {
        if (registry_source(owner->status) != NULL)
            name =
                cached ? "java/lang/NoClassDefFoundError" : "java/lang/ExceptionInInitializerError";
        else if (owner->status == MC_REGISTRY_FAILED_INITIALIZATION) {
            const mc_registry_error *nested = owner == block ? item : block;
            if (nested != NULL && same_cause(owner, nested, true) &&
                registry_source(nested->status) != NULL)
                name = "java/lang/NoClassDefFoundError";
        }
    } else if (!cached && e->registry.active_initializer_class == NULL)
        name = registry_source(e->registry.status);
    if (name != NULL) {
        w->state.native_policy = e->living_damage.native_policy = false;
        w->state.source_exception = e->living_damage.source_exception = true;
        w->state.exception_class = e->living_damage.exception_class = name;
        project(w, e);
    }
    return false;
}

static bool registry(struct mc_mob_death_work *w, mc_entity_effect_error *e) {
    if (w->state.registry_held)
        return true;
    if (w->registry == NULL || w->registry->entities != w->actor->classes)
        return failure(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_REGISTRY,
                       "Living death reached actual code Registry");
    const mc_registry_status status = mc_registry_classes_retain(
        w->registry, MC_REGISTRY_CLIENT, w->actor->classes->profile, &e->registry);
    if (status != MC_REGISTRY_OK)
        return registry_failure(w, e, status, "Living death code Registry occurrence", false);
    w->state.registry_held = true;
    return true;
}

static bool world(struct mc_mob_death_work *w, mc_world_runtime *value, mc_entity_effect_error *e) {
    if (value == NULL)
        return null_reference(w, e, "Living death actual null World receiver");
    /* There are exactly two independent World captures in this source body.
     * Native bindings retain each occurrence without adding a Registry read. */
    if (w->world_count == 2 || !mc_world_runtime_binding_retain(value))
        return failure(w, e, MC_ENTITY_LIMIT, MC_ENTITY_EFFECT_CAUSE_OWNER,
                       "Living death reached World occurrence");
    w->worlds[w->world_count++] = value;
    return true;
}

static bool callback(struct mc_mob_death_work *w, mc_entity *source, bool score,
                     mc_entity_effect_error *e) {
    death_at(w, e, score ? 17 : 26);
    if (w->source == NULL) {
        if (source->classes != w->actor->classes || source->holds == 0 ||
            source->phase != MC_ENTITY_READY || source->class_literal == NULL ||
            !mc_entity_is_kind(source, source->class_literal->kind))
            return failure(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                           "Living death reached actual attacker owner");
        const mc_entity_status status = mc_entity_retain(source);
        if (status != MC_ENTITY_OK)
            return failure(w, e, status, MC_ENTITY_EFFECT_CAUSE_OWNER,
                           "Living death attacker callback occurrence");
        w->source = source;
        w->state.source_hold_acquired = true;
        w->state.source_identity = (uintptr_t)source;
    }
    if (mc_entity_is_kind(source, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        mc_player_site reached;
        if (score)
            w->state.score_callback_entered = true;
        else
            w->state.kill_callback_entered = true;
        const bool returned = mc_player_death_callback(source, w->registry, w->actor,
                                                       w->state.score, score, &reached, e);
        w->state.child =
            site(reached.class_name, reached.method, reached.descriptor, (int32_t)reached.pc);
        if (!returned) {
            w->state.status = e->status;
            w->state.operation = e->operation;
            w->state.native_policy = !e->living_damage.source_exception;
            w->state.source_exception = e->living_damage.source_exception;
            w->state.exception_class = e->living_damage.exception_class;
            project(w, e);
            return false;
        }
        if (score)
            w->state.score_callback_returned = true;
        else
            w->state.kill_callback_returned = true;
        return true;
    }
    if (mc_entity_is_kind(source, MC_ENTITY_CLASS_KIND_PLAYER))
        return failure(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL,
                       score ? "Living death query-only Player score override"
                             : "Living death query-only Player kill override");
    /* All 39 supplied-client literal hierarchies and their omitted parents
     * were inspected. Only abstract GS overrides these two SN descriptors;
     * the constructed DC path is handled above. Every other admitted canonical
     * source resolves directly to SN's actual literal return0 declarations. */
    _Static_assert(MC_ENTITY_CLASS_KIND_COUNT == 39, "Audit death callbacks for new Entity kinds");
    w->state.child = site("sn", score ? "c" : "a", score ? "(Lsn;I)V" : "(Lls;)V", 0);
    if (score) {
        w->state.score_callback_entered = true;
        w->state.score_callback_returned = true;
    } else {
        w->state.kill_callback_entered = true;
        w->state.kill_callback_returned = true;
    }
    return true;
}

static bool sheep_byte(struct mc_mob_death_work *w, const char *method, int8_t *out,
                       mc_entity_effect_error *e) {
    w->state.child = site("dl", method, method[0] == 's' ? "()Z" : "()I", 6);
    w->state.watcher_pc = e->watcher_bytecode = 6;
    if (w->actor->watcher == NULL)
        return null_reference(w, e, "Sheep death null DataWatcher invoke receiver");
    w->state.watcher_pc = e->watcher_bytecode = 8;
    e->watcher_index = 16;
    const mc_entity_watch_map *map = w->actor->watcher->map;
    if (map == NULL)
        return null_reference(w, e, "Sheep death null watcher Map receiver");
    const mc_entity_watch_record *record = NULL;
    size_t seen = 0;
    for (const mc_entity_watch_node *node = map->head; node != NULL; node = node->next) {
        if (seen++ >= map->size)
            return failure(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                           "Sheep death watcher Map native ownership");
        if (node->key == 16) {
            record = node->record;
            break;
        }
    }
    w->state.watcher_pc = e->watcher_bytecode = 16;
    if (record == NULL)
        return null_reference(w, e, "Sheep death null watcher record receiver");
    w->state.watcher_pc = e->watcher_bytecode = 19;
    if (mc_entity_watch_payload_kind_of(record) != MC_ENTITY_WATCH_PAYLOAD_BYTE) {
        if (mc_entity_watch_payload_kind_of(record) == MC_ENTITY_WATCH_PAYLOAD_NULL) {
            w->state.watcher_pc = e->watcher_bytecode = 22;
            return null_reference(w, e, "Sheep death null boxed Byte unbox receiver");
        }
        if (mc_entity_watch_payload_known(record))
            return exception(w, e, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_WATCHER_VALUE,
                             "java/lang/ClassCastException", "Sheep death actual Byte cast");
        return failure(w, e, MC_ENTITY_STATE_ERROR,
                       MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_WATCHER_VALUE,
                       "Sheep death watcher typed Byte representation");
    }
    *out = record->byte_value;
    w->state.watcher_pc = 25;
    e->watcher_index = -1;
    e->watcher_bytecode = 0;
    return true;
}

static bool cleanup_failure(struct mc_mob_death_work *w, mc_entity_effect_error *e,
                            mc_entity_status status, const char *op) {
    w->state.cleanup_status = status;
    w->state.cleanup_operation = op;
    if (e != NULL) {
        e->status = status;
        e->operation = op;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        project(w, e);
    }
    return false;
}

static bool drop(struct mc_mob_death_work *w, bool numeric, mc_entity_effect_error *e) {
    ++w->state.drops_attempted;
    w->drop_reached = true;
    const mc_entity_status status =
        numeric ? mc_entity_drop_numeric_begin(w->registry, w->actor, w->state.drop_id, 1, 0.0f,
                                               &w->drop, &w->drop_error)
                : mc_entity_drop_stack_begin(w->registry, w->actor, w->stack, 0.0f, &w->drop,
                                             &w->drop_error);
    mc_mob_death_state current = view(w);
    w->state = current;
    if (status != MC_ENTITY_OK) {
        e->entity = w->drop_error.entity;
        e->registry = w->state.registry = w->drop_error.registry;
        w->state.child = site(w->drop_error.site.class_name, w->drop_error.site.method,
                              w->drop_error.site.descriptor, (int32_t)w->drop_error.site.pc);
        const mc_entity_effect_cause cause =
            w->drop_error.source_exception && w->drop_error.exception_class != NULL &&
                    strcmp(w->drop_error.exception_class, "java/lang/NullPointerException") == 0
                ? MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE
            : w->drop_error.publication != NULL               ? MC_ENTITY_EFFECT_CAUSE_WORLD
            : w->drop_error.registry.status != MC_REGISTRY_OK ? MC_ENTITY_EFFECT_CAUSE_REGISTRY
            : w->drop_error.entity.profile != MC_JAVA_RANDOM_PROFILE_OK
                ? MC_ENTITY_EFFECT_CAUSE_RANDOM
            : status == MC_ENTITY_UNSUPPORTED_FP ? MC_ENTITY_EFFECT_CAUSE_FP
                                                 : MC_ENTITY_EFFECT_CAUSE_OWNER;
        (void)failure(w, e, status, cause, w->drop_error.operation);
        w->state.native_policy = e->living_damage.native_policy = w->drop_error.native_policy;
        w->state.source_exception = e->living_damage.source_exception =
            w->drop_error.source_exception;
        w->state.exception_class = e->living_damage.exception_class = w->drop_error.exception_class;
        project(w, e);
        return false;
    }
    ++w->state.drops_returned;
    const mc_entity_status retired = mc_entity_drop_retire(&w->drop, &w->drop_cleanup);
    if (retired != MC_ENTITY_OK)
        return cleanup_failure(w, e, retired, "Living death completed Item drop retirement");
    ++w->state.drops_retired;
    w->drop_reached = false;
    w->state.drop_error = NULL;
    return true;
}

static bool sheep_q(struct mc_mob_death_work *w, mc_entity_effect_error *e) {
    q_at(w, "dl", 1);
    int8_t value;
    if (!sheep_byte(w, "s", &value, e))
        return false;
    w->state.sheared_read = true;
    w->state.sheared = ((int32_t)value & 16) != 0;
    if (w->state.sheared)
        goto returned;
    q_at(w, "dl", 8);
    w->state.stack_status = mc_item_stack_allocate(&w->stack);
    if (w->state.stack_status != MC_ITEM_STACK_OK)
        return failure(w, e,
                       w->state.stack_status == MC_ITEM_STACK_ALLOCATION_FAILED
                           ? MC_ENTITY_ALLOCATION_FAILED
                           : MC_ENTITY_LIMIT,
                       MC_ENTITY_EFFECT_CAUSE_OWNER, "Sheep death actual RAW Stack allocation");
    w->state.stack_allocated = true;
    w->state.sheep_stack_identity = (uintptr_t)w->stack;
    q_at(w, "dl", 12);
    if (!registry(w, e))
        return false;
    const mc_block *block = NULL;
    const mc_registry_status status =
        mc_registry_block_static_get(w->registry, "ac", &block, &e->registry);
    if (status != MC_REGISTRY_OK)
        return registry_failure(w, e, status, "Sheep death actual UU.ac active use", true);
    q_at(w, "dl", 15);
    if (block == NULL)
        return null_reference(w, e, "Sheep death null Wool bn receiver");
    w->state.drop_id = mc_block_fields_get(block)->bn;
    q_at(w, "dl", 20);
    if (!sheep_byte(w, "r", &value, e))
        return false;
    w->state.color_read = true;
    w->state.color = (int32_t)value & 15;
    q_at(w, "dl", 23);
    w->state.stack_status = mc_item_stack_construct(w->stack, w->state.drop_id, 1, w->state.color);
    if (w->state.stack_status != MC_ITEM_STACK_OK)
        return failure(w, e, MC_ENTITY_ITEM_FAILED, MC_ENTITY_EFFECT_CAUSE_OWNER,
                       "Sheep death actual numeric Stack constructor");
    w->state.stack_constructed = true;
    w->state.quantity = 1;
    q_at(w, "dl", 27);
    if (!drop(w, false, e))
        return false;
returned:
    q_at(w, "dl", 31);
    w->state.q_returned = true;
    return true;
}

static bool item_id(struct mc_mob_death_work *w, const char *field, mc_entity_effect_error *e) {
    if (!registry(w, e))
        return false;
    w->state.child = w->state.site;
    const mc_item *item = NULL;
    mc_registry_status status =
        mc_registry_item_static_get(w->registry, field, &item, &e->registry);
    if (status != MC_REGISTRY_OK)
        return registry_failure(w, e, status, "Living death actual Item getstatic", true);
    w->state.site.pc += 3;
    w->state.child = w->state.site;
    if (strcmp(w->state.site.method, "j") == 0)
        w->state.j_pc = (uint16_t)w->state.site.pc;
    else if (strcmp(w->state.site.method, "q") == 0)
        w->state.q_pc = (uint16_t)w->state.site.pc;
    else
        w->state.creeper_pc = (uint16_t)w->state.site.pc;
    status = mc_registry_item_source_id(w->registry, item, &w->state.drop_id, &e->registry);
    if (status != MC_REGISTRY_OK) {
        w->state.registry = e->registry;
        if (status == MC_REGISTRY_NULL_REFERENCE)
            return null_reference(w, e, "Living death null Item bf receiver");
        return registry_failure(w, e, status, "Living death actual Item bf read", false);
    }
    return true;
}

static bool item_j(struct mc_mob_death_work *w, mc_entity_effect_error *e) {
    const mc_entity_class_kind kind = w->state.kind;
    if (kind == MC_ENTITY_CLASS_KIND_WOLF) {
        j_at(w, "gi", 1);
        w->state.drop_id = -1;
        return true;
    }
    if (kind == MC_ENTITY_CLASS_KIND_SLIME) {
        j_at(w, "uw", 1);
        int8_t size;
        mc_entity_watcher_byte_error read;
        const bool returned = mc_entity_watcher_byte_read_entered(w->actor, 16, &size, &read);
        if (!returned) {
            const bool receiver = read.stage == MC_ENTITY_WATCHER_BYTE_STAGE_FLAG;
            w->state.watcher_pc = e->watcher_bytecode = receiver ? 6 : read.pc;
            e->watcher_index = read.index;
            w->state.child =
                receiver ? site("uw", "v", "()I", 6) : site("ud", "a", "(I)B", read.pc);
            if (read.cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE)
                return null_reference(w, e, read.operation);
            if (read.cause == MC_ENTITY_EFFECT_CAUSE_CLASS_CAST)
                return exception(w, e, read.cause, "java/lang/ClassCastException", read.operation);
            return failure(w, e, read.status, read.cause, read.operation);
        }
        w->state.watcher_pc = 25;
        e->watcher_index = -1;
        e->watcher_bytecode = 0;
        w->state.child = site("uw", "v", "()I", 9);
        if (size != 1) {
            w->state.drop_id = 0;
            j_at(w, "uw", 16);
            return true;
        }
        j_at(w, "uw", 8);
        if (!item_id(w, "aK", e))
            return false;
        j_at(w, "uw", 14);
        return true;
    }
    const char *cl = kind == MC_ENTITY_CLASS_KIND_PIG       ? "wh"
                     : kind == MC_ENTITY_CLASS_KIND_COW     ? "bx"
                     : kind == MC_ENTITY_CLASS_KIND_ZOMBIE  ? "uz"
                     : kind == MC_ENTITY_CLASS_KIND_SPIDER  ? "cn"
                     : kind == MC_ENTITY_CLASS_KIND_CREEPER ? "gb"
                     : kind == MC_ENTITY_CLASS_KIND_GHAST   ? "bp"
                                                            : "ww";
    const bool burning = kind == MC_ENTITY_CLASS_KIND_PIG && w->actor->base.bv > 0;
    const char *field = kind == MC_ENTITY_CLASS_KIND_PIG       ? (burning ? "ap" : "ao")
                        : kind == MC_ENTITY_CLASS_KIND_COW     ? "aD"
                        : kind == MC_ENTITY_CLASS_KIND_SPIDER  ? "I"
                        : kind == MC_ENTITY_CLASS_KIND_CREEPER ? "K"
                        : kind == MC_ENTITY_CLASS_KIND_GHAST   ? "K"
                                                               : "J";
    const uint16_t getstatic_pc = kind == MC_ENTITY_CLASS_KIND_PIG ? (burning ? 7u : 14u) : 0u;
    j_at(w, cl, getstatic_pc);
    if (!item_id(w, field, e))
        return false;
    j_at(w, cl, (uint16_t)(getstatic_pc + 6u));
    return true;
}

static bool squid_q(struct mc_mob_death_work *w, mc_entity_effect_error *e) {
    q_at(w, "xt", 5);
    w->state.quantity_attempted = true;
    mc_java_random *random = w->actor->random;
    if (random == NULL)
        return null_reference(w, e, "Squid death null quantity Random receiver");
    int32_t quantity;
    if (!mc_java_random_next_bounded(random, 3, &quantity))
        return failure(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_RANDOM,
                       "Squid death actual quantity Random.nextInt(3)");
    w->state.quantity = mc_java_i32_add(quantity, 1);
    w->state.quantity_returned = true;
    for (w->state.index = 0; w->state.index < w->state.quantity; ++w->state.index) {
        q_at(w, "xt", 19);
        w->state.stack_allocated = w->state.stack_constructed = false;
        w->state.stack_status = mc_item_stack_allocate(&w->stack);
        if (w->state.stack_status != MC_ITEM_STACK_OK)
            return failure(w, e,
                           w->state.stack_status == MC_ITEM_STACK_ALLOCATION_FAILED
                               ? MC_ENTITY_ALLOCATION_FAILED
                               : MC_ENTITY_LIMIT,
                           MC_ENTITY_EFFECT_CAUSE_OWNER, "Squid death actual RAW Stack allocation");
        w->state.stack_allocated = true;
        q_at(w, "xt", 23);
        if (!registry(w, e))
            return false;
        const mc_item *item = NULL;
        mc_registry_status status =
            mc_registry_item_static_get(w->registry, "aU", &item, &e->registry);
        if (status != MC_REGISTRY_OK)
            return registry_failure(w, e, status, "Squid death actual Ink Sac Item getstatic",
                                    true);
        q_at(w, "xt", 28);
        w->state.child = site("iz", "<init>", "(Lgm;II)V", 2);
        status = mc_registry_item_source_id(w->registry, item, &w->state.drop_id, &e->registry);
        if (status != MC_REGISTRY_OK) {
            w->state.registry = e->registry;
            if (status == MC_REGISTRY_NULL_REFERENCE)
                return null_reference(w, e, "Squid death null Item Stack constructor bf receiver");
            return registry_failure(w, e, status, "Squid death actual Stack Item bf read", false);
        }
        w->state.stack_status = mc_item_stack_construct(w->stack, w->state.drop_id, 1, 0);
        if (w->state.stack_status != MC_ITEM_STACK_OK)
            return failure(w, e, MC_ENTITY_ITEM_FAILED, MC_ENTITY_EFFECT_CAUSE_OWNER,
                           "Squid death actual Item Stack numeric constructor");
        w->state.stack_constructed = true;
        q_at(w, "xt", 32);
        if (!drop(w, false, e))
            return false;
        mc_item_stack_release(w->stack);
        w->stack = NULL;
    }
    q_at(w, "xt", 42);
    w->state.q_returned = true;
    return true;
}

static bool skeleton_q(struct mc_mob_death_work *w, mc_entity_effect_error *e) {
    for (unsigned int loop = 0; loop < 2; ++loop) {
        const bool second = loop != 0;
        q_at(w, "fr", second ? 39u : 5u);
        bool *attempted =
            second ? &w->state.second_quantity_attempted : &w->state.quantity_attempted;
        bool *returned = second ? &w->state.second_quantity_returned : &w->state.quantity_returned;
        int32_t *quantity = second ? &w->state.second_quantity : &w->state.quantity;
        *attempted = true;
        mc_java_random *random = w->actor->random;
        if (random == NULL)
            return null_reference(w, e, "Skeleton death null quantity Random receiver");
        if (!mc_java_random_next_bounded(random, 3, quantity))
            return failure(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_RANDOM,
                           "Skeleton death actual quantity Random.nextInt(3)");
        *returned = true;
        for (w->state.index = 0; w->state.index < *quantity; ++w->state.index) {
            q_at(w, "fr", second ? 51u : 17u);
            if (!item_id(w, second ? "aV" : "j", e))
                return false;
            q_at(w, "fr", second ? 58u : 24u);
            w->state.drop_wrapper_pc = 4;
            w->state.child = site("sn", "b", "(II)Lhl;", 4);
            if (!drop(w, true, e))
                return false;
        }
    }
    q_at(w, "fr", 68);
    w->state.q_returned = true;
    return true;
}

static bool creeper_tail(struct mc_mob_death_work *w, mc_entity *source,
                         mc_entity_effect_error *e) {
    w->state.creeper_pc = 6;
    w->state.site = site("gb", "b", "(Lsn;)V", 6);
    if (source != NULL && mc_entity_is_kind(source, MC_ENTITY_CLASS_KIND_SKELETON)) {
        w->state.record_reached = true;
        w->state.creeper_pc = 13;
        w->state.site = site("gb", "b", "(Lsn;)V", 13);
        if (!item_id(w, "bd", e))
            return false;
        w->state.creeper_pc = 24;
        w->state.site = site("gb", "b", "(Lsn;)V", 24);
        w->state.second_quantity_attempted = true;
        mc_java_random *random = w->actor->random;
        if (random == NULL)
            return null_reference(w, e, "Creeper death null record Random receiver");
        if (!mc_java_random_next_bounded(random, 2, &w->state.second_quantity))
            return failure(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_RANDOM,
                           "Creeper death actual record Random.nextInt(2)");
        w->state.second_quantity_returned = true;
        w->state.drop_id = mc_java_i32_add(w->state.drop_id, w->state.second_quantity);
        w->state.creeper_pc = 29;
        w->state.site = site("gb", "b", "(Lsn;)V", 29);
        w->state.drop_wrapper_pc = 4;
        w->state.child = site("sn", "b", "(II)Lhl;", 4);
        if (!drop(w, true, e))
            return false;
    }
    w->state.creeper_pc = 33;
    w->state.site = site("gb", "b", "(Lsn;)V", 33);
    return true;
}

static bool living_q(struct mc_mob_death_work *w, mc_entity_effect_error *e) {
    q_at(w, "ls", 1);
    if (!item_j(w, e))
        return false;
    if (w->state.drop_id > 0) {
        q_at(w, "ls", 14);
        w->state.quantity_attempted = true;
        mc_java_random *random = w->actor->random;
        if (random == NULL)
            return null_reference(w, e, "Living death null quantity Random receiver");
        if (!mc_java_random_next_bounded(random, 3, &w->state.quantity))
            return failure(w, e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_RANDOM,
                           "Living death actual quantity Random.nextInt(3)");
        w->state.quantity_returned = true;
        for (w->state.index = 0; w->state.index < w->state.quantity; ++w->state.index) {
            q_at(w, "ls", 28);
            w->state.drop_wrapper_pc = 4;
            w->state.child = site("sn", "b", "(II)Lhl;", 4);
            if (!drop(w, true, e))
                return false;
        }
    }
    q_at(w, "ls", 38);
    w->state.q_returned = true;
    return true;
}

static bool body(struct mc_mob_death_work *w, mc_entity *source, mc_entity_effect_error *e) {
    death_at(w, e, 1);
    const mc_living_entity_fields *v = mc_entity_living_fields(w->actor);
    if (v->T >= 0 && source != NULL) {
        w->state.score = mc_entity_living_fields(w->actor)->T;
        if (!callback(w, source, true, e))
            return false;
    }
    if (source != NULL && !callback(w, source, false, e))
        return false;
    mc_entity_living_fields(w->actor)->ah = true;
    w->state.ah_stored = true;
    death_at(w, e, 38);
    mc_world_runtime *captured = w->actor->base.world;
    w->state.gate_world_identity = (uintptr_t)captured;
    if (!world(w, captured, e))
        return false;
    w->state.world_b_read = true;
    w->state.world_remote = captured->fields.B;
    if (!w->state.world_remote) {
        death_at(w, e, 45);
        w->state.q_entered = true;
        if (!(w->state.kind == MC_ENTITY_CLASS_KIND_SHEEP      ? sheep_q(w, e)
              : w->state.kind == MC_ENTITY_CLASS_KIND_SKELETON ? skeleton_q(w, e)
              : w->state.kind == MC_ENTITY_CLASS_KIND_SQUID    ? squid_q(w, e)
                                                               : living_q(w, e)))
            return false;
    }
    captured = w->actor->base.world; /* Actual independent World49 after q. */
    w->state.status_world_identity = (uintptr_t)captured;
    death_at(w, e, 54);
    if (!world(w, captured, e))
        return false;
    w->state.status_entered = true;
    w->state.child = site("fd", "a", "(Lsn;B)V", 0);
    /* Client FD's entire status body is the actual return0. */
    w->state.status_returned = true;
    death_at(w, e, 57);
    if (w->state.kind == MC_ENTITY_CLASS_KIND_CREEPER && !creeper_tail(w, source, e))
        return false;
    w->state.returned = true;
    return true;
}

bool mc_mob_death_inspect(const mc_entity *p, mc_mob_death_state *out) {
    if (p == NULL || out == NULL || !mc_entity_has_mob(p) || p->mob->pending_death == NULL)
        return false;
    *out = view(p->mob->pending_death);
    return true;
}

mc_entity_status mc_mob_death_cleanup(mc_entity *p, mc_entity_effect_error *e) {
    if (p == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (!mc_entity_has_mob(p) || p->mob->pending_death == NULL)
        return MC_ENTITY_OK;
    struct mc_mob_death_work *w = p->mob->pending_death;
    if (w->state.active) {
        (void)cleanup_failure(w, e, MC_ENTITY_BUSY, "Living death active invocation cleanup");
        return MC_ENTITY_BUSY;
    }
    const bool had_drop = w->drop != NULL;
    const mc_entity_status status = mc_entity_drop_retire(&w->drop, &w->drop_cleanup);
    if (status != MC_ENTITY_OK) {
        (void)cleanup_failure(w, e, status, "Living death retained Item drop cleanup");
        return status;
    }
    if (had_drop)
        ++w->state.drops_retired;
    w->drop_reached = false;
    w->state.drop_error = NULL;
    if (w->state.registry_held && !mc_registry_runtime_borrow_release(w->registry)) {
        (void)cleanup_failure(w, e, MC_ENTITY_BUSY, "Living death Registry occurrence cleanup");
        return MC_ENTITY_BUSY;
    }
    w->state.registry_held = false;
    mc_item_stack_release(w->stack);
    w->stack = NULL;
    if (w->source != NULL) {
        mc_entity_release(w->source);
        w->source = NULL;
        w->state.source_hold_released = true;
    }
    for (size_t i = 0; i < w->world_count; ++i)
        mc_world_runtime_binding_release(w->worlds[i]);
    w->world_count = 0;
    w->state.pending = false;
    w->state.receiver_held = false;
    w->state.cleanup_status = MC_ENTITY_OK;
    w->state.cleanup_operation = NULL;
    project(w, e);
    p->mob->pending_death = NULL;
    free(w);
    mc_entity_release(p); /* May be the receiver's last occurrence. No later read. */
    return MC_ENTITY_OK;
}

bool mc_mob_death_entered(mc_entity *p, mc_registry_classes *registry_value, mc_entity *source,
                          mc_entity_effect_error *e) {
    if (e == NULL)
        return false;
    if (p == NULL || !mc_entity_has_mob(p) || (!animal(p) && !monster(p) && !remaining_living(p)) ||
        p->phase != MC_ENTITY_READY || p->classes->source != MC_ENTITY_CLIENT || p->holds == 0) {
        e->status = MC_ENTITY_STATE_ERROR;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Living death entered canonical client owner";
        return false;
    }
    if (p->mob->pending_death != NULL) {
        e->status = MC_ENTITY_BUSY;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Living death existing pending work";
        project(p->mob->pending_death, e);
        return false;
    }
    struct mc_mob_death_work *w = calloc(1, sizeof(*w));
    if (w == NULL) {
        e->status = MC_ENTITY_ALLOCATION_FAILED;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Living death reached invocation work allocation";
        return false;
    }
    w->actor = p;
    w->registry = registry_value;
    w->state = (mc_mob_death_state){.kind = p->class_literal->kind,
                                    .receiver_identity = (uintptr_t)p,
                                    .entered = true,
                                    .native_policy = true,
                                    .site = {"ls", "b", "(Lsn;)V", 0},
                                    .child = {.pc = -1}};
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK) {
        (void)failure(w, e, held, MC_ENTITY_EFFECT_CAUSE_OWNER,
                      "Living death receiver self occurrence");
        free(w);
        return false;
    }
    w->state.receiver_held = w->state.pending = w->state.active = true;
    p->mob->pending_death = w;
    const bool returned = body(w, source, e);
    w->state.active = false;
    project(w, e);
    if (!returned)
        return false;
    return mc_mob_death_cleanup(p, e) == MC_ENTITY_OK;
}
