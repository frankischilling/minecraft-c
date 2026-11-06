#include "../references_internal.h"
#include "internal.h"
#include "mc/world/runtime/presentation.h"
#include <stdlib.h>
#include <string.h>

void mc_mob_tick_error_init(mc_mob_tick_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->site.pc = e->child_site.pc = -1;
    e->registry.initializer_pc = e->registry.active_initializer_pc = -1;
}

void mt_at(mc_mob_tick_scope *s, const char *c, const char *m, const char *d, unsigned int pc) {
    s->error->site = (mc_mob_site){c, m, d, (int32_t)pc};
    s->error->state.source_pc = pc;
    if (strcmp(c, "ls") == 0)
        s->error->state.living_pc = pc;
    else if (strcmp(c, "ii") == 0)
        s->error->state.creature_pc = pc;
    else if (strcmp(c, "gz") == 0)
        s->error->state.monster_pc = pc;
    else
        s->error->state.concrete_pc = pc;
}

bool mt_fail(mc_mob_tick_scope *s, mc_entity_status status, const char *op) {
    s->error->status = status;
    s->error->operation = op;
    s->error->native_policy = !s->error->source_exception;
    return false;
}

bool mt_null(mc_mob_tick_scope *s, const char *op) {
    (void)mt_fail(s, MC_ENTITY_STATE_ERROR, op);
    s->error->native_policy = false;
    s->error->source_exception = true;
    s->error->exception_class = "java/lang/NullPointerException";
    return false;
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

void mt_registry_classify(mc_mob_tick_scope *s, const mc_registry_error *error) {
    const mc_registry_classes *classes = s->registry;
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
    const bool cached = error->status == MC_REGISTRY_FAILED_INITIALIZATION;
    const mc_registry_error *owner = block != NULL && same_cause(error, block, cached) ? block
                                     : item != NULL && same_cause(error, item, cached) ? item
                                                                                       : NULL;
    const char *exception = NULL;
    if (owner != NULL) {
        if (registry_source(owner->status) != NULL)
            exception =
                cached ? "java/lang/NoClassDefFoundError" : "java/lang/ExceptionInInitializerError";
        else if (owner->status == MC_REGISTRY_FAILED_INITIALIZATION) {
            /* A reached nested erroneous class propagates its Error unchanged.
             * Only the other actual base owner's immutable matching cause can
             * prove this source Error; unknown class latches remain native. */
            const mc_registry_error *nested = owner == block ? item : block;
            if (nested != NULL && same_cause(owner, nested, true) &&
                registry_source(nested->status) != NULL)
                exception = "java/lang/NoClassDefFoundError";
        }
    } else if (!cached && error->active_initializer_class == NULL)
        exception = registry_source(error->status);
    if (exception != NULL) {
        s->error->native_policy = false;
        s->error->source_exception = true;
        s->error->exception_class = exception;
    }
}

bool mt_brightness_fail(mc_mob_tick_scope *s, const char *operation) {
    (void)mt_fail(s, MC_ENTITY_WORLD_FAILED, operation);
    const mc_world_brightness_error *error = &s->error->detail.brightness;
    const char *exception = NULL;
    if (error->source_exception == MC_WORLD_BRIGHTNESS_SOURCE_NULL_DIMENSION ||
        error->source_exception == MC_WORLD_BRIGHTNESS_SOURCE_NULL_TABLE)
        exception = "java/lang/NullPointerException";
    else if (error->source_exception == MC_WORLD_BRIGHTNESS_SOURCE_INDEX)
        exception = "java/lang/ArrayIndexOutOfBoundsException";
    if (exception != NULL) {
        s->error->native_policy = false;
        s->error->source_exception = true;
        s->error->exception_class = exception;
    } else
        mt_registry_classify(s, &error->query.registry);
    return false;
}

bool mt_fp(mc_mob_tick_scope *s) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           mt_fail(s, MC_ENTITY_UNSUPPORTED_FP, "Mob tick floating controls");
}

bool mt_math(mc_mob_tick_scope *s) {
    if (s->math == NULL)
        return mt_fail(s, MC_ENTITY_STATE_ERROR, "Mob tick actual Math namespace");
    const mc_math_class_status status = mc_math_classes_ensure(s->math, &s->error->math);
    return status == MC_MATH_CLASS_OK ||
           mt_fail(s, status == MC_MATH_CLASS_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR,
                   "Mob tick Math active use");
}

bool mt_work(mc_mob_tick_scope *s) {
    if (s->work == s->max_work)
        return mt_fail(s, MC_ENTITY_LIMIT, "Mob pending iteration ceiling");
    ++s->work;
    s->error->state.work = s->work;
    return true;
}

bool mt_int(mc_mob_tick_scope *s, int32_t bound, int32_t *out) {
    if (s->entity->random == NULL)
        return mt_null(s, "Mob null private Random receiver");
    if (!mc_java_random_next_bounded(s->entity->random, bound, out))
        return mt_fail(s, MC_ENTITY_STATE_ERROR, "Mob private Random bound");
    ++s->error->state.concrete_random_calls;
    return true;
}

bool mt_float(mc_mob_tick_scope *s, float *out) {
    if (s->entity->random == NULL)
        return mt_null(s, "Mob null private Random receiver");
    *out = mc_java_random_next_float(s->entity->random);
    ++s->error->state.concrete_random_calls;
    return true;
}

bool mt_world(mc_mob_tick_scope *s, mc_world_runtime **out) {
    mc_world_runtime *w = s->entity->base.world;
    if (w == NULL)
        return mt_null(s, "Mob null World receiver");
    *out = w;
    return true;
}

bool mt_nearest(mc_mob_tick_scope *s, double radius, mc_entity **out) {
    mc_world_runtime *w;
    if (!mt_world(s, &w))
        return false;
    const double x = s->entity->base.x, y = s->entity->base.y, z = s->entity->base.z;
    if (mc_world_runtime_call_enter(w, &s->error->world) != MC_WORLD_STORAGE_OK)
        return mt_fail(s, MC_ENTITY_WORLD_FAILED, s->error->world.operation);
    mc_entity *best = NULL;
    double distance = -1.0;
    bool ok = false;
    for (int32_t i = 0;; i = mc_java_i32_add(i, 1)) {
        s->error->child_site = (mc_mob_site){"fd", "a", "(DDDD)Lgs;", 17};
        mc_world_reference_list *list = w->fields.d;
        if (list == NULL) {
            mt_null(s, "Mob nearest null Player List");
            break;
        }
        const mc_world_entity_list_ledger *ledger = &w->player_membership;
        if (list != ledger->list || list->count != ledger->count) {
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Mob nearest actual Player List ownership");
            break;
        }
        if (i >= 0 && (size_t)(uint32_t)i >= list->count) {
            ok = true;
            break;
        }
        s->error->child_site.pc = 31;
        if (!mt_work(s))
            break;
        if (i < 0 || list->items == NULL || list->items[i] != ledger->entries[i]) {
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Mob nearest actual Player occurrence");
            break;
        }
        mc_entity *p = ledger->entries[i];
        s->error->child_site.pc = 36;
        if (p != NULL && !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Mob nearest actual Player cast");
            break;
        }
        s->error->child_site.pc = 47;
        if (p == NULL) {
            mt_null(s, "Mob nearest null Player distance receiver");
            break;
        }
        if (!mt_fp(s))
            break;
        const double dx = at_sub(p->base.x, x), dy = at_sub(p->base.y, y);
        const double dz = at_sub(p->base.z, z);
        const double d = at_add(at_add(at_mul(dx, dx), at_mul(dy, dy)), at_mul(dz, dz));
        if ((radius < 0.0 || d < at_mul(radius, radius)) && (distance == -1.0 || d < distance)) {
            const mc_entity_status status = mc_entity_retain(p);
            if (status != MC_ENTITY_OK) {
                mt_fail(s, status, "Mob nearest Player local hold");
                break;
            }
            mc_entity_release(best);
            best = p;
            distance = d;
        }
    }
    mc_world_runtime_call_leave(w);
    if (!ok) {
        mc_entity_release(best);
        return false;
    }
    s->error->child_site = (mc_mob_site){0};
    *out = best;
    return true;
}

bool mt_literal(mc_mob_tick_scope *s, const char *text, const mc_registry_string **out) {
    if (s->registry == NULL || s->registry->entities != s->entity->classes)
        return mt_fail(s, MC_ENTITY_STATE_ERROR, "Mob actual code Registry namespace");
    const mc_registry_status status =
        mc_registry_runtime_literal_ascii(s->registry, text, out, &s->error->registry);
    if (status == MC_REGISTRY_OK)
        return true;
    (void)mt_fail(s, status == MC_REGISTRY_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR,
                  s->error->registry.message);
    mt_registry_classify(s, &s->error->registry);
    return false;
}

bool mt_sound(mc_mob_tick_scope *s, const char *text, float volume, float pitch) {
    mc_world_runtime *w = s->entity->base.world;
    const mc_registry_string *name;
    if (!mt_literal(s, text, &name))
        return false;
    return mt_sound_prepared(s, w, name, volume, pitch);
}

bool mt_sound_prepared(mc_mob_tick_scope *s, mc_world_runtime *w, const mc_registry_string *name,
                       float volume, float pitch) {
    if (w == NULL)
        return mt_null(s, "Mob null captured sound World receiver");
    s->error->child = MC_MOB_TICK_CHILD_SOUND;
    const mc_world_storage_status status =
        mc_world_runtime_entity_sound(w, s->entity, name, volume, pitch, &s->error->detail.sound);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_access_failure *failure = &s->error->detail.sound.world_access_failure;
        const char *exception = NULL;
        if (mc_world_access_failure_source_exception(failure, &exception)) {
            s->error->source_exception = true;
            s->error->exception_class = exception;
        }
        return mt_fail(s, MC_ENTITY_WORLD_FAILED, "Mob actual Entity sound child");
    }
    s->error->child = MC_MOB_TICK_CHILD_NONE;
    ++s->error->state.sounds;
    return true;
}

bool mt_particle(mc_mob_tick_scope *s, const char *text, double x, double y, double z, double vx,
                 double vy, double vz) {
    mc_world_runtime *w = s->entity->base.world;
    const mc_registry_string *name;
    if (!mt_literal(s, text, &name))
        return false;
    return mt_particle_prepared(s, w, name, x, y, z, vx, vy, vz);
}

bool mt_particle_prepared(mc_mob_tick_scope *s, mc_world_runtime *w, const mc_registry_string *name,
                          double x, double y, double z, double vx, double vy, double vz) {
    if (w == NULL)
        return mt_null(s, "Mob null captured particle World receiver");
    s->error->child = MC_MOB_TICK_CHILD_PRESENTATION;
    if (mc_world_runtime_particle(w, name, x, y, z, vx, vy, vz, &s->error->detail.presentation) !=
        MC_WORLD_STORAGE_OK) {
        const char *exception = NULL;
        if (mc_world_access_failure_source_exception(
                &s->error->detail.presentation.world_access_failure, &exception)) {
            s->error->source_exception = true;
            s->error->exception_class = exception;
        }
        return mt_fail(s, MC_ENTITY_WORLD_FAILED, "Mob actual particle child");
    }
    s->error->child = MC_MOB_TICK_CHILD_NONE;
    ++s->error->state.particles;
    return true;
}

bool mt_water(mc_mob_tick_scope *s, bool *out) {
    s->error->child = MC_MOB_TICK_CHILD_MATERIAL;
    const mc_entity_status status = mc_entity_environment_in_water(
        s->entity, s->registry,
        s->environment_limits == NULL ? NULL : &s->environment_limits->material, out,
        &s->error->detail.material);
    if (status != MC_ENTITY_OK)
        return mt_fail(s, status, "Mob actual water virtual");
    s->error->child = MC_MOB_TICK_CHILD_NONE;
    return true;
}

bool mt_lava(mc_mob_tick_scope *s, bool *out) {
    s->error->child = MC_MOB_TICK_CHILD_MATERIAL;
    const mc_entity_status status = mc_entity_environment_lava(
        s->entity, s->registry,
        s->environment_limits == NULL ? NULL : &s->environment_limits->material, out,
        &s->error->detail.material);
    if (status != MC_ENTITY_OK)
        return mt_fail(s, status, "Mob actual lava virtual");
    s->error->child = MC_MOB_TICK_CHILD_NONE;
    return true;
}

static mc_entity_watch_record *watch(mc_mob_tick_scope *s, int32_t key, int32_t type) {
    mc_entity_watcher *w = s->entity->watcher;
    if (w == NULL || w->map == NULL) {
        mt_null(s, "Mob watcher or Map null receiver");
        return NULL;
    }
    size_t count = 0;
    for (mc_entity_watch_node *n = w->map->head; n != NULL; n = n->next) {
        if (count++ >= w->map->size) {
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Mob watcher Map ownership");
            return NULL;
        }
        if (n->key == key) {
            if (n->record == NULL)
                mt_null(s, "Mob null watcher record");
            else if (n->record->type != type)
                mt_fail(s, MC_ENTITY_STATE_ERROR, "Mob watcher actual value type");
            else
                return n->record;
            return NULL;
        }
    }
    mt_null(s, "Mob absent watcher record");
    return NULL;
}

bool mt_watch_byte(mc_mob_tick_scope *s, int32_t key, int8_t *out) {
    mc_entity_watch_record *r = watch(s, key, 0);
    if (r == NULL)
        return false;
    *out = r->byte_value;
    return true;
}

bool mt_watch_byte_set(mc_mob_tick_scope *s, int32_t key, int8_t value) {
    mc_entity_watch_record *r = watch(s, key, 0);
    if (r == NULL)
        return false;
    if (r->byte_value != value) {
        r->byte_value = value;
        r->dirty = true;
        s->entity->watcher->dirty = true;
    }
    return true;
}

bool mt_watch_int_set(mc_mob_tick_scope *s, int32_t key, int32_t value) {
    mc_entity_watch_record *r = watch(s, key, 2);
    if (r == NULL)
        return false;
    if (r->integer_value != value) {
        r->integer_value = value;
        r->dirty = true;
        s->entity->watcher->dirty = true;
    }
    return true;
}

bool mt_watch_string(mc_mob_tick_scope *s, int32_t key, const mc_player_string **out) {
    mc_entity_watch_record *r = watch(s, key, 4);
    if (r == NULL)
        return false;
    *out = r->string_value;
    return true;
}

bool mt_kill(mc_mob_tick_scope *s) {
    if (mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_SLIME))
        return mt_fail(s, MC_ENTITY_STATE_ERROR, "Slime actual K splitting child");
    s->entity->base.be = true;
    return true;
}

bool mt_target_get(mc_mob_tick_scope *s, bool creature, mc_entity **out) {
    const mc_entity_status status =
        creature ? mc_mob_creature_target_get(s->entity, out, &s->error->reference)
                 : mc_mob_look_target_get(s->entity, out, &s->error->reference);
    return status == MC_ENTITY_OK || mt_fail(s, status, "Mob actual target field occurrence");
}

bool mt_target_store(mc_mob_tick_scope *s, bool creature, mc_entity *value) {
    const mc_entity_status status =
        creature ? mc_mob_creature_target_store(s->entity, value, &s->error->reference)
                 : mc_mob_look_target_store(s->entity, value, &s->error->reference);
    return status == MC_ENTITY_OK || mt_fail(s, status, "Mob actual target field store");
}
