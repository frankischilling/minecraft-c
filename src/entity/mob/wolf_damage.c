#include "wolf_damage_internal.h"

#include "../../item/render/internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../../world/runtime/world_access_internal.h"
#include "../arrow/references_internal.h"
#include "../bounds/bounds_internal.h"
#include "../player/player_internal.h"
#include "../watcher_payload_internal.h"
#include "damage_internal.h"
#include "death_internal.h"
#include "internal.h"
#include "mc/core/java/java.h"
#include "mc/core/java/string_case.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "references_internal.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

struct mc_wolf_damage_work {
    mc_wolf_damage_state state;
    mc_entity_effect_error error, child_cleanup;
    mc_entity *actor, *source;
    mc_world_runtime *world;
    mc_entity_bounds *bounds, *expanded;
    mc_entity_bounds_error bounds_error;
    mc_world_class_entity_list *wolves;
    mc_world_nearby_error nearby;
    mc_world_source_chunk_error source_chunk;
    mc_world_access_failure receipts[4];
    size_t receipt_count;
    struct mc_mob_death_work *parent_death;
    bool source_held, world_held, source_copied, query_failed;
};

static mc_wolf_damage_site site(const char *cl, const char *method, const char *descriptor,
                                unsigned int pc) {
    return (mc_wolf_damage_site){cl, method, descriptor, (uint16_t)pc};
}

static mc_wolf_damage_state view(const struct mc_wolf_damage_work *w) {
    mc_wolf_damage_state state = w->state;
    state.receiver = state.pending ? w->actor : NULL;
    state.world = state.pending ? w->world : NULL;
    state.bounds = state.pending ? w->bounds : NULL;
    state.expanded = state.pending ? w->expanded : NULL;
    state.wolves = state.pending ? w->wolves : NULL;
    state.parent_error = state.pending ? &w->error : NULL;
    state.nearby = state.pending && state.query_entered ? &w->nearby : NULL;
    state.bounds_error =
        state.pending && w->bounds_error.status != MC_ENTITY_OK ? &w->bounds_error : NULL;
    state.parent_cleanup_owned = w->parent_death != NULL;
    return state;
}

static void project(const struct mc_wolf_damage_work *w, mc_entity_effect_error *e) {
    if (e != NULL) {
        *e = w->error;
        e->wolf_damage = view(w);
    }
}

static void at(struct mc_wolf_damage_work *w, unsigned int pc) {
    w->state.site = site("gi", "a", "(Lsn;I)Z", pc);
    w->error.source_class = "gi";
    w->error.source_bytecode = w->error.damage_bytecode = (uint16_t)pc;
    w->error.living_damage.wrapper_pc = (uint16_t)pc;
}

static void child(struct mc_wolf_damage_work *w, const char *cl, const char *method,
                  const char *descriptor, unsigned int pc) {
    w->state.child = site(cl, method, descriptor, pc);
    w->error.living_damage.child_site.class_name = cl;
    w->error.living_damage.child_site.method = method;
    w->error.living_damage.child_site.descriptor = descriptor;
    w->error.living_damage.child_site.pc = (uint16_t)pc;
}

static bool fail(struct mc_wolf_damage_work *w, mc_entity_status status,
                 mc_entity_effect_cause cause, const char *operation) {
    w->state.status = w->error.status = status;
    w->state.operation = w->error.operation = operation;
    w->error.cause = cause;
    w->state.native_policy = w->error.living_damage.native_policy = true;
    w->state.source_exception = w->error.living_damage.source_exception = false;
    w->state.source_error = w->error.living_damage.source_error = false;
    w->state.exception_class = w->error.living_damage.exception_class = NULL;
    return false;
}

static bool exception(struct mc_wolf_damage_work *w, const char *name, const char *operation) {
    (void)fail(w, MC_ENTITY_STATE_ERROR,
               strcmp(name, "java/lang/NullPointerException") == 0
                   ? MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE
                   : MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_WATCHER_VALUE,
               operation);
    w->state.native_policy = w->error.living_damage.native_policy = false;
    w->state.source_exception = w->error.living_damage.source_exception = true;
    w->state.exception_class = w->error.living_damage.exception_class = name;
    return false;
}

static mc_entity_watch_record *watch(struct mc_wolf_damage_work *w, mc_entity *wolf, int32_t key,
                                     mc_entity_watch_payload_kind expected, bool setter) {
    const char *method = setter ? "b" : expected == MC_ENTITY_WATCH_PAYLOAD_STRING ? "c" : "a";
    const char *descriptor = setter                                       ? "(ILjava/lang/Object;)V"
                             : expected == MC_ENTITY_WATCH_PAYLOAD_STRING ? "(I)Ljava/lang/String;"
                                                                          : "(I)B";
    if (wolf->watcher == NULL) {
        (void)exception(w, "java/lang/NullPointerException",
                        "Wolf damage null DataWatcher receiver");
        return NULL;
    }
    child(w, "ud", method, descriptor, 8);
    if (wolf->watcher->map == NULL) {
        (void)exception(w, "java/lang/NullPointerException",
                        "Wolf damage null watcher Map receiver");
        return NULL;
    }
    size_t seen = 0;
    for (mc_entity_watch_node *node = wolf->watcher->map->head; node != NULL; node = node->next) {
        if (seen++ >= wolf->watcher->map->size) {
            (void)fail(w, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                       "Wolf damage watcher Map owning extent");
            return NULL;
        }
        if (node->key != key)
            continue;
        child(w, "ud", method, descriptor, setter ? 19u : 16u);
        if (node->record == NULL) {
            (void)exception(w, "java/lang/NullPointerException", "Wolf damage null watcher record");
            return NULL;
        }
        child(w, "ud", method, descriptor, setter ? 22u : 19u);
        if (setter) {
            if (!mc_entity_watch_payload_known(node->record)) {
                (void)fail(w, MC_ENTITY_STATE_ERROR,
                           MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_WATCHER_VALUE,
                           "Wolf damage unrepresented old watcher Object");
                return NULL;
            }
        } else {
            const mc_entity_watch_payload_result status =
                mc_entity_watch_payload_check(node->record, expected);
            if (status != MC_ENTITY_WATCH_PAYLOAD_OK) {
                if (status == MC_ENTITY_WATCH_PAYLOAD_NULL_UNBOX) {
                    child(w, "ud", method, descriptor, 22);
                    (void)exception(w, "java/lang/NullPointerException",
                                    "Wolf damage null Byte unbox");
                } else if (status == MC_ENTITY_WATCH_PAYLOAD_WRONG_CLASS)
                    (void)exception(w, "java/lang/ClassCastException",
                                    "Wolf damage typed watcher cast");
                else
                    (void)fail(w, MC_ENTITY_STATE_ERROR,
                               MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_WATCHER_VALUE,
                               "Wolf damage unrepresented watcher Object");
                return NULL;
            }
        }
        return node->record;
    }
    child(w, "ud", method, descriptor, setter ? 19u : 16u);
    (void)exception(w, "java/lang/NullPointerException", "Wolf damage absent watcher record");
    return NULL;
}

static bool flag(struct mc_wolf_damage_work *w, mc_entity *wolf, const char *method, int mask,
                 bool *out) {
    child(w, "gi", method, "()Z", 6);
    mc_entity_watch_record *record = watch(w, wolf, 16, MC_ENTITY_WATCH_PAYLOAD_BYTE, false);
    if (record == NULL)
        return false;
    *out = (record->byte_value & mask) != 0;
    return true;
}

static bool setter(struct mc_wolf_damage_work *w, mc_entity *wolf, bool angry, bool value) {
    const char *method = angry ? "c" : "b";
    child(w, "gi", method, "(Z)V", 6);
    mc_entity_watch_record *old = watch(w, wolf, 16, MC_ENTITY_WATCH_PAYLOAD_BYTE, false);
    if (old == NULL)
        return false;
    const int mask = angry ? 2 : 1;
    const int8_t next = mc_java_i8_narrow(value ? old->byte_value | mask : old->byte_value & ~mask);
    child(w, "gi", method, "(Z)V", value ? 27u : 47u);
    mc_entity_watch_record *fresh = watch(w, wolf, 16, MC_ENTITY_WATCH_PAYLOAD_BYTE, true);
    if (fresh == NULL)
        return false;
    if (mc_entity_watch_byte_replace(fresh, next))
        wolf->watcher->dirty = true;
    return true;
}

static bool instance(struct mc_wolf_damage_work *w, mc_entity *value, mc_entity_class_kind kind,
                     bool *out) {
    *out = false;
    if (value == NULL)
        return true;
    if (value->classes != w->actor->classes || value->holds == 0 ||
        value->phase != MC_ENTITY_READY || value->class_literal == NULL ||
        !mc_entity_is_kind(value, value->class_literal->kind))
        return fail(w, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Wolf damage reached actual source Class owner");
    const mc_entity_class_literal *type = NULL;
    if (mc_entity_class_literal_get(w->actor->classes, kind, &type) != MC_ENTITY_OK ||
        mc_entity_class_literal_is_assignable_from(type, value->class_literal, out) != MC_ENTITY_OK)
        return fail(w, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Wolf damage reached Class.isAssignableFrom");
    return true;
}

static bool source_hold(struct mc_wolf_damage_work *w) {
    if (w->source == NULL || w->source_held)
        return true;
    const mc_entity_status status = mc_entity_retain(w->source);
    if (status != MC_ENTITY_OK)
        return fail(w, status, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Wolf damage current source local occurrence");
    w->source_held = true;
    return true;
}

static bool target_store(struct mc_wolf_damage_work *w, mc_entity *wolf) {
    const mc_entity_status status = mc_mob_creature_target_store(wolf, w->source, &w->error.entity);
    return status == MC_ENTITY_OK || fail(w, status, MC_ENTITY_EFFECT_CAUSE_OWNER,
                                          "Wolf damage reached Creature target store");
}

static bool capture_query(struct mc_wolf_damage_work *w) {
    if (!w->query_failed)
        return true;
    mc_world_nearby_error *error = &w->nearby;
    if (error->query.source_chunk != NULL && !w->source_copied) {
        w->source_chunk = *error->query.source_chunk;
        w->source_copied = true;
        error->query.source_chunk = &w->source_chunk;
    }
    const mc_world_access_failure *receipts[] = {
        &error->batch.publication.world_access_failure, &error->tile_entity.world_access_failure,
        &error->tile_entity.publication.world_access_failure,
        &error->tile_entity.batch.publication.world_access_failure};
    for (size_t i = 0; i < sizeof(receipts) / sizeof(*receipts); ++i) {
        if (receipts[i]->receipt == NULL)
            continue;
        bool held = false;
        for (size_t j = 0; j < w->receipt_count; ++j)
            if (w->receipts[j].receipt == receipts[i]->receipt)
                held = true;
        if (held)
            continue;
        if (w->receipt_count == sizeof(w->receipts) / sizeof(*w->receipts)) {
            w->state.cleanup_status = MC_ENTITY_LIMIT;
            w->state.cleanup_operation = "Wolf damage query receipt extent";
            return false;
        }
        const mc_world_storage_status status =
            mc_world_access_failure_retain(receipts[i], &w->receipts[w->receipt_count]);
        if (status != MC_WORLD_STORAGE_OK) {
            w->state.cleanup_status =
                status == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR;
            w->state.cleanup_operation = "Wolf damage actual query receipt capture";
            return false;
        }
        ++w->receipt_count;
    }
    return true;
}

static void classify_query(struct mc_wolf_damage_work *w) {
    const mc_world_nearby_error *error = &w->nearby;
    const char *name = NULL;
    if (error->query.registry.status != MC_REGISTRY_OK &&
        mc_world_runtime_registry_status(error->query.registry.status) == error->query.status) {
        mc_item_render_throwable kind;
        mc_item_render_registry_classification(mc_world_runtime_registry(w->world),
                                               &error->query.registry, &kind, &name);
    }
    if (w->source_copied && w->source_chunk.storage.status == error->query.status &&
        w->source_chunk.storage.operation == error->query.operation &&
        w->source_chunk.source.status != MC_WORLD_SOURCE_OK &&
        w->source_chunk.source.source_exception && !w->source_chunk.source.native_policy &&
        w->source_chunk.source.exception_class != NULL)
        name = w->source_chunk.source.exception_class;

    const struct {
        const mc_world_access_failure *receipt;
        mc_world_storage_status status;
        const char *operation;
    } receipts[] = {{&error->batch.publication.world_access_failure,
                     error->batch.publication.status, error->batch.publication.operation},
                    {&error->tile_entity.world_access_failure, error->tile_entity.status,
                     error->tile_entity.operation},
                    {&error->tile_entity.publication.world_access_failure,
                     error->tile_entity.publication.status,
                     error->tile_entity.publication.operation},
                    {&error->tile_entity.batch.publication.world_access_failure,
                     error->tile_entity.batch.publication.status,
                     error->tile_entity.batch.publication.operation}};

    for (size_t i = 0; name == NULL && i < sizeof(receipts) / sizeof(*receipts); ++i)
        if (receipts[i].status == error->query.status &&
            receipts[i].operation == error->query.operation)
            (void)mc_world_access_failure_source_exception(receipts[i].receipt, &name);
    if (name != NULL) {
        w->state.exception_class = w->error.living_damage.exception_class = name;
        w->state.native_policy = w->error.living_damage.native_policy = false;
        const size_t length = strlen(name);
        w->state.source_error = w->error.living_damage.source_error =
            length >= 5 && strcmp(name + length - 5, "Error") == 0;
        w->state.source_exception = w->error.living_damage.source_exception =
            !w->state.source_error;
    }
}

static bool group(struct mc_wolf_damage_work *w) {
    mc_entity *wolf = w->actor;
    at(w, 101);
    w->world = wolf->base.world;
    w->state.world_identity = (uintptr_t)w->world;
    if (w->world != NULL) {
        if (!mc_world_runtime_binding_retain(w->world))
            return fail(w, MC_ENTITY_LIMIT, MC_ENTITY_EFFECT_CAUSE_OWNER,
                        "Wolf damage captured query World occurrence");
        w->world_held = true;
    }
    const mc_entity_class_literal *wolf_class = NULL;
    at(w, 105);
    if (mc_entity_class_literal_get(wolf->classes, MC_ENTITY_CLASS_KIND_WOLF, &wolf_class) !=
        MC_ENTITY_OK)
        return fail(w, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Wolf damage Wolf Class literal");
    at(w, 124);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(w, MC_ENTITY_UNSUPPORTED_FP, MC_ENTITY_EFFECT_CAUSE_FP,
                    "Wolf damage strict group bounds arithmetic");
    const double x0 = wolf->base.x, y0 = wolf->base.y, z0 = wolf->base.z;
    const double x1 = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(wolf->base.x), _mm_set_sd(1.0)));
    const double y1 = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(wolf->base.y), _mm_set_sd(1.0)));
    const double z1 = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(wolf->base.z), _mm_set_sd(1.0)));
    at(w, 137);
    mc_entity_status status = mc_entity_bounds_pool_get(wolf->classes, x0, y0, z0, x1, y1, z1,
                                                        &w->bounds, &w->bounds_error);
    if (status != MC_ENTITY_OK)
        return fail(w, status, MC_ENTITY_EFFECT_CAUSE_OWNER, "Wolf damage actual pooled bounds");
    at(w, 149);
    status = mc_entity_bounds_expand(wolf->classes, w->bounds, 16.0, 4.0, 16.0, &w->expanded,
                                     &w->bounds_error);
    if (status != MC_ENTITY_OK)
        return fail(w, status, MC_ENTITY_EFFECT_CAUSE_OWNER, "Wolf damage actual bounds expansion");
    at(w, 152);
    if (w->world == NULL)
        return exception(w, "java/lang/NullPointerException",
                         "Wolf damage null captured query World");
    w->state.query_entered = true;
    const mc_world_storage_status queried =
        mc_world_runtime_class_entities(w->world, wolf_class, w->expanded, &w->wolves, &w->nearby);
    if (queried != MC_WORLD_STORAGE_OK) {
        w->query_failed = true;
        (void)fail(w, MC_ENTITY_WORLD_FAILED, MC_ENTITY_EFFECT_CAUSE_WORLD,
                   "Wolf damage actual Wolf class query");
        (void)capture_query(w);
        classify_query(w);
        return false;
    }
    w->state.query_returned = true;
    at(w, 157);
    size_t count;
    if (mc_world_class_entity_list_count(w->wolves, &count) != MC_WORLD_STORAGE_OK)
        return fail(w, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Wolf damage actual result List iterator");
    for (size_t i = 0; i < count; ++i) {
        mc_entity *other = NULL, *present = NULL;
        at(w, 176);
        if (mc_world_class_entity_list_get(w->wolves, i, &other) != MC_WORLD_STORAGE_OK ||
            !mc_entity_is_kind(other, MC_ENTITY_CLASS_KIND_WOLF))
            return fail(w, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                        "Wolf damage actual Wolf List occurrence");
        if (w->state.wolves_examined != SIZE_MAX)
            ++w->state.wolves_examined;
        bool tame;
        at(w, 195);
        if (!flag(w, other, "D", 4, &tame))
            return false;
        if (tame)
            continue;
        at(w, 203);
        if (mc_mob_creature_target_get(other, &present, &w->error.entity) != MC_ENTITY_OK)
            return fail(w, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                        "Wolf damage actual queried target occurrence");
        if (present != NULL)
            continue;
        at(w, 212);
        if (!target_store(w, other))
            return false;
        if (w->state.wolves_alerted != SIZE_MAX)
            ++w->state.wolves_alerted;
        bool player;
        at(w, 216);
        if (!instance(w, w->source, MC_ENTITY_CLASS_KIND_PLAYER, &player))
            return false;
        if (player) {
            at(w, 225);
            if (!setter(w, other, true, true))
                return false;
        }
    }
    return true;
}

static bool same_owner(struct mc_wolf_damage_work *w, bool *out) {
    /* GS.l261 is loaded before the actual GI.A265 argument. */
    if (w->source->local_player == NULL)
        return fail(w, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Wolf damage reached concrete Player name storage");
    const mc_player_string *name = w->source->local_player->fields.player.l;
    child(w, "gi", "A", "()Ljava/lang/String;", 6);
    mc_entity_watch_record *record = watch(w, w->actor, 17, MC_ENTITY_WATCH_PAYLOAD_STRING, false);
    if (record == NULL)
        return false;
    const mc_player_string *owner = mc_entity_watch_string_value(record);
    child(w, "java/lang/String", "equalsIgnoreCase", "(Ljava/lang/String;)Z", 0);
    if (name == NULL)
        return exception(w, "java/lang/NullPointerException",
                         "Wolf damage null Player name receiver");
    mc_player_string_view left = {0}, right = {0};
    if (!mc_player_string_read(name, &left) ||
        (owner != NULL && !mc_player_string_read(owner, &right)))
        return fail(w, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Wolf damage actual owner String occurrence");
    *out = owner != NULL &&
           mc_java_string_equals_ignore_case(left.units, left.length, right.units, right.length);
    return true;
}

static bool body(struct mc_wolf_damage_work *w, mc_registry_classes *registry,
                 mc_math_classes *math, bool *out) {
    at(w, 2);
    if (!setter(w, w->actor, false, false))
        return false;
    w->state.sitting_cleared = true;
    int32_t amount = w->state.amount;
    if (w->source != NULL) {
        bool player, arrow;
        at(w, 10);
        if (!instance(w, w->source, MC_ENTITY_CLASS_KIND_PLAYER, &player) || !source_hold(w))
            return false;
        if (!player) {
            at(w, 17);
            if (!instance(w, w->source, MC_ENTITY_CLASS_KIND_ARROW, &arrow))
                return false;
            if (!arrow)
                amount = mc_java_i32_add(amount, 1) / 2;
        }
    }
    w->state.parent_amount = amount;
    at(w, 32);
    w->state.parent_entered = true;
    const bool parent = mc_mob_damage_living_parent_entered(
        w->actor, registry, math, w->source, amount, &w->state.parent_result, &w->error);
    w->state.child =
        site(w->error.living_damage.child_site.class_name, w->error.living_damage.child_site.method,
             w->error.living_damage.child_site.descriptor, w->error.living_damage.child_site.pc);
    if (w->actor->mob->pending_death != NULL) {
        w->parent_death = w->actor->mob->pending_death;
        w->state.parent_cleanup_owned = true;
    }
    if (!parent) {
        w->state.status = w->error.status;
        w->state.operation = w->error.operation;
        w->state.native_policy = w->error.living_damage.native_policy;
        w->state.source_exception = w->error.living_damage.source_exception;
        w->state.source_error = w->error.living_damage.source_error;
        w->state.exception_class = w->error.living_damage.exception_class;
        return false;
    }
    w->state.parent_returned = true;
    w->error.living_damage.living_returned = true;
    if (!w->state.parent_result) {
        at(w, 284);
        *out = false;
        return true;
    }
    bool tame, angry;
    at(w, 39);
    if (!flag(w, w->actor, "D", 4, &tame))
        return false;
    if (!tame) {
        at(w, 46);
        if (!flag(w, w->actor, "C", 2, &angry))
            return false;
    } else
        angry = false;
    if (!tame && !angry) {
        bool player;
        at(w, 53);
        if (!instance(w, w->source, MC_ENTITY_CLASS_KIND_PLAYER, &player))
            return false;
        if (player) {
            at(w, 61);
            if (!setter(w, w->actor, true, true))
                return false;
            at(w, 66);
            if (!target_store(w, w->actor))
                return false;
        }
        bool arrow;
        at(w, 70);
        if (!instance(w, w->source, MC_ENTITY_CLASS_KIND_ARROW, &arrow))
            return false;
        if (arrow) {
            mc_entity *shooter = NULL;
            at(w, 80);
            if (mc_arrow_shooter_get(w->source, &shooter, &w->error.entity) != MC_ENTITY_OK)
                return fail(w, w->error.entity.status, MC_ENTITY_EFFECT_CAUSE_OWNER,
                            "Wolf damage first Arrow shooter occurrence");
            if (shooter != NULL) {
                at(w, 90);
                if (mc_arrow_shooter_get(w->source, &shooter, &w->error.entity) != MC_ENTITY_OK)
                    return fail(w, w->error.entity.status, MC_ENTITY_EFFECT_CAUSE_OWNER,
                                "Wolf damage second Arrow shooter occurrence");
                if (shooter != NULL) {
                    const mc_entity_status held = mc_entity_retain(shooter);
                    if (held != MC_ENTITY_OK)
                        return fail(w, held, MC_ENTITY_EFFECT_CAUSE_OWNER,
                                    "Wolf damage resolved shooter local occurrence");
                }
                mc_entity *old = w->source;
                const bool old_held = w->source_held;
                w->source = shooter;
                w->source_held = shooter != NULL;
                w->state.source_identity = (uintptr_t)shooter;
                if (old_held)
                    mc_entity_release(old);
            }
        }
        bool living;
        at(w, 95);
        if (!instance(w, w->source, MC_ENTITY_CLASS_KIND_LIVING_ENTITY, &living))
            return false;
        if (living && !group(w))
            return false;
    } else if (w->source != NULL && w->source != w->actor) {
        at(w, 244);
        if (!flag(w, w->actor, "D", 4, &tame))
            return false;
        if (tame) {
            bool player;
            at(w, 251);
            if (!instance(w, w->source, MC_ENTITY_CLASS_KIND_PLAYER, &player))
                return false;
            if (player) {
                bool same;
                at(w, 268);
                if (!same_owner(w, &same))
                    return false;
                if (same) {
                    at(w, 275);
                    *out = true;
                    return true;
                }
            }
        }
        at(w, 278);
        if (!target_store(w, w->actor))
            return false;
    }
    at(w, 282);
    *out = true;
    return true;
}

bool mc_mob_wolf_damage_inspect(const mc_entity *p, mc_wolf_damage_state *out) {
    if (p == NULL || out == NULL || !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_WOLF) ||
        p->mob == NULL || p->mob->pending_wolf_damage == NULL)
        return false;
    *out = view(p->mob->pending_wolf_damage);
    return true;
}

static mc_entity_status cleanup_failed(struct mc_wolf_damage_work *w, mc_entity_effect_error *out,
                                       mc_entity_status status, const char *operation) {
    w->state.cleanup_status = status;
    w->state.cleanup_operation = operation;
    project(w, out);
    if (out != NULL) {
        out->status = status;
        out->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        out->operation = operation;
    }
    return status;
}

mc_entity_status mc_mob_wolf_damage_cleanup(mc_entity *p, mc_entity_effect_error *out) {
    if (p == NULL || !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_WOLF) || p->mob == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    struct mc_wolf_damage_work *w = p->mob->pending_wolf_damage;
    if (w == NULL)
        return MC_ENTITY_OK;
    if (w->state.active)
        return cleanup_failed(w, out, MC_ENTITY_BUSY, "Wolf active hurt invocation cleanup");
    if (w->parent_death != NULL) {
        if (p->mob->pending_death != w->parent_death)
            return cleanup_failed(w, out, MC_ENTITY_STATE_ERROR,
                                  "Wolf damage exact newly acquired parent death identity");
        mc_entity_effect_error_init(&w->child_cleanup);
        const mc_entity_status status = mc_mob_death_cleanup(p, &w->child_cleanup);
        w->error.death = w->child_cleanup.death;
        if (status != MC_ENTITY_OK)
            return cleanup_failed(w, out, status, "Wolf damage parent death checked cleanup");
        w->parent_death = NULL;
    }
    if (!capture_query(w))
        return cleanup_failed(w, out, w->state.cleanup_status, w->state.cleanup_operation);
    const mc_entity_bounds *boxes[] = {w->bounds, w->expanded};
    const size_t box_count = (size_t)(boxes[0] != NULL) + (size_t)(boxes[1] != NULL);
    for (size_t i = 0; i < 2; ++i)
        if (boxes[i] != NULL && (mc_entity_bounds_native_holds(p->classes, boxes[i]) <
                                     1u + (size_t)(boxes[0] == boxes[1]) ||
                                 p->classes->bounds_holds < box_count))
            return cleanup_failed(w, out, MC_ENTITY_STATE_ERROR,
                                  "Wolf damage retained bounds owning occurrences");
    if (p->holds < 1u + (size_t)(w->source_held && w->source == p) ||
        (w->source_held && w->source->holds == 0) ||
        (w->world_held && w->world->persistent_bindings == 0))
        return cleanup_failed(w, out, MC_ENTITY_STATE_ERROR,
                              "Wolf damage retained actor/source/World occurrences");
    for (size_t i = 0; i < w->receipt_count; ++i)
        if (w->receipts[i].receipt->holds < 2 ||
            w->receipts[i].receipt->receiver != w->receipts[i].receiver ||
            w->receipts[i].receipt->render_global != w->receipts[i].render_global ||
            w->receipts[i].receiver->receipt_holds == 0)
            return cleanup_failed(w, out, MC_ENTITY_STATE_ERROR,
                                  "Wolf damage retained query receipt occurrence");
    mc_world_class_entity_list_release(w->wolves);
    if (w->expanded != NULL)
        (void)mc_entity_bounds_release(p->classes, w->expanded);
    if (w->bounds != NULL)
        (void)mc_entity_bounds_release(p->classes, w->bounds);
    for (size_t i = 0; i < w->receipt_count; ++i)
        mc_world_access_failure_release(&w->receipts[i]);
    if (w->world_held)
        mc_world_runtime_binding_release(w->world);
    w->state.pending = false;
    w->state.cleanup_status = MC_ENTITY_OK;
    w->state.cleanup_operation = NULL;
    project(w, out);
    p->mob->pending_wolf_damage = NULL;
    if (w->source_held)
        mc_entity_release(w->source);
    free(w);
    mc_entity_release(p);
    return MC_ENTITY_OK;
}

bool mc_mob_wolf_damage_entered(mc_entity *p, mc_registry_classes *registry, mc_math_classes *math,
                                mc_entity *source, int32_t amount, bool *out,
                                mc_entity_effect_error *e) {
    if (e == NULL)
        return false;
    if (p == NULL || !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_WOLF) || !mc_entity_has_mob(p) ||
        p->phase != MC_ENTITY_READY || p->holds == 0 || p->classes->source != MC_ENTITY_CLIENT ||
        out == NULL || p->mob->pending_wolf_damage != NULL || p->mob->pending_death != NULL) {
        e->status = MC_ENTITY_STATE_ERROR;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Wolf damage entered canonical client owner/fresh invocation";
        return false;
    }
    struct mc_wolf_damage_work *w = calloc(1, sizeof(*w));
    if (w == NULL) {
        e->status = MC_ENTITY_ALLOCATION_FAILED;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Wolf damage reached retained invocation allocation";
        return false;
    }
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK) {
        free(w);
        e->status = held;
        e->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        e->operation = "Wolf damage actual receiver self occurrence";
        return false;
    }
    w->actor = p;
    w->source = source;
    w->error = *e;
    w->error.damage_arguments_stored = true;
    w->error.damage_source_identity = (uintptr_t)source;
    w->error.damage_amount = amount;
    w->state = (mc_wolf_damage_state){.entered = true,
                                      .pending = true,
                                      .active = true,
                                      .receiver_identity = (uintptr_t)p,
                                      .argument_source_identity = (uintptr_t)source,
                                      .source_identity = (uintptr_t)source,
                                      .amount = amount};
    mc_world_nearby_error_init(&w->nearby);
    p->mob->pending_wolf_damage = w;
    bool answer = false;
    const bool completed = body(w, registry, math, &answer);
    w->state.active = false;
    w->state.returned = completed;
    project(w, e);
    if (!completed)
        return false;
    if (mc_mob_wolf_damage_cleanup(p, e) != MC_ENTITY_OK)
        return false;
    e->living_damage.returned = true;
    e->living_damage.result_stored = true;
    e->living_damage.result = answer;
    *out = answer;
    return true;
}
