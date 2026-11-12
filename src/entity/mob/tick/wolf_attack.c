#include "../../watcher_payload_internal.h"
#include "internal.h"

static void at(mc_mob_tick_scope *s, unsigned int pc) {
    mt_at(s, "gi", "a", "(Lsn;F)V", pc);
}

static bool returned(mc_mob_tick_scope *s) {
    at(s, 199);
    s->error->state.attack.returned = true;
    s->error->state.attack.caller_return_site = s->error->site;
    return true;
}

/* Actual GI.D()Z at caller184: UD.a(16)B, then mask4. Keep its null record,
 * checked Byte cast and unbox sites distinct from the caller and from the
 * later target damage dispatch. No tame setter or health mutation occurs. */
static bool tame(mc_mob_tick_scope *s, bool *out) {
    s->error->child_site = (mc_mob_site){"gi", "D", "()Z", 6};
    mc_entity_watcher *watcher = s->entity->watcher;
    if (watcher == NULL)
        return mt_null(s, "Wolf attack null tame watcher receiver");
    s->error->child_site = (mc_mob_site){"ud", "a", "(I)B", 8};
    mc_entity_watch_map *map = watcher->map;
    if (map == NULL)
        return mt_null(s, "Wolf attack null tame watcher Map receiver");
    const mc_entity_watch_record *record = NULL;
    size_t count = 0;
    for (mc_entity_watch_node *node = map->head; node != NULL; node = node->next) {
        if (count++ >= map->size)
            return mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf attack watcher native Map ownership");
        if (node->key == 16) {
            record = node->record;
            break;
        }
    }
    if (record == NULL) {
        s->error->child_site.pc = 16;
        return mt_null(s, "Wolf attack null tame watcher record receiver");
    }
    if (mc_entity_watch_payload_kind_of(record) != MC_ENTITY_WATCH_PAYLOAD_BYTE) {
        s->error->child_site.pc = 19;
        if (mc_entity_watch_payload_kind_of(record) == MC_ENTITY_WATCH_PAYLOAD_NULL) {
            s->error->child_site.pc = 22;
            return mt_null(s, "Wolf attack null tame Byte unbox receiver");
        }
        if (mc_entity_watch_payload_known(record)) {
            s->error->source_exception = true;
            s->error->exception_class = "java/lang/ClassCastException";
        }
        return mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf attack tame watcher actual Byte cast");
    }
    *out = (record->byte_value & 4) != 0;
    s->error->child_site = (mc_mob_site){0};
    return true;
}

/* Entered by the same real runner as II.f_. Actor and target have independent
 * native occurrences. Source fields remain live and are read in GI order;
 * no jump callback, cooldown gate, World query or Random substitute is added. */
bool mt_wolf_attack(mc_mob_tick_scope *s, mc_entity *target, float distance) {
    s->error->state.attack = (mc_mob_tick_attack_arguments){
        .stored = true, .target_identity = (uintptr_t)target, .distance = distance};
    at(s, 2);
    if (!mt_fp(s))
        return false;
    if (distance > 2.f && distance < 6.f) {
        int32_t choice;
        at(s, 19);
        if (!mt_int(s, 10, &choice))
            return false;
        if (choice == 0) {
            if (!s->entity->base.aX)
                return returned(s); /* Branch29 goes directly to199, never bite130. */
            at(s, 33);
            if (target == NULL)
                return mt_null(s, "Wolf pounce null target X field receiver");
            at(s, 40);
            if (!mt_fp(s))
                return false;
            const double dx = at_sub(target->base.x, s->entity->base.x);
            const double dz = at_sub(target->base.z, s->entity->base.z);
            const double squared = at_add(at_mul(dx, dx), at_mul(dz, dz));
            at(s, 62);
            if (!mt_math(s))
                return false;
            const float length = at_root(squared); /* in.a(D)F narrows sqrt exactly once. */
            at(s, 72);
            if (!mt_fp(s))
                return false;
            s->entity->base.velocity_x =
                at_add(at_mul(at_mul(at_div(dx, at_widen(length)), .5), .800000011920929),
                       at_mul(s->entity->base.velocity_x, .20000000298023224));
            s->entity->base.velocity_z =
                at_add(at_mul(at_mul(at_div(dz, at_widen(length)), .5), .800000011920929),
                       at_mul(s->entity->base.velocity_z, .20000000298023224));
            s->entity->base.velocity_y = .4000000059604645;
            return returned(s);
        }
    }

    at(s, 135);
    if (!mt_fp(s))
        return false;
    if (!(at_widen(distance) < 1.5))
        return returned(s);
    at(s, 140);
    if (target == NULL)
        return mt_null(s, "Wolf bite null target bounds field receiver");
    const mc_entity_bounds *target_bounds = target->bounds;
    at(s, 143);
    if (target_bounds == NULL)
        return mt_null(s, "Wolf bite null target maxY field receiver");
    const double target_max = target_bounds->max_y;
    const mc_entity_bounds *own_bounds = s->entity->bounds;
    at(s, 150);
    if (own_bounds == NULL)
        return mt_null(s, "Wolf bite null actor minY field receiver");
    if (!(target_max > own_bounds->min_y))
        return returned(s);
    target_bounds = target->bounds; /* Independent source field158. */
    at(s, 161);
    if (target_bounds == NULL)
        return mt_null(s, "Wolf bite null target minY field receiver");
    const double target_min = target_bounds->min_y;
    own_bounds = s->entity->bounds; /* Independent source field165. */
    at(s, 168);
    if (own_bounds == NULL)
        return mt_null(s, "Wolf bite null actor maxY field receiver");
    if (!(target_min < own_bounds->max_y))
        return returned(s);
    mt_fields(s)->ae = 20; /* Source178 occurs before tame184 and target hurt195. */
    at(s, 184);
    bool tamed;
    if (!tame(s, &tamed))
        return false;
    at(s, 195);
    if (!mt_attack_hurt(s, target, tamed ? 4 : 2))
        return false;
    return returned(s); /* The actual damage Boolean is discarded at198. */
}
