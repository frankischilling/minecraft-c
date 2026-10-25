#ifndef MC_TEST_ENTITY_TURN_FIXTURE_H
#define MC_TEST_ENTITY_TURN_FIXTURE_H

#include "mc/entity/movement/turn.h"

#include "entity_internal.h"

#include <stddef.h>
#include <string.h>

/* Quiescent observation of actual public-constructed Entity owners. Only the
 * four original public angle fields are prepared by the finite test inputs;
 * phase, class, holds, IDs, child owners and constructor entropy are untouched. */
typedef struct turn_guard {
    unsigned char entity[sizeof(mc_entity)];
    unsigned char bounds[sizeof(mc_entity_bounds)];
    unsigned char random[sizeof(mc_java_random)];
    mc_entity_watcher_snapshot watcher;
    mc_entity_class_snapshot classes;
    size_t profile_bytes;
} turn_guard;

static inline uint32_t turn_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static inline float turn_float(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static inline bool turn_capture(mc_entity *entity, turn_guard *guard) {
    if (entity == NULL || entity->bounds == NULL || entity->random == NULL ||
        mc_entity_classes_get_snapshot(entity->classes, &guard->classes) != MC_ENTITY_OK ||
        mc_entity_watcher_get_snapshot(entity->watcher, &guard->watcher) != MC_ENTITY_OK)
        return false;
    memcpy(guard->entity, entity, sizeof(*entity));
    memcpy(guard->bounds, entity->bounds, sizeof(*entity->bounds));
    memcpy(guard->random, entity->random, sizeof(*entity->random));
    guard->profile_bytes = mc_java_random_profile_allocation_bytes(entity->classes->profile);
    return true;
}

static inline bool turn_unchanged_other_fields(mc_entity *entity, const turn_guard *before) {
    turn_guard after;
    if (!turn_capture(entity, &after))
        return false;
    const size_t offsets[] = {
        offsetof(mc_entity_base_fields, yaw), offsetof(mc_entity_base_fields, aT),
        offsetof(mc_entity_base_fields, aU), offsetof(mc_entity_base_fields, aV)};
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        const size_t offset = offsetof(mc_entity, base) + offsets[i];
        memcpy(after.entity + offset, before->entity + offset, sizeof(float));
    }
    const mc_entity_class_snapshot *a = &after.classes, *b = &before->classes;
    const mc_entity_watcher_snapshot *wa = &after.watcher, *wb = &before->watcher;
    return memcmp(after.entity, before->entity, sizeof(after.entity)) == 0 &&
           memcmp(after.bounds, before->bounds, sizeof(after.bounds)) == 0 &&
           memcmp(after.random, before->random, sizeof(after.random)) == 0 &&
           after.profile_bytes == before->profile_bytes && a->source == b->source &&
           a->entity_class == b->entity_class && a->bounds_class == b->bounds_class &&
           a->watcher_class == b->watcher_class && a->next_id == b->next_id &&
           a->bounds_pool_cursor == b->bounds_pool_cursor &&
           a->bounds_pool_identity == b->bounds_pool_identity &&
           a->watcher_types_identity == b->watcher_types_identity &&
           a->watcher_type_count == b->watcher_type_count && a->live_entities == b->live_entities &&
           a->allocation_bytes == b->allocation_bytes && wa->dirty == wb->dirty &&
           wa->size == wb->size && wa->record == wb->record && wa->index == wb->index &&
           wa->type == wb->type && wa->byte_value == wb->byte_value &&
           wa->record_dirty == wb->record_dirty;
}

static inline void turn_prepare(mc_entity *entity, const uint32_t angles[4]) {
    entity->base.yaw = turn_float(angles[0]);
    entity->base.aT = turn_float(angles[1]);
    entity->base.aU = turn_float(angles[2]);
    entity->base.aV = turn_float(angles[3]);
}

#endif
