#ifndef MC_ENTITY_WATCHER_PAYLOAD_INTERNAL_H
#define MC_ENTITY_WATCHER_PAYLOAD_INTERNAL_H

#include "entity_internal.h"

typedef enum mc_entity_watch_payload_result {
    MC_ENTITY_WATCH_PAYLOAD_OK,
    MC_ENTITY_WATCH_PAYLOAD_NULL_UNBOX,
    MC_ENTITY_WATCH_PAYLOAD_WRONG_CLASS,
    MC_ENTITY_WATCH_PAYLOAD_UNREPRESENTED
} mc_entity_watch_payload_result;

/* ma.a is the declared type; ma.c is an independent Object. A NULL String
 * reference is also an actual NULL Object, regardless of its native pointer
 * alternative. Unknown tags have no proved Java runtime class. */
static inline mc_entity_watch_payload_kind
mc_entity_watch_payload_kind_of(const mc_entity_watch_record *record) {
    return record->payload_kind == MC_ENTITY_WATCH_PAYLOAD_STRING && record->string_value == NULL
               ? MC_ENTITY_WATCH_PAYLOAD_NULL
               : record->payload_kind;
}

static inline bool mc_entity_watch_payload_known(const mc_entity_watch_record *record) {
    const mc_entity_watch_payload_kind kind = mc_entity_watch_payload_kind_of(record);
    return kind == MC_ENTITY_WATCH_PAYLOAD_NULL || kind == MC_ENTITY_WATCH_PAYLOAD_BYTE ||
           kind == MC_ENTITY_WATCH_PAYLOAD_INTEGER || kind == MC_ENTITY_WATCH_PAYLOAD_STRING;
}

static inline mc_entity_watch_payload_result
mc_entity_watch_payload_check(const mc_entity_watch_record *record,
                              mc_entity_watch_payload_kind expected) {
    const mc_entity_watch_payload_kind kind = mc_entity_watch_payload_kind_of(record);
    if (kind == expected ||
        (kind == MC_ENTITY_WATCH_PAYLOAD_NULL && expected == MC_ENTITY_WATCH_PAYLOAD_STRING))
        return MC_ENTITY_WATCH_PAYLOAD_OK;
    if (kind == MC_ENTITY_WATCH_PAYLOAD_NULL)
        return MC_ENTITY_WATCH_PAYLOAD_NULL_UNBOX;
    return mc_entity_watch_payload_known(record) ? MC_ENTITY_WATCH_PAYLOAD_WRONG_CLASS
                                                 : MC_ENTITY_WATCH_PAYLOAD_UNREPRESENTED;
}

static inline const struct mc_player_string *
mc_entity_watch_string_value(const mc_entity_watch_record *record) {
    return mc_entity_watch_payload_kind_of(record) == MC_ENTITY_WATCH_PAYLOAD_STRING
               ? record->string_value
               : NULL;
}

/* New Byte/Integer.equals(old Object) determines replacement. These helpers
 * run only after callers prove a represented old Object and their real Map /
 * record lookup. Descriptor metadata is never changed by an Object store. */
static inline bool mc_entity_watch_byte_replace(mc_entity_watch_record *record, int8_t value) {
    if (record->payload_kind == MC_ENTITY_WATCH_PAYLOAD_BYTE && record->byte_value == value)
        return false;
    record->payload_kind = MC_ENTITY_WATCH_PAYLOAD_BYTE;
    record->byte_value = value;
    record->dirty = true;
    return true;
}

static inline bool mc_entity_watch_integer_replace(mc_entity_watch_record *record, int32_t value) {
    if (record->payload_kind == MC_ENTITY_WATCH_PAYLOAD_INTEGER && record->integer_value == value)
        return false;
    record->payload_kind = MC_ENTITY_WATCH_PAYLOAD_INTEGER;
    record->integer_value = value;
    record->dirty = true;
    return true;
}

#endif
