#include "../../watcher_payload_internal.h"
#include "internal.h"

static void at(mc_mob_tick_scope *s, bool unseen, unsigned int pc) {
    mt_at(s, "gb", unseen ? "b" : "a", "(Lsn;F)V", pc);
}

/* The valid source value is an actual Byte record. Getter failures distinguish
 * UD's Map/record/cast/unbox operations. Setter's inconsistent old boxed type
 * remains an explicit representation boundary, never a fabricated cast. */
static mc_entity_watch_record *record(mc_mob_tick_scope *s, int32_t key, bool setter) {
    s->error->child_site = (mc_mob_site){"gb",
                                         setter      ? "e"
                                         : key == 16 ? "v"
                                                     : "s",
                                         setter      ? "(I)V"
                                         : key == 16 ? "()I"
                                                     : "()Z",
                                         setter ? 11 : 6};
    mc_entity_watcher *watcher = s->entity->watcher;
    if (watcher == NULL) {
        mt_null(s, "Creeper null watcher invoke receiver");
        return NULL;
    }
    s->error->child_site =
        (mc_mob_site){"ud", setter ? "b" : "a", setter ? "(ILjava/lang/Object;)V" : "(I)B", 8};
    mc_entity_watch_map *map = watcher->map;
    if (map == NULL) {
        mt_null(s, "Creeper null watcher Map receiver");
        return NULL;
    }
    mc_entity_watch_record *value = NULL;
    size_t count = 0;
    for (mc_entity_watch_node *node = map->head; node != NULL; node = node->next) {
        if (count++ >= map->size) {
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Creeper watcher Map native ownership");
            return NULL;
        }
        if (node->key == key) {
            value = node->record;
            break;
        }
    }
    if (value == NULL) {
        s->error->child_site.pc = setter ? 19 : 16;
        mt_null(s, "Creeper null watcher record receiver");
        return NULL;
    }
    if (setter ? !mc_entity_watch_payload_known(value)
               : mc_entity_watch_payload_kind_of(value) != MC_ENTITY_WATCH_PAYLOAD_BYTE) {
        s->error->child_site.pc = setter ? 22 : 19;
        if (setter) {
            mt_fail(s, MC_ENTITY_STATE_ERROR,
                    "Creeper watcher setter old boxed type representation");
        } else if (mc_entity_watch_payload_kind_of(value) == MC_ENTITY_WATCH_PAYLOAD_NULL) {
            s->error->child_site.pc = 22;
            mt_null(s, "Creeper null watcher Byte unbox receiver");
        } else if (mc_entity_watch_payload_known(value)) {
            s->error->source_exception = true;
            s->error->exception_class = "java/lang/ClassCastException";
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Creeper watcher actual non-Byte value cast");
        } else {
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Creeper watcher getter boxed value representation");
        }
        return NULL;
    }
    return value;
}

static bool direction_get(mc_mob_tick_scope *s, int32_t key, int8_t *out) {
    mc_entity_watch_record *value = record(s, key, false);
    if (value == NULL)
        return false;
    *out = value->byte_value;
    s->error->child_site = (mc_mob_site){.pc = -1};
    return true;
}

static bool direction_set(mc_mob_tick_scope *s, int32_t input) {
    mc_entity_watch_record *value = record(s, 16, true);
    if (value == NULL)
        return false;
    const int8_t narrowed = mc_java_i8_narrow(input); /* Actual GB.e i2b7. */
    if (mc_entity_watch_byte_replace(value, narrowed))
        s->entity->watcher->dirty = true;
    s->error->child_site = (mc_mob_site){.pc = -1};
    return true;
}

static bool remote(mc_mob_tick_scope *s, bool unseen, bool *out) {
    mc_world_runtime *world = s->entity->base.world; /* Actual World field1. */
    at(s, unseen, 4);
    if (world == NULL)
        return mt_null(s, "Creeper attack null World remote field receiver");
    *out = world->fields.B;
    return true;
}

static void arguments(mc_mob_tick_scope *s, mc_entity *target, float distance, bool unseen) {
    s->error->state.attack = (mc_mob_tick_attack_arguments){.stored = true,
                                                            .unseen = unseen,
                                                            .target_identity = (uintptr_t)target,
                                                            .distance = distance};
}

static bool explosion(mc_mob_tick_scope *s, bool powered) {
    mc_entity *p = s->entity;
    mc_world_runtime *world = p->base.world; /* Fresh World90 or116. */
    const double x = p->base.x, y = p->base.y, z = p->base.z;
    const float radius = powered ? 6.0f : 3.0f;
    s->error->state.explosion =
        (mc_mob_tick_explosion_arguments){.stored = true,
                                          .invoke_reached = true,
                                          .world_identity = (uintptr_t)world,
                                          .source_identity = (uintptr_t)p,
                                          .x = x,
                                          .y = y,
                                          .z = z,
                                          .radius = radius};
    at(s, false, powered ? 108u : 134u);
    if (world == NULL)
        return mt_null(s, "Creeper null captured explosion World invoke receiver");
    s->error->child_site = (mc_mob_site){"fd", "a", "(Lsn;DDDF)Lqx;", 0};
    return mt_fail(s, MC_ENTITY_STATE_ERROR, "Creeper actual FD/QX explosion child");
}

bool mt_creeper_attack(mc_mob_tick_scope *s, mc_entity *target, float distance) {
    arguments(s, target, distance, false);
    bool is_remote;
    if (!remote(s, false, &is_remote))
        return false;
    if (is_remote) {
        at(s, false, 10);
        s->error->state.attack.returned = true;
        return true;
    }
    at(s, false, 12);
    int8_t direction;
    if (!direction_get(s, 16, &direction))
        return false;
    at(s, false, direction <= 0 ? 23u : 34u);
    if (!mt_fp(s))
        return false;
    const bool ignite = direction <= 0 ? distance < 3.0f : distance < 7.0f;
    mc_mob_entity_state *m = s->entity->mob;
    if (ignite) {
        at(s, false, 39);
        if (m->creeper_a == 0) {
            mc_world_runtime *world = s->entity->base.world; /* Captured46 before literal50. */
            const mc_registry_string *sound;
            at(s, false, 50);
            if (!mt_literal(s, "random.fuse", &sound))
                return false;
            at(s, false, 55);
            if (!mt_sound_prepared(s, world, sound, 1.0f, .5f))
                return false;
        }
        at(s, false, 60);
        if (!direction_set(s, 1))
            return false;
        m->creeper_a = mc_java_i32_add(m->creeper_a, 1); /* Fresh a65/store70. */
        at(s, false, 74);
        if (m->creeper_a >= 30) {
            at(s, false, 83);
            int8_t powered;
            if (!direction_get(s, 17, &powered))
                return false;
            /* No K139 or attacking144 store follows an unreturned child. */
            return explosion(s, powered == 1);
        }
        at(s, false, 144);
        m->creature_attacking = true;
    } else {
        at(s, false, 152);
        if (!direction_set(s, -1))
            return false;
        m->creeper_a = mc_java_i32_sub(m->creeper_a, 1); /* Fresh a157/store162. */
        if (m->creeper_a < 0)
            m->creeper_a = 0; /* Actual store174, only on the cooling branch. */
    }
    at(s, false, 177);
    s->error->state.attack.returned = true;
    return true;
}

bool mt_creeper_unseen(mc_mob_tick_scope *s, mc_entity *target, float distance) {
    arguments(s, target, distance, true);
    bool is_remote;
    if (!remote(s, true, &is_remote))
        return false;
    if (is_remote) {
        at(s, true, 10);
        s->error->state.attack.returned = true;
        return true;
    }
    mc_mob_entity_state *m = s->entity->mob;
    at(s, true, 12);
    if (m->creeper_a > 0) {
        at(s, true, 20);
        if (!direction_set(s, -1))
            return false;
        m->creeper_a = mc_java_i32_sub(m->creeper_a, 1); /* Actual store30. */
        if (m->creeper_a < 0)
            m->creeper_a = 0; /* Actual store42. */
    }
    at(s, true, 45);
    s->error->state.attack.returned = true;
    return true;
}
