#include "ally_alert_occurrences_internal.h"

#include "../../world/runtime/runtime_internal.h"
#include "../entity_internal.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct ally_occurrence_count {
    mc_entity *entity;
    size_t required;
} ally_occurrence_count;

enum { ALLY_OPTIONAL_SCRATCH_BYTES = 1024u * 1024u };

static bool quadratic(const mc_player_ally_alert_state *s, bool player_held, bool target_held,
                      size_t count) {
    for (size_t i = 0; i < count; ++i) {
        mc_entity *entity = NULL;
        if (mc_world_class_entity_list_get(s->wolves, i, &entity) != MC_WORLD_STORAGE_OK)
            return false;
        size_t holds = (size_t)(player_held && s->player == entity) +
                       (size_t)(target_held && s->target == entity);
        for (size_t j = 0; j < count; ++j) {
            mc_entity *other = NULL;
            if (mc_world_class_entity_list_get(s->wolves, j, &other) != MC_WORLD_STORAGE_OK)
                return false;
            if (other == entity)
                ++holds;
        }
        if (entity->holds < holds)
            return false;
    }
    return true;
}

static size_t pointer_bucket(const mc_entity *entity, size_t mask) {
    /* Only identity is counted. No source Entity.equals/hashCode is invoked. */
    uintptr_t value = (uintptr_t)entity;
    value ^= value >> 17;
    value *= UINT64_C(0x9e3779b97f4a7c15);
    value ^= value >> 32;
    return (size_t)value & mask;
}

bool mc_player_ally_alert_occurrences_valid(const mc_player_ally_alert_state *s, bool player_held,
                                            bool target_held) {
    if ((player_held && s->player->holds < 1u + (size_t)(s->player == s->target)) ||
        (target_held && s->target->holds < 1u + (size_t)(s->target == s->player)))
        return false;
    if (s->wolves == NULL)
        return true;
    size_t count;
    if (mc_world_class_entity_list_count(s->wolves, &count) != MC_WORLD_STORAGE_OK ||
        s->world->local_leases == 0)
        return false;
    if (count <= 8)
        return quadratic(s, player_held, target_held, count);

    /* At most half full. Extent/allocation refusal retains the old native
     * behavior rather than making cleanup depend on another allocation. */
    if (count > SIZE_MAX / 2u)
        return quadratic(s, player_held, target_held, count);
    size_t capacity = 16;
    while (capacity < count * 2u) {
        if (capacity > SIZE_MAX / 2u)
            return quadratic(s, player_held, target_held, count);
        capacity *= 2u;
    }
    if (capacity > SIZE_MAX / sizeof(ally_occurrence_count) ||
        capacity > ALLY_OPTIONAL_SCRATCH_BYTES / sizeof(ally_occurrence_count))
        return quadratic(s, player_held, target_held, count);
    ally_occurrence_count *table = calloc(capacity, sizeof(*table));
    if (table == NULL)
        return quadratic(s, player_held, target_held, count);

    bool valid = true;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *entity = NULL;
        if (mc_world_class_entity_list_get(s->wolves, i, &entity) != MC_WORLD_STORAGE_OK) {
            valid = false;
            break;
        }
        size_t slot = pointer_bucket(entity, capacity - 1u);
        while (table[slot].entity != NULL && table[slot].entity != entity)
            slot = (slot + 1u) & (capacity - 1u);
        if (table[slot].entity == NULL) {
            table[slot].entity = entity;
            table[slot].required = (size_t)(player_held && s->player == entity) +
                                   (size_t)(target_held && s->target == entity);
        }
        ++table[slot].required;
    }
    for (size_t i = 0; valid && i < capacity; ++i)
        if (table[i].entity != NULL && table[i].entity->holds < table[i].required)
            valid = false;
    free(table);
    return valid;
}
