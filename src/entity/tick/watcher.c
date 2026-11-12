#include "../watcher_payload_internal.h"
#include "tick_internal.h"

/* UD.b compares the new Byte with the independent old Object payload. */
static bool update(mc_entity *entity, int8_t value, mc_entity_tick_error *e) {
    mc_entity_watcher *watcher = entity->watcher; /* Reload at sn13/33. */
    e->watcher_index = 0;
    e->watcher_bytecode = 8;
    if (watcher == NULL)
        return mc_tick_null(e, "Flag setter null watcher receiver", e->flag_bytecode);
    mc_entity_watch_map *map = watcher->map;
    if (map == NULL)
        return mc_tick_null(e, "Watcher setter null Map receiver", 8);
    mc_entity_watch_node *node = map->head;
    size_t count = 0;
    while (node != NULL) {
        if (count++ == map->size)
            return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Watcher setter Map ownership", 8);
        if (node->key == 0)
            break;
        node = node->next;
    }
    mc_entity_watch_record *record = node == NULL ? NULL : node->record;
    e->watcher_bytecode = 19;
    if (record == NULL)
        return mc_tick_null(e, "Watcher setter null record b() receiver", 19);
    e->watcher_bytecode = 22;
    if (!mc_entity_watch_payload_known(record))
        return mc_tick_fail(e, MC_ENTITY_STATE_ERROR,
                            "Watcher setter unsupported current Object value projection", 22);
    if (mc_entity_watch_byte_replace(record, value)) {
        watcher->dirty = true; /* ud.b40. */
    }
    return true;
}

bool mc_tick_flag(mc_entity *entity, int32_t bit, bool value, mc_entity_tick_error *e) {
    e->source_class = "sn";
    e->source_method = "b(IZ)V";
    e->flag_bytecode = 5;
    int8_t old;
    if (!mc_tick_effect(e, mc_entity_effect_watcher_byte(entity, 0, &old, &e->effect),
                        "Flag typed Byte getter", 5))
        return false;
    const int32_t mask = mc_java_i32_shl(1, bit);
    const int32_t changed = value ? ((int32_t)old | mask) : ((int32_t)old & ~mask);
    e->flag_bytecode = value ? 27u : 49u;
    return update(entity, mc_java_i8_narrow(changed), e);
}

mc_entity_status mc_entity_tick_flag_set(mc_entity *entity, int32_t bit, bool value,
                                         mc_entity_tick_error *out) {
    mc_entity_tick_error frame;
    mc_entity_tick_error_init(&frame);
    if (mc_tick_enter(entity, &frame)) {
        (void)mc_tick_flag(entity, bit, value, &frame);
        mc_entity_release(entity);
    }
    return mc_tick_finish(&frame, out);
}
