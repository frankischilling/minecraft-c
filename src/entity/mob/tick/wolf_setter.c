#include "internal.h"

/* Actual GI.d(Z)V and its existing typed UD Byte projection. This helper is
 * called only through the canonical shared native runner. The source watcher
 * is loaded afresh for getter6 and setter27/47; no fixture scope is fabricated. */
static mc_entity_watch_record *tame_record(mc_mob_tick_scope *s, bool setter,
                                           unsigned int invoke_pc) {
    s->error->child_site = (mc_mob_site){"gi", "d", "(Z)V", (int32_t)invoke_pc};
    mc_entity_watcher *watcher = s->entity->watcher;
    if (watcher == NULL) {
        mt_null(s, "Wolf tame null watcher invoke receiver");
        return NULL;
    }
    s->error->child_site =
        (mc_mob_site){"ud", setter ? "b" : "a", setter ? "(ILjava/lang/Object;)V" : "(I)B", 8};
    mc_entity_watch_map *map = watcher->map;
    if (map == NULL) {
        mt_null(s, "Wolf tame null watcher Map receiver");
        return NULL;
    }
    mc_entity_watch_record *record = NULL;
    size_t count = 0;
    for (mc_entity_watch_node *node = map->head; node != NULL; node = node->next) {
        if (count++ >= map->size) {
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf tame watcher Map native ownership");
            return NULL;
        }
        if (node->key == 16) {
            record = node->record;
            break;
        }
    }
    if (record == NULL) {
        s->error->child_site.pc = setter ? 19 : 16;
        mt_null(s, "Wolf tame null watcher record receiver");
        return NULL;
    }
    if (record->type != 0) {
        s->error->child_site.pc = setter ? 22 : 19;
        if (setter)
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf tame setter old boxed type representation");
        else if (record->type == 4 && record->string_value == NULL) {
            s->error->child_site.pc = 22;
            mt_null(s, "Wolf tame null Byte unbox receiver");
        } else if (record->type == 2 || record->type == 4) {
            s->error->source_exception = true;
            s->error->exception_class = "java/lang/ClassCastException";
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf tame actual non-Byte cast");
        } else
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf tame getter boxed value representation");
        return NULL;
    }
    return record;
}

bool mt_wolf_tame_entered(mc_mob_tick_scope *s, bool value) {
    mc_mob_tick_wolf_tame_state *r = &s->error->state.wolf_tame;
    r->entered = true;
    r->value = value;
    mt_at(s, "gi", "d", "(Z)V", 6);
    mc_entity_watch_record *old = tame_record(s, false, 6);
    if (old == NULL)
        return false;
    const int8_t previous = old->byte_value;
    r->previous = previous;
    const int8_t next = mc_java_i8_narrow(value ? previous | 4 : previous & -5);
    r->next = next;
    const unsigned int invoke = value ? 27u : 47u;
    r->setter_pc = invoke;
    mt_at(s, "gi", "d", "(Z)V", invoke);
    mc_entity_watch_record *current = tame_record(s, true, invoke);
    if (current == NULL)
        return false;
    /* Real Byte.equals controls both dirty stores. It is not a blanket flag
     * reset or a direct fixture mutation of the watcher record. */
    if (current->byte_value != next) {
        current->byte_value = next;
        current->dirty = true;
        s->entity->watcher->dirty = true;
    }
    mt_at(s, "gi", "d", "(Z)V", 50);
    r->returned = true;
    s->error->child_site = (mc_mob_site){.pc = -1};
    return true;
}
